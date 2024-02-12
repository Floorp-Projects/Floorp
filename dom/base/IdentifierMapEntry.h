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

#include <utility>

#include "PLDHashTable.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/TreeOrderedArray.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentList.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashtable.h"

class nsIContent;
class nsINode;

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

  /**
   * @see Document::IDTargetObserver, this is just here to avoid include hell.
   */
  typedef bool (*IDTargetObserver)(Element* aOldElement, Element* aNewelement,
                                   void* aData);

 public:
  // We use DependentAtomOrString as our external key interface.  This allows
  // consumers to use an nsAString, for example, without forcing a copy.
  struct DependentAtomOrString final {
    MOZ_IMPLICIT DependentAtomOrString(nsAtom* aAtom)
        : mAtom(aAtom), mString(nullptr) {}
    MOZ_IMPLICIT DependentAtomOrString(const nsAString& aString)
        : mAtom(nullptr), mString(&aString) {}
    DependentAtomOrString(const DependentAtomOrString& aOther) = default;

    nsAtom* mAtom;
    const nsAString* mString;
  };

  typedef const DependentAtomOrString& KeyType;
  typedef const DependentAtomOrString* KeyTypePointer;

  explicit IdentifierMapEntry(const DependentAtomOrString* aKey);
  IdentifierMapEntry(IdentifierMapEntry&& aOther) = default;
  ~IdentifierMapEntry() = default;

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

      return mKey.mAtom->Equals(*aOtherKey->mString);
    }

    if (aOtherKey->mAtom) {
      return aOtherKey->mAtom->Equals(mKey.mString);
    }

    return mKey.mString.Equals(*aOtherKey->mString);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  static PLDHashNumber HashKey(const KeyTypePointer aKey) {
    return aKey->mAtom ? aKey->mAtom->hash() : HashString(*aKey->mString);
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
  Element* GetIdElement() const { return mIdContentList->SafeElementAt(0); }

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
  bool HasIdElementExposedAsHTMLDocumentProperty() const;

  bool HasContentChangeCallback() { return mChangeCallbacks != nullptr; }
  void AddContentChangeCallback(IDTargetObserver aCallback, void* aData,
                                bool aForImage);
  void RemoveContentChangeCallback(IDTargetObserver aCallback, void* aData,
                                   bool aForImage);

  /**
   * Remove all elements and notify change listeners.
   */
  void ClearAndNotify();

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
  // We use an OwningAtomOrString as our internal key storage.  It needs to own
  // the key string, whether in atom or string form.
  struct OwningAtomOrString final {
    OwningAtomOrString(const OwningAtomOrString& aOther) = delete;
    OwningAtomOrString(OwningAtomOrString&& aOther) = default;

    explicit OwningAtomOrString(const DependentAtomOrString& aOther)
        // aOther may have a null mString, so jump through a bit of a hoop in
        // that case.  I wish there were a way to just default-initialize
        // mString in that situation...  We could also make mString not const
        // and only assign to it if aOther.mString is not null, but having it be
        // const is nice.
        : mAtom(aOther.mAtom),
          mString(aOther.mString ? *aOther.mString : u""_ns) {}

    RefPtr<nsAtom> mAtom;
    nsString mString;
  };

  IdentifierMapEntry(const IdentifierMapEntry& aOther) = delete;
  IdentifierMapEntry& operator=(const IdentifierMapEntry& aOther) = delete;

  void FireChangeCallbacks(Element* aOldElement, Element* aNewElement,
                           bool aImageOnly = false);

  OwningAtomOrString mKey;
  dom::TreeOrderedArray<Element> mIdContentList;
  RefPtr<nsBaseContentList> mNameContentList;
  UniquePtr<nsTHashtable<ChangeCallbackEntry> > mChangeCallbacks;
  RefPtr<Element> mImageElement;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_IdentifierMapEntry_h
