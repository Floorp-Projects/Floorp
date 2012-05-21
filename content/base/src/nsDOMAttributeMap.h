/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the |attributes| property of DOM Core's nsIDOMNode object.
 */

#ifndef nsDOMAttributeMap_h___
#define nsDOMAttributeMap_h___

#include "nsIDOMNamedNodeMap.h"
#include "nsString.h"
#include "nsRefPtrHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "prbit.h"
#include "nsIDOMNode.h"

class nsIAtom;
class nsIContent;
class nsDOMAttribute;
class nsINodeInfo;
class nsIDocument;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

/**
 * Structure used as a key for caching nsDOMAttributes in nsDOMAttributeMap's mAttributeCache.
 */
class nsAttrKey
{
public:
  /**
   * The namespace of the attribute
   */
  PRInt32  mNamespaceID;

  /**
   * The atom for attribute, weak ref. is fine as we only use it for the
   * hashcode, we never dereference it.
   */
  nsIAtom* mLocalName;

  nsAttrKey(PRInt32 aNs, nsIAtom* aName)
    : mNamespaceID(aNs), mLocalName(aName) {}

  nsAttrKey(const nsAttrKey& aAttr)
    : mNamespaceID(aAttr.mNamespaceID), mLocalName(aAttr.mLocalName) {}
};

/**
 * PLDHashEntryHdr implementation for nsAttrKey.
 */
class nsAttrHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsAttrKey& KeyType;
  typedef const nsAttrKey* KeyTypePointer;

  nsAttrHashKey(KeyTypePointer aKey) : mKey(*aKey) {}
  nsAttrHashKey(const nsAttrHashKey& aCopy) : mKey(aCopy.mKey) {}
  ~nsAttrHashKey() {}

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const
    {
      return mKey.mLocalName == aKey->mLocalName &&
             mKey.mNamespaceID == aKey->mNamespaceID;
    }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      if (!aKey)
        return 0;

      return mozilla::HashGeneric(aKey->mNamespaceID, aKey->mLocalName);
    }
  enum { ALLOW_MEMMOVE = true };

private:
  nsAttrKey mKey;
};

// Helper class that implements the nsIDOMNamedNodeMap interface.
class nsDOMAttributeMap : public nsIDOMNamedNodeMap
{
public:
  typedef mozilla::dom::Element Element;

  nsDOMAttributeMap(Element *aContent);
  virtual ~nsDOMAttributeMap();

  /**
   * Initialize the map. Must be called before the map is used.
   */
  bool Init();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNamedNodeMap interface
  NS_DECL_NSIDOMNAMEDNODEMAP

  void DropReference();

  Element* GetContent()
  {
    return mContent;
  }

  /**
   * Called when mContent is moved into a new document.
   * Updates the nodeinfos of all owned nodes.
   */
  nsresult SetOwnerDocument(nsIDocument* aDocument);

  /**
   * Drop an attribute from the map's cache (does not remove the attribute
   * from the node!)
   */
  void DropAttribute(PRInt32 aNamespaceID, nsIAtom* aLocalName);

  /**
   * Returns the number of attribute nodes currently in the map.
   * Note: this is just the number of cached attribute nodes, not the number of
   * attributes in mContent.
   *
   * @return The number of attribute nodes in the map.
   */
  PRUint32 Count() const;

  typedef nsRefPtrHashtable<nsAttrHashKey, nsDOMAttribute> AttrCache;

  /**
   * Enumerates over the attribute nodess in the map and calls aFunc for each
   * one. If aFunc returns PL_DHASH_STOP we'll stop enumerating at that point.
   *
   * @return The number of attribute nodes that aFunc was called for.
   */
  PRUint32 Enumerate(AttrCache::EnumReadFunction aFunc, void *aUserArg) const;

  nsDOMAttribute* GetItemAt(PRUint32 aIndex, nsresult *rv);
  nsDOMAttribute* GetNamedItem(const nsAString& aAttrName, nsresult *rv);

  static nsDOMAttributeMap* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMNamedNodeMap> map_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMNamedNodeMap pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(map_qi == static_cast<nsIDOMNamedNodeMap*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMAttributeMap*>(aSupports);
  }

  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMAttributeMap)

private:
  Element *mContent; // Weak reference

  /**
   * Cache of nsDOMAttributes.
   */
  AttrCache mAttributeCache;

  /**
   * SetNamedItem() (aWithNS = false) and SetNamedItemNS() (aWithNS =
   * true) implementation.
   */
  nsresult SetNamedItemInternal(nsIDOMNode *aNode,
                                nsIDOMNode **aReturn,
                                bool aWithNS);

  /**
   * GetNamedItemNS() implementation taking |aRemove| for GetAttribute(),
   * which is used by RemoveNamedItemNS().
   */
  nsresult GetNamedItemNSInternal(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName,
                                  nsIDOMNode** aReturn,
                                  bool aRemove = false);

  nsDOMAttribute* GetAttribute(nsINodeInfo* aNodeInfo, bool aNsAware);

  /**
   * Remove an attribute, returns the removed node.
   */
  nsresult RemoveAttribute(nsINodeInfo*     aNodeInfo,
                           nsIDOMNode**     aReturn);
};


#endif /* nsDOMAttributeMap_h___ */
