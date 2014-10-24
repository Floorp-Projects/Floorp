/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ProxyAccessible_h
#define mozilla_a11y_ProxyAccessible_h

#include "mozilla/a11y/Role.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace a11y {

class DocAccessibleParent;

class ProxyAccessible
{
public:

  ProxyAccessible(uint64_t aID, ProxyAccessible* aParent,
                  DocAccessibleParent* aDoc, role aRole) :
     mParent(aParent), mDoc(aDoc), mID(aID), mRole(aRole), mOuterDoc(false)
  {
    MOZ_COUNT_CTOR(ProxyAccessible);
  }
  ~ProxyAccessible() { MOZ_COUNT_DTOR(ProxyAccessible); }

  void AddChildAt(uint32_t aIdx, ProxyAccessible* aChild)
  { mChildren.InsertElementAt(aIdx, aChild); }

  uint32_t ChildrenCount() const { return mChildren.Length(); }

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

  /**
   * Set aDesc to the description of the proxied accessible.
   */
  void Description(nsString& aDesc) const;

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
  ProxyAccessible() :
    mParent(nullptr), mDoc(nullptr), mWrapper(0), mID(0)
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

}
}

#endif
