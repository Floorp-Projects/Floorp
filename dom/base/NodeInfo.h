/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class that represents a prefix/namespace/localName triple; a single
 * nodeinfo is shared by all elements in a document that have that
 * prefix, namespace, and localName.
 *
 * nsNodeInfoManagers are internal objects that manage a list of
 * NodeInfos, every document object should hold a strong reference to
 * a nsNodeInfoManager and every NodeInfo also holds a strong reference
 * to their owning manager. When a NodeInfo is no longer used it will
 * automatically remove itself from its owner manager, and when all
 * NodeInfos have been removed from a nsNodeInfoManager and all external
 * references are released the nsNodeInfoManager deletes itself.
 */

#ifndef mozilla_dom_NodeInfo_h___
#define mozilla_dom_NodeInfo_h___

#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "nsAtom.h"
#include "plhash.h"

class nsIDocument;
class nsNodeInfoManager;

namespace mozilla {
namespace dom {

class NodeInfo final
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(NodeInfo)
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_NATIVE_CLASS_WITH_CUSTOM_DELETE(NodeInfo)

  /*
   * Get the name from this node as a string, this does not include the prefix.
   *
   * For the HTML element "<body>" this will return "body" and for the XML
   * element "<html:body>" this will return "body".
   */
  void GetName(nsAString& aName) const;

  /*
   * Get the name from this node as an atom, this does not include the prefix.
   * This function never returns a null atom.
   *
   * For the HTML element "<body>" this will return the "body" atom and for
   * the XML element "<html:body>" this will return the "body" atom.
   */
  nsAtom* NameAtom() const
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

  /*
   * Get the prefix from this node as a string.
   *
   * For the HTML element "<body>" this will return a null string and for
   * the XML element "<html:body>" this will return the string "html".
   */
  void GetPrefix(nsAString& aPrefix) const;

  /*
   * Get the prefix from this node as an atom.
   *
   * For the HTML element "<body>" this will return a null atom and for
   * the XML element "<html:body>" this will return the "html" atom.
   */
  nsAtom* GetPrefixAtom() const
  {
    return mInner.mPrefix;
  }

  /*
   * Get the namespace URI for a node, if the node has a namespace URI.
   */
  void GetNamespaceURI(nsAString& aNameSpaceURI) const;

  /*
   * Get the namespace ID for a node if the node has a namespace, if not this
   * returns kNameSpaceID_None.
   */
  int32_t NamespaceID() const
  {
    return mInner.mNamespaceID;
  }

  /*
   * Get the nodetype for the node. Returns the values specified in nsIDOMNode
   * for nsIDOMNode.nodeType
   */
  uint16_t NodeType() const
  {
    return mInner.mNodeType;
  }

  /*
   * Get the extra name, used by PIs and DocTypes, for the node.
   */
  nsAtom* GetExtraName() const
  {
    return mInner.mExtraName;
  }

  /**
   * Get the owning node info manager. Only to be used inside Gecko, you can't
   * really do anything with the pointer outside Gecko anyway.
   */
  nsNodeInfoManager* NodeInfoManager() const
  {
    return mOwnerManager;
  }

  /*
   * Utility functions that can be used to check if a nodeinfo holds a specific
   * name, name and prefix, name and prefix and namespace ID, or just
   * namespace ID.
   */
  inline bool Equals(NodeInfo* aNodeInfo) const;

  bool NameAndNamespaceEquals(NodeInfo* aNodeInfo) const;

  bool Equals(nsAtom* aNameAtom) const
  {
    return mInner.mName == aNameAtom;
  }

  bool Equals(nsAtom* aNameAtom, nsAtom* aPrefixAtom) const
  {
    return (mInner.mName == aNameAtom) && (mInner.mPrefix == aPrefixAtom);
  }

  bool Equals(nsAtom* aNameAtom, int32_t aNamespaceID) const
  {
    return ((mInner.mName == aNameAtom) &&
            (mInner.mNamespaceID == aNamespaceID));
  }

  bool Equals(nsAtom* aNameAtom, nsAtom* aPrefixAtom, int32_t aNamespaceID) const
  {
    return ((mInner.mName == aNameAtom) &&
            (mInner.mPrefix == aPrefixAtom) &&
            (mInner.mNamespaceID == aNamespaceID));
  }

  bool NamespaceEquals(int32_t aNamespaceID) const
  {
    return mInner.mNamespaceID == aNamespaceID;
  }

  inline bool Equals(const nsAString& aName) const;

  inline bool Equals(const nsAString& aName, const nsAString& aPrefix) const;

  inline bool Equals(const nsAString& aName, int32_t aNamespaceID) const;

  inline bool Equals(const nsAString& aName, const nsAString& aPrefix, int32_t aNamespaceID) const;

  bool NamespaceEquals(const nsAString& aNamespaceURI) const;

  inline bool QualifiedNameEquals(nsAtom* aNameAtom) const;

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

private:
  NodeInfo() = delete;
  NodeInfo(const NodeInfo& aOther) = delete;

  // NodeInfo is only constructed by nsNodeInfoManager which is a friend class.
  // aName and aOwnerManager may not be null.
  NodeInfo(nsAtom* aName, nsAtom* aPrefix, int32_t aNamespaceID,
           uint16_t aNodeType, nsAtom* aExtraName,
           nsNodeInfoManager* aOwnerManager);

  ~NodeInfo();

public:
  bool CanSkip();

  /**
   * This method gets called by the cycle collector when it's time to delete
   * this object.
   */
  void DeleteCycleCollectable();

protected:
  /*
   * NodeInfoInner is used for two things:
   *
   *   1. as a member in nsNodeInfo for holding the name, prefix and
   *      namespace ID
   *   2. as the hash key in the hash table in nsNodeInfoManager
   *
   * NodeInfoInner does not do any kind of reference counting,
   * that's up to the user of this class. Since NodeInfoInner is
   * typically used as a member of NodeInfo, the hash table doesn't
   * need to delete the keys. When the value (NodeInfo) is deleted
   * the key is automatically deleted.
   */

  class NodeInfoInner
  {
  public:
    NodeInfoInner()
      : mName(nullptr), mPrefix(nullptr), mNamespaceID(kNameSpaceID_Unknown),
        mNodeType(0), mNameString(nullptr), mExtraName(nullptr),
        mHash(0), mHashInitialized(false)
    {
    }
    NodeInfoInner(nsAtom *aName, nsAtom *aPrefix, int32_t aNamespaceID,
                    uint16_t aNodeType, nsAtom* aExtraName)
      : mName(aName), mPrefix(aPrefix), mNamespaceID(aNamespaceID),
        mNodeType(aNodeType), mNameString(nullptr), mExtraName(aExtraName),
        mHash(aName->hash()), mHashInitialized(true)
    {
    }
    NodeInfoInner(const nsAString& aTmpName, nsAtom *aPrefix,
                    int32_t aNamespaceID, uint16_t aNodeType)
      : mName(nullptr), mPrefix(aPrefix), mNamespaceID(aNamespaceID),
        mNodeType(aNodeType), mNameString(&aTmpName), mExtraName(nullptr),
        mHash(0), mHashInitialized(false)
    {
    }

    nsAtom* const MOZ_OWNING_REF mName;
    nsAtom* MOZ_OWNING_REF mPrefix;
    int32_t             mNamespaceID;
    uint16_t            mNodeType; // As defined by nsIDOMNode.nodeType
    const nsAString* const mNameString;
    nsAtom* MOZ_OWNING_REF mExtraName; // Only used by PIs and DocTypes
    PLHashNumber      mHash;
    bool              mHashInitialized;
  };

  // nsNodeInfoManager needs to pass mInner to the hash table.
  friend class ::nsNodeInfoManager;

  // This is a non-owning reference, but it's safe since it's set to nullptr
  // by nsNodeInfoManager::DropDocumentReference when the document is destroyed.
  nsIDocument* MOZ_NON_OWNING_REF mDocument; // Cache of mOwnerManager->mDocument

  NodeInfoInner mInner;

  RefPtr<nsNodeInfoManager> mOwnerManager;

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

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_NodeInfo_h___ */
