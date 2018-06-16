/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TabGroup_h
#define TabGroup_h

#include "nsHashKeys.h"
#include "nsISupportsImpl.h"
#include "nsIPrincipal.h"
#include "nsTHashtable.h"
#include "nsString.h"

#include "mozilla/Atomics.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/RefPtr.h"

class mozIDOMWindowProxy;
class nsIDocShellTreeItem;
class nsIDocument;
class nsPIDOMWindowOuter;

namespace mozilla {
class AbstractThread;
class ThrottledEventQueue;
namespace dom {
class TabChild;

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
class TabChild;

class TabGroup final : public SchedulerGroup,
                       public LinkedListElement<TabGroup>
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

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TabGroup, override)

  static TabGroup*
  GetChromeTabGroup();

  // Checks if the TabChild already has a TabGroup assigned to it in
  // IPDL. Returns this TabGroup if it does. This could happen if the parent
  // process created the PBrowser and we needed to assign a TabGroup immediately
  // upon receiving the IPDL message. This method is main thread only.
  static TabGroup* GetFromActor(TabChild* aTabChild);

  static TabGroup* GetFromWindow(mozIDOMWindowProxy* aWindow);

  explicit TabGroup(bool aIsChrome = false);

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

  // Returns the size of the set of "similar-origin" DocGroups. To
  // only consider DocGroups with at least one active document, call
  // Count with 'aActiveOnly' = true
  uint32_t Count(bool aActiveOnly = false) const;

  // Returns the nsIDocShellTreeItem with the given name, searching each of the
  // docShell trees which are within this TabGroup. It will pass itself as
  // aRequestor to each docShellTreeItem which it asks to search for the name,
  // and will not search the docShellTreeItem which is passed as aRequestor.
  //
  // This method is used in order to correctly namespace named windows based on
  // their unit of related browsing contexts.
  //
  // It is illegal to pass in the special case-insensitive names "_blank",
  // "_self", "_parent" or "_top", as those should be handled elsewhere.
  nsresult
  FindItemWithName(const nsAString& aName,
                   nsIDocShellTreeItem* aRequestor,
                   nsIDocShellTreeItem* aOriginalRequestor,
                   nsIDocShellTreeItem** aFoundItem);

  nsTArray<nsPIDOMWindowOuter*> GetTopLevelWindows() const;
  const nsTArray<nsPIDOMWindowOuter*>& GetWindows() { return mWindows; }

  // This method is always safe to call off the main thread. The nsIEventTarget
  // can always be used off the main thread.
  nsISerialEventTarget* EventTargetFor(TaskCategory aCategory) const override;

  void WindowChangedBackgroundStatus(bool aIsNowBackground);

  // Returns true if all of the TabGroup's top-level windows are in
  // the background.
  bool IsBackground() const override;

  // Increase/Decrease the number of IndexedDB transactions/databases for the
  // decision making of the preemption in the scheduler.
  Atomic<uint32_t>& IndexedDBTransactionCounter()
  {
    return mNumOfIndexedDBTransactions;
  }

  Atomic<uint32_t>& IndexedDBDatabaseCounter()
  {
    return mNumOfIndexedDBDatabases;
  }

  static LinkedList<TabGroup>* GetTabGroupList()
  {
    return sTabGroups;
  }

  // This returns true if all the window objects in all the TabGroups are
  // either inactive (for example in bfcache) or are in background tabs which
  // can be throttled.
  static bool HasOnlyThrottableTabs();

private:
  virtual AbstractThread*
  AbstractMainThreadForImpl(TaskCategory aCategory) override;

  TabGroup* AsTabGroup() override { return this; }

  void EnsureThrottledEventQueues();

  ~TabGroup();

  // Thread-safe members
  Atomic<bool> mLastWindowLeft;
  Atomic<bool> mThrottledQueuesInitialized;
  Atomic<uint32_t> mNumOfIndexedDBTransactions;
  Atomic<uint32_t> mNumOfIndexedDBDatabases;
  const bool mIsChrome;

  // Main thread only
  DocGroupMap mDocGroups;
  nsTArray<nsPIDOMWindowOuter*> mWindows;
  uint32_t mForegroundCount;

  static LinkedList<TabGroup>* sTabGroups;
};

} // namespace dom
} // namespace mozilla

#endif // defined(TabGroup_h)
