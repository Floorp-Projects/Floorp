/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsNodeInfo_h___
#define nsNodeInfo_h___

#include "nsINodeInfo.h"
#include "plhash.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

class nsNodeInfoManager;

class nsNodeInfo : public nsINodeInfo
{
public:
  NS_DECL_ISUPPORTS

  // nsINodeInfo
  NS_IMETHOD GetQualifiedName(nsAString &aQualifiedName) const;
  NS_IMETHOD GetLocalName(nsAString& aLocalName) const;
  NS_IMETHOD GetNamespaceURI(nsAString& aNameSpaceURI) const;
  NS_IMETHOD GetIDAttributeAtom(nsIAtom** aResult) const;
  NS_IMETHOD SetIDAttributeAtom(nsIAtom* aResult);
  NS_IMETHOD GetNodeInfoManager(nsINodeInfoManager*& aNodeInfoManager) const;
  NS_IMETHOD_(PRBool) Equals(const nsAString& aName) const;
  NS_IMETHOD_(PRBool) Equals(const nsAString& aName,
                             const nsAString& aPrefix) const;
  NS_IMETHOD_(PRBool) Equals(const nsAString& aName,
                             PRInt32 aNamespaceID) const;
  NS_IMETHOD_(PRBool) Equals(const nsAString& aName, const nsAString& aPrefix,
                             PRInt32 aNamespaceID) const;
  NS_IMETHOD_(PRBool) NamespaceEquals(const nsAString& aNamespaceURI) const;
  NS_IMETHOD_(PRBool) QualifiedNameEquals(const nsAString& aQualifiedName) const;

  NS_IMETHOD NameChanged(nsIAtom *aName, nsINodeInfo*& aResult);
  NS_IMETHOD PrefixChanged(nsIAtom *aPrefix, nsINodeInfo*& aResult);
  NS_IMETHOD GetDocument(nsIDocument*& aDocument) const;
  NS_IMETHOD GetDocumentPrincipal(nsIPrincipal** aPrincipal) const;

  // nsNodeInfo
  nsNodeInfo();
  virtual ~nsNodeInfo();

  /*
   * Note! Init() must be called exactly once on every nsNodeInfo before
   * the object is used, if Init() returns an error code the nsNodeInfo
   * should not be used.
   *
   * aName and aOwnerManager may not be null.
   */
  nsresult Init(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                nsNodeInfoManager *aOwnerManager);

protected:
  nsCOMPtr<nsIAtom>   mIDAttributeAtom;

  nsNodeInfoManager* mOwnerManager; // Strong reference!
};

#endif /* nsNodeInfo_h___ */
