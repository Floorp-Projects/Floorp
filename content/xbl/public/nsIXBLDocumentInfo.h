/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
class nsIXBLPrototypeBinding;

// {5C4D9674-A2CF-4ddf-9F65-E1806C34D28D}
#define NS_IXBLDOCUMENTINFO_IID \
{ 0x5c4d9674, 0xa2cf, 0x4ddf, { 0x9f, 0x65, 0xe1, 0x80, 0x6c, 0x34, 0xd2, 0x8d } }

class nsIXBLDocumentInfo : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLDOCUMENTINFO_IID; return iid; }

  NS_IMETHOD GetDocument(nsIDocument** aResult)=0;
  
  NS_IMETHOD GetScriptAccess(PRBool* aResult)=0;
  NS_IMETHOD SetScriptAccess(PRBool aAccess)=0;

  NS_IMETHOD GetDocumentURI(nsCString& aDocURI)=0;

  NS_IMETHOD GetPrototypeBinding(const nsAReadableCString& aRef, nsIXBLPrototypeBinding** aResult)=0;
  NS_IMETHOD SetPrototypeBinding(const nsAReadableCString& aRef, nsIXBLPrototypeBinding* aBinding)=0;
};

extern nsresult
NS_NewXBLDocumentInfo(nsIDocument* aDocument, nsIXBLDocumentInfo** aResult);

#endif // nsIXBLDocumentInfo_h__
