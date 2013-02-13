/*
 * BioBloomCategorizer.cpp
 *
 *  Created on: Sep 7, 2012
 *      Author: cjustin
 */
//todo: UNIT TESTS!
#include <sstream>
#include <string>
#include <getopt.h>
#include "boost/unordered/unordered_map.hpp"
#include "Common/HashManager.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include "BioBloomClassifier.h"
#include "DataLayer/Options.h"
using namespace std;

/*
 * Parses input string into separate strings, returning a vector.
 */
vector<string> convertInputString(const string &inputString)
{
	vector<string> currentInfoFile;
	string temp;
	stringstream converter(inputString);
	while (converter >> temp) {
		currentInfoFile.push_back(temp);
	}
	return currentInfoFile;
}

void folderCheck(const string &path)
{
	struct stat sb;

	if (stat(path.c_str(), &sb) == 0) {
		if (!S_ISDIR(sb.st_mode)) {
			cerr << "Error: Output folder - file exists with this name. "
					<< path << endl;
			exit(1);
		}
	} else {
		cerr << "Error: Output folder does not exist. " << path << endl;
		exit(1);
	}
}

void printHelpDialog()
{
	static const char dialog[] =
			"Usage: BioBloomCategorizer [OPTION]... -f \"[FILTER1] [FILTER2]...\" [FILE]...\n"
					"Categorize Sequences. The input format may be FASTA, FASTQ, qseq,\n"
					"export, SAM or BAM format and compressed with gz, bz2 or xz and\n"
					"may be tarred.\n"
					"\n"
					"  -p, --prefix=N         Output prefix to use. Otherwise will output\n"
					"                         to current directory.\n"
					"  -t, --min_hit_thr=N    Minimum Hit Threshold Value. The absolute\n"
					"                         hit number needed for a hit to be considered\n"
					"                         a match.[2]\n"
					"  -m, --min_hit_pro=N    Minimum Hit Proportion Threshold Value. The\n"
					"                         Proportion needed for a hit to be considered\n"
					"                         a match. [0.2]\n"
					"  -f, --filter_files=N   List of filter files to use. Required option.\n"
					"                         Eg. \"filter1.bf filter2.bf\"\n"
					"  -o, --output_fastq     Output categorized reads in FastQ files.\n"
					"  -e, --paired_mode      Uses paired-end information. Does not work\n"
					"                         with BAM or SAM files.\n"
					"  -c, --counts=N         Outputs summary of raw counts of user\n"
					"                         specified hit counts to each filter of each\n"
					"                         read or read-pair. [0]\n"
					"      --chastity         Discard and do not evaluate unchaste reads.\n"
					"      --no-chastity      Do not discard and evaluate unchaste reads.\n"
					"                         [default]\n"
					"  -h, --help             Display this dialog.\n"
					"\n"
					"Report bugs to <cjustin@bcgsc.ca>.";
	cerr << dialog << endl;
	exit(0);
}

int main(int argc, char *argv[])
{
	opt::chastityFilter = false;

	//switch statement variable
	int c;

	//command line variables
	string rawInputFiles = "";
	string outputPrefix = "";
	string filtersFile = "";
	size_t minHit = 2;
	double percentHit = 0.2;
	bool printReads = false;
	bool die = false;
	bool paired = false;
	size_t rawCounts = 0;

	//todo Finish chasity options
	//long form arguments
	static struct option long_options[] = {
			{
					"prefix", optional_argument, NULL, 'p' }, {
					"min_hit_thr", optional_argument, NULL, 't' }, {
					"min_hit_per", optional_argument, NULL, 'm' }, {
					"output_fastq", no_argument, NULL, 'o' }, {
					"filter_files", required_argument, NULL, 'f' }, {
					"paired_mode", no_argument, NULL, 'e' }, {
					"counts", no_argument, NULL, 'c' }, {
					"help", no_argument, NULL, 'h' }, {
					"chastity", no_argument, NULL }, {
					"no-chastity", no_argument, NULL }, {
					NULL, 0, NULL, 0 } };

	//actual checking step
	//Todo: add checks for duplicate options being set
	int option_index = 0;
	while ((c = getopt_long(argc, argv, "f:t:om:p:hec:", long_options,
			&option_index)) != -1)
	{
		switch (c) {
		case 'm': {
			stringstream convert(optarg);
			if (!(convert >> percentHit)) {
				cerr << "Error - Invalid parameter! m: " << optarg << endl;
				return 0;
			}
			if (percentHit > 1) {
				cerr << "Error -m cannot be greater than 1 " << optarg << endl;
				return 0;
			}
			break;
		}
		case 't': {
			stringstream convert(optarg);
			if (!(convert >> minHit)) {
				cerr << "Error - Invalid parameter! t: " << optarg << endl;
				return 0;
			}
			break;
		}
		case 'f': {
			filtersFile = optarg;
			break;
		}
		case 'p': {
			outputPrefix = optarg;
			break;
		}
		case 'h': {
			printHelpDialog();
			break;
		}
		case 'o': {
			printReads = true;
			break;
		}
		case 'e': {
			paired = true;
			break;
		}
		case 'c': {
			stringstream convert(optarg);
			if (!(convert >> rawCounts)) {
				cerr << "Error - Invalid parameter! c: " << optarg << endl;
				return 0;
			}
			break;
		}
		default: {
			die = true;
			break;
		}
		}
	}

	vector<string> filterFilePaths = convertInputString(filtersFile);
	vector<string> inputFiles = convertInputString(rawInputFiles);

	while (optind < argc) {
		inputFiles.push_back(argv[optind]);
		optind++;
	}

	//check validity of inputs for paired end mode
	if (paired) {
		if (inputFiles.size() != 2
				|| inputFiles[0].substr(inputFiles[0].size() - 4) == "bam"
				|| inputFiles[1].substr(inputFiles[0].size() - 4) == "bam")
		{
			cerr << "Usage of paired end mode:\n"
					<< "BioBloomCategorizer [OPTION]... -f \"[FILTER1] [FILTER2]...\" [FILEPAIR1] [FILEPAIR2]\n"
					<< "BAM or SAM files do not currently work with this option."
					<< endl;
			exit(1);
		}
	}

	//Check needed options
	if (inputFiles.size() == 0) {
		cerr << "Error: Need Input File" << endl;
		die = true;
	}
	if (filterFilePaths.size() == 0) {
		cerr << "Error: Need Filter File (-f)" << endl;
		die = true;
	}
	if (die) {
		cerr << "Try '--help' for more information.\n";
		exit(EXIT_FAILURE);
	}
	//check if output folder exists
	if (outputPrefix.find('/') != string::npos) {
		string tempStr = outputPrefix.substr(0, outputPrefix.find_last_of("/"));
		folderCheck(tempStr);
	}

	//load filters
	BioBloomClassifier BBC(filterFilePaths, minHit, percentHit, rawCounts);

	//filtering step
	//create directory structure if it does not exist
	if (paired) {
		if (printReads) {
			BBC.filterPairPrint(inputFiles[0], inputFiles[1], outputPrefix);
		} else {
			BBC.filterPair(inputFiles[0], inputFiles[1], outputPrefix);
		}
	} else {
		if (printReads) {
			BBC.filterPrint(inputFiles, outputPrefix);
		} else {
			BBC.filter(inputFiles, outputPrefix);
		}
	}

}
