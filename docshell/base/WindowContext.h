/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowContext_h
#define mozilla_dom_WindowContext_h

#include "mozilla/Span.h"
#include "mozilla/dom/MaybeDiscarded.h"
#include "mozilla/dom/SyncedContext.h"

namespace mozilla {
namespace dom {

class WindowGlobalParent;
class WindowGlobalInit;

#define MOZ_EACH_WC_FIELD(FIELD)                                       \
  FIELD(CookieBehavior, Maybe<uint32_t>)                               \
  FIELD(IsOnContentBlockingAllowList, bool)                            \
  /* Whether the given window hierarchy is third party. See            \
   * ThirdPartyUtil::IsThirdPartyWindow for details */                 \
  FIELD(IsThirdPartyWindow, bool)                                      \
  /* Whether this window's channel has been marked as a third-party    \
   * tracking resource */                                              \
  FIELD(IsThirdPartyTrackingResourceWindow, bool)                      \
  /* Mixed-Content: If the corresponding documentURI is https,         \
   * then this flag is true. */                                        \
  FIELD(IsSecure, bool)                                                \
  /* Whether the user has overriden the mixed content blocker to allow \
   * mixed content loads to happen */                                  \
  FIELD(AllowMixedContent, bool)                                       \
  FIELD(EmbedderPolicy, nsILoadInfo::CrossOriginEmbedderPolicy)

class WindowContext : public nsISupports, public nsWrapperCache {
  MOZ_DECL_SYNCED_CONTEXT(WindowContext, MOZ_EACH_WC_FIELD)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WindowContext)

 public:
  static already_AddRefed<WindowContext> GetById(uint64_t aInnerWindowId);
  static LogModule* GetLog();

  BrowsingContext* GetBrowsingContext() const { return mBrowsingContext; }
  BrowsingContextGroup* Group() const;
  uint64_t Id() const { return InnerWindowId(); }
  uint64_t InnerWindowId() const { return mInnerWindowId; }
  uint64_t OuterWindowId() const { return mOuterWindowId; }
  bool IsDiscarded() const { return mIsDiscarded; }

  bool IsCached() const;

  bool IsInProcess() { return mInProcess; }

  // Get the parent WindowContext of this WindowContext, taking the BFCache into
  // account. This will not cross chrome/content <browser> boundaries.
  WindowContext* GetParentWindowContext();
  WindowContext* TopWindowContext();

  Span<RefPtr<BrowsingContext>> Children() { return mChildren; }

  // Cast this object to it's parent-process canonical form.
  WindowGlobalParent* Canonical();

  nsIGlobalObject* GetParentObject() const;
  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void Discard();

  struct IPCInitializer {
    uint64_t mInnerWindowId;
    uint64_t mOuterWindowId;
    uint64_t mBrowsingContextId;

    FieldTuple mFields;

    bool operator==(const IPCInitializer& aOther) const {
      return mInnerWindowId == aOther.mInnerWindowId &&
             mOuterWindowId == aOther.mOuterWindowId &&
             mBrowsingContextId == aOther.mBrowsingContextId &&
             mFields == aOther.mFields;
    }
  };
  IPCInitializer GetIPCInitializer();

  static void CreateFromIPC(IPCInitializer&& aInit);

 protected:
  WindowContext(BrowsingContext* aBrowsingContext, uint64_t aInnerWindowId,
                uint64_t aOuterWindowId, bool aInProcess, FieldTuple&& aFields);
  virtual ~WindowContext();

  virtual void Init();

 private:
  friend class BrowsingContext;
  friend class WindowGlobalChild;
  friend class WindowGlobalActor;

  void AppendChildBrowsingContext(BrowsingContext* aBrowsingContext);
  void RemoveChildBrowsingContext(BrowsingContext* aBrowsingContext);

  // Send a given `BaseTransaction` object to the correct remote.
  void SendCommitTransaction(ContentParent* aParent,
                             const BaseTransaction& aTxn, uint64_t aEpoch);
  void SendCommitTransaction(ContentChild* aChild, const BaseTransaction& aTxn,
                             uint64_t aEpoch);

  bool CheckOnlyOwningProcessCanSet(ContentParent* aSource);

  // Overload `CanSet` to get notifications for a particular field being set.
  bool CanSet(FieldIndex<IDX_IsSecure>, const bool& aIsSecure,
              ContentParent* aSource);
  bool CanSet(FieldIndex<IDX_AllowMixedContent>, const bool& aAllowMixedContent,
              ContentParent* aSource);

  bool CanSet(FieldIndex<IDX_CookieBehavior>, const Maybe<uint32_t>& aValue,
              ContentParent* aSource);

  bool CanSet(FieldIndex<IDX_IsOnContentBlockingAllowList>, const bool& aValue,
              ContentParent* aSource);

  bool CanSet(FieldIndex<IDX_EmbedderPolicy>, const bool& aValue,
              ContentParent* aSource) {
    return true;
  }

  bool CanSet(FieldIndex<IDX_IsThirdPartyWindow>,
              const bool& IsThirdPartyWindow, ContentParent* aSource);
  bool CanSet(FieldIndex<IDX_IsThirdPartyTrackingResourceWindow>,
              const bool& aIsThirdPartyTrackingResourceWindow,
              ContentParent* aSource);

  // Overload `DidSet` to get notifications for a particular field being set.
  //
  // You can also overload the variant that gets the old value if you need it.
  template <size_t I>
  void DidSet(FieldIndex<I>) {}
  template <size_t I, typename T>
  void DidSet(FieldIndex<I>, T&& aOldValue) {}

  uint64_t mInnerWindowId;
  uint64_t mOuterWindowId;
  RefPtr<BrowsingContext> mBrowsingContext;

  // --- NEVER CHANGE `mChildren` DIRECTLY! ---
  // Changes to this list need to be synchronized to the list within our
  // `mBrowsingContext`, and should only be performed through the
  // `AppendChildBrowsingContext` and `RemoveChildBrowsingContext` methods.
  nsTArray<RefPtr<BrowsingContext>> mChildren;

  bool mIsDiscarded = false;
  bool mInProcess = false;
};

using WindowContextTransaction = WindowContext::BaseTransaction;
using WindowContextInitializer = WindowContext::IPCInitializer;
using MaybeDiscardedWindowContext = MaybeDiscarded<WindowContext>;

// Don't specialize the `Transaction` object for every translation unit it's
// used in. This should help keep code size down.
extern template class syncedcontext::Transaction<WindowContext>;

}  // namespace dom

namespace ipc {
template <>
struct IPDLParamTraits<dom::MaybeDiscarded<dom::WindowContext>> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const dom::MaybeDiscarded<dom::WindowContext>& aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor,
                   dom::MaybeDiscarded<dom::WindowContext>* aResult);
};

template <>
struct IPDLParamTraits<dom::WindowContext::IPCInitializer> {
  static void Write(IPC::Message* aMessage, IProtocol* aActor,
                    const dom::WindowContext::IPCInitializer& aInitializer);

  static bool Read(const IPC::Message* aMessage, PickleIterator* aIterator,
                   IProtocol* aActor,
                   dom::WindowContext::IPCInitializer* aInitializer);
};
}  // namespace ipc
}  // namespace mozilla

#endif  // !defined(mozilla_dom_WindowContext_h)
