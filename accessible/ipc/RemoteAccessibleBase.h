/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessibleBase_h
#define mozilla_a11y_RemoteAccessibleBase_h

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

enum Interfaces {
  HYPERTEXT = 1,
  HYPERLINK = 1 << 1,
  IMAGE = 1 << 2,
  VALUE = 1 << 3,
  TABLE = 1 << 4,
  TABLECELL = 1 << 5,
  DOCUMENT = 1 << 6,
  SELECTION = 1 << 7,
  ACTION = 1 << 8,
};

template <class Derived>
class RemoteAccessibleBase {
 public:
  ~RemoteAccessibleBase() { MOZ_ASSERT(!mWrapper); }

  void AddChildAt(uint32_t aIdx, Derived* aChild) {
    mChildren.InsertElementAt(aIdx, aChild);
  }

  uint32_t ChildrenCount() const { return mChildren.Length(); }
  Derived* RemoteChildAt(uint32_t aIdx) const { return mChildren[aIdx]; }
  Derived* RemoteFirstChild() const {
    return mChildren.Length() ? mChildren[0] : nullptr;
  }
  Derived* RemoteLastChild() const {
    return mChildren.Length() ? mChildren[mChildren.Length() - 1] : nullptr;
  }
  Derived* RemotePrevSibling() const {
    int32_t idx = IndexInParent();
    if (idx == -1) {
      return nullptr;  // No parent.
    }
    return idx > 0 ? RemoteParent()->mChildren[idx - 1] : nullptr;
  }
  Derived* RemoteNextSibling() const {
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

  // XXX evaluate if this is fast enough.
  int32_t IndexInParent() const {
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
  role Role() const { return mRole; }

  /**
   * Return true if this is an embedded object.
   */
  bool IsEmbeddedObject() const {
    role role = Role();
    return role != roles::TEXT_LEAF && role != roles::WHITESPACE &&
           role != roles::STATICTEXT;
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

  /**
   * Return true if this proxy is a DocAccessibleParent.
   */
  bool IsDoc() const { return mIsDoc; }
  DocAccessibleParent* AsDoc() const { return IsDoc() ? mDoc : nullptr; }

  // XXX checking mRole alone may not result in same behavior as Accessibles
  // due to ARIA roles. See bug 1210477.
  inline bool IsTable() const {
    return mRole == roles::TABLE || mRole == roles::MATHML_TABLE;
  }
  inline bool IsTableRow() const {
    return (mRole == roles::ROW || mRole == roles::MATHML_TABLE_ROW ||
            mRole == roles::MATHML_LABELED_ROW);
  }
  inline bool IsTableCell() const {
    return (mRole == roles::CELL || mRole == roles::COLUMNHEADER ||
            mRole == roles::ROWHEADER || mRole == roles::GRID_CELL ||
            mRole == roles::MATHML_CELL);
  }

 protected:
  RemoteAccessibleBase(uint64_t aID, Derived* aParent,
                       DocAccessibleParent* aDoc, role aRole,
                       uint32_t aInterfaces)
      : mParent(aParent->ID()),
        mDoc(aDoc),
        mWrapper(0),
        mID(aID),
        mRole(aRole),
        mOuterDoc(false),
        mIsDoc(false),
        mHasValue(aInterfaces & Interfaces::VALUE),
        mIsHyperLink(aInterfaces & Interfaces::HYPERLINK),
        mIsHyperText(aInterfaces & Interfaces::HYPERTEXT),
        mIsSelection(aInterfaces & Interfaces::SELECTION) {}

  explicit RemoteAccessibleBase(DocAccessibleParent* aThisAsDoc)
      : mParent(kNoParent),
        mDoc(aThisAsDoc),
        mWrapper(0),
        mID(0),
        mRole(roles::DOCUMENT),
        mOuterDoc(false),
        mIsDoc(true),
        mHasValue(false),
        mIsHyperLink(false),
        mIsHyperText(false),
        mIsSelection(false) {}

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

 private:
  bool mOuterDoc : 1;

 public:
  const bool mIsDoc : 1;
  const bool mHasValue : 1;
  const bool mIsHyperLink : 1;
  const bool mIsHyperText : 1;
  const bool mIsSelection : 1;
};

extern template class RemoteAccessibleBase<RemoteAccessible>;

}  // namespace a11y
}  // namespace mozilla

#endif
