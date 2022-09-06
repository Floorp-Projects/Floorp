/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowRoot_h__
#define nsWindowRoot_h__

class nsIGlobalObject;

#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "nsIWeakReferenceUtils.h"
#include "nsPIWindowRoot.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashSet.h"
#include "nsHashKeys.h"

class nsWindowRoot final : public nsPIWindowRoot {
 public:
  explicit nsWindowRoot(nsPIDOMWindowOuter* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  mozilla::EventListenerManager* GetExistingListenerManager() const override;
  mozilla::EventListenerManager* GetOrCreateListenerManager() override;

  bool ComputeDefaultWantsUntrusted(mozilla::ErrorResult& aRv) final;

  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool DispatchEvent(
      mozilla::dom::Event& aEvent, mozilla::dom::CallerType aCallerType,
      mozilla::ErrorResult& aRv) override;

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;

  nsresult PostHandleEvent(mozilla::EventChainPostVisitor& aVisitor) override;

  // nsPIWindowRoot

  nsPIDOMWindowOuter* GetWindow() override;

  nsresult GetControllers(bool aForVisibleWindow,
                          nsIControllers** aResult) override;
  nsresult GetControllerForCommand(const char* aCommand, bool aForVisibleWindow,
                                   nsIController** _retval) override;

  void GetEnabledDisabledCommands(
      nsTArray<nsCString>& aEnabledCommands,
      nsTArray<nsCString>& aDisabledCommands) override;

  already_AddRefed<nsINode> GetPopupNode() override;
  void SetPopupNode(nsINode* aNode) override;

  void SetParentTarget(mozilla::dom::EventTarget* aTarget) override {
    mParent = aTarget;
  }
  mozilla::dom::EventTarget* GetParentTarget() override { return mParent; }
  nsPIDOMWindowOuter* GetOwnerGlobalForBindingsInternal() override;
  nsIGlobalObject* GetOwnerGlobal() const override;

  nsIGlobalObject* GetParentObject();

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(nsWindowRoot)

  void AddBrowser(nsIRemoteTab* aBrowser) override;
  void RemoveBrowser(nsIRemoteTab* aBrowser) override;
  void EnumerateBrowsers(BrowserEnumerator aEnumFunc, void* aArg) override;

  bool ShowFocusRings() override { return mShowFocusRings; }

  void SetShowFocusRings(bool aEnable) override { mShowFocusRings = aEnable; }

 protected:
  virtual ~nsWindowRoot();

  void GetEnabledDisabledCommandsForControllers(
      nsIControllers* aControllers, nsTHashSet<nsCString>& aCommandsHandled,
      nsTArray<nsCString>& aEnabledCommands,
      nsTArray<nsCString>& aDisabledCommands);

  // Members
  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  // We own the manager, which owns event listeners attached to us.
  RefPtr<mozilla::EventListenerManager> mListenerManager;  // [Strong]
  nsWeakPtr mPopupNode;

  // True if focus rings are enabled for this window hierarchy.
  bool mShowFocusRings;

  nsCOMPtr<mozilla::dom::EventTarget> mParent;

  // The BrowserParents that are currently registered with this top-level
  // window.
  using WeakBrowserTable = nsTHashSet<RefPtr<nsIWeakReference>>;
  WeakBrowserTable mWeakBrowsers;
};

extern already_AddRefed<mozilla::dom::EventTarget> NS_NewWindowRoot(
    nsPIDOMWindowOuter* aWindow);

#endif
