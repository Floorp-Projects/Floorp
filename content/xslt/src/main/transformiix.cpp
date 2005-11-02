/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Nathan Pride, npride@wavo.com
 *    -- fixed document base when stylesheet is specified,
 *       it was defaulting to the XML document.
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *    -- redirect non-data output (banner, errors) to stderr
 *
 */


#include "txStandaloneXSLTProcessor.h"
#include "nsXPCOM.h"
#include <fstream.h>
#include "nsDoubleHashtable.h"
#include "nsVoidArray.h"

/**
 * Prints the command line help screen to the console
 */
void printHelp()
{
  cerr << "transfrmx [-h] [-i xml-file] [-s xslt-file] [-o output-file]" << endl << endl;
  cerr << "Options:";
  cerr << endl << endl;
  cerr << "\t-i  specify XML file to process" << endl;
  cerr << "\t-s  specify XSLT file to use for processing (default: use stylesheet" << endl
       << "\t\tspecified in XML file)" << endl;
  cerr << "\t-o  specify output file (default: write to stdout)" << endl;
  cerr << "\t-h  this help screen" << endl;
  cerr << endl;
  cerr << "You may use '-' in place of the output-file to explicitly specify" << endl;
  cerr << "standard output." << endl;
  cerr << endl;
}

/**
 * Prints the command line usage information to the console
 */
void printUsage()
{
  cerr << "transfrmx [-h] [-i xml-file] [-s xslt-file] [-o output-file]" << endl << endl;
  cerr << "For more infomation use the -h flag" << endl;
}

class txOptionEntry : public PLDHashCStringEntry
{
public:
    txOptionEntry(const void* aKey) : PLDHashCStringEntry(aKey)
    {
    }
    ~txOptionEntry()
    {
    }
    nsCStringArray mValues;
};

DECL_DHASH_WRAPPER(txOptions, txOptionEntry, nsACString&)
DHASH_WRAPPER(txOptions, txOptionEntry, nsACString&)

/**
 * Parses the command line
 */
void parseCommandLine(int argc, char** argv, txOptions& aOptions)
{
    nsCAutoString flag;

    for (int i = 1; i < argc; ++i) {
        nsDependentCString arg(argv[i]);
        if (*argv[i] == '-' && arg.Length() > 1) {
            // clean up previous flag
            if (!flag.IsEmpty()) {
                aOptions.AddEntry(flag);
                flag.Truncate();
            }

            // get next flag
            flag = Substring(arg, 1, arg.Length() - 1);
        }
        else {
            txOptionEntry* option = aOptions.AddEntry(flag);
            if (option) {
                option->mValues.AppendCString(nsCString(arg));
            }
            flag.Truncate();
        }
    }

    if (!flag.IsEmpty()) {
        aOptions.AddEntry(flag);
    }
}

/**
 * The TransforMiiX command line interface
 */
int main(int argc, char** argv)
{
    using namespace std;
    nsresult rv;
    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!txXSLTProcessor::init())
        return 1;

    txOptions options;
    if (NS_FAILED(options.Init(4))) {
        return 1;
    }
    parseCommandLine(argc, argv, options);

    if (!options.GetEntry(NS_LITERAL_CSTRING("q"))) {
        NS_NAMED_LITERAL_CSTRING(copyright, "(C) 1999 The MITRE Corporation, Keith Visco, and contributors");
        cerr << "TransforMiiX ";
        cerr << MOZILLA_VERSION << endl;
        cerr << copyright.get() << endl;
        //-- print banner line
        PRUint32 fillSize = copyright.Length() + 1;
        PRUint32 counter;
        for (counter = 0; counter < fillSize; ++counter)
            cerr << '-';
        cerr << endl << endl;
    }

    if (options.GetEntry(NS_LITERAL_CSTRING("h"))) {
        printHelp();
        return 0;
    }

    //-- handle output stream
    ostream* resultOutput = &cout;
    ofstream resultFileStream;

    txOptionEntry* option = options.GetEntry(NS_LITERAL_CSTRING("o"));
    if (option &&
        option->mValues.Count() > 0 &&
        !option->mValues[0]->Equals(NS_LITERAL_CSTRING("-"))) {
        resultFileStream.open(option->mValues[0]->get(), ios::out);
        if (!resultFileStream) {
            cerr << "error opening output file: ";
            cerr << option->mValues[0]->get() << endl;
            return -1;
        }
        resultOutput = &resultFileStream;
    }

    option = options.GetEntry(NS_LITERAL_CSTRING("i"));
    if (!option || option->mValues.Count() == 0) {
        cerr << "you must specify at least a source XML path" << endl;
        printUsage();
        return -1;
    }

    SimpleErrorObserver obs;
    txStandaloneXSLTProcessor proc;

    txOptionEntry* styleOption = options.GetEntry(NS_LITERAL_CSTRING("s"));
    if (!styleOption || styleOption->mValues.Count() == 0) {
        rv = proc.transform(*option->mValues[0], *resultOutput, obs);
    }
    else {
        // XXX TODO: Handle multiple stylesheets
        rv = proc.transform(*option->mValues[0], *styleOption->mValues[0],
                            *resultOutput, obs);
    }

    if (NS_FAILED(rv)) {
        cerr << "transformation failed with " << hex << rv << endl;
    }

    resultFileStream.close();
    txXSLTProcessor::shutdown();
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    return 0;
}
