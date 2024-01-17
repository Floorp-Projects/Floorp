/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContextGroup_h
#define mozilla_dom_BrowsingContextGroup_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/FunctionRef.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashSet.h"
#include "nsWrapperCache.h"
#include "nsXULAppAPI.h"

namespace mozilla {
class ThrottledEventQueue;

namespace dom {

// Amount of time allowed between alert/prompt/confirm before enabling
// the stop dialog checkbox.
#define DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT 3  // 3 sec

class BrowsingContext;
class WindowContext;
class ContentParent;
class DocGroup;

// A BrowsingContextGroup represents the Unit of Related Browsing Contexts in
// the standard.
//
// A BrowsingContext may not hold references to other BrowsingContext objects
// which are not in the same BrowsingContextGroup.
class BrowsingContextGroup final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BrowsingContextGroup)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(BrowsingContextGroup)

  // Interact with the list of synced contexts. This controls the lifecycle of
  // the BrowsingContextGroup and contexts loaded within them.
  void Register(nsISupports* aContext);
  void Unregister(nsISupports* aContext);

  // Control which processes will be used to host documents loaded in this
  // BrowsingContextGroup. There should only ever be one host process per remote
  // type.
  //
  // A new host process will be subscribed to the BrowsingContextGroup unless it
  // is still launching, in which case it will subscribe itself when it is done
  // launching.
  void EnsureHostProcess(ContentParent* aProcess);

  // A removed host process will no longer be used to host documents loaded in
  // this BrowsingContextGroup.
  void RemoveHostProcess(ContentParent* aProcess);

  // Synchronize the current BrowsingContextGroup state down to the given
  // content process, and continue updating it.
  //
  // You rarely need to call this directly, as it's automatically called by
  // |EnsureHostProcess| as needed.
  void Subscribe(ContentParent* aProcess);

  // Stop synchronizing the current BrowsingContextGroup state down to a given
  // content process. The content process must no longer be a host process.
  void Unsubscribe(ContentParent* aProcess);

  // Look up the process which should be used to host documents with this
  // RemoteType. This will be a non-dead process associated with this
  // BrowsingContextGroup, if possible.
  ContentParent* GetHostProcess(const nsACString& aRemoteType);

  // When a BrowsingContext is being discarded, we may want to keep the
  // corresponding BrowsingContextGroup alive until the other process
  // acknowledges that the BrowsingContext has been discarded. A `KeepAlive`
  // will be added to the `BrowsingContextGroup`, delaying destruction.
  void AddKeepAlive();
  void RemoveKeepAlive();

  // A `KeepAlivePtr` will hold both a strong reference to the
  // `BrowsingContextGroup` and holds a `KeepAlive`. When the pointer is
  // dropped, it will release both the strong reference and the keepalive.
  struct KeepAliveDeleter {
    void operator()(BrowsingContextGroup* aPtr) {
      if (RefPtr<BrowsingContextGroup> ptr = already_AddRefed(aPtr)) {
        ptr->RemoveKeepAlive();
      }
    }
  };
  using KeepAlivePtr = UniquePtr<BrowsingContextGroup, KeepAliveDeleter>;
  KeepAlivePtr MakeKeepAlivePtr();

  // Call when we want to check if we should suspend or resume all top level
  // contexts.
  void UpdateToplevelsSuspendedIfNeeded();

  // Get a reference to the list of toplevel contexts in this
  // BrowsingContextGroup.
  nsTArray<RefPtr<BrowsingContext>>& Toplevels() { return mToplevels; }
  void GetToplevels(nsTArray<RefPtr<BrowsingContext>>& aToplevels) {
    aToplevels.AppendElements(mToplevels);
  }

  uint64_t Id() { return mId; }

  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Get or create a BrowsingContextGroup with the given ID.
  static already_AddRefed<BrowsingContextGroup> GetOrCreate(uint64_t aId);
  static already_AddRefed<BrowsingContextGroup> GetExisting(uint64_t aId);
  static already_AddRefed<BrowsingContextGroup> Create(
      bool aPotentiallyCrossOriginIsolated = false);
  static already_AddRefed<BrowsingContextGroup> Select(
      WindowContext* aParent, BrowsingContext* aOpener);

  // Like `Create` but only generates and reserves a new ID without actually
  // creating the BrowsingContextGroup object.
  static uint64_t CreateId(bool aPotentiallyCrossOriginIsolated = false);

  // For each 'ContentParent', except for 'aExcludedParent',
  // associated with this group call 'aCallback'.
  template <typename Func>
  void EachOtherParent(ContentParent* aExcludedParent, Func&& aCallback) {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
    for (const auto& key : mSubscribers) {
      if (key != aExcludedParent) {
        aCallback(key);
      }
    }
  }

  // For each 'ContentParent' associated with
  // this group call 'aCallback'.
  template <typename Func>
  void EachParent(Func&& aCallback) {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
    for (const auto& key : mSubscribers) {
      aCallback(key);
    }
  }

  nsresult QueuePostMessageEvent(nsIRunnable* aRunnable);

  void FlushPostMessageEvents();

  // Increase or decrease the suspension level in InputTaskManager
  void UpdateInputTaskManagerIfNeeded(bool aIsActive);

  static BrowsingContextGroup* GetChromeGroup();

  void GetDocGroups(nsTArray<DocGroup*>& aDocGroups);

  // Called by Document when a Document needs to be added to a DocGroup.
  already_AddRefed<DocGroup> AddDocument(const nsACString& aKey,
                                         Document* aDocument);

  // Called by Document when a Document needs to be removed from a DocGroup.
  // aDocGroup should be from aDocument. This is done to avoid the assert
  // in GetDocGroup() which can crash when called during unlinking.
  void RemoveDocument(Document* aDocument, DocGroup* aDocGroup);

  mozilla::ThrottledEventQueue* GetTimerEventQueue() const {
    return mTimerEventQueue;
  }

  mozilla::ThrottledEventQueue* GetWorkerEventQueue() const {
    return mWorkerEventQueue;
  }

  void SetAreDialogsEnabled(bool aAreDialogsEnabled) {
    mAreDialogsEnabled = aAreDialogsEnabled;
  }

  bool GetAreDialogsEnabled() { return mAreDialogsEnabled; }

  bool GetDialogAbuseCount() { return mDialogAbuseCount; }

  // For tests only.
  void ResetDialogAbuseState();

  bool DialogsAreBeingAbused();

  TimeStamp GetLastDialogQuitTime() { return mLastDialogQuitTime; }

  void SetLastDialogQuitTime(TimeStamp aLastDialogQuitTime) {
    mLastDialogQuitTime = aLastDialogQuitTime;
  }

  // Whether all toplevel documents loaded in this group are allowed to be
  // Cross-Origin Isolated.
  //
  // This does not reflect the actual value of `crossOriginIsolated`, as that
  // also requires that the document is loaded within a `webCOOP+COEP` content
  // process.
  bool IsPotentiallyCrossOriginIsolated();

  static void GetAllGroups(nsTArray<RefPtr<BrowsingContextGroup>>& aGroups);

  void IncInputEventSuspensionLevel();
  void DecInputEventSuspensionLevel();

  void ChildDestroy();

 private:
  friend class CanonicalBrowsingContext;

  explicit BrowsingContextGroup(uint64_t aId);
  ~BrowsingContextGroup();

  void MaybeDestroy();
  void Destroy();

  bool ShouldSuspendAllTopLevelContexts() const;

  bool HasActiveBC();
  void DecInputTaskManagerSuspensionLevel();
  void IncInputTaskManagerSuspensionLevel();

  uint64_t mId;

  uint32_t mKeepAliveCount = 0;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool mDestroyed = false;
#endif

  // A BrowsingContextGroup contains a series of {Browsing,Window}Context
  // objects. They are addressed using a hashtable to avoid linear lookup when
  // adding or removing elements from the set.
  //
  // FIXME: This list is only required over a counter to keep nested
  // non-discarded contexts within discarded contexts alive. It should be
  // removed in the future.
  // FIXME: Consider introducing a better common base than `nsISupports`?
  nsTHashSet<nsRefPtrHashKey<nsISupports>> mContexts;

  // The set of toplevel browsing contexts in the current BrowsingContextGroup.
  nsTArray<RefPtr<BrowsingContext>> mToplevels;

  //  Whether or not all toplevels in this group should be suspended
  bool mToplevelsSuspended = false;

  // DocGroups are thread-safe, and not able to be cycle collected,
  // but we still keep strong pointers. When all Documents are removed
  // from DocGroup (by the BrowsingContextGroup), the DocGroup is
  // removed from here.
  nsRefPtrHashtable<nsCStringHashKey, DocGroup> mDocGroups;

  // The content process which will host documents in this BrowsingContextGroup
  // which need to be loaded with a given remote type.
  //
  // A non-launching host process must also be a subscriber, though a launching
  // host process may not yet be subscribed, and a subscriber need not be a host
  // process.
  nsRefPtrHashtable<nsCStringHashKey, ContentParent> mHosts;

  nsTHashSet<nsRefPtrHashKey<ContentParent>> mSubscribers;

  // A queue to store postMessage events during page load, the queue will be
  // flushed once the page is loaded
  RefPtr<mozilla::ThrottledEventQueue> mPostMessageEventQueue;

  RefPtr<mozilla::ThrottledEventQueue> mTimerEventQueue;
  RefPtr<mozilla::ThrottledEventQueue> mWorkerEventQueue;

  // A counter to keep track of the input event suspension level of this BCG
  //
  // We use BrowsingContextGroup to emulate process isolation in Fission, so
  // documents within the same the same BCG will behave like they share
  // the same input task queue.
  uint32_t mInputEventSuspensionLevel = 0;
  // Whether this BCG has increased the suspension level in InputTaskManager
  bool mHasIncreasedInputTaskManagerSuspensionLevel = false;

  // This flag keeps track of whether dialogs are
  // currently enabled for windows of this group.
  // It's OK to have these local to each process only because even if
  // frames from two/three different sites (and thus, processes) coordinate a
  // dialog abuse attack, they would only the double/triple number of dialogs,
  // as it is still limited per-site.
  bool mAreDialogsEnabled = true;

  // This counts the number of windows that have been opened in rapid succession
  // (i.e. within dom.successive_dialog_time_limit of each other). It is reset
  // to 0 once a dialog is opened after dom.successive_dialog_time_limit seconds
  // have elapsed without any other dialogs.
  // See comment for mAreDialogsEnabled as to why it's ok to have this local to
  // each process.
  uint32_t mDialogAbuseCount = 0;

  // This holds the time when the last modal dialog was shown. If more than
  // MAX_DIALOG_LIMIT dialogs are shown within the time span defined by
  // dom.successive_dialog_time_limit, we show a checkbox or confirmation prompt
  // to allow disabling of further dialogs from windows in this BC group.
  TimeStamp mLastDialogQuitTime;
};
}  // namespace dom
}  // namespace mozilla

inline void ImplCycleCollectionUnlink(
    mozilla::dom::BrowsingContextGroup::KeepAlivePtr& aField) {
  aField = nullptr;
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    mozilla::dom::BrowsingContextGroup::KeepAlivePtr& aField, const char* aName,
    uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

#endif  // !defined(mozilla_dom_BrowsingContextGroup_h)
