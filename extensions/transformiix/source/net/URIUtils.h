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

#include "String.h"
#include "baseutils.h"
#include <iostream.h>
#include <fstream.h>

/**
 * A utility class for URI handling
 * Not yet finished, only handles file URI at this point
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *  - moved initialization of contanst shorts and chars to URIUtils.cpp
 * </PRE>
 *
**/

class URIUtils {


public:

    static const String HTTP_PROTOCOL;
    static const String FILE_PROTOCOL;

    /**
     * the path separator for an URI
    **/
    static const char HREF_PATH_SEP;

    /**
     * The Device separator for an URI
    **/
    static const char DEVICE_SEP;

    /**
     * The Port separator for an URI
    **/
    static const char PORT_SEP;

    /**
     * The Protocal separator for an URI
    **/
    static const char PROTOCOL_SEP;


    static istream* URIUtils::getInputStream
        (String& href, String& documentBase, String& errMsg);

	/**
	 * Returns the document base of the href argument
     * The document base will be appended to the given dest String
    **/
    static void getDocumentBase(String& href, String& dest);

	/**
     * Resolves the given href argument, using the given documentBase
     * if necessary.
     * The new resolved href will be appended to the given dest String
    **/
    static void resolveHref(String& href, String& documentBase, String& dest);


private:

    static const short PROTOCOL_MODE;
    static const short HOST_MODE;
    static const short PORT_MODE;
    static const short PATH_MODE;

    struct ParsedURI {
        MBool  isMalformed;
        String fragmentIdentifier;
        String host;
        String protocol;
        String port;
        String path;
    };

    static istream* openStream(ParsedURI* uri);
    static ParsedURI* parseURI(const String& uri);



}; //-- URIUtils
