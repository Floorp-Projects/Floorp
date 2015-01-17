/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the |attributes| property of DOM Core's Element object.
 */

#ifndef nsDOMAttributeMap_h
#define nsDOMAttributeMap_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMMozNamedAttrMap.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsIAtom;
class nsIDocument;

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

  explicit nsAttrHashKey(KeyTypePointer aKey) : mKey(*aKey) {}
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
                        , public nsWrapperCache
{
public:
  typedef mozilla::dom::Attr Attr;
  typedef mozilla::dom::Element Element;
  typedef mozilla::ErrorResult ErrorResult;

  explicit nsDOMAttributeMap(Element *aContent);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(nsDOMAttributeMap)

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

  typedef nsRefPtrHashtable<nsAttrHashKey, Attr> AttrCache;

  /**
   * Enumerates over the attribute nodess in the map and calls aFunc for each
   * one. If aFunc returns PL_DHASH_STOP we'll stop enumerating at that point.
   *
   * @return The number of attribute nodes that aFunc was called for.
   */
  uint32_t Enumerate(AttrCache::EnumReadFunction aFunc, void *aUserArg) const;

  Element* GetParentObject() const
  {
    return mContent;
  }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  Attr* GetNamedItem(const nsAString& aAttrName);
  Attr* NamedGetter(const nsAString& aAttrName, bool& aFound);
  bool NameIsEnumerable(const nsAString& aName);
  already_AddRefed<Attr>
  SetNamedItem(Attr& aAttr, ErrorResult& aError)
  {
    return SetNamedItemInternal(aAttr, false, aError);
  }
  already_AddRefed<Attr>
  RemoveNamedItem(mozilla::dom::NodeInfo* aNodeInfo, ErrorResult& aError);
  already_AddRefed<Attr>
  RemoveNamedItem(const nsAString& aName, ErrorResult& aError);
 
  Attr* Item(uint32_t aIndex);
  Attr* IndexedGetter(uint32_t aIndex, bool& aFound);
  uint32_t Length() const;

  Attr*
  GetNamedItemNS(const nsAString& aNamespaceURI,
                 const nsAString& aLocalName);
  already_AddRefed<Attr>
  SetNamedItemNS(Attr& aNode, ErrorResult& aError)
  {
    return SetNamedItemInternal(aNode, true, aError);
  }
  already_AddRefed<Attr>
  RemoveNamedItemNS(const nsAString& aNamespaceURI, const nsAString& aLocalName,
                    ErrorResult& aError);

  void GetSupportedNames(unsigned, nsTArray<nsString>& aNames)
  {
    // No supported names we want to show up in iteration.
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
  virtual ~nsDOMAttributeMap();

private:
  nsCOMPtr<Element> mContent;

  /**
   * Cache of Attrs. It's usually empty, and thus initialized lazily.
   */
  mozilla::UniquePtr<AttrCache> mAttributeCache;

  void EnsureAttributeCache();

  /**
   * SetNamedItem() (aWithNS = false) and SetNamedItemNS() (aWithNS =
   * true) implementation.
   */
  already_AddRefed<Attr>
  SetNamedItemInternal(Attr& aNode, bool aWithNS, ErrorResult& aError);

  already_AddRefed<mozilla::dom::NodeInfo>
  GetAttrNodeInfo(const nsAString& aNamespaceURI,
                  const nsAString& aLocalName);

  Attr* GetAttribute(mozilla::dom::NodeInfo* aNodeInfo, bool aNsAware);

  /**
   * Remove an attribute, returns the removed node.
   */
  already_AddRefed<Attr> RemoveAttribute(mozilla::dom::NodeInfo* aNodeInfo);
};

// XXX khuey yes this is crazy.  The bindings code needs to see this include,
// but if we pull it in at the top of the file we get a circular include
// problem.
#include "mozilla/dom/Element.h"

#endif /* nsDOMAttributeMap_h */
