/*
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
 * Larry Fitzpatrick, OpenText, lef@opentext.com
 *   -- 19990806
 *     -- moved initialization of constant shorts and chars to 
 *        URIUtils.cpp
 *
 * Peter Van der Beken
 *   -- 20000326
 *     -- added Mozilla integration code
 *
 */

#ifndef TRANSFRMX_URIUTILS_H
#define TRANSFRMX_URIUTILS_H

#include "baseutils.h"
#ifdef TX_EXE
#include <fstream.h>
#include <iostream.h>
#include "nsString.h"
#else
#include "nsIDOMNode.h"

class nsIScriptSecurityManager;
extern nsIScriptSecurityManager *gTxSecurityManager;

#endif

/**
 * A utility class for URI handling
 * Not yet finished, only handles file URI at this point
**/

class URIUtils {


public:

#ifdef TX_EXE
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


    static istream* getInputStream
        (const nsAString& href, nsAString& errMsg);

    /**
     * Returns the document base of the href argument
     * The document base will be appended to the given dest String
    **/
    static void getDocumentBase(const nsAFlatString& href, nsAString& dest);

#else /* TX_EXE */

    /*
     * Checks if a caller is allowed to access a given node
     */
    static PRBool CanCallerAccess(nsIDOMNode *aNode);

#endif /* TX_EXE */

    /**
     * Resolves the given href argument, using the given documentBase
     * if necessary.
     * The new resolved href will be appended to the given dest String
    **/
    static void resolveHref(const nsAString& href, const nsAString& base,
                            nsAString& dest);

private:

#ifdef TX_EXE
    enum ParseMode {
        PROTOCOL_MODE,
        HOST_MODE,
        PORT_MODE,
        PATH_MODE
    };

    struct ParsedURI {
        MBool  isMalformed;
        nsString fragmentIdentifier;
        nsString host;
        nsString protocol;
        nsString port;
        nsString path;
    };

    static istream* openStream(ParsedURI* uri);
    static ParsedURI* parseURI(const nsAString& aUri);
#endif

}; //-- URIUtils

/* */
#endif
