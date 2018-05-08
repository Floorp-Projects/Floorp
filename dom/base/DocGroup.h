/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DocGroup_h
#define DocGroup_h

#include "nsISupportsImpl.h"
#include "nsIPrincipal.h"
#include "nsTHashtable.h"
#include "nsString.h"

#include "mozilla/dom/TabGroup.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/PerformanceCounter.h"


namespace mozilla {
class AbstractThread;
namespace dom {

class PerformanceInfo;

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

class DocGroup final
{
public:
  typedef nsTArray<nsIDocument*>::iterator Iterator;
  friend class TabGroup;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DocGroup)

  // Returns NS_ERROR_FAILURE and sets |aString| to an empty string if the TLD
  // service isn't available. Returns NS_OK on success, but may still set
  // |aString| may still be set to an empty string.
  static MOZ_MUST_USE nsresult
  GetKey(nsIPrincipal* aPrincipal, nsACString& aString);

  bool MatchesKey(const nsACString& aKey)
  {
    return aKey == mKey;
  }

  PerformanceCounter* GetPerformanceCounter()
  {
    return mPerformanceCounter;
  }

  PerformanceInfo
  ReportPerformanceInfo();

  TabGroup* GetTabGroup()
  {
    return mTabGroup;
  }
  mozilla::dom::CustomElementReactionsStack* CustomElementReactionsStack()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mReactionsStack) {
      mReactionsStack = new mozilla::dom::CustomElementReactionsStack();
    }

    return mReactionsStack;
  }
  void RemoveDocument(nsIDocument* aWindow);

  // Iterators for iterating over every document within the DocGroup
  Iterator begin()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mDocuments.begin();
  }
  Iterator end()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mDocuments.end();
  }

  nsresult Dispatch(TaskCategory aCategory,
                    already_AddRefed<nsIRunnable>&& aRunnable);

  nsISerialEventTarget* EventTargetFor(TaskCategory aCategory) const;

  AbstractThread*
  AbstractMainThreadFor(TaskCategory aCategory);

  // Ensure that it's valid to access the DocGroup at this time.
  void ValidateAccess() const
  {
    mTabGroup->ValidateAccess();
  }

  // Return a pointer that can be continually checked to see if access to this
  // DocGroup is valid. This pointer should live at least as long as the
  // DocGroup.
  bool* GetValidAccessPtr();

  // Append aSlot to the list of signal slot list, and queue a mutation observer
  // microtask.
  void SignalSlotChange(HTMLSlotElement& aSlot);

  void MoveSignalSlotListTo(nsTArray<RefPtr<HTMLSlotElement>>& aDest);

  // List of DocGroups that has non-empty signal slot list.
  static AutoTArray<RefPtr<DocGroup>, 2>* sPendingDocGroups;

private:
  DocGroup(TabGroup* aTabGroup, const nsACString& aKey);
  ~DocGroup();

  nsCString mKey;
  RefPtr<TabGroup> mTabGroup;
  nsTArray<nsIDocument*> mDocuments;
  RefPtr<mozilla::dom::CustomElementReactionsStack> mReactionsStack;
  nsTArray<RefPtr<HTMLSlotElement>> mSignalSlotList;
  // This pointer will be null if dom.performance.enable_scheduler_timing is
  // false (default value)
  RefPtr<mozilla::PerformanceCounter> mPerformanceCounter;
};

} // namespace dom
} // namespace mozilla

#endif // defined(DocGroup_h)
