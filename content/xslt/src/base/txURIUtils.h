/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef TRANSFRMX_URIUTILS_H
#define TRANSFRMX_URIUTILS_H

#include "txCore.h"
#ifdef TX_EXE
#include <fstream.h>
#include <iostream.h>
#include "nsString.h"
#else
class nsIDocument;
class nsIDOMNode;
class nsIScriptSecurityManager;
extern nsIScriptSecurityManager *gTxSecurityManager;

#endif

/**
 * A utility class for URI handling
 * Not yet finished, only handles file URI at this point
**/

#ifdef TX_EXE
class txParsedURL
{
public:
    void init(const nsAFlatString& aSpec);
    void resolve(const txParsedURL& aRef, txParsedURL& aDest);
    void getFile(nsString& aResult) const
    {
        aResult = mPath + mName;
    }
    nsString mPath, mName, mRef;
};
#endif

class URIUtils {
public:

#ifdef TX_EXE
    /**
     * the path separator for an URI
    **/
    static const char HREF_PATH_SEP;

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

    /**
     * Reset the given document with the document of the source node
     */
    static void ResetWithSource(nsIDocument *aNewDoc, nsIDOMNode *aSourceNode);

#endif /* TX_EXE */

    /**
     * Resolves the given href argument, using the given documentBase
     * if necessary.
     * The new resolved href will be appended to the given dest String
    **/
    static void resolveHref(const nsAString& href, const nsAString& base,
                            nsAString& dest);
}; //-- URIUtils

/* */
#endif
