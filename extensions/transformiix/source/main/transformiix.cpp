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
 *    -- read XML from stdin when -i is omitted
 *    -- accept '-' to specify stdin/stdout on command line
 *
 */


#include "XSLTProcessor.h"
#include "CommandLineUtils.h"

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

    if (!txInit())
        return 1;
    XSLTProcessor xsltProcessor;

    //-- add ErrorObserver
    SimpleErrorObserver seo;
    xsltProcessor.addErrorObserver(seo);

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
        cerr << xsltProcessor.getAppName() << " ";
        cerr << xsltProcessor.getAppVersion() << endl;
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


    //-- open XML file
    istream* xmlInput = &cin;
    if (xmlFilename && ! xmlFilename->isEqual("-")) {
      xmlInput = new ifstream(NS_LossyConvertUCS2toASCII(*xmlFilename).get(),
                              ios::in);
    }

    //-- handle output stream
    ostream* resultOutput = &cout;
    ofstream resultFileStream;
    if ( outFilename && ! outFilename->isEqual("-")) {
        resultFileStream.open(NS_LossyConvertUCS2toASCII(*outFilename).get(),
                              ios::out);
        if ( !resultFileStream ) {
            cerr << "error opening output file: " << *xmlFilename << endl;
            return -1;
        }
        resultOutput = &resultFileStream;
    }

    //-- process
    String documentBase;
    if ( !xsltFilename ) {
      if (!xmlFilename) {
        cerr << "you must specify XSLT file with -s option if XML is read from standard input" << endl;
        printUsage();
        return -1;
      }
      xsltProcessor.process(*xmlInput, *xmlFilename, *resultOutput);
    }
    else {
        //-- open XSLT file
        ifstream xsltInput(NS_LossyConvertUCS2toASCII(*xsltFilename).get(),
                           ios::in);
        xsltProcessor.process(*xmlInput, *xmlFilename, xsltInput, *xsltFilename, *resultOutput);
    }
    resultFileStream.close();
    if (xmlInput != &cin)
        delete xmlInput;
    txShutdown();
    return 0;
} //-- main

void printHelp() {
  cerr << "transfrmx [-h] [-i xml-file] [-s xslt-file] [-o output-file]" << endl << endl;
  cerr << "Options:";
  cerr << endl << endl;
  cerr << "\t-i  specify XML file to process (default: read from stdin)" << endl;
  cerr << "\t-s  specify XSLT file to use for processing (default: use stylesheet" << endl
       << "\t\tspecified in XML file)" << endl;
  cerr << "\t-o  specify output file (default: write to stdout)" << endl;
  cerr << "\t-h  this help screen" << endl;
  cerr << endl;
  cerr << "You may use '-' in place of xml-file or output-file to explicitly specify" << endl;
  cerr << "respectively the standard input and standard output." << endl;
  cerr << "If the XML is read from stdin, then the -s option is mandatory." << endl;
  cerr << endl;
}
void printUsage() {
  cerr << "transfrmx [-h] [-i xml-file] [-s xslt-file] [-o output-file]" << endl << endl;
  cerr << "For more infomation use the -h flag"<<endl;
} //-- printUsage

