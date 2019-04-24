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
#include "nsTHashtable.h"
#include "nsHashKeys.h"

class nsWindowRoot final : public nsPIWindowRoot {
 public:
  explicit nsWindowRoot(nsPIDOMWindowOuter* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  virtual mozilla::EventListenerManager* GetExistingListenerManager()
      const override;
  virtual mozilla::EventListenerManager* GetOrCreateListenerManager() override;

  bool ComputeDefaultWantsUntrusted(mozilla::ErrorResult& aRv) final;

  bool DispatchEvent(mozilla::dom::Event& aEvent,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aRv) override;

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;

  nsresult PostHandleEvent(mozilla::EventChainPostVisitor& aVisitor) override;

  // nsPIWindowRoot

  virtual nsPIDOMWindowOuter* GetWindow() override;

  virtual nsresult GetControllers(bool aForVisibleWindow,
                                  nsIControllers** aResult) override;
  virtual nsresult GetControllerForCommand(const char* aCommand,
                                           bool aForVisibleWindow,
                                           nsIController** _retval) override;

  virtual void GetEnabledDisabledCommands(
      nsTArray<nsCString>& aEnabledCommands,
      nsTArray<nsCString>& aDisabledCommands) override;

  virtual already_AddRefed<nsINode> GetPopupNode() override;
  virtual void SetPopupNode(nsINode* aNode) override;

  virtual void SetParentTarget(mozilla::dom::EventTarget* aTarget) override {
    mParent = aTarget;
  }
  virtual mozilla::dom::EventTarget* GetParentTarget() override {
    return mParent;
  }
  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindingsInternal() override;
  virtual nsIGlobalObject* GetOwnerGlobal() const override;

  nsIGlobalObject* GetParentObject();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsWindowRoot)

  virtual void AddBrowser(mozilla::dom::BrowserParent* aBrowser) override;
  virtual void RemoveBrowser(mozilla::dom::BrowserParent* aBrowser) override;
  virtual void EnumerateBrowsers(BrowserEnumerator aEnumFunc,
                                 void* aArg) override;

  virtual bool ShowAccelerators() override { return mShowAccelerators; }

  virtual bool ShowFocusRings() override { return mShowFocusRings; }

  virtual void SetShowAccelerators(bool aEnable) override {
    mShowAccelerators = aEnable;
  }

  virtual void SetShowFocusRings(bool aEnable) override {
    mShowFocusRings = aEnable;
  }

 protected:
  virtual ~nsWindowRoot();

  void GetEnabledDisabledCommandsForControllers(
      nsIControllers* aControllers,
      nsTHashtable<nsCharPtrHashKey>& aCommandsHandled,
      nsTArray<nsCString>& aEnabledCommands,
      nsTArray<nsCString>& aDisabledCommands);

  // Members
  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  // We own the manager, which owns event listeners attached to us.
  RefPtr<mozilla::EventListenerManager> mListenerManager;  // [Strong]
  nsWeakPtr mPopupNode;

  // True if focus rings and accelerators are enabled for this
  // window hierarchy.
  bool mShowAccelerators;
  bool mShowFocusRings;

  nsCOMPtr<mozilla::dom::EventTarget> mParent;

  // The BrowserParents that are currently registered with this top-level
  // window.
  typedef nsTHashtable<nsRefPtrHashKey<nsIWeakReference>> WeakBrowserTable;
  WeakBrowserTable mWeakBrowsers;
};

extern already_AddRefed<mozilla::dom::EventTarget> NS_NewWindowRoot(
    nsPIDOMWindowOuter* aWindow);

#endif
