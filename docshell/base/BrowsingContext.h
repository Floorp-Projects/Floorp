/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BrowsingContext_h
#define BrowsingContext_h

#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsIDocShell;

namespace mozilla {

class LogModule;

namespace dom {

// BrowsingContext, in this context, is the cross process replicated
// environment in which information about documents is stored. In
// particular the tree structure of nested browsing contexts is
// represented by the tree of BrowsingContexts.
//
// The tree of BrowsingContexts in created in step with its
// corresponding nsDocShell, and when nsDocShells are connected
// through a parent/child relationship, so are BrowsingContexts. The
// major difference is that BrowsingContexts are replicated (synced)
// to the parent process, making it possible to traverse the
// BrowsingContext tree for a tab, in both the parent and the child
// process.
class BrowsingContext
  : public nsWrapperCache
  , public SupportsWeakPtr<BrowsingContext>
  , public LinkedListElement<RefPtr<BrowsingContext>>
{
public:
  static void Init();
  static LogModule* GetLog();
  static void CleanupContexts(uint64_t aProcessId);

  static already_AddRefed<BrowsingContext> Get(uint64_t aId);

  // Create a new BrowsingContext for 'aDocShell'. The id will be
  // generated so that it is unique across all content child processes
  // and the content parent process.
  explicit BrowsingContext(nsIDocShell* aDocShell);
  // Create a BrowsingContext for a particular BrowsingContext id, in
  // the case where the id is known beforehand and a nsDocShell isn't
  // needed (e.g. when creating BrowsingContexts in the parent
  // process).
  BrowsingContext(uint64_t aBrowsingContextId,
                  const nsAString& aName,
                  const Maybe<uint64_t>& aProcessId = Nothing());

  // Attach the current BrowsingContext to its parent, in both the
  // child and the parent process. If 'aParent' is null, 'this' is
  // taken to be a root BrowsingContext.
  void Attach(BrowsingContext* aParent);

  // Detach the current BrowsingContext from its parent, in both the
  // child and the parent process.
  void Detach();

  // Remove all children from the current BrowsingContext and cache
  // them to allow them to be attached again.
  void CacheChildren();

  bool IsCached();

  void SetName(const nsAString& aName) { mName = aName; }
  void GetName(nsAString& aName) { aName = mName; }
  bool NameEquals(const nsAString& aName) { return mName.Equals(aName); }

  uint64_t Id() const { return mBrowsingContextId; }
  uint64_t OwnerProcessId() const;
  bool IsOwnedByProcess() const { return mProcessId.isSome(); }

  already_AddRefed<BrowsingContext> GetParent()
  {
    return do_AddRef(mParent.get());
  }

  void GetChildren(nsTArray<RefPtr<BrowsingContext>>& aChildren);

  static void GetRootBrowsingContexts(
    nsTArray<RefPtr<BrowsingContext>>& aBrowsingContexts);

  nsISupports* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(BrowsingContext)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BrowsingContext)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(BrowsingContext)

  using Children = AutoCleanLinkedList<RefPtr<BrowsingContext>>;

private:
  virtual ~BrowsingContext();

  const uint64_t mBrowsingContextId;

  // Indicates which process owns the docshell. Only valid in the
  // parent process.
  Maybe<uint64_t> mProcessId;

  WeakPtr<BrowsingContext> mParent;
  Children mChildren;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsString mName;
};

} // namespace dom
} // namespace mozilla
#endif
