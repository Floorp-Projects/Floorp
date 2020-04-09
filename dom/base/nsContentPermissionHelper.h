/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentPermissionHelper_h
#define nsContentPermissionHelper_h

#include "nsIContentPermissionPrompt.h"
#include "nsTArray.h"
#include "nsIMutableArray.h"
#include "mozilla/dom/PContentPermissionRequestChild.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/PermissionDelegateHandler.h"

// Microsoft's API Name hackery sucks
// XXXbz Doing this in a header is a gigantic footgun. See
// https://bugzilla.mozilla.org/show_bug.cgi?id=932421#c3 for why.
#undef LoadImage

class nsPIDOMWindowInner;
class nsContentPermissionRequestProxy;
class VisibilityChangeListener;

// Forward declare IPC::Principal here which is defined in
// PermissionMessageUtils.h. Include this file will transitively includes
// "windows.h" and it defines
//   #define CreateEvent CreateEventW
//   #define LoadImage LoadImageW
// That will mess up windows build.
namespace IPC {
class Principal;
}  // namespace IPC

namespace mozilla {
namespace dom {

class Element;
class PermissionRequest;
class ContentPermissionRequestParent;
class PContentPermissionRequestParent;

class ContentPermissionType : public nsIContentPermissionType {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONTYPE

  ContentPermissionType(const nsACString& aType,
                        const nsTArray<nsString>& aOptions);

 protected:
  virtual ~ContentPermissionType();

  nsCString mType;
  nsTArray<nsString> mOptions;
};

class nsContentPermissionUtils {
 public:
  static uint32_t ConvertPermissionRequestToArray(
      nsTArray<PermissionRequest>& aSrcArray, nsIMutableArray* aDesArray);

  static uint32_t ConvertArrayToPermissionRequest(
      nsIArray* aSrcArray, nsTArray<PermissionRequest>& aDesArray);

  static nsresult CreatePermissionArray(const nsACString& aType,
                                        const nsTArray<nsString>& aOptions,
                                        nsIArray** aTypesArray);

  static PContentPermissionRequestParent* CreateContentPermissionRequestParent(
      const nsTArray<PermissionRequest>& aRequests, Element* aElement,
      nsIPrincipal* aPrincipal, nsIPrincipal* aTopLevelPrincipal,
      const bool aIsHandlingUserInput,
      const bool aMaybeUnsafePermissionDelegate, const TabId& aTabId);

  static nsresult AskPermission(nsIContentPermissionRequest* aRequest,
                                nsPIDOMWindowInner* aWindow);

  static nsTArray<PContentPermissionRequestParent*>
  GetContentPermissionRequestParentById(const TabId& aTabId);

  static void NotifyRemoveContentPermissionRequestParent(
      PContentPermissionRequestParent* aParent);

  static nsTArray<PContentPermissionRequestChild*>
  GetContentPermissionRequestChildById(const TabId& aTabId);

  static void NotifyRemoveContentPermissionRequestChild(
      PContentPermissionRequestChild* aChild);
};

class nsContentPermissionRequester final
    : public nsIContentPermissionRequester {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUESTER

  explicit nsContentPermissionRequester(nsPIDOMWindowInner* aWindow);

 private:
  virtual ~nsContentPermissionRequester();

  nsWeakPtr mWindow;
  RefPtr<VisibilityChangeListener> mListener;
};

nsresult TranslateChoices(
    JS::HandleValue aChoices,
    const nsTArray<PermissionRequest>& aPermissionRequests,
    nsTArray<PermissionChoice>& aTranslatedChoices);

class ContentPermissionRequestBase : public nsIContentPermissionRequest {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ContentPermissionRequestBase)

  NS_IMETHOD GetTypes(nsIArray** aTypes) override;
  NS_IMETHOD GetPrincipal(nsIPrincipal** aPrincipal) override;
  NS_IMETHOD GetDelegatePrincipal(const nsACString& aType,
                                  nsIPrincipal** aPrincipal) override;
  NS_IMETHOD GetTopLevelPrincipal(nsIPrincipal** aTopLevelPrincipal) override;
  NS_IMETHOD GetWindow(mozIDOMWindow** aWindow) override;
  NS_IMETHOD GetElement(mozilla::dom::Element** aElement) override;
  NS_IMETHOD GetIsHandlingUserInput(bool* aIsHandlingUserInput) override;
  NS_IMETHOD GetMaybeUnsafePermissionDelegate(
      bool* aMaybeUnsafePermissionDelegate) override;
  NS_IMETHOD GetRequester(nsIContentPermissionRequester** aRequester) override;
  // Overrides for Allow() and Cancel() aren't provided by this class.
  // That is the responsibility of the subclasses.

  enum class PromptResult {
    Granted,
    Denied,
    Pending,
  };
  nsresult ShowPrompt(PromptResult& aResult);

  PromptResult CheckPromptPrefs();

  // Check if the permission has an opportunity to request.
  bool CheckPermissionDelegate();

  enum class DelayedTaskType {
    Allow,
    Deny,
    Request,
  };
  void RequestDelayedTask(nsIEventTarget* aTarget, DelayedTaskType aType);

 protected:
  ContentPermissionRequestBase(nsIPrincipal* aPrincipal,
                               nsPIDOMWindowInner* aWindow,
                               const nsACString& aPrefName,
                               const nsACString& aType);
  virtual ~ContentPermissionRequestBase() = default;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mTopLevelPrincipal;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
  RefPtr<PermissionDelegateHandler> mPermissionHandler;
  nsCString mPrefName;
  nsCString mType;
  bool mIsHandlingUserInput;
  bool mMaybeUnsafePermissionDelegate;
};

}  // namespace dom
}  // namespace mozilla

using mozilla::dom::ContentPermissionRequestParent;

class nsContentPermissionRequestProxy : public nsIContentPermissionRequest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  explicit nsContentPermissionRequestProxy(
      ContentPermissionRequestParent* parent);

  nsresult Init(const nsTArray<mozilla::dom::PermissionRequest>& requests);

  void OnParentDestroyed();

  void NotifyVisibility(const bool& aIsVisible);

 private:
  class nsContentPermissionRequesterProxy final
      : public nsIContentPermissionRequester {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTPERMISSIONREQUESTER

    explicit nsContentPermissionRequesterProxy(
        ContentPermissionRequestParent* aParent)
        : mParent(aParent), mWaitGettingResult(false) {}

    void NotifyVisibilityResult(const bool& aIsVisible);

   private:
    virtual ~nsContentPermissionRequesterProxy() = default;

    ContentPermissionRequestParent* mParent;
    bool mWaitGettingResult;
    nsCOMPtr<nsIContentPermissionRequestCallback> mGetCallback;
    nsCOMPtr<nsIContentPermissionRequestCallback> mOnChangeCallback;
  };

  virtual ~nsContentPermissionRequestProxy();

  // Non-owning pointer to the ContentPermissionRequestParent object which owns
  // this proxy.
  ContentPermissionRequestParent* mParent;
  nsTArray<mozilla::dom::PermissionRequest> mPermissionRequests;
  RefPtr<nsContentPermissionRequesterProxy> mRequester;
};

/**
 * RemotePermissionRequest will send a prompt ipdl request to b2g process.
 */
class RemotePermissionRequest final
    : public nsIContentPermissionRequestCallback,
      public mozilla::dom::PContentPermissionRequestChild {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUESTCALLBACK

  RemotePermissionRequest(nsIContentPermissionRequest* aRequest,
                          nsPIDOMWindowInner* aWindow);

  // It will be called when prompt dismissed.  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  // because we don't have MOZ_CAN_RUN_SCRIPT bits in IPC code yet.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvNotifyResult(
      const bool& aAllow, nsTArray<PermissionChoice>&& aChoices);

  mozilla::ipc::IPCResult RecvGetVisibility();

  void IPDLAddRef() {
    mIPCOpen = true;
    AddRef();
  }

  void IPDLRelease() {
    mIPCOpen = false;
    Release();
  }

  void Destroy();

  bool IPCOpen() const { return mIPCOpen && !mDestroyed; }

 private:
  virtual ~RemotePermissionRequest();

  MOZ_CAN_RUN_SCRIPT
  void DoAllow(JS::HandleValue aChoices);
  MOZ_CAN_RUN_SCRIPT
  void DoCancel();

  nsCOMPtr<nsIContentPermissionRequest> mRequest;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  bool mIPCOpen;
  bool mDestroyed;
  RefPtr<VisibilityChangeListener> mListener;
};

#endif  // nsContentPermissionHelper_h
