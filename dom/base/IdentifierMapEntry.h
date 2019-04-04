/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Entry for the Document or ShadowRoot's identifier map.
 */

#ifndef mozilla_IdentifierMapEntry_h
#define mozilla_IdentifierMapEntry_h

#include "PLDHashTable.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/dom/TreeOrderedArray.h"
#include "mozilla/net/ReferrerPolicy.h"

#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashtable.h"

class nsIContent;
class nsContentList;
class nsBaseContentList;

namespace mozilla {
namespace dom {
class Document;
class Element;
}  // namespace dom

/**
 * Right now our identifier map entries contain information for 'name'
 * and 'id' mappings of a given string. This is so that
 * nsHTMLDocument::ResolveName only has to do one hash lookup instead
 * of two. It's not clear whether this still matters for performance.
 *
 * We also store the document.all result list here. This is mainly so that
 * when all elements with the given ID are removed and we remove
 * the ID's IdentifierMapEntry, the document.all result is released too.
 * Perhaps the document.all results should have their own hashtable
 * in nsHTMLDocument.
 */
class IdentifierMapEntry : public PLDHashEntryHdr {
  typedef dom::Document Document;
  typedef dom::Element Element;
  typedef net::ReferrerPolicy ReferrerPolicy;

  /**
   * @see Document::IDTargetObserver, this is just here to avoid include hell.
   */
  typedef bool (*IDTargetObserver)(Element* aOldElement, Element* aNewelement,
                                   void* aData);

 public:
  struct AtomOrString {
    MOZ_IMPLICIT AtomOrString(nsAtom* aAtom) : mAtom(aAtom) {}
    MOZ_IMPLICIT AtomOrString(const nsAString& aString) : mString(aString) {}
    AtomOrString(const AtomOrString& aOther)
        : mAtom(aOther.mAtom), mString(aOther.mString) {}

    AtomOrString(AtomOrString&& aOther)
        : mAtom(aOther.mAtom.forget()), mString(aOther.mString) {}

    RefPtr<nsAtom> mAtom;
    const nsString mString;
  };

  typedef const AtomOrString& KeyType;
  typedef const AtomOrString* KeyTypePointer;

  explicit IdentifierMapEntry(const AtomOrString& aKey);
  explicit IdentifierMapEntry(const AtomOrString* aKey);
  IdentifierMapEntry(IdentifierMapEntry&& aOther);
  ~IdentifierMapEntry();

  nsString GetKeyAsString() const {
    if (mKey.mAtom) {
      return nsAtomString(mKey.mAtom);
    }

    return mKey.mString;
  }

  bool KeyEquals(const KeyTypePointer aOtherKey) const {
    if (mKey.mAtom) {
      if (aOtherKey->mAtom) {
        return mKey.mAtom == aOtherKey->mAtom;
      }

      return mKey.mAtom->Equals(aOtherKey->mString);
    }

    if (aOtherKey->mAtom) {
      return aOtherKey->mAtom->Equals(mKey.mString);
    }

    return mKey.mString.Equals(aOtherKey->mString);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  static PLDHashNumber HashKey(const KeyTypePointer aKey) {
    return aKey->mAtom ? aKey->mAtom->hash() : HashString(aKey->mString);
  }

  enum { ALLOW_MEMMOVE = false };

  void AddNameElement(nsINode* aDocument, Element* aElement);
  void RemoveNameElement(Element* aElement);
  bool IsEmpty();
  nsBaseContentList* GetNameContentList() { return mNameContentList; }
  bool HasNameElement() const;

  /**
   * Returns the element if we know the element associated with this
   * id. Otherwise returns null.
   */
  Element* GetIdElement() { return mIdContentList->SafeElementAt(0); }

  /**
   * Returns the list of all elements associated with this id.
   */
  const nsTArray<Element*>& GetIdElements() const { return mIdContentList; }

  /**
   * If this entry has a non-null image element set (using SetImageElement),
   * the image element will be returned, otherwise the same as GetIdElement().
   */
  Element* GetImageIdElement() {
    return mImageElement ? mImageElement.get() : GetIdElement();
  }

  /**
   * This can fire ID change callbacks.
   */
  void AddIdElement(Element* aElement);
  /**
   * This can fire ID change callbacks.
   */
  void RemoveIdElement(Element* aElement);
  /**
   * Set the image element override for this ID. This will be returned by
   * GetIdElement(true) if non-null.
   */
  void SetImageElement(Element* aElement);
  bool HasIdElementExposedAsHTMLDocumentProperty();

  bool HasContentChangeCallback() { return mChangeCallbacks != nullptr; }
  void AddContentChangeCallback(IDTargetObserver aCallback, void* aData,
                                bool aForImage);
  void RemoveContentChangeCallback(IDTargetObserver aCallback, void* aData,
                                   bool aForImage);

  void Traverse(nsCycleCollectionTraversalCallback* aCallback);

  struct ChangeCallback {
    IDTargetObserver mCallback;
    void* mData;
    bool mForImage;
  };

  struct ChangeCallbackEntry : public PLDHashEntryHdr {
    typedef const ChangeCallback KeyType;
    typedef const ChangeCallback* KeyTypePointer;

    explicit ChangeCallbackEntry(const ChangeCallback* aKey) : mKey(*aKey) {}
    ChangeCallbackEntry(ChangeCallbackEntry&& aOther)
        : PLDHashEntryHdr(std::move(aOther)), mKey(std::move(aOther.mKey)) {}

    KeyType GetKey() const { return mKey; }
    bool KeyEquals(KeyTypePointer aKey) const {
      return aKey->mCallback == mKey.mCallback && aKey->mData == mKey.mData &&
             aKey->mForImage == mKey.mForImage;
    }

    static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
    static PLDHashNumber HashKey(KeyTypePointer aKey) {
      return HashGeneric(aKey->mCallback, aKey->mData);
    }
    enum { ALLOW_MEMMOVE = true };

    ChangeCallback mKey;
  };

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

 private:
  IdentifierMapEntry(const IdentifierMapEntry& aOther) = delete;
  IdentifierMapEntry& operator=(const IdentifierMapEntry& aOther) = delete;

  void FireChangeCallbacks(Element* aOldElement, Element* aNewElement,
                           bool aImageOnly = false);

  AtomOrString mKey;
  dom::TreeOrderedArray<Element> mIdContentList;
  RefPtr<nsBaseContentList> mNameContentList;
  nsAutoPtr<nsTHashtable<ChangeCallbackEntry> > mChangeCallbacks;
  RefPtr<Element> mImageElement;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_IdentifierMapEntry_h
