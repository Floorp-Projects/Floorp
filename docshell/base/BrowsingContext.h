/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContext_h
#define mozilla_dom_BrowsingContext_h

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

class ContentParent;

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
//
// Trees of BrowsingContexts should only ever contain nodes of the
// same BrowsingContext::Type. This is enforced by asserts in the
// BrowsingContext::Create* methods.
class BrowsingContext : public nsWrapperCache,
                        public SupportsWeakPtr<BrowsingContext>,
                        public LinkedListElement<RefPtr<BrowsingContext>> {
 public:
  enum class Type { Chrome, Content };

  static void Init();
  static LogModule* GetLog();
  static void CleanupContexts(uint64_t aProcessId);

  // Look up a BrowsingContext in the current process by ID.
  static already_AddRefed<BrowsingContext> Get(uint64_t aId);

  // Create a brand-new BrowsingContext object.
  static already_AddRefed<BrowsingContext> Create(BrowsingContext* aParent,
                                                  BrowsingContext* aOpener,
                                                  const nsAString& aName,
                                                  Type aType);

  // Create a BrowsingContext object from over IPC.
  static already_AddRefed<BrowsingContext> CreateFromIPC(
      BrowsingContext* aParent, BrowsingContext* aOpener,
      const nsAString& aName, uint64_t aId, ContentParent* aOriginProcess);

  // Get the DocShell for this BrowsingContext if it is in-process, or
  // null if it's not.
  nsIDocShell* GetDocShell() { return mDocShell; }
  void SetDocShell(nsIDocShell* aDocShell);

  // Attach the current BrowsingContext to its parent, in both the child and the
  // parent process. BrowsingContext objects are created attached by default, so
  // this method need only be called when restoring cached BrowsingContext
  // objects.
  void Attach();

  // Detach the current BrowsingContext from its parent, in both the
  // child and the parent process.
  void Detach();

  // Remove all children from the current BrowsingContext and cache
  // them to allow them to be attached again.
  void CacheChildren();

  // Determine if the current BrowsingContext was 'cached' by the logic in
  // CacheChildren.
  bool IsCached();

  // TODO(farre): We should sync changes from SetName to the parent
  // process. [Bug 1490303]
  void SetName(const nsAString& aName) { mName = aName; }
  void GetName(nsAString& aName) { aName = mName; }
  bool NameEquals(const nsAString& aName) { return mName.Equals(aName); }

  bool IsContent() const { return mType == Type::Content; }

  uint64_t Id() const { return mBrowsingContextId; }

  BrowsingContext* GetParent() { return mParent; }

  void GetChildren(nsTArray<RefPtr<BrowsingContext>>& aChildren);

  BrowsingContext* GetOpener() { return mOpener; }

  void SetOpener(BrowsingContext* aOpener);

  static void GetRootBrowsingContexts(
      nsTArray<RefPtr<BrowsingContext>>& aBrowsingContexts);

  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(BrowsingContext)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BrowsingContext)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(BrowsingContext)

  using Children = AutoCleanLinkedList<RefPtr<BrowsingContext>>;

 protected:
  virtual ~BrowsingContext();
  BrowsingContext(BrowsingContext* aParent, BrowsingContext* aOpener,
                  const nsAString& aName, uint64_t aBrowsingContextId,
                  Type aType);

 private:
  // Type of BrowsingContent
  const Type mType;

  // Unique id identifying BrowsingContext
  const uint64_t mBrowsingContextId;

  WeakPtr<BrowsingContext> mParent;
  Children mChildren;
  WeakPtr<BrowsingContext> mOpener;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsString mName;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowsingContext_h)
