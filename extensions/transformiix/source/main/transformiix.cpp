/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

#include "XSLProcessor.h"

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
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
int main(int argc, char** argv) {

    XSLProcessor xslProcessor;

    String copyright("(C) 1999 The MITRE Corporation");
    cout << xslProcessor.getAppName() << " ";
    cout << xslProcessor.getAppVersion() << copyright <<endl;

    //-- print banner line
    Int32 fillSize = 1;
    fillSize += xslProcessor.getAppName().length();
    fillSize += xslProcessor.getAppVersion().length() + copyright.length();
    String fill;
    fill.setLength(fillSize, '-');
    cout << fill <<endl<<endl;

    //-- add ErrorObserver
    SimpleErrorObserver seo;
    xslProcessor.addErrorObserver(seo);

    //-- available flags
    StringList flags;
    flags.add(new String("i"));          // XML input
    flags.add(new String("s"));          // XSL input
    flags.add(new String("o"));          // Output filename

    NamedMap options;
    options.setObjectDeletion(MB_TRUE);
    CommandLineUtils::getOptions(options, argc, argv, flags);

    if ( options.get("h") ) {
        printHelp();
        return 0;
    }
    String* xmlFilename = (String*)options.get("i");
    String* xslFilename = (String*)options.get("s");
    String* outFilename = (String*)options.get("o");

    if ( !xmlFilename ) {
        cout << " missing XML filename."<<endl <<endl;
        printUsage();
        return -1;
    }
    char* chars = 0;

    //-- open XML file
    chars = new char[xmlFilename->length()+1];
    ifstream xmlInput(xmlFilename->toChar(chars), ios::in);
    delete chars;

    //-- create document base
    String documentBase;
    URIUtils::getDocumentBase(*xmlFilename, documentBase);

    //-- handle output stream
    ostream* resultOutput = &cout;
    ofstream resultFileStream;
    if ( outFilename ) {
        chars = new char[outFilename->length()+1];
        resultFileStream.open(outFilename->toChar(chars), ios::out);
        delete chars;
        if ( !resultFileStream ) {
            cout << "error opening output file: " << *xmlFilename << endl;
            return -1;
        }
        resultOutput = &resultFileStream;
    }
    //-- process
    if ( !xslFilename ) {
        xslProcessor.process(xmlInput, documentBase, *resultOutput);
    }
    else {
        //-- open XSL file
        chars = new char[xslFilename->length()+1];
        ifstream xslInput(xslFilename->toChar(chars), ios::in);
        delete chars;
        xslProcessor.process(xmlInput, xslInput, *resultOutput);
    }
    resultFileStream.close();
    return 0;
} //-- main

void printHelp() {
    cout << "The following flags are available for use with TransforMiiX -";
    cout<<endl<<endl;
    cout << "-i  filename  : The XML file to process" << endl;
    cout << "-o  filename  : The Output file to create" << endl;
    cout << "-s  filename  : The XSL file to use for processing  (Optional)" << endl;
    cout << "-h            : This help screen                    (Optional)" << endl;
    cout << endl;
}
void printUsage() {
    cout << endl;
    cout << "usage:";
    cout << "transfrmx -i xml-file [-s xsl-file] [-o output-file]"<<endl;
    cout << endl;
    cout << "for more infomation use the -h flag"<<endl;
} //-- printUsage

