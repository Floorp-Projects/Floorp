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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
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

#ifndef nsAttrName_h___
#define nsAttrName_h___

#include "nsINodeInfo.h"
#include "nsIAtom.h"
#include "nsDOMString.h"

typedef unsigned long PtrBits;

#define NS_ATTRNAME_NODEINFO_BIT 1
class nsAttrName
{
public:
  nsAttrName(const nsAttrName& aOther)
    : mBits(aOther.mBits)
  {
    AddRefInternalName();
  }

  explicit nsAttrName(nsIAtom* aAtom)
    : mBits(NS_REINTERPRET_CAST(PtrBits, aAtom))
  {
    NS_ASSERTION(aAtom, "null atom-name in nsAttrName");
    NS_ADDREF(aAtom);
  }

  explicit nsAttrName(nsINodeInfo* aNodeInfo)
  {
    NS_ASSERTION(aNodeInfo, "null nodeinfo-name in nsAttrName");
    if (aNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
      mBits = NS_REINTERPRET_CAST(PtrBits, aNodeInfo->NameAtom());
      NS_ADDREF(aNodeInfo->NameAtom());
    }
    else {
      mBits = NS_REINTERPRET_CAST(PtrBits, aNodeInfo) |
              NS_ATTRNAME_NODEINFO_BIT;
      NS_ADDREF(aNodeInfo);
    }
  }

  ~nsAttrName()
  {
    ReleaseInternalName();
  }

  void SetTo(nsINodeInfo* aNodeInfo)
  {
    NS_ASSERTION(aNodeInfo, "null nodeinfo-name in nsAttrName");

    ReleaseInternalName();
    if (aNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
      mBits = NS_REINTERPRET_CAST(PtrBits, aNodeInfo->NameAtom());
      NS_ADDREF(aNodeInfo->NameAtom());
    }
    else {
      mBits = NS_REINTERPRET_CAST(PtrBits, aNodeInfo) |
              NS_ATTRNAME_NODEINFO_BIT;
      NS_ADDREF(aNodeInfo);
    }
  }

  void SetTo(nsIAtom* aAtom)
  {
    NS_ASSERTION(aAtom, "null atom-name in nsAttrName");

    ReleaseInternalName();
    mBits = NS_REINTERPRET_CAST(PtrBits, aAtom);
    NS_ADDREF(aAtom);
  }

  PRBool IsAtom() const
  {
    return !(mBits & NS_ATTRNAME_NODEINFO_BIT);
  }

  nsINodeInfo* NodeInfo() const
  {
    NS_ASSERTION(!IsAtom(), "getting nodeinfo-value of atom-name");
    return NS_REINTERPRET_CAST(nsINodeInfo*, mBits & ~NS_ATTRNAME_NODEINFO_BIT);
  }

  nsIAtom* Atom() const
  {
    NS_ASSERTION(IsAtom(), "getting atom-value of nodeinfo-name");
    return NS_REINTERPRET_CAST(nsIAtom*, mBits);
  }

  PRBool Equals(const nsAttrName& aOther) const
  {
    return mBits == aOther.mBits;
  }

  // Faster comparison in the case we know the namespace is null
  PRBool Equals(nsIAtom* aAtom) const
  {
    return NS_REINTERPRET_CAST(PtrBits, aAtom) == mBits;
  }

  PRBool Equals(nsIAtom* aLocalName, PRInt32 aNamespaceID) const
  {
    if (aNamespaceID == kNameSpaceID_None) {
      return Equals(aLocalName);
    }
    return !IsAtom() && NodeInfo()->Equals(aLocalName, aNamespaceID);
  }

  PRInt32 NamespaceID() const
  {
    return IsAtom() ? kNameSpaceID_None : NodeInfo()->NamespaceID();
  }

  nsIAtom* LocalName() const
  {
    return IsAtom() ? Atom() : NodeInfo()->NameAtom();
  }

  nsIAtom* GetPrefix() const
  {
    return IsAtom() ? nsnull : NodeInfo()->GetPrefixAtom();
  }

  PRBool QualifiedNameEquals(const nsACString& aName) const
  {
    return IsAtom() ? Atom()->EqualsUTF8(aName) :
                      NodeInfo()->QualifiedNameEquals(aName);
  }

  void GetQualifiedName(nsAString& aStr) const
  {
    if (IsAtom()) {
      Atom()->ToString(aStr);
    }
    else {
      NodeInfo()->GetQualifiedName(aStr);
    }
  }

  void GetPrefix(nsAString& aStr) const
  {
    if (IsAtom()) {
      SetDOMStringToNull(aStr);
    }
    else {
      NodeInfo()->GetPrefix(aStr);
    }
  }

  PRUint32 HashValue() const
  {
    // mBits and PRUint32 might have different size. This should silence
    // any warnings or compile-errors. This is what the implementation of
    // NS_PTR_TO_INT32 does to take care of the same problem.
    return mBits - 0;
  }

  PRBool IsSmaller(nsIAtom* aOther) const
  {
    return mBits < NS_REINTERPRET_CAST(PtrBits, aOther);
  }

private:

  void AddRefInternalName()
  {
    // Since both nsINodeInfo and nsIAtom inherit nsISupports as its first
    // interface we can safely assume that it's first in the vtable
    nsISupports* name = NS_REINTERPRET_CAST(nsISupports *,
      mBits & ~NS_ATTRNAME_NODEINFO_BIT);

    NS_ADDREF(name);
  }

  void ReleaseInternalName()
  {
    // Since both nsINodeInfo and nsIAtom inherit nsISupports as its first
    // interface we can safely assume that it's first in the vtable
    nsISupports* name = NS_REINTERPRET_CAST(nsISupports *,
      mBits & ~NS_ATTRNAME_NODEINFO_BIT);

    NS_RELEASE(name);
  }

  PtrBits mBits;
};

#endif
