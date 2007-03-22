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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

  Private interface to the XBL DocumentInfo

*/

#ifndef nsIXBLDocumentInfo_h__
#define nsIXBLDocumentInfo_h__

#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsIScriptContext;
class nsXBLPrototypeBinding;
class nsIURI;
class nsACString;

// 2d8334b0-e0b8-4a23-9153-dc89ac275894
#define NS_IXBLDOCUMENTINFO_IID \
{ 0x2d8334b0, 0xe0b8, 0x4a23, { 0x91, 0x52, 0xdc, 0x89, 0xac, 0x27, 0x58, 0x94 } }

class nsIXBLDocumentInfo : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXBLDOCUMENTINFO_IID)

  NS_IMETHOD GetDocument(nsIDocument** aResult)=0;
  
  NS_IMETHOD GetScriptAccess(PRBool* aResult)=0;

  /* Never returns null */
  NS_IMETHOD_(nsIURI*) DocumentURI()=0;

  NS_IMETHOD GetPrototypeBinding(const nsACString& aRef, nsXBLPrototypeBinding** aResult)=0;
  NS_IMETHOD SetPrototypeBinding(const nsACString& aRef, nsXBLPrototypeBinding* aBinding)=0;

  NS_IMETHOD SetFirstPrototypeBinding(nsXBLPrototypeBinding* aBinding)=0;

  NS_IMETHOD FlushSkinStylesheets()=0;

  // Tells whether the scheme of the document URI is "chrome".
  NS_IMETHOD_(PRBool) IsChrome()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXBLDocumentInfo, NS_IXBLDOCUMENTINFO_IID)

nsresult
NS_NewXBLDocumentInfo(nsIDocument* aDocument, nsIXBLDocumentInfo** aResult);

#endif // nsIXBLDocumentInfo_h__
