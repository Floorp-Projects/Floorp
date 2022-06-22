/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ContainStyleScopeManager_h_
#define ContainStyleScopeManager_h_

#include "mozilla/dom/Element.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"
#include "nsQuoteList.h"
#include "nsCounterManager.h"
#include <memory>

class nsIContent;
class nsAtom;

namespace mozilla {

class ContainStyleScopeManager;

/* Implementation of a self-contained `contain: style` scope which manages its
 * own counters and quotes. Since the `counters()` function has read access to
 * other `contain: style` scopes, USE counter nodes may link across `contain:
 * style` scopes. */
class ContainStyleScope final {
 public:
  ContainStyleScope(ContainStyleScopeManager* aManager,
                    ContainStyleScope* aParent, nsIContent* aContent)
      : mQuoteList(this),
        mCounterManager(this),
        mScopeManager(aManager),
        mParent(aParent),
        mContent(aContent) {
    MOZ_ASSERT(aManager);
    if (mParent) {
      mParent->AddChild(this);
    }
  }

  ~ContainStyleScope() {
    if (mParent) {
      mParent->RemoveChild(this);
    }
  }

  nsQuoteList& GetQuoteList() { return mQuoteList; }
  nsCounterManager& GetCounterManager() { return mCounterManager; }
  ContainStyleScopeManager& GetScopeManager() { return *mScopeManager; }
  ContainStyleScope* GetParent() { return mParent; }
  nsIContent* GetContent() { return mContent; }

  void AddChild(ContainStyleScope* aScope) { mChildren.AppendElement(aScope); }
  void RemoveChild(ContainStyleScope* aScope) {
    mChildren.RemoveElement(aScope);
  }
  const nsTArray<ContainStyleScope*>& GetChildren() const { return mChildren; }

  void RecalcAllCounters();
  void RecalcAllQuotes();

  // Find the element in the given nsGenConList that directly precedes
  // the mContent node of this ContainStyleScope in the flat tree. Can
  // return null if no element in the list precedes the content.
  nsGenConNode* GetPrecedingElementInGenConList(nsGenConList*);

 private:
  nsQuoteList mQuoteList;
  nsCounterManager mCounterManager;

  // We are owned by the |mScopeManager|, so this is guaranteed to be a live
  // pointer as long as we are alive as well.
  ContainStyleScopeManager* mScopeManager;

  // Although parent and child relationships are represented as raw pointers
  // here, |mScopeManager| is responsible for managing creation and deletion of
  // all these data structures and also that it happens in the correct order.
  ContainStyleScope* mParent;
  nsTArray<ContainStyleScope*> mChildren;

  // |mContent| is guaranteed to outlive this scope because mScopeManager will
  // delete the scope when the corresponding frame for |mContent| is destroyed.
  nsIContent* mContent;
};

/* Management of the tree `contain: style` scopes. This class ensures that
 * recalculation is done top-down, so that nodes that rely on other nodes in
 * ancestor `contain: style` scopes are calculated properly. */
class ContainStyleScopeManager {
 public:
  ContainStyleScopeManager() : mRootScope(this, nullptr, nullptr) {}
  ContainStyleScope& GetRootScope() { return mRootScope; }
  ContainStyleScope& GetOrCreateScopeForContent(nsIContent*);
  ContainStyleScope& GetScopeForContent(nsIContent*);

  void Clear();

  // If this frame creates a `contain: style` scope, destroy that scope and
  // all of its child scopes.
  void DestroyScopesFor(nsIFrame*);

  // Destroy this scope and all its children starting from the leaf nodes.
  void DestroyScope(ContainStyleScope*);

  bool DestroyCounterNodesFor(nsIFrame*);
  bool AddCounterChanges(nsIFrame* aNewFrame);
  nsCounterList* GetOrCreateCounterList(dom::Element&, nsAtom* aCounterName);

  bool CounterDirty(nsAtom* aCounterName);
  void SetCounterDirty(nsAtom* aCounterName);
  void RecalcAllCounters();
  void SetAllCountersDirty();

  bool DestroyQuoteNodesFor(nsIFrame*);
  nsQuoteList* QuoteListFor(dom::Element&);
  void RecalcAllQuotes();

#if defined(DEBUG) || defined(MOZ_LAYOUT_DEBUGGER)
  void DumpCounters();
#endif

#ifdef ACCESSIBILITY
  void GetSpokenCounterText(nsIFrame* aFrame, nsAString& aText);
#endif

 private:
  ContainStyleScope mRootScope;
  nsClassHashtable<nsPtrHashKey<nsIContent>, ContainStyleScope> mScopes;
  nsTHashSet<nsRefPtrHashKey<nsAtom>> mDirtyCounters;
};

}  // namespace mozilla

#endif /* ContainStyleScopeManager_h_ */
