/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*



 */

#ifndef nsIXULPrototypeCache_h__
#define nsIXULPrototypeCache_h__

#include "nsISupports.h"
class nsICSSStyleSheet;
class nsIURI;
class nsIXULPrototypeDocument;
class nsCString;
class nsIDocument;
class nsIXBLDocumentInfo;

// {3A0A0FC1-8349-11d3-BE47-00104BDE6048}
#define NS_XULPROTOTYPECACHE_CID \
{ 0x3a0a0fc1, 0x8349, 0x11d3, { 0xbe, 0x47, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

// {3A0A0FC0-8349-11d3-BE47-00104BDE6048}
#define NS_IXULPROTOTYPECACHE_IID \
{ 0x3a0a0fc0, 0x8349, 0x11d3, { 0xbe, 0x47, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }


class nsIXULPrototypeCache : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXULPROTOTYPECACHE_IID);

    NS_IMETHOD GetPrototype(nsIURI* aURI, nsIXULPrototypeDocument** _result) = 0;
    NS_IMETHOD PutPrototype(nsIXULPrototypeDocument* aDocument) = 0;
    NS_IMETHOD FlushPrototypes() = 0;

    NS_IMETHOD GetStyleSheet(nsIURI* aURI, nsICSSStyleSheet** _result) = 0;
    NS_IMETHOD PutStyleSheet(nsICSSStyleSheet* aStyleSheet) = 0;
    NS_IMETHOD FlushStyleSheets() = 0;

    NS_IMETHOD GetScript(nsIURI* aURI, void** aScriptObject) = 0;
    NS_IMETHOD PutScript(nsIURI* aURI, void* aScriptObject) = 0;
    NS_IMETHOD FlushScripts() = 0;

    NS_IMETHOD GetXBLDocumentInfo(const nsCString& aString, nsIXBLDocumentInfo** aResult) = 0;
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
};


extern NS_IMETHODIMP
NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#endif // nsIXULPrototypeCache_h__
