/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the web scripts access security code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

#ifndef nsWebScriptsAccess_h__
#define nsWebScriptsAccess_h__

#include "nsIWebScriptsAccessService.h"
#include "nsIScriptSecurityManager.h" 
#include "nsHashtable.h"
#include "nsIXMLHttpRequest.h"
#include "nsVoidArray.h"
#include "nsWSAUtils.h"

class nsIDOMDocument;
class nsIDOMNode;
class nsIDOMNodeList;

class AccessInfo
{
public:
  AccessInfo() : mType(0), mFrom(0)
  {
    // do nothing
  }
  ~AccessInfo() 
  {
    nsMemory::Free(mType);
    nsMemory::Free(mFrom);
  }

  PRUnichar* mType;
  PRUnichar* mFrom;
};

class AccessInfoEntry
{
public:
  AccessInfoEntry(PRInt32 aFlags = 0) : mFlags(aFlags) 
  {
    // do nothing
  }
  ~AccessInfoEntry() 
  {
    PRUint32 count = mInfoArray.Count();
    while (count) {
      AccessInfo* info = 
        NS_REINTERPRET_CAST(AccessInfo*, mInfoArray.ElementAt(--count));
      delete info;
    }
  }

  PRInt32 mFlags;
  nsVoidArray mInfoArray; // holds AccessInfo objects
};

class nsWebScriptsAccess : public nsIWebScriptsAccessService
{
public:
  nsWebScriptsAccess();
  virtual ~nsWebScriptsAccess();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSCRIPTSACCESSSERVICE

protected:
  nsresult GetAccessInfoEntry(const char* aKey, AccessInfoEntry** aEntry);
  nsresult GetDocument(const char* aDeclFilePath, nsIDOMDocument** aDocument);
  nsresult GetCodebaseURI(nsIURI** aCodebase);
  nsresult CreateEntry(const char* aKey, const PRBool aIsDelegated, AccessInfoEntry** aEntry);
  nsresult CreateEntry(nsIDOMDocument* aDocument, const PRBool aIsDelegated, AccessInfoEntry** aAccessInfoEntry);
  nsresult CreateEntry(nsIDOMNodeList* aAllowList, AccessInfoEntry** aAccessInfoEntry);
  nsresult CreateEntry(const PRInt32 aFlags, AccessInfoEntry** aAccessInfoEntry);
  nsresult CreateDelegatedEntry(AccessInfoEntry** aAccessInfoEntry);
  nsresult CheckAccess(AccessInfoEntry* aAccessInfoEntry, const nsAString& aRequestType, PRBool* aAccessGranted);
  nsresult ValidateDocument(nsIDOMDocument* aDocument, PRBool* aIsValid);
  nsresult IsPublicService(const char* aHost, PRBool* aReturn);
  
  nsCOMPtr<nsIURI> mServiceURI;
  nsCOMPtr<nsIXMLHttpRequest> mRequest;
  nsCOMPtr<nsIScriptSecurityManager> mSecurityManager;
  nsStringArray mMasterServices;
  nsHashtable mAccessInfoTable;

  const nsLiteralString kNamespace2002;
  // Element set
  const nsLiteralString kWebScriptAccessTag;
  const nsLiteralString kDelegateTag;
  const nsLiteralString kAllowTag;
  // Attribute set
  const nsLiteralString kTypeAttr;
  const nsLiteralString kFromAttr;
  // Default attribute value
  const nsLiteralString kAny;
  // Method name. Note: This method should be implemented by master services.
  const nsLiteralString kIsServicePublic;
};

#endif

