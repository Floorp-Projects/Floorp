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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsNodeInfo_h___
#define nsNodeInfo_h___

#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "plhash.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

class nsNodeInfo : public nsINodeInfo
{
public:
  NS_DECL_ISUPPORTS

  // nsINodeInfo
  virtual void GetQualifiedName(nsAString &aQualifiedName) const;
  virtual void GetLocalName(nsAString& aLocalName) const;
  virtual nsresult GetNamespaceURI(nsAString& aNameSpaceURI) const;
  virtual PRBool Equals(const nsAString& aName) const;
  virtual PRBool Equals(const nsAString& aName,
                        const nsAString& aPrefix) const;
  virtual PRBool Equals(const nsAString& aName, PRInt32 aNamespaceID) const;
  virtual PRBool Equals(const nsAString& aName, const nsAString& aPrefix,
                        PRInt32 aNamespaceID) const;
  virtual PRBool NamespaceEquals(const nsAString& aNamespaceURI) const;
  virtual PRBool QualifiedNameEquals(const nsACString& aQualifiedName) const;

  nsIDocument* GetDocument() const
  {
    return mOwnerManager->GetDocument();
  }

  nsIPrincipal *GetDocumentPrincipal() const
  {
    return mOwnerManager->GetDocumentPrincipal();
  }

  // nsNodeInfo
  // Create objects with Create
public:
  static nsNodeInfo *Create();
private:
  nsNodeInfo();
protected:
  virtual ~nsNodeInfo();

public:
  /*
   * Note! Init() must be called exactly once on every nsNodeInfo before
   * the object is used, if Init() returns an error code the nsNodeInfo
   * should not be used.
   *
   * aName and aOwnerManager may not be null.
   */
  nsresult Init(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                nsNodeInfoManager *aOwnerManager);

  /**
   * Call before shutdown to clear the cache and free memory for this class.
   */
  static void ClearCache();

private:
  void Clear();

  static nsNodeInfo *sCachedNodeInfo;

  /**
   * This method gets called by Release() when it's time to delete 
   * this object, instead of always deleting the object we'll put the
   * object in the cache unless the cache is already full.
   */
   void LastRelease();
};

#endif /* nsNodeInfo_h___ */
