/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ProxyAccessibleBase_h
#define mozilla_a11y_ProxyAccessibleBase_h

#include "mozilla/a11y/Role.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "Accessible.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRect.h"
#include "Accessible.h"

namespace mozilla {
namespace a11y {

class Accessible;
class Attribute;
class DocAccessibleParent;
class ProxyAccessible;
enum class RelationType;

enum Interfaces
{
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
class ProxyAccessibleBase
{
public:
  ~ProxyAccessibleBase()
  {
    MOZ_ASSERT(!mWrapper);
  }

  void AddChildAt(uint32_t aIdx, Derived* aChild)
  { mChildren.InsertElementAt(aIdx, aChild); }

  uint32_t ChildrenCount() const { return mChildren.Length(); }
  Derived* ChildAt(uint32_t aIdx) const { return mChildren[aIdx]; }
  Derived* FirstChild() const
    { return mChildren.Length() ? mChildren[0] : nullptr; }
  Derived* LastChild() const
    { return mChildren.Length() ? mChildren[mChildren.Length() - 1] : nullptr; }
  Derived* PrevSibling() const
  {
    size_t idx = IndexInParent();
    return idx > 0 ? Parent()->mChildren[idx - 1] : nullptr;
  }
  Derived* NextSibling() const
  {
    size_t idx = IndexInParent();
    return idx + 1 < Parent()->mChildren.Length() ? Parent()->mChildren[idx + 1]
    : nullptr;
  }

  // XXX evaluate if this is fast enough.
  size_t IndexInParent() const { return
    Parent()->mChildren.IndexOf(static_cast<const Derived*>(this)); }
  uint32_t EmbeddedChildCount() const;
  int32_t IndexOfEmbeddedChild(const Derived* aChild);
  Derived* EmbeddedChildAt(size_t aChildIdx);
  bool MustPruneChildren() const;

  void Shutdown();

  void SetChildDoc(DocAccessibleParent*);

  /**
   * Remove The given child.
   */
  void RemoveChild(Derived* aChild)
    { mChildren.RemoveElement(aChild); }

  /**
   * Return the proxy for the parent of the wrapped accessible.
   */
  Derived* Parent() const { return mParent; }

  Accessible* OuterDocOfRemoteBrowser() const;

  /**
   * Get the role of the accessible we're proxying.
   */
  role Role() const { return mRole; }

  /**
   * Return true if this is an embedded object.
   */
  bool IsEmbeddedObject() const
  {
    role role = Role();
    return role != roles::TEXT_LEAF &&
           role != roles::WHITESPACE &&
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

protected:
  ProxyAccessibleBase(uint64_t aID, Derived* aParent,
                      DocAccessibleParent* aDoc, role aRole,
                      uint32_t aInterfaces)
    : mParent(aParent)
    , mDoc(aDoc)
    , mWrapper(0)
    , mID(aID)
    , mRole(aRole)
    , mOuterDoc(false)
    , mIsDoc(false)
    , mHasValue(aInterfaces & Interfaces::VALUE)
    , mIsHyperLink(aInterfaces & Interfaces::HYPERLINK)
    , mIsHyperText(aInterfaces & Interfaces::HYPERTEXT)
  {
  }

  explicit ProxyAccessibleBase(DocAccessibleParent* aThisAsDoc) :
    mParent(nullptr), mDoc(aThisAsDoc), mWrapper(0), mID(0),
    mRole(roles::DOCUMENT), mOuterDoc(false), mIsDoc(true), mHasValue(false),
    mIsHyperLink(false), mIsHyperText(false)
  {}

protected:
  Derived* mParent;

private:
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
  const bool mIsDoc: 1;
  const bool mHasValue: 1;
  const bool mIsHyperLink: 1;
  const bool mIsHyperText: 1;
};

extern template class ProxyAccessibleBase<ProxyAccessible>;

}
}

#endif
