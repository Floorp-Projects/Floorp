/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ProxyAccessible_h
#define mozilla_a11y_ProxyAccessible_h

#include "mozilla/a11y/Role.h"
#include "nsIAccessibleText.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace a11y {

class Attribute;
class DocAccessibleParent;
enum class RelationType;

class ProxyAccessible
{
public:

  ProxyAccessible(uint64_t aID, ProxyAccessible* aParent,
                  DocAccessibleParent* aDoc, role aRole) :
     mParent(aParent), mDoc(aDoc), mWrapper(0), mID(aID), mRole(aRole),
     mOuterDoc(false)
  {
    MOZ_COUNT_CTOR(ProxyAccessible);
  }
  ~ProxyAccessible()
  {
    MOZ_COUNT_DTOR(ProxyAccessible);
    MOZ_ASSERT(!mWrapper);
  }

  void AddChildAt(uint32_t aIdx, ProxyAccessible* aChild)
  { mChildren.InsertElementAt(aIdx, aChild); }

  uint32_t ChildrenCount() const { return mChildren.Length(); }
  ProxyAccessible* ChildAt(uint32_t aIdx) const { return mChildren[aIdx]; }
  bool MustPruneChildren() const;

  void Shutdown();

  void SetChildDoc(DocAccessibleParent*);

  /**
   * Remove The given child.
   */
  void RemoveChild(ProxyAccessible* aChild)
    { mChildren.RemoveElement(aChild); }

  /**
   * Return the proxy for the parent of the wrapped accessible.
   */
  ProxyAccessible* Parent() const { return mParent; }

  /**
   * Get the role of the accessible we're proxying.
   */
  role Role() const { return mRole; }

  /*
   * Return the states for the proxied accessible.
   */
  uint64_t State() const;

  /*
   * Set aName to the name of the proxied accessible.
   */
  void Name(nsString& aName) const;

  /*
   * Set aValue to the value of the proxied accessible.
   */
  void Value(nsString& aValue) const;

  /**
   * Set aDesc to the description of the proxied accessible.
   */
  void Description(nsString& aDesc) const;

  /**
   * Get the set of attributes on the proxied accessible.
   */
  void Attributes(nsTArray<Attribute> *aAttrs) const;

  /**
   * Return set of targets of given relation type.
   */
  nsTArray<ProxyAccessible*> RelationByType(RelationType aType) const;

  /**
   * Get all relations for this accessible.
   */
  void Relations(nsTArray<RelationType>* aTypes,
                 nsTArray<nsTArray<ProxyAccessible*>>* aTargetSets) const;

  /**
   * Get the text between the given offsets.
   */
  void TextSubstring(int32_t aStartOffset, int32_t aEndOfset,
                     nsString& aText) const;

  void GetTextAfterOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                          nsString& aText, int32_t* aStartOffset,
                          int32_t* aEndOffset);

  void GetTextAtOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                       nsString& aText, int32_t* aStartOffset,
                       int32_t* aEndOffset);

  void GetTextBeforeOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                           nsString& aText, int32_t* aStartOffset,
                           int32_t* aEndOffset);

  /**
   * Allow the platform to store a pointers worth of data on us.
   */
  uintptr_t GetWrapper() const { return mWrapper; }
  void SetWrapper(uintptr_t aWrapper) { mWrapper = aWrapper; }

  /*
   * Return the ID of the accessible being proxied.
   */
  uint64_t ID() const { return mID; }

protected:
  explicit ProxyAccessible(DocAccessibleParent* aThisAsDoc) :
    mParent(nullptr), mDoc(aThisAsDoc), mWrapper(0), mID(0),
    mRole(roles::DOCUMENT)
  { MOZ_COUNT_CTOR(ProxyAccessible); }

protected:
  ProxyAccessible* mParent;

private:
  nsTArray<ProxyAccessible*> mChildren;
  DocAccessibleParent* mDoc;
  uintptr_t mWrapper;
  uint64_t mID;
  role mRole : 31;
  bool mOuterDoc : 1;
};

enum Interfaces
{
  HYPERTEXT = 1
};

}
}

#endif
