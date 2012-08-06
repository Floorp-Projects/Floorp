/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsINodeInfo is an interface to node info, such as name, prefix, namespace
 * ID and possibly other data that is shared between nodes (elements
 * and attributes) that have the same name, prefix and namespace ID within
 * the same document.
 *
 * nsNodeInfoManager's are internal objects that manage a list of
 * nsINodeInfo's, every document object should hold a strong reference to
 * a nsNodeInfoManager and every nsINodeInfo also holds a strong reference
 * to their owning manager. When a nsINodeInfo is no longer used it will
 * automatically remove itself from its owner manager, and when all
 * nsINodeInfo's have been removed from a nsNodeInfoManager and all external
 * references are released the nsNodeInfoManager deletes itself.
 *
 * -- jst@netscape.com
 */

#ifndef nsINodeInfo_h___
#define nsINodeInfo_h___

#include "nsCOMPtr.h"            // for member
#include "nsIAtom.h"             // for member (in nsCOMPtr)
#include "nsINameSpaceManager.h" // for kNameSpaceID_*
#include "nsISupports.h"         // for base class

#ifdef MOZILLA_INTERNAL_API
#include "nsDOMString.h"
#endif

class nsIDocument;
class nsIURI;
class nsIPrincipal;
class nsNodeInfoManager;

// IID for the nsINodeInfo interface
#define NS_INODEINFO_IID      \
{ 0xc5188ea1, 0x0a9c, 0x43e6, \
 { 0x95, 0x90, 0xcc, 0x43, 0x6b, 0xe9, 0xcf, 0xa0 } }

class nsINodeInfo : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODEINFO_IID)

  nsINodeInfo()
    : mInner(nullptr, nullptr, kNameSpaceID_None, 0, nullptr),
      mOwnerManager(nullptr)
  {
  }

  /*
   * Get the name from this node as a string, this does not include the prefix.
   *
   * For the HTML element "<body>" this will return "body" and for the XML
   * element "<html:body>" this will return "body".
   */
  void GetName(nsAString& aName) const
  {
    mInner.mName->ToString(aName);
  }

  /*
   * Get the name from this node as an atom, this does not include the prefix.
   * This function never returns a null atom.
   *
   * For the HTML element "<body>" this will return the "body" atom and for
   * the XML element "<html:body>" this will return the "body" atom.
   */
  nsIAtom* NameAtom() const
  {
    return mInner.mName;
  }

  /*
   * Get the qualified name from this node as a string, the qualified name
   * includes the prefix, if one exists.
   *
   * For the HTML element "<body>" this will return "body" and for the XML
   * element "<html:body>" this will return "html:body".
   */
  const nsString& QualifiedName() const {
    return mQualifiedName;
  }

  /*
   * Returns the node's nodeName as defined in DOM Core
   */
  const nsString& NodeName() const {
    return mNodeName;
  }

  /*
   * Returns the node's localName as defined in DOM Core
   */
  const nsString& LocalName() const {
    return mLocalName;
  }

#ifdef MOZILLA_INTERNAL_API
  /*
   * Get the prefix from this node as a string.
   *
   * For the HTML element "<body>" this will return a null string and for
   * the XML element "<html:body>" this will return the string "html".
   */
  void GetPrefix(nsAString& aPrefix) const
  {
    if (mInner.mPrefix) {
      mInner.mPrefix->ToString(aPrefix);
    } else {
      SetDOMStringToNull(aPrefix);
    }
  }
#endif

  /*
   * Get the prefix from this node as an atom.
   *
   * For the HTML element "<body>" this will return a null atom and for
   * the XML element "<html:body>" this will return the "html" atom.
   */
  nsIAtom* GetPrefixAtom() const
  {
    return mInner.mPrefix;
  }

  /*
   * Get the namespace URI for a node, if the node has a namespace URI.
   */
  virtual nsresult GetNamespaceURI(nsAString& aNameSpaceURI) const = 0;

  /*
   * Get the namespace ID for a node if the node has a namespace, if not this
   * returns kNameSpaceID_None.
   */
  PRInt32 NamespaceID() const
  {
    return mInner.mNamespaceID;
  }

  /*
   * Get the nodetype for the node. Returns the values specified in nsIDOMNode
   * for nsIDOMNode.nodeType
   */
  PRUint16 NodeType() const
  {
    return mInner.mNodeType;
  }

  /*
   * Get the extra name, used by PIs and DocTypes, for the node.
   */
  nsIAtom* GetExtraName() const
  {
    return mInner.mExtraName;
  }

  /*
   * Get and set the ID attribute atom for this node.
   * See http://www.w3.org/TR/1998/REC-xml-19980210#sec-attribute-types
   * for the definition of an ID attribute.
   *
   */
  nsIAtom* GetIDAttributeAtom() const
  {
    return mIDAttributeAtom;
  }

  void SetIDAttributeAtom(nsIAtom* aID)
  {
    mIDAttributeAtom = aID;
  }

  /**
   * Get the owning node info manager. Only to be used inside Gecko, you can't
   * really do anything with the pointer outside Gecko anyway.
   */
  nsNodeInfoManager *NodeInfoManager() const
  {
    return mOwnerManager;
  }

  /*
   * Utility functions that can be used to check if a nodeinfo holds a specific
   * name, name and prefix, name and prefix and namespace ID, or just
   * namespace ID.
   */
  bool Equals(nsINodeInfo *aNodeInfo) const
  {
    return aNodeInfo == this || aNodeInfo->Equals(mInner.mName, mInner.mPrefix,
                                                  mInner.mNamespaceID);
  }

  bool NameAndNamespaceEquals(nsINodeInfo *aNodeInfo) const
  {
    return aNodeInfo == this || aNodeInfo->Equals(mInner.mName,
                                                  mInner.mNamespaceID);
  }

  bool Equals(nsIAtom *aNameAtom) const
  {
    return mInner.mName == aNameAtom;
  }

  bool Equals(nsIAtom *aNameAtom, nsIAtom *aPrefixAtom) const
  {
    return (mInner.mName == aNameAtom) && (mInner.mPrefix == aPrefixAtom);
  }

  bool Equals(nsIAtom *aNameAtom, PRInt32 aNamespaceID) const
  {
    return ((mInner.mName == aNameAtom) &&
            (mInner.mNamespaceID == aNamespaceID));
  }

  bool Equals(nsIAtom *aNameAtom, nsIAtom *aPrefixAtom,
                PRInt32 aNamespaceID) const
  {
    return ((mInner.mName == aNameAtom) &&
            (mInner.mPrefix == aPrefixAtom) &&
            (mInner.mNamespaceID == aNamespaceID));
  }

  bool NamespaceEquals(PRInt32 aNamespaceID) const
  {
    return mInner.mNamespaceID == aNamespaceID;
  }

  bool Equals(const nsAString& aName) const
  {
    return mInner.mName->Equals(aName);
  }

  bool Equals(const nsAString& aName, const nsAString& aPrefix) const
  {
    return mInner.mName->Equals(aName) &&
      (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
  }

  bool Equals(const nsAString& aName, PRInt32 aNamespaceID) const
  {
    return mInner.mNamespaceID == aNamespaceID &&
      mInner.mName->Equals(aName);
  }

  bool Equals(const nsAString& aName, const nsAString& aPrefix,
                PRInt32 aNamespaceID) const
  {
    return mInner.mName->Equals(aName) && mInner.mNamespaceID == aNamespaceID &&
      (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
  }

  virtual bool NamespaceEquals(const nsAString& aNamespaceURI) const = 0;

  bool QualifiedNameEquals(nsIAtom* aNameAtom) const
  {
    NS_PRECONDITION(aNameAtom, "Must have name atom");
    if (!GetPrefixAtom())
      return Equals(aNameAtom);

    return aNameAtom->Equals(mQualifiedName);
  }

  bool QualifiedNameEquals(const nsAString& aQualifiedName) const
  {
    return mQualifiedName == aQualifiedName;
  }

  /*
   * Retrieve a pointer to the document that owns this node info.
   */
  nsIDocument* GetDocument() const
  {
    return mDocument;
  }

protected:
  /*
   * nsNodeInfoInner is used for two things:
   *
   *   1. as a member in nsNodeInfo for holding the name, prefix and
   *      namespace ID
   *   2. as the hash key in the hash table in nsNodeInfoManager
   *
   * nsNodeInfoInner does not do any kind of reference counting,
   * that's up to the user of this class. Since nsNodeInfoInner is
   * typically used as a member of nsNodeInfo, the hash table doesn't
   * need to delete the keys. When the value (nsNodeInfo) is deleted
   * the key is automatically deleted.
   */

  class nsNodeInfoInner
  {
  public:
    nsNodeInfoInner()
      : mName(nullptr), mPrefix(nullptr), mNamespaceID(kNameSpaceID_Unknown),
        mNodeType(0), mNameString(nullptr), mExtraName(nullptr)
    {
    }
    nsNodeInfoInner(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                    PRUint16 aNodeType, nsIAtom* aExtraName)
      : mName(aName), mPrefix(aPrefix), mNamespaceID(aNamespaceID),
        mNodeType(aNodeType), mNameString(nullptr), mExtraName(aExtraName)
    {
    }
    nsNodeInfoInner(const nsAString& aTmpName, nsIAtom *aPrefix,
                    PRInt32 aNamespaceID, PRUint16 aNodeType)
      : mName(nullptr), mPrefix(aPrefix), mNamespaceID(aNamespaceID),
        mNodeType(aNodeType), mNameString(&aTmpName), mExtraName(nullptr)
    {
    }

    nsIAtom*            mName;
    nsIAtom*            mPrefix;
    PRInt32             mNamespaceID;
    PRUint16            mNodeType; // As defined by nsIDOMNode.nodeType
    const nsAString*    mNameString;
    nsIAtom*            mExtraName; // Only used by PIs and DocTypes
  };

  // nsNodeInfoManager needs to pass mInner to the hash table.
  friend class nsNodeInfoManager;

  nsIDocument* mDocument; // Weak. Cache of mOwnerManager->mDocument

  nsNodeInfoInner mInner;

  nsCOMPtr<nsIAtom> mIDAttributeAtom;
  nsNodeInfoManager* mOwnerManager; // Strong reference!

  /*
   * Members for various functions of mName+mPrefix that we can be
   * asked to compute.
   */

  // Qualified name
  nsString mQualifiedName;

  // nodeName for the node.
  nsString mNodeName;

  // localName for the node. This is either equal to mInner.mName, or a
  // void string, depending on mInner.mNodeType.
  nsString mLocalName;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINodeInfo, NS_INODEINFO_IID)

#endif /* nsINodeInfo_h___ */
