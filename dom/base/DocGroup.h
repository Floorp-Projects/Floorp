/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DocGroup_h
#define DocGroup_h

#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsIPrincipal.h"
#include "nsTHashtable.h"
#include "nsString.h"

#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

// Two browsing contexts are considered "related" if they are reachable from one
// another through window.opener, window.parent, or window.frames. This is the
// spec concept of a "unit of related browsing contexts"
//
// Two browsing contexts are considered "similar-origin" if they can be made to
// have the same origin by setting document.domain. This is the spec concept of
// a "unit of similar-origin related browsing contexts"
//
// A TabGroup is a set of browsing contexts which are all "related". Within a
// TabGroup, browsing contexts are broken into "similar-origin" DocGroups. In
// more detail, a DocGroup is actually a collection of inner windows, and a
// TabGroup is a collection of DocGroups. A TabGroup typically will contain
// (through its DocGroups) the inner windows from one or more tabs related by
// window.opener. A DocGroup is a member of exactly one TabGroup. Inner windows
// that aren't the current window of an outer window are not part of any
// DocGroup.

class TabGroup;

class DocGroup {
private:
  typedef nsTArray<nsPIDOMWindowInner*> WindowArray;
public:
  typedef WindowArray::iterator Iterator;

  friend class TabGroup;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DocGroup)

  static void GetKey(nsIPrincipal* aPrincipal, nsACString& aString);
  bool MatchesKey(const nsACString& aKey) {
    return aKey == mKey;
  }
  TabGroup* GetTabGroup() {
    return mTabGroup;
  }
  void Remove(nsPIDOMWindowInner* aWindow);

  // Iterators for iterating over every window within the DocGroup
  Iterator begin() {
    return mWindows.begin();
  }
  Iterator end() {
    return mWindows.end();
  }

private:
  DocGroup(TabGroup* aTabGroup, const nsACString& aKey);
  ~DocGroup();

  nsCString mKey;
  RefPtr<TabGroup> mTabGroup;
  WindowArray mWindows;
};


class TabGroup {
private:
  class HashEntry : public nsCStringHashKey {
  public:
    // NOTE: Weak reference. The DocGroup destructor removes itself from itw
    // owning TabGroup.
    DocGroup* mDocGroup;
    explicit HashEntry(const nsACString* aKey);
  };

  typedef nsTHashtable<HashEntry> DocGroupMap;
public:
  typedef DocGroupMap::Iterator Iterator;

  friend class DocGroup;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TabGroup)

  TabGroup() {}

  // Get the docgroup for the corresponding doc group key.
  // Returns null if the given key hasn't been seen yet.
  already_AddRefed<DocGroup>
  GetDocGroup(const nsACString& aKey);

  already_AddRefed<DocGroup>
  JoinDocGroup(const nsACString& aKey, nsPIDOMWindowInner* aWindow);

  Iterator Iter() {
    return mDocGroups.Iter();
  }

private:
  ~TabGroup();
  DocGroupMap mDocGroups;
};

} // namespace dom
} // namespace mozilla

#endif // defined(DocGroup_h)
