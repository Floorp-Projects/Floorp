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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

/*

  Private interface to the XBL DocumentInfo

*/

#ifndef nsIXBLDocumentInfo_h__
#define nsIXBLDocumentInfo_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsISupportsArray.h"

class nsIContent;
class nsIDocument;
class nsIScriptContext;
class nsIXBLPrototypeHandler;

// {5C4D9674-A2CF-4ddf-9F65-E1806C34D28D}
#define NS_IXBLDOCUMENTINFO_IID \
{ 0x5c4d9674, 0xa2cf, 0x4ddf, { 0x9f, 0x65, 0xe1, 0x80, 0x6c, 0x34, 0xd2, 0x8d } }

class nsIXBLDocumentInfo : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLDOCUMENTINFO_IID; return iid; }

  NS_IMETHOD GetDocument(nsIDocument** aResult)=0;
  NS_IMETHOD GetRuleProcessors(nsISupportsArray** aResult)=0;

  NS_IMETHOD GetScriptAccess(PRBool* aResult)=0;
  NS_IMETHOD SetScriptAccess(PRBool aAccess)=0;

  NS_IMETHOD GetPrototypeHandler(const nsCString& aRef, nsIXBLPrototypeHandler** aResult)=0;
  NS_IMETHOD SetPrototypeHandler(const nsCString& aRef, nsIXBLPrototypeHandler* aHandler)=0;
};

extern nsresult
NS_NewXBLDocumentInfo(nsIDocument* aDocument, nsIXBLDocumentInfo** aResult);

#endif // nsIXBLDocumentInfo_h__
