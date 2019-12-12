/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TabGroup_h
#define TabGroup_h

#include "nsHashKeys.h"
#include "nsISupportsImpl.h"
#include "nsTHashtable.h"
#include "nsString.h"

#include "mozilla/Atomics.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/RefPtr.h"

class mozIDOMWindowProxy;
class nsIDocShellTreeItem;
class nsPIDOMWindowOuter;

namespace mozilla {
class AbstractThread;
class ThrottledEventQueue;
namespace dom {
class Document;
class BrowserChild;

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

class DocGroup;
class BrowserChild;

class TabGroup final : public SchedulerGroup,
                       public LinkedListElement<TabGroup> {
 private:
  class HashEntry : public nsCStringHashKey {
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

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TabGroup, override)

  static TabGroup* GetChromeTabGroup();

  // Checks if the BrowserChild already has a TabGroup assigned to it in
  // IPDL. Returns this TabGroup if it does. This could happen if the parent
  // process created the PBrowser and we needed to assign a TabGroup immediately
  // upon receiving the IPDL message. This method is main thread only.
  static TabGroup* GetFromActor(BrowserChild* aBrowserChild);

  static TabGroup* GetFromWindow(mozIDOMWindowProxy* aWindow);

  explicit TabGroup(bool aIsChrome = false);

  // Get the docgroup for the corresponding doc group key.
  // Returns null if the given key hasn't been seen yet.
  already_AddRefed<DocGroup> GetDocGroup(const nsACString& aKey);

  already_AddRefed<DocGroup> AddDocument(const nsACString& aKey,
                                         Document* aDocument);

  // Join the specified TabGroup, returning a reference to it. If aTabGroup is
  // nullptr, create a new tabgroup to join.
  static already_AddRefed<TabGroup> Join(nsPIDOMWindowOuter* aWindow,
                                         TabGroup* aTabGroup);

  void Leave(nsPIDOMWindowOuter* aWindow);

  void MaybeDestroy();

  Iterator Iter() { return mDocGroups.Iter(); }

  const nsTArray<nsPIDOMWindowOuter*>& GetWindows() { return mWindows; }

  // This method is always safe to call off the main thread. The nsIEventTarget
  // can always be used off the main thread.
  nsISerialEventTarget* EventTargetFor(TaskCategory aCategory) const override;

  static LinkedList<TabGroup>* GetTabGroupList() { return sTabGroups; }

 private:
  virtual AbstractThread* AbstractMainThreadForImpl(
      TaskCategory aCategory) override;

  TabGroup* AsTabGroup() override { return this; }

  void EnsureThrottledEventQueues();

  ~TabGroup();

  // Thread-safe members
  Atomic<bool> mLastWindowLeft;
  Atomic<bool> mThrottledQueuesInitialized;
  const bool mIsChrome;

  // Main thread only
  DocGroupMap mDocGroups;
  nsTArray<nsPIDOMWindowOuter*> mWindows;

  static LinkedList<TabGroup>* sTabGroups;
};

}  // namespace dom
}  // namespace mozilla

#endif  // defined(TabGroup_h)
