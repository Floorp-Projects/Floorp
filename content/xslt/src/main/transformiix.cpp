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
#include "CommandLineUtils.h"
#include "nsXPCOM.h"
#include <fstream.h>

  //--------------/
 //- Prototypes -/
//--------------/

/**
 * Prints the command line help screen to the console
**/
void printHelp();

/**
 * prints the command line usage information to the console
**/
void printUsage();

/**
 * The TransforMiiX command line interface
**/
int main(int argc, char** argv) {

    nsresult rv;
    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!txXSLTProcessor::txInit())
        return 1;

    //-- available flags
    StringList flags;
    flags.add(new String("i"));          // XML input
    flags.add(new String("s"));          // XSL input
    flags.add(new String("o"));          // Output filename
    flags.add(new String("h"));          // help
    flags.add(new String("q"));          // quiet

    NamedMap options;
    options.setObjectDeletion(MB_TRUE);
    CommandLineUtils::getOptions(options, argc, argv, flags);

    if (!options.get(String("q"))) {
        String copyright("(C) 1999 The MITRE Corporation, Keith Visco, and contributors");
        cerr << "TransforMiiX ";
        cerr << "1.2b pre" << endl;
        cerr << copyright << endl;
        //-- print banner line
        PRUint32 fillSize = copyright.length() + 1;
        PRUint32 counter;
        for (counter = 0; counter < fillSize; ++counter)
            cerr << '-';
        cerr << endl << endl;
    }

    if (options.get(String("h"))) {
        printHelp();
        return 0;
    }
    String* xmlFilename = (String*)options.get(String("i"));
    String* xsltFilename = (String*)options.get(String("s"));
    String* outFilename = (String*)options.get(String("o"));

    //-- handle output stream
    ostream* resultOutput = &cout;
    ofstream resultFileStream;
    if (outFilename && !outFilename->isEqual("-")) {
        resultFileStream.open(NS_LossyConvertUCS2toASCII(*outFilename).get(),
                              ios::out);
        if (!resultFileStream) {
            cerr << "error opening output file: " << *xmlFilename << endl;
            return -1;
        }
        resultOutput = &resultFileStream;
    }

    SimpleErrorObserver obs;
    txStandaloneXSLTProcessor proc;
    //-- process
    if (!xsltFilename) {
        if (!xmlFilename) {
            cerr << "you must specify at least a source XML path" << endl;
            printUsage();
            return -1;
        }
        rv = proc.transform(*xmlFilename, *resultOutput, obs);
    }
    else {
        //-- open XSLT file
        rv = proc.transform(*xmlFilename, *xsltFilename, *resultOutput, obs);
    }
    resultFileStream.close();
    txXSLTProcessor::txShutdown();
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    return 0;
} //-- main

void printHelp() {
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
void printUsage() {
  cerr << "transfrmx [-h] [-i xml-file] [-s xslt-file] [-o output-file]" << endl << endl;
  cerr << "For more infomation use the -h flag"<<endl;
} //-- printUsage

