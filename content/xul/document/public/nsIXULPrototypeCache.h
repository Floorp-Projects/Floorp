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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*



 */

#ifndef nsIXULPrototypeCache_h__
#define nsIXULPrototypeCache_h__

#include "nsISupports.h"
class nsICSSStyleSheet;
class nsIURI;
class nsIXULPrototypeDocument;
class nsIXULDocument;
class nsCString;
class nsIDocument;
class nsIXBLDocumentInfo;
class nsIFastLoadService;

// {3A0A0FC1-8349-11d3-BE47-00104BDE6048}
#define NS_XULPROTOTYPECACHE_CID \
{ 0x3a0a0fc1, 0x8349, 0x11d3, { 0xbe, 0x47, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

// {F53A6C7E-0344-4543-9213-AFFFD30AC2BE}
#define NS_IXULPROTOTYPECACHE_IID \
{ 0xf53a6c7e, 0x344, 0x4543, { 0x92, 0x13, 0xaf, 0xff, 0xd3, 0xa, 0xc2, 0xbe } };


class nsIXULPrototypeCache : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXULPROTOTYPECACHE_IID)

    NS_IMETHOD GetPrototype(nsIURI* aURI, nsIXULPrototypeDocument** _result) = 0;
    NS_IMETHOD PutPrototype(nsIXULPrototypeDocument* aDocument) = 0;
    NS_IMETHOD FlushPrototypes() = 0;

    NS_IMETHOD GetStyleSheet(nsIURI* aURI, nsICSSStyleSheet** _result) = 0;
    NS_IMETHOD PutStyleSheet(nsICSSStyleSheet* aStyleSheet) = 0;
    NS_IMETHOD FlushStyleSheets() = 0;

    NS_IMETHOD GetScript(nsIURI* aURI, PRUint32 *aLangID, void** aScriptObject) = 0;
    NS_IMETHOD PutScript(nsIURI* aURI, PRUint32 aLangID, void* aScriptObject) = 0;
    NS_IMETHOD FlushScripts() = 0;

    NS_IMETHOD GetXBLDocumentInfo(nsIURI* aURL, nsIXBLDocumentInfo** aResult) = 0;
    NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocument) = 0;

    NS_IMETHOD FlushXBLInformation() = 0;

    /**
     * Flush the cache; remove all XUL prototype documents, style
     * sheets, and scripts.
     */
    NS_IMETHOD Flush() = 0;

    /**
     * Determine if the prototype cache is enabled
     */
    NS_IMETHOD GetEnabled(PRBool* aIsEnabled) = 0;

    /**
     * Stop the FastLoad process abruptly, removing the FastLoad file.
     */
    NS_IMETHOD AbortFastLoads() = 0;

    /** 
     * Retrieve the FastLoad service
     */
    NS_IMETHOD GetFastLoadService(nsIFastLoadService** aResult) = 0;

    /** 
     * Remove a XULDocument from the set of loading documents
     */
    NS_IMETHOD RemoveFromFastLoadSet(nsIURI* aDocumentURI) = 0;

    /** 
     * Write Prototype Document to FastLoad file
     */
    NS_IMETHOD WritePrototype(nsIXULPrototypeDocument* aDocument) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXULPrototypeCache, NS_IXULPROTOTYPECACHE_IID)

NS_IMETHODIMP
NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult);


const char XUL_FASTLOAD_FILE_BASENAME[] = "XUL";

// Increase the subtractor when changing version, say when changing the
// (opaque to FastLoad code) format of JS script, function, regexp, etc.
// XDR serializations.
#define XUL_FASTLOAD_FILE_VERSION       (0xfeedbeef - 13)

#define XUL_SERIALIZATION_BUFFER_SIZE   (64 * 1024)
#define XUL_DESERIALIZATION_BUFFER_SIZE (8 * 1024)


#endif // nsIXULPrototypeCache_h__
