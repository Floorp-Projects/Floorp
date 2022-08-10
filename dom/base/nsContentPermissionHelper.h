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

namespace mozilla::dom {

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

  // Converts blindly, that is, strings are not matched against any list.
  //
  // @param aSrcArray needs to contain elements of type
  // `nsIContentPermissionType`.
  static void ConvertArrayToPermissionRequest(
      nsIArray* aSrcArray, nsTArray<PermissionRequest>& aDesArray);

  static nsresult CreatePermissionArray(const nsACString& aType,
                                        const nsTArray<nsString>& aOptions,
                                        nsIArray** aTypesArray);

  // @param aIsRequestDelegatedToUnsafeThirdParty see
  // ContentPermissionRequestParent.
  static PContentPermissionRequestParent* CreateContentPermissionRequestParent(
      const nsTArray<PermissionRequest>& aRequests, Element* aElement,
      nsIPrincipal* aPrincipal, nsIPrincipal* aTopLevelPrincipal,
      const bool aHasValidTransientUserGestureActivation,
      const bool aIsRequestDelegatedToUnsafeThirdParty, const TabId& aTabId);

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

nsresult TranslateChoices(
    JS::Handle<JS::Value> aChoices,
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
  NS_IMETHOD GetHasValidTransientUserGestureActivation(
      bool* aHasValidTransientUserGestureActivation) override;
  NS_IMETHOD GetIsRequestDelegatedToUnsafeThirdParty(
      bool* aIsRequestDelegatedToUnsafeThirdParty) override;
  // Overrides for Allow() and Cancel() aren't provided by this class.
  // That is the responsibility of the subclasses.

  enum class PromptResult {
    Granted,
    Denied,
    Pending,
  };
  nsresult ShowPrompt(PromptResult& aResult);

  PromptResult CheckPromptPrefs() const;

  // Check if the permission has an opportunity to request.
  bool CheckPermissionDelegate() const;

  enum class DelayedTaskType {
    Allow,
    Deny,
    Request,
  };
  void RequestDelayedTask(nsIEventTarget* aTarget, DelayedTaskType aType);

 protected:
  // @param aPrefName see `mPrefName`.
  // @param aType see `mType`.
  ContentPermissionRequestBase(nsIPrincipal* aPrincipal,
                               nsPIDOMWindowInner* aWindow,
                               const nsACString& aPrefName,
                               const nsACString& aType);
  virtual ~ContentPermissionRequestBase() = default;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mTopLevelPrincipal;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<PermissionDelegateHandler> mPermissionHandler;

  // The prefix of a pref which allows tests to bypass showing the prompt.
  // Tests will have to set both of
  // ${mPrefName}.prompt.testing and
  // ${mPrefName}.prompt.testing.allow
  // to either true or false. If no such testing is required, mPrefName may be
  // empty.
  const nsCString mPrefName;

  // The type of the request, such as "autoplay-media-audible".
  const nsCString mType;

  bool mHasValidTransientUserGestureActivation;

  // See nsIPermissionDelegateHandler.maybeUnsafePermissionDelegate`.
  bool mIsRequestDelegatedToUnsafeThirdParty;
};

}  // namespace mozilla::dom

using mozilla::dom::ContentPermissionRequestParent;

class nsContentPermissionRequestProxy : public nsIContentPermissionRequest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  explicit nsContentPermissionRequestProxy(
      ContentPermissionRequestParent* parent);

  nsresult Init(const nsTArray<mozilla::dom::PermissionRequest>& requests);

  void OnParentDestroyed();

 private:
  virtual ~nsContentPermissionRequestProxy();

  // Non-owning pointer to the ContentPermissionRequestParent object which owns
  // this proxy.
  ContentPermissionRequestParent* mParent;
  nsTArray<mozilla::dom::PermissionRequest> mPermissionRequests;
};

/**
 * RemotePermissionRequest will send a prompt ipdl request to the chrome process
 * (https://wiki.mozilla.org/Security/Sandbox/Process_model#Chrome_process_.28Parent.29).
 */
class RemotePermissionRequest final
    : public mozilla::dom::PContentPermissionRequestChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemotePermissionRequest)

  RemotePermissionRequest(nsIContentPermissionRequest* aRequest,
                          nsPIDOMWindowInner* aWindow);

  // It will be called when prompt dismissed.  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  // because we don't have MOZ_CAN_RUN_SCRIPT bits in IPC code yet.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvNotifyResult(
      const bool& aAllow, nsTArray<PermissionChoice>&& aChoices);

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
  void DoAllow(JS::Handle<JS::Value> aChoices);
  MOZ_CAN_RUN_SCRIPT
  void DoCancel();

  nsCOMPtr<nsIContentPermissionRequest> mRequest;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  bool mIPCOpen;
  bool mDestroyed;
};

#endif  // nsContentPermissionHelper_h
