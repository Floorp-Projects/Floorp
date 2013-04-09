/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the |attributes| property of DOM Core's Element object.
 */

#ifndef nsDOMAttributeMap_h
#define nsDOMAttributeMap_h

#include "nsIDOMMozNamedAttrMap.h"
#include "nsStringGlue.h"
#include "nsRefPtrHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMAttr.h"
#include "mozilla/ErrorResult.h"

class nsIAtom;
class nsINodeInfo;
class nsIDocument;

namespace mozilla {
namespace dom {
class Attr;
class Element;
} // namespace dom
} // namespace mozilla

/**
 * Structure used as a key for caching Attrs in nsDOMAttributeMap's mAttributeCache.
 */
class nsAttrKey
{
public:
  /**
   * The namespace of the attribute
   */
  int32_t  mNamespaceID;

  /**
   * The atom for attribute, weak ref. is fine as we only use it for the
   * hashcode, we never dereference it.
   */
  nsIAtom* mLocalName;

  nsAttrKey(int32_t aNs, nsIAtom* aName)
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

// Helper class that implements the nsIDOMMozNamedAttrMap interface.
class nsDOMAttributeMap : public nsIDOMMozNamedAttrMap
{
public:
  typedef mozilla::dom::Element Element;

  nsDOMAttributeMap(Element *aContent);
  virtual ~nsDOMAttributeMap();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMMozNamedAttrMap interface
  NS_DECL_NSIDOMMOZNAMEDATTRMAP

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
  void DropAttribute(int32_t aNamespaceID, nsIAtom* aLocalName);

  /**
   * Returns the number of attribute nodes currently in the map.
   * Note: this is just the number of cached attribute nodes, not the number of
   * attributes in mContent.
   *
   * @return The number of attribute nodes in the map.
   */
  uint32_t Count() const;

  typedef nsRefPtrHashtable<nsAttrHashKey, mozilla::dom::Attr> AttrCache;

  /**
   * Enumerates over the attribute nodess in the map and calls aFunc for each
   * one. If aFunc returns PL_DHASH_STOP we'll stop enumerating at that point.
   *
   * @return The number of attribute nodes that aFunc was called for.
   */
  uint32_t Enumerate(AttrCache::EnumReadFunction aFunc, void *aUserArg) const;

  mozilla::dom::Attr* GetItemAt(uint32_t aIndex, nsresult *rv);
  mozilla::dom::Attr* GetNamedItem(const nsAString& aAttrName);

  static nsDOMAttributeMap* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMMozNamedAttrMap> map_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMMozNamedAttrMap pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(map_qi == static_cast<nsIDOMMozNamedAttrMap*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMAttributeMap*>(aSupports);
  }

  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMAttributeMap)

  mozilla::dom::Attr* GetNamedItemNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
                                 mozilla::ErrorResult& aError);

  already_AddRefed<mozilla::dom::Attr> SetNamedItemNS(nsIDOMAttr *aNode,
                                                  mozilla::ErrorResult& aError)
  {
    return SetNamedItemInternal(aNode, true, aError);
  }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  Element *mContent; // Weak reference

  /**
   * Cache of Attrs.
   */
  AttrCache mAttributeCache;

  /**
   * SetNamedItem() (aWithNS = false) and SetNamedItemNS() (aWithNS =
   * true) implementation.
   */
  already_AddRefed<mozilla::dom::Attr>
    SetNamedItemInternal(nsIDOMAttr *aNode,
                         bool aWithNS,
                         mozilla::ErrorResult& aError);

  already_AddRefed<nsINodeInfo>
  GetAttrNodeInfo(const nsAString& aNamespaceURI,
                  const nsAString& aLocalName,
                  mozilla::ErrorResult& aError);

  mozilla::dom::Attr* GetAttribute(nsINodeInfo* aNodeInfo, bool aNsAware);

  /**
   * Remove an attribute, returns the removed node.
   */
  already_AddRefed<mozilla::dom::Attr> RemoveAttribute(nsINodeInfo* aNodeInfo);
};


#endif /* nsDOMAttributeMap_h */
