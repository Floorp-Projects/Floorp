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
#include "nsIDOMEventListener.h"

// Microsoft's API Name hackery sucks
// XXXbz Doing this in a header is a gigantic footgun. See
// https://bugzilla.mozilla.org/show_bug.cgi?id=932421#c3 for why.
#undef LoadImage

class nsPIDOMWindow;
class nsContentPermissionRequestProxy;

// Forward declare IPC::Principal here which is defined in
// PermissionMessageUtils.h. Include this file will transitively includes
// "windows.h" and it defines
//   #define CreateEvent CreateEventW
//   #define LoadImage LoadImageW
// That will mess up windows build.
namespace IPC {
class Principal;
} // namespace IPC

class VisibilityChangeListener final : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  explicit VisibilityChangeListener(nsPIDOMWindow* aWindow);

  void RemoveListener();
  void SetCallback(nsIContentPermissionRequestCallback* aCallback);
  already_AddRefed<nsIContentPermissionRequestCallback> GetCallback();

private:
  virtual ~VisibilityChangeListener() {}

  nsWeakPtr mWindow;
  nsCOMPtr<nsIContentPermissionRequestCallback> mCallback;
};

namespace mozilla {
namespace dom {

class Element;
class PermissionRequest;
class ContentPermissionRequestParent;
class PContentPermissionRequestParent;

class ContentPermissionType : public nsIContentPermissionType
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONTYPE

  ContentPermissionType(const nsACString& aType,
                        const nsACString& aAccess,
                        const nsTArray<nsString>& aOptions);

protected:
  virtual ~ContentPermissionType();

  nsCString mType;
  nsCString mAccess;
  nsTArray<nsString> mOptions;
};

class nsContentPermissionUtils
{
public:
  static uint32_t
  ConvertPermissionRequestToArray(nsTArray<PermissionRequest>& aSrcArray,
                                  nsIMutableArray* aDesArray);

  static uint32_t
  ConvertArrayToPermissionRequest(nsIArray* aSrcArray,
                                  nsTArray<PermissionRequest>& aDesArray);

  static nsresult
  CreatePermissionArray(const nsACString& aType,
                        const nsACString& aAccess,
                        const nsTArray<nsString>& aOptions,
                        nsIArray** aTypesArray);

  static PContentPermissionRequestParent*
  CreateContentPermissionRequestParent(const nsTArray<PermissionRequest>& aRequests,
                                       Element* element,
                                       const IPC::Principal& principal,
                                       const TabId& aTabId);

  static nsresult
  AskPermission(nsIContentPermissionRequest* aRequest, nsPIDOMWindow* aWindow);

  static nsTArray<PContentPermissionRequestParent*>
  GetContentPermissionRequestParentById(const TabId& aTabId);

  static void
  NotifyRemoveContentPermissionRequestParent(PContentPermissionRequestParent* aParent);

  static nsTArray<PContentPermissionRequestChild*>
  GetContentPermissionRequestChildById(const TabId& aTabId);

  static void
  NotifyRemoveContentPermissionRequestChild(PContentPermissionRequestChild* aChild);
};

class nsContentPermissionRequester final : public nsIContentPermissionRequester
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUESTER

  explicit nsContentPermissionRequester(nsPIDOMWindow* aWindow);

private:
  virtual ~nsContentPermissionRequester();

  nsCOMPtr<nsPIDOMWindow> mWindow;
  RefPtr<VisibilityChangeListener> mListener;
};

} // namespace dom
} // namespace mozilla

using mozilla::dom::ContentPermissionRequestParent;

class nsContentPermissionRequestProxy : public nsIContentPermissionRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  nsContentPermissionRequestProxy();

  nsresult Init(const nsTArray<mozilla::dom::PermissionRequest>& requests,
                ContentPermissionRequestParent* parent);

  void OnParentDestroyed();

  void NotifyVisibility(const bool& aIsVisible);

private:
  class nsContentPermissionRequesterProxy final : public nsIContentPermissionRequester {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTPERMISSIONREQUESTER

    explicit nsContentPermissionRequesterProxy(ContentPermissionRequestParent* aParent)
      : mParent(aParent)
      , mWaitGettingResult(false) {}

    void NotifyVisibilityResult(const bool& aIsVisible);

  private:
    virtual ~nsContentPermissionRequesterProxy() {}

    ContentPermissionRequestParent* mParent;
    bool mWaitGettingResult;
    nsCOMPtr<nsIContentPermissionRequestCallback> mGetCallback;
    nsCOMPtr<nsIContentPermissionRequestCallback> mOnChangeCallback;
  };

  virtual ~nsContentPermissionRequestProxy();

  // Non-owning pointer to the ContentPermissionRequestParent object which owns this proxy.
  ContentPermissionRequestParent* mParent;
  nsTArray<mozilla::dom::PermissionRequest> mPermissionRequests;
  RefPtr<nsContentPermissionRequesterProxy> mRequester;
};

/**
 * RemotePermissionRequest will send a prompt ipdl request to b2g process.
 */
class RemotePermissionRequest final : public nsIContentPermissionRequestCallback
                                    , public mozilla::dom::PContentPermissionRequestChild
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUESTCALLBACK

  RemotePermissionRequest(nsIContentPermissionRequest* aRequest,
                          nsPIDOMWindow* aWindow);

  // It will be called when prompt dismissed.
  virtual bool RecvNotifyResult(const bool &aAllow,
                                InfallibleTArray<PermissionChoice>&& aChoices) override;

  virtual bool RecvGetVisibility() override;

  void IPDLAddRef()
  {
    mIPCOpen = true;
    AddRef();
  }

  void IPDLRelease()
  {
    mIPCOpen = false;
    Release();
  }

  void Destroy();

  bool IPCOpen() const { return mIPCOpen && !mDestroyed; }

private:
  virtual ~RemotePermissionRequest()
  {
    MOZ_ASSERT(!mIPCOpen, "Protocol must not be open when RemotePermissionRequest is destroyed.");
  }

  void DoAllow(JS::HandleValue aChoices);
  void DoCancel();

  nsCOMPtr<nsIContentPermissionRequest> mRequest;
  nsCOMPtr<nsPIDOMWindow>               mWindow;
  bool                                  mIPCOpen;
  bool                                  mDestroyed;
  RefPtr<VisibilityChangeListener>    mListener;
};

#endif // nsContentPermissionHelper_h

