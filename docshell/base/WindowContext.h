/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowContext_h
#define mozilla_dom_WindowContext_h

#include "mozilla/dom/MaybeDiscarded.h"
#include "mozilla/dom/SyncedContext.h"
#include "mozilla/net/NeckoChannelParams.h"

namespace mozilla {
namespace dom {

#define MOZ_EACH_WC_FIELD(FIELD)                                       \
  FIELD(OuterWindowId, uint64_t)                                       \
  FIELD(CookieJarSettings, Maybe<mozilla::net::CookieJarSettingsArgs>) \
  FIELD(HasStoragePermission, bool)

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
  uint64_t OuterWindowId() const { return GetOuterWindowId(); }
  bool IsDiscarded() const { return mIsDiscarded; }

  // Cast this object to it's parent-process canonical form.
  WindowGlobalParent* Canonical();

  nsIGlobalObject* GetParentObject() const;
  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void Discard();

  struct IPCInitializer {
    uint64_t mInnerWindowId;
    uint64_t mBrowsingContextId;

    FieldTuple mFields;
  };
  IPCInitializer GetIPCInitializer() {
    return {mInnerWindowId, mBrowsingContext->Id(), mFields.Fields()};
  }

  static already_AddRefed<WindowContext> Create(WindowGlobalChild* aWindow);
  static void CreateFromIPC(IPCInitializer&& aInit);

 protected:
  WindowContext(BrowsingContext* aBrowsingContext, uint64_t aInnerWindowId,
                FieldTuple&& aFields);
  virtual ~WindowContext();

  void Init();

 private:
  // Send a given `BaseTransaction` object to the correct remote.
  void SendCommitTransaction(ContentParent* aParent,
                             const BaseTransaction& aTxn, uint64_t aEpoch);
  void SendCommitTransaction(ContentChild* aChild, const BaseTransaction& aTxn,
                             uint64_t aEpoch);

  // Overload `CanSet` to get notifications for a particular field being set.
  bool CanSet(FieldIndex<IDX_OuterWindowId>, const uint64_t& aValue,
              ContentParent* aSource) {
    return GetOuterWindowId() == 0 && aValue != 0;
  }

  bool CanSet(FieldIndex<IDX_CookieJarSettings>,
              const Maybe<mozilla::net::CookieJarSettingsArgs>& aValue,
              ContentParent* aSource) {
    return true;
  }

  bool CanSet(FieldIndex<IDX_HasStoragePermission>, const bool& aValue,
              ContentParent* aSource) {
    return true;
  }

  // Overload `DidSet` to get notifications for a particular field being set.
  template <size_t I>
  void DidSet(FieldIndex<I>) {}

  uint64_t mInnerWindowId;
  RefPtr<BrowsingContext> mBrowsingContext;

  bool mIsDiscarded = false;
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
