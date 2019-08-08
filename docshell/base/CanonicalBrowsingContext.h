/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanonicalBrowsingContext_h
#define mozilla_dom_CanonicalBrowsingContext_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

class nsIDocShell;

namespace mozilla {
namespace dom {

class WindowGlobalParent;

// CanonicalBrowsingContext is a BrowsingContext living in the parent
// process, with whatever extra data that a BrowsingContext in the
// parent needs.
class CanonicalBrowsingContext final : public BrowsingContext {
 public:
  static already_AddRefed<CanonicalBrowsingContext> Get(uint64_t aId);
  static CanonicalBrowsingContext* Cast(BrowsingContext* aContext);
  static const CanonicalBrowsingContext* Cast(const BrowsingContext* aContext);

  bool IsOwnedByProcess(uint64_t aProcessId) const {
    return mProcessId == aProcessId;
  }
  uint64_t OwnerProcessId() const { return mProcessId; }
  ContentParent* GetContentParent() const;

  void GetCurrentRemoteType(nsAString& aRemoteType, ErrorResult& aRv) const;

  void SetOwnerProcessId(uint64_t aProcessId);

  void GetWindowGlobals(nsTArray<RefPtr<WindowGlobalParent>>& aWindows);

  // Called by WindowGlobalParent to register and unregister window globals.
  void RegisterWindowGlobal(WindowGlobalParent* aGlobal);
  void UnregisterWindowGlobal(WindowGlobalParent* aGlobal);

  // The current active WindowGlobal.
  WindowGlobalParent* GetCurrentWindowGlobal() const {
    return mCurrentWindowGlobal;
  }
  void SetCurrentWindowGlobal(WindowGlobalParent* aGlobal);

  WindowGlobalParent* GetEmbedderWindowGlobal() const {
    return mEmbedderWindowGlobal;
  }
  void SetEmbedderWindowGlobal(WindowGlobalParent* aGlobal);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // This functions would set/reset its user gesture activation flag and then
  // notify other browsing contexts which are not the one related with the
  // current window global to set/reset the flag. (the corresponding browsing
  // context of the current global window has been set/reset before calling this
  // function)
  void NotifySetUserGestureActivationFromIPC(bool aIsUserGestureActivation);

  // This function is used to start the autoplay media which are delayed to
  // start. If needed, it would also notify the content browsing context which
  // are related with the canonical browsing content tree to start delayed
  // autoplay media.
  void NotifyStartDelayedAutoplayMedia();

  // This function is used to mute or unmute all media within a tab. It would
  // set the media mute property for the top level window and propagate it to
  // other top level windows in other processes.
  void NotifyMediaMutedChanged(bool aMuted);

  // Validate that the given process is allowed to perform the given
  // transaction. aSource is |nullptr| if set in the parent process.
  bool ValidateTransaction(const Transaction& aTransaction,
                           ContentParent* aSource);

  void SetFieldEpochsForChild(ContentParent* aChild,
                              const FieldEpochs& aEpochs);
  const FieldEpochs& GetFieldEpochsForChild(ContentParent* aChild);

 protected:
  void Traverse(nsCycleCollectionTraversalCallback& cb);
  void Unlink();

  using Type = BrowsingContext::Type;
  CanonicalBrowsingContext(BrowsingContext* aParent,
                           BrowsingContextGroup* aGroup,
                           uint64_t aBrowsingContextId, uint64_t aProcessId,
                           Type aType);

 private:
  friend class BrowsingContext;

  // XXX(farre): Store a ContentParent pointer here rather than mProcessId?
  // Indicates which process owns the docshell.
  uint64_t mProcessId;

  // All live window globals within this browsing context.
  nsTHashtable<nsRefPtrHashKey<WindowGlobalParent>> mWindowGlobals;
  RefPtr<WindowGlobalParent> mCurrentWindowGlobal;
  RefPtr<WindowGlobalParent> mEmbedderWindowGlobal;

  // Generation information for each content process which has interacted with
  // this CanonicalBrowsingContext, by ChildID.
  nsDataHashtable<nsUint64HashKey, FieldEpochs> mChildFieldEpochs;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_CanonicalBrowsingContext_h)
