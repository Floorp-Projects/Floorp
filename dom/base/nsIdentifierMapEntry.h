/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all our document implementations.
 */

#ifndef nsIdentifierMapEntry_h
#define nsIdentifierMapEntry_h

#include "PLDHashTable.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/dom/Element.h"
#include "mozilla/net/ReferrerPolicy.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsContentList.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsTArray.h"
#include "nsTHashtable.h"

class nsIContent;

/**
 * Right now our identifier map entries contain information for 'name'
 * and 'id' mappings of a given string. This is so that
 * nsHTMLDocument::ResolveName only has to do one hash lookup instead
 * of two. It's not clear whether this still matters for performance.
 *
 * We also store the document.all result list here. This is mainly so that
 * when all elements with the given ID are removed and we remove
 * the ID's nsIdentifierMapEntry, the document.all result is released too.
 * Perhaps the document.all results should have their own hashtable
 * in nsHTMLDocument.
 */
class nsIdentifierMapEntry : public PLDHashEntryHdr
{
public:
  struct AtomOrString
  {
    MOZ_IMPLICIT AtomOrString(nsIAtom* aAtom) : mAtom(aAtom) {}
    MOZ_IMPLICIT AtomOrString(const nsAString& aString) : mString(aString) {}
    AtomOrString(const AtomOrString& aOther)
      : mAtom(aOther.mAtom)
      , mString(aOther.mString)
    {
    }

    AtomOrString(AtomOrString&& aOther)
      : mAtom(aOther.mAtom.forget())
      , mString(aOther.mString)
    {
    }

    nsCOMPtr<nsIAtom> mAtom;
    const nsString mString;
  };

  typedef const AtomOrString& KeyType;
  typedef const AtomOrString* KeyTypePointer;

  typedef mozilla::dom::Element Element;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

  explicit nsIdentifierMapEntry(const AtomOrString& aKey)
    : mKey(aKey)
  {
  }
  explicit nsIdentifierMapEntry(const AtomOrString* aKey)
    : mKey(aKey ? *aKey : nullptr)
  {
  }
  nsIdentifierMapEntry(nsIdentifierMapEntry&& aOther) :
    mKey(mozilla::Move(aOther.GetKey())),
    mIdContentList(mozilla::Move(aOther.mIdContentList)),
    mNameContentList(aOther.mNameContentList.forget()),
    mChangeCallbacks(aOther.mChangeCallbacks.forget()),
    mImageElement(aOther.mImageElement.forget())
  {
  }
  ~nsIdentifierMapEntry();

  KeyType GetKey() const { return mKey; }

  nsString GetKeyAsString() const
  {
    if (mKey.mAtom) {
      return nsAtomString(mKey.mAtom);
    }

    return mKey.mString;
  }

  bool KeyEquals(const KeyTypePointer aOtherKey) const
  {
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

  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
    return aKey->mAtom ?
      aKey->mAtom->hash() : mozilla::HashString(aKey->mString);
  }

  enum { ALLOW_MEMMOVE = false };

  void AddNameElement(nsINode* aDocument, Element* aElement);
  void RemoveNameElement(Element* aElement);
  bool IsEmpty();
  nsBaseContentList* GetNameContentList() {
    return mNameContentList;
  }
  bool HasNameElement() const {
    return mNameContentList && mNameContentList->Length() != 0;
  }

  /**
   * Returns the element if we know the element associated with this
   * id. Otherwise returns null.
   */
  Element* GetIdElement();
  /**
   * Returns the list of all elements associated with this id.
   */
  const nsTArray<Element*>& GetIdElements() const {
    return mIdContentList;
  }
  /**
   * If this entry has a non-null image element set (using SetImageElement),
   * the image element will be returned, otherwise the same as GetIdElement().
   */
  Element* GetImageIdElement();
  /**
   * Append all the elements with this id to aElements
   */
  void AppendAllIdContent(nsCOMArray<nsIContent>* aElements);
  /**
   * This can fire ID change callbacks.
   * @return true if the content could be added, false if we failed due
   * to OOM.
   */
  bool AddIdElement(Element* aElement);
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
  void AddContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                void* aData, bool aForImage);
  void RemoveContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                void* aData, bool aForImage);

  void Traverse(nsCycleCollectionTraversalCallback* aCallback);

  struct ChangeCallback {
    nsIDocument::IDTargetObserver mCallback;
    void* mData;
    bool mForImage;
  };

  struct ChangeCallbackEntry : public PLDHashEntryHdr {
    typedef const ChangeCallback KeyType;
    typedef const ChangeCallback* KeyTypePointer;

    explicit ChangeCallbackEntry(const ChangeCallback* aKey) :
      mKey(*aKey) { }
    ChangeCallbackEntry(const ChangeCallbackEntry& toCopy) :
      mKey(toCopy.mKey) { }

    KeyType GetKey() const { return mKey; }
    bool KeyEquals(KeyTypePointer aKey) const {
      return aKey->mCallback == mKey.mCallback &&
             aKey->mData == mKey.mData &&
             aKey->mForImage == mKey.mForImage;
    }

    static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      return mozilla::HashGeneric(aKey->mCallback, aKey->mData);
    }
    enum { ALLOW_MEMMOVE = true };

    ChangeCallback mKey;
  };

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  nsIdentifierMapEntry(const nsIdentifierMapEntry& aOther) = delete;
  nsIdentifierMapEntry& operator=(const nsIdentifierMapEntry& aOther) = delete;

  void FireChangeCallbacks(Element* aOldElement, Element* aNewElement,
                           bool aImageOnly = false);

  AtomOrString mKey;
  // empty if there are no elements with this ID.
  // The elements are stored as weak pointers.
  AutoTArray<Element*, 1> mIdContentList;
  RefPtr<nsBaseContentList> mNameContentList;
  nsAutoPtr<nsTHashtable<ChangeCallbackEntry> > mChangeCallbacks;
  RefPtr<Element> mImageElement;
};

#endif // #ifndef nsIdentifierMapEntry_h
