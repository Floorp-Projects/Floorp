/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsNodeInfo_h___
#define nsNodeInfo_h___

#include "nsINodeInfo.h"
#include "nsINameSpaceManager.h"
#include "plhash.h"

/*
 * nsNodeInfoInner is used for two things:
 *
 *   1. as a member in nsNodeInfo for holding the name, prefix and
 *      namespace ID
 *   2. as the hash key in the hash table in nsNodeInfoManager
 *
 * nsNodeInfoInner does not do any kind of reference counting, that's up
 * to the user of this class, since nsNodeInfoInner is a member of
 * nsNodeInfo the hash table doesn't need to delete the keys, when the
 * value (nsNodeInfo) the key is automatically deleted.
 */

struct nsNodeInfoInner
{
  nsNodeInfoInner(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID)
    : mName(aName), mPrefix(aPrefix), mNamespaceID(aNamespaceID) {}

  nsNodeInfoInner()
    : mName(nsnull), mPrefix(nsnull), mNamespaceID(kNameSpaceID_None) {}

  static PRIntn PR_CALLBACK KeyCompare(const void *key1, const void *key2);
  static PLHashNumber PR_CALLBACK GetHashValue(const void *key);

  nsIAtom*            mName;
  nsIAtom*            mPrefix;
  PRInt32             mNamespaceID;
};


class nsNodeInfoManager;

class nsNodeInfo : public nsINodeInfo
{
public:
  NS_DECL_ISUPPORTS

  // nsINodeInfo
  NS_IMETHOD GetName(nsAWritableString& aName);
  NS_IMETHOD GetNameAtom(nsIAtom*& aAtom);
  NS_IMETHOD GetQualifiedName(nsAWritableString& aQualifiedName);
  NS_IMETHOD GetLocalName(nsAWritableString& aLocalName);
  NS_IMETHOD GetPrefix(nsAWritableString& aPrefix);
  NS_IMETHOD GetPrefixAtom(nsIAtom*& aAtom);
  NS_IMETHOD GetNamespaceURI(nsAWritableString& aNameSpaceURI);
  NS_IMETHOD GetNamespaceID(PRInt32& aResult);
  NS_IMETHOD GetNodeInfoManager(nsINodeInfoManager*& aNodeInfoManager);
  NS_IMETHOD_(PRBool) Equals(nsIAtom *aNameAtom);
  NS_IMETHOD_(PRBool) Equals(const nsAReadableString& aName);

  NS_IMETHOD_(PRBool) Equals(nsIAtom *aNameAtom, nsIAtom *aPrefixAtom);
  NS_IMETHOD_(PRBool) Equals(const nsAReadableString& aName,
                             const nsAReadableString& aPrefix);
  NS_IMETHOD_(PRBool) Equals(nsIAtom *aNameAtom, PRInt32 aNamespaceID);
  NS_IMETHOD_(PRBool) Equals(const nsAReadableString& aName, PRInt32 aNamespaceID);
  NS_IMETHOD_(PRBool) Equals(nsIAtom *aNameAtom, nsIAtom *aPrefixAtom,
                             PRInt32 aNamespaceID);
  NS_IMETHOD_(PRBool) Equals(const nsAReadableString& aName, const nsAReadableString& aPrefix,
                             PRInt32 aNamespaceID);
  NS_IMETHOD_(PRBool) NamespaceEquals(PRInt32 aNamespaceID);
  NS_IMETHOD_(PRBool) NamespaceEquals(const nsAReadableString& aNamespaceURI);
  NS_IMETHOD NameChanged(nsIAtom *aName, nsINodeInfo*& aResult);
  NS_IMETHOD PrefixChanged(nsIAtom *aPrefix, nsINodeInfo*& aResult);

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
  friend class nsNodeInfoManager; // The NodeInfoManager needs to pass this
                                  // to the hash table.
  nsNodeInfoInner mInner;

  nsNodeInfoManager*  mOwnerManager; // Strong reference!
};

#endif /* nsNodeInfo_h___ */
