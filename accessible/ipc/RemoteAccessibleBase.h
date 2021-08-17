/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessibleBase_h
#define mozilla_a11y_RemoteAccessibleBase_h

#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/Role.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "LocalAccessible.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRect.h"
#include "LocalAccessible.h"

namespace mozilla {
namespace a11y {

class LocalAccessible;
class Attribute;
class DocAccessibleParent;
class RemoteAccessible;
enum class RelationType;

template <class Derived>
class RemoteAccessibleBase : public Accessible {
 public:
  virtual ~RemoteAccessibleBase() { MOZ_ASSERT(!mWrapper); }

  virtual bool IsRemote() const override { return true; }

  void AddChildAt(uint32_t aIdx, Derived* aChild) {
    mChildren.InsertElementAt(aIdx, aChild);
  }

  virtual uint32_t ChildCount() const override { return mChildren.Length(); }
  Derived* RemoteChildAt(uint32_t aIdx) const {
    return mChildren.SafeElementAt(aIdx);
  }
  Derived* RemoteFirstChild() const {
    return mChildren.Length() ? mChildren[0] : nullptr;
  }
  Derived* RemoteLastChild() const {
    return mChildren.Length() ? mChildren[mChildren.Length() - 1] : nullptr;
  }
  Derived* RemotePrevSibling() const {
    if (IsDoc()) {
      // The normal code path doesn't work for documents because the parent
      // might be a local OuterDoc, but IndexInParent() will return 1.
      // A document is always a single child of an OuterDoc anyway.
      return nullptr;
    }
    int32_t idx = IndexInParent();
    if (idx == -1) {
      return nullptr;  // No parent.
    }
    return idx > 0 ? RemoteParent()->mChildren[idx - 1] : nullptr;
  }
  Derived* RemoteNextSibling() const {
    if (IsDoc()) {
      // The normal code path doesn't work for documents because the parent
      // might be a local OuterDoc, but IndexInParent() will return 1.
      // A document is always a single child of an OuterDoc anyway.
      return nullptr;
    }
    int32_t idx = IndexInParent();
    if (idx == -1) {
      return nullptr;  // No parent.
    }
    MOZ_ASSERT(idx >= 0);
    size_t newIdx = idx + 1;
    return newIdx < RemoteParent()->mChildren.Length()
               ? RemoteParent()->mChildren[newIdx]
               : nullptr;
  }

  // Accessible hierarchy method overrides

  virtual Accessible* Parent() const override {
    if (Derived* parent = RemoteParent()) {
      return parent;
    }

    return OuterDocOfRemoteBrowser();
  }

  virtual Accessible* ChildAt(uint32_t aIndex) const override {
    return RemoteChildAt(aIndex);
  }

  virtual Accessible* NextSibling() const override {
    return RemoteNextSibling();
  }

  virtual Accessible* PrevSibling() const override {
    return RemotePrevSibling();
  }

  // XXX evaluate if this is fast enough.
  virtual int32_t IndexInParent() const override {
    Derived* parent = RemoteParent();
    if (!parent) {
      return -1;
    }
    return parent->mChildren.IndexOf(static_cast<const Derived*>(this));
  }
  uint32_t EmbeddedChildCount() const;
  int32_t IndexOfEmbeddedChild(const Derived* aChild);
  Derived* EmbeddedChildAt(size_t aChildIdx);

  void Shutdown();

  void SetChildDoc(DocAccessibleParent* aChildDoc);
  void ClearChildDoc(DocAccessibleParent* aChildDoc);

  /**
   * Remove The given child.
   */
  void RemoveChild(Derived* aChild) { mChildren.RemoveElement(aChild); }

  /**
   * Return the proxy for the parent of the wrapped accessible.
   */
  Derived* RemoteParent() const;

  LocalAccessible* OuterDocOfRemoteBrowser() const;

  /**
   * Get the role of the accessible we're proxying.
   */
  virtual role Role() const override { return mRole; }

  /**
   * Return true if this is an embedded object.
   */
  bool IsEmbeddedObject() const {
    role role = Role();
    return role != roles::TEXT_LEAF && role != roles::WHITESPACE &&
           role != roles::STATICTEXT;
  }

  virtual bool IsLink() const override {
    if (IsHTMLLink()) {
      // XXX: HTML links always return true for IsLink.
      return true;
    }

    if (IsText()) {
      return false;
    }

    if (Accessible* parent = Parent()) {
      return parent->IsHyperText();
    }

    return false;
  }

  virtual bool HasNumericValue() const override {
    // XXX: We combine the aria and native "has numeric value" field
    // when we serialize the local accessible into eNumericValue.
    return HasGenericType(eNumericValue);
  }

  /**
   * Allow the platform to store a pointers worth of data on us.
   */
  uintptr_t GetWrapper() const { return mWrapper; }
  void SetWrapper(uintptr_t aWrapper) { mWrapper = aWrapper; }

  /*
   * Return the ID of the accessible being proxied.
   */
  uint64_t ID() const { return mID; }

  /**
   * Return the document containing this proxy, or the proxy itself if it is a
   * document.
   */
  DocAccessibleParent* Document() const { return mDoc; }

  DocAccessibleParent* AsDoc() const { return IsDoc() ? mDoc : nullptr; }

 protected:
  RemoteAccessibleBase(uint64_t aID, Derived* aParent,
                       DocAccessibleParent* aDoc, role aRole, AccType aType,
                       AccGenericType aGenericTypes, uint8_t aRoleMapEntryIndex)
      : Accessible(aType, aGenericTypes, aRoleMapEntryIndex),
        mParent(aParent->ID()),
        mDoc(aDoc),
        mWrapper(0),
        mID(aID),
        mRole(aRole) {}

  explicit RemoteAccessibleBase(DocAccessibleParent* aThisAsDoc)
      : Accessible(),
        mParent(kNoParent),
        mDoc(aThisAsDoc),
        mWrapper(0),
        mID(0),
        mRole(roles::DOCUMENT) {
    mGenericTypes = eDocument | eHyperText;
  }

 protected:
  void SetParent(Derived* aParent);

 private:
  uintptr_t mParent;
  static const uintptr_t kNoParent = UINTPTR_MAX;

  friend Derived;

  nsTArray<Derived*> mChildren;
  DocAccessibleParent* mDoc;
  uintptr_t mWrapper;
  uint64_t mID;

 protected:
  // XXX DocAccessibleParent gets to change this to change the role of
  // documents.
  role mRole : 27;
};

extern template class RemoteAccessibleBase<RemoteAccessible>;

}  // namespace a11y
}  // namespace mozilla

#endif
