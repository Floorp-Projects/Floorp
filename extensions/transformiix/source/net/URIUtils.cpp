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
//package com.kvisco.net;

#include "URIUtils.h"

/**
 * URIUtils
 * A set of utilities for handling URIs
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *  - moved initialization of contanst shorts and chars from
 *    URIUtils.h to here
 * </PRE>
 *
**/

//- Constants -/

const String URIUtils::HTTP_PROTOCOL  = "http";
const String URIUtils::FILE_PROTOCOL  = "file";
const char   URIUtils::HREF_PATH_SEP  = '/';
const char   URIUtils::DEVICE_SEP     = '|';
const char   URIUtils::PORT_SEP       = ':';
const char   URIUtils::PROTOCOL_SEP   = ':';
const short  URIUtils::PROTOCOL_MODE  = 1;
const short  URIUtils::HOST_MODE      = 2;
const short  URIUtils::PORT_MODE      = 3;
const short  URIUtils::PATH_MODE      = 4;


/**
 * Returns an InputStream for the file represented by the href
 * argument
 * @param href the href of the file to get the input stream for.
 * @param documentBase the document base of the href argument, if it
 * is a relative href
 * set documentBase to null if there is none.
 * @return an InputStream to the desired resource
 * @exception java.io.FileNotFoundException when the file could not be
 * found
**/
istream* URIUtils::getInputStream
    (String& href, String& documentBase, String& errMsg)
{

    istream* inStream = 0;

    //-- check for URL
    ParsedURI* uri = parseURI(href);
    if ( !uri->isMalformed ) {
        inStream = openStream(uri);
        delete uri;
        return inStream;
    }
    delete uri;

    //-- join document base + href
    String xHref;
    if (documentBase.length() > 0) {
        xHref.append(documentBase);
        if (documentBase.charAt(documentBase.length()-1) != HREF_PATH_SEP)
            xHref.append(HREF_PATH_SEP);
    }
    xHref.append(href);

    //-- check new href
    uri = parseURI(xHref);
    if ( !uri->isMalformed ) {
        inStream = openStream(uri);
    }
    else {
        // Try local files
        char* fchars = new char[xHref.length()+1];
        ifstream* inFile = new ifstream(xHref.toChar(fchars), ios::in);
        delete fchars;
        if ( ! *inFile ) {
            fchars = new char[href.length()+1];
            (*inFile).open(href.toChar(fchars), ios::in);
            delete fchars;
        }
        inStream = inFile;
    }
    delete uri;

    return inStream;

} //-- getInputStream

/**
    * Returns the document base of the href argument
    * @return the document base of the given href
**/
void URIUtils::getDocumentBase(String& href, String& dest) {

    //-- use temp str so the subString method doesn't destroy dest
    String docBase("");

    if (href.length() != 0) {

        int idx = -1;
        //-- check for URL
        ParsedURI* uri = parseURI(href);
        if ( !uri->isMalformed ) {
            idx = href.lastIndexOf(HREF_PATH_SEP);
        }
        else {
            //-- The following contains a fix from Shane Hathaway
            //-- to handle the case when both "\" and "/" appear in filename
            int idx2 = href.lastIndexOf(HREF_PATH_SEP);
            //idx = href.lastIndexOf(File.separator);
            idx = -1; //-- hack change later
            if (idx2 > idx) idx = idx2;
        }
        if (idx >= 0) href.subString(0,idx, docBase);
        delete uri;
    }
    dest.append(docBase);

} //-- getDocumentBase

/**
 * Resolves the given href argument, using the given documentBase
 * if necessary.
 * The new resolved href will be appended to the given dest String
**/
void URIUtils::resolveHref(String& href, String& documentBase, String& dest) {


    //-- check for URL
    ParsedURI* uri = parseURI(href);
    if ( !uri->isMalformed ) {
        dest.append(href);
        delete uri;
        return;
    }


    //-- join document base + href
    String xHref;
    if (documentBase.length() > 0) {
        xHref.append(documentBase);
        if (documentBase.charAt(documentBase.length()-1) != HREF_PATH_SEP)
            xHref.append(HREF_PATH_SEP);
    }
    xHref.append(href);

    //-- check new href
    ParsedURI* newUri = parseURI(xHref);
    if ( !newUri->isMalformed ) {
        dest.append(xHref);
    }
    else {
        // Try local files
        char* xHrefChars = new char[xHref.length()+1];
        ifstream inFile(xHref.toChar(xHrefChars), ios::in);
        if ( inFile ) dest.append(xHref);
        else dest.append(href);
        inFile.close();
        delete xHrefChars;
    }
    delete uri;
    delete newUri;

} //-- resolveHref

istream* URIUtils::openStream(ParsedURI* uri) {
    if ( !uri ) return 0;
    // check protocol

    istream* inStream = 0;
    if ( FILE_PROTOCOL.isEqual(uri->protocol) ) {
        char* fchars = new char[uri->path.length()+1];
        ifstream* inFile = new ifstream(uri->path.toChar(fchars), ios::in);
        delete fchars;
        inStream = inFile;
    }

    return inStream;
} //-- openStream

/*  */

URIUtils::ParsedURI* URIUtils::parseURI(const String& uri) {

    ParsedURI* uriTokens = new ParsedURI;
    uriTokens->isMalformed = MB_FALSE;

    short mode = PROTOCOL_MODE;

    // look for protocol
    int totalCount = uri.length();
    int charCount = 0;
    char prevCh = '\0';
    int fslash = 0;
    String buffer(uri.length());
    while ( charCount < totalCount ) {
        char ch = uri.charAt(charCount++);
        switch(ch) {
            case '.' :
                if ( mode == PROTOCOL_MODE ) {
                    uriTokens->isMalformed = MB_TRUE;
                    mode = HOST_MODE;
                }
                buffer.append(ch);
                break;
            case ':' :
            {
                switch ( mode ) {
                    case PROTOCOL_MODE :
                        uriTokens->protocol = buffer;
                        buffer.clear();
                        mode = HOST_MODE;
                        break;
                    case HOST_MODE :

                        uriTokens->host = buffer;
                        buffer.clear();
                        mode = PORT_MODE;
                        break;
                    default:
                        break;
                }
                break;
            }
            case '/' :
                switch ( mode ) {
                    case HOST_MODE :
                        if ( buffer.length() != 0 ) {
                            mode = PATH_MODE;
                            buffer.append(ch);
                        }
                        else if ( fslash == 2 ) mode = PATH_MODE;
                        else ++fslash;
                        break;
                    case PORT_MODE :
                        mode = PATH_MODE;
                        uriTokens->port.append(buffer);
                        buffer.clear();
                        break;
                    default:
                        buffer.append(ch);
                        break;
                }
                break;
            default:
                buffer.append(ch);
        }
        prevCh = ch;
    }

    if ( mode == PROTOCOL_MODE ) {
        uriTokens->isMalformed = MB_TRUE;
    }
    //-- finish remaining mode
    if ( buffer.length() > 0 ) {
        switch ( mode ) {
            case PROTOCOL_MODE :
                uriTokens->protocol.append(buffer);
                break;
            case HOST_MODE :
                uriTokens->host.append(buffer);
                break;
            case PORT_MODE :
                uriTokens->port.append(buffer);
                break;
            case PATH_MODE :
                uriTokens->path.append(buffer);
                break;
            default:
                break;
        }
    }
    return uriTokens;
} //-- parseURI

/**
 *
**
void URIUtils::test(const String& str) {
    cout << "parsing: " << str << endl;
    ParsedURI* uri = parseURI(str);
    cout << "protocol : " << uri->protocol << endl;
    cout << "host     : " << uri->host << endl;
    cout << "port     : " << uri->port << endl;
    cout << "path     : " << uri->path << endl;
    cout << "malformed: " << uri->isMalformed << endl;
    delete uri;
} //-- test

/**
 * The test class for the URIUtils
**
void main(int argc, char** argv) {
    URIUtils::test("file:///c|\\test");
    URIUtils::test("http://my.domain.com");
    URIUtils::test("my.domain.com");
    URIUtils::test("http://my.domain.com:80");
    URIUtils::test("http://my.domain.com:88/foo.html");

    String url("http://my.domain.com:88/foo.html");
    String docBase;
    URIUtils::getDocumentBase(url, docBase);
    cout << "url          : " << url <<endl;
    cout << "document base: " << docBase <<endl;
    String localPart("foo.html");
    url.clear();
    URIUtils::resolveHref(localPart, docBase, url);
    cout << "local part   : " << localPart << endl;
    cout << "resolved url : " << url << endl;

}
/* -*- */

