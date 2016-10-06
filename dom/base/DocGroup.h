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
// more detail, a DocGroup is actually a collection of documents, and a
// TabGroup is a collection of DocGroups. A TabGroup typically will contain
// (through its DocGroups) the documents from one or more tabs related by
// window.opener. A DocGroup is a member of exactly one TabGroup.

class TabGroup;

class DocGroup final : public nsISupports
{
public:
  typedef nsTArray<nsIDocument*>::iterator Iterator;
  friend class TabGroup;

  NS_DECL_THREADSAFE_ISUPPORTS

  static void GetKey(nsIPrincipal* aPrincipal, nsACString& aString);
  bool MatchesKey(const nsACString& aKey)
  {
    return aKey == mKey;
  }
  TabGroup* GetTabGroup()
  {
    return mTabGroup;
  }
  void RemoveDocument(nsIDocument* aWindow);

  // Iterators for iterating over every document within the DocGroup
  Iterator begin()
  {
    return mDocuments.begin();
  }
  Iterator end()
  {
    return mDocuments.end();
  }

private:
  DocGroup(TabGroup* aTabGroup, const nsACString& aKey);
  ~DocGroup();

  nsCString mKey;
  RefPtr<TabGroup> mTabGroup;
  nsTArray<nsIDocument*> mDocuments;
};


class TabGroup final : public nsISupports
{
private:
  class HashEntry : public nsCStringHashKey
  {
  public:
    // NOTE: Weak reference. The DocGroup destructor removes itself from its
    // owning TabGroup.
    DocGroup* mDocGroup;
    explicit HashEntry(const nsACString* aKey);
  };

  typedef nsTHashtable<HashEntry> DocGroupMap;
public:
  typedef DocGroupMap::Iterator Iterator;

  friend class DocGroup;

  NS_DECL_THREADSAFE_ISUPPORTS

  static TabGroup*
  GetChromeTabGroup();

  TabGroup();

  // Get the docgroup for the corresponding doc group key.
  // Returns null if the given key hasn't been seen yet.
  already_AddRefed<DocGroup>
  GetDocGroup(const nsACString& aKey);

  already_AddRefed<DocGroup>
  AddDocument(const nsACString& aKey, nsIDocument* aDocument);

  // Join the specified TabGroup, returning a reference to it. If aTabGroup is
  // nullptr, create a new tabgroup to join.
  static already_AddRefed<TabGroup>
  Join(nsPIDOMWindowOuter* aWindow, TabGroup* aTabGroup);

  void Leave(nsPIDOMWindowOuter* aWindow);

  Iterator Iter()
  {
    return mDocGroups.Iter();
  }

private:
  ~TabGroup();
  DocGroupMap mDocGroups;
  nsTArray<nsPIDOMWindowOuter*> mWindows;
};

} // namespace dom
} // namespace mozilla

#endif // defined(DocGroup_h)
