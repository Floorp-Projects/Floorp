/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowserElementAudioChannel.h"

#include "mozilla/Services.h"
#include "mozilla/dom/BrowserElementAudioChannelBinding.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ToJSValue.h"
#include "AudioChannelService.h"
#include "nsIBrowserElementAPI.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMRequest.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsISystemMessagesInternal.h"
#include "nsITabParent.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"

namespace {

void
AssertIsInMainProcess()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
}

} // anonymous namespace

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(BrowserElementAudioChannel, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BrowserElementAudioChannel, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BrowserElementAudioChannel)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(BrowserElementAudioChannel,
                                   DOMEventTargetHelper,
                                   mFrameLoader,
                                   mFrameWindow,
                                   mTabParent,
                                   mBrowserElementAPI)

/* static */ already_AddRefed<BrowserElementAudioChannel>
BrowserElementAudioChannel::Create(nsPIDOMWindow* aWindow,
                                   nsIFrameLoader* aFrameLoader,
                                   nsIBrowserElementAPI* aAPI,
                                   AudioChannel aAudioChannel,
                                   const nsAString& aManifestURL,
                                   ErrorResult& aRv)
{
  RefPtr<BrowserElementAudioChannel> ac =
    new BrowserElementAudioChannel(aWindow, aFrameLoader, aAPI,
                                   aAudioChannel, aManifestURL);

  aRv = ac->Initialize();
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return ac.forget();
}

BrowserElementAudioChannel::BrowserElementAudioChannel(
                                                nsPIDOMWindow* aWindow,
                                                nsIFrameLoader* aFrameLoader,
                                                nsIBrowserElementAPI* aAPI,
                                                AudioChannel aAudioChannel,
                                                const nsAString& aManifestURL)
  : DOMEventTargetHelper(aWindow)
  , mFrameLoader(aFrameLoader)
  , mBrowserElementAPI(aAPI)
  , mAudioChannel(aAudioChannel)
  , mManifestURL(aManifestURL)
  , mState(eStateUnknown)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    nsAutoString name;
    AudioChannelService::GetAudioChannelString(aAudioChannel, name);

    nsAutoCString topic;
    topic.Assign("audiochannel-activity-");
    topic.Append(NS_ConvertUTF16toUTF8(name));

    obs->AddObserver(this, topic.get(), true);
  }
}

BrowserElementAudioChannel::~BrowserElementAudioChannel()
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    nsAutoString name;
    AudioChannelService::GetAudioChannelString(mAudioChannel, name);

    nsAutoCString topic;
    topic.Assign("audiochannel-activity-");
    topic.Append(NS_ConvertUTF16toUTF8(name));

    obs->RemoveObserver(this, topic.get());
  }
}

nsresult
BrowserElementAudioChannel::Initialize()
{
  if (!mFrameLoader) {
    nsCOMPtr<nsPIDOMWindow> window = GetOwner();
    if (!window) {
      return NS_ERROR_FAILURE;
    }

    mFrameWindow = window->GetScriptableTop();
    mFrameWindow = mFrameWindow->GetOuterWindow();
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = mFrameLoader->GetDocShell(getter_AddRefs(docShell));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (docShell) {
    nsCOMPtr<nsPIDOMWindow> window = docShell->GetWindow();
    if (!window) {
      return NS_ERROR_FAILURE;
    }

    mFrameWindow = window->GetScriptableTop();
    mFrameWindow = mFrameWindow->GetOuterWindow();
    return NS_OK;
  }

  rv = mFrameLoader->GetTabParent(getter_AddRefs(mTabParent));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(mTabParent);
  return NS_OK;
}

JSObject*
BrowserElementAudioChannel::WrapObject(JSContext *aCx,
                                       JS::Handle<JSObject*> aGivenProto)
{
  return BrowserElementAudioChannelBinding::Wrap(aCx, this, aGivenProto);
}

AudioChannel
BrowserElementAudioChannel::Name() const
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  return mAudioChannel;
}

namespace {

class BaseRunnable : public nsRunnable
{
protected:
  nsCOMPtr<nsPIDOMWindow> mParentWindow;
  nsCOMPtr<nsPIDOMWindow> mFrameWindow;
  RefPtr<DOMRequest> mRequest;
  AudioChannel mAudioChannel;

  virtual void DoWork(AudioChannelService* aService,
                      JSContext* aCx) = 0;

public:
  BaseRunnable(nsPIDOMWindow* aParentWindow, nsPIDOMWindow* aFrameWindow,
               DOMRequest* aRequest, AudioChannel aAudioChannel)
    : mParentWindow(aParentWindow)
    , mFrameWindow(aFrameWindow)
    , mRequest(aRequest)
    , mAudioChannel(aAudioChannel)
  {}

  NS_IMETHODIMP Run() override
  {
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    MOZ_ASSERT(service);

    AutoJSAPI jsapi;
    if (!jsapi.Init(mParentWindow)) {
      mRequest->FireError(NS_ERROR_FAILURE);
      return NS_OK;
    }

    DoWork(service, jsapi.cx());
    return NS_OK;
  }
};

class GetVolumeRunnable final : public BaseRunnable
{
public:
  GetVolumeRunnable(nsPIDOMWindow* aParentWindow, nsPIDOMWindow* aFrameWindow,
                    DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
  {}

protected:
  virtual void DoWork(AudioChannelService* aService, JSContext* aCx) override
  {
    float volume = aService->GetAudioChannelVolume(mFrameWindow, mAudioChannel);

    JS::Rooted<JS::Value> value(aCx);
    if (!ToJSValue(aCx, volume, &value)) {
      mRequest->FireError(NS_ERROR_FAILURE);
      return;
    }

    mRequest->FireSuccess(value);
  }
};

class GetMutedRunnable final : public BaseRunnable
{
public:
  GetMutedRunnable(nsPIDOMWindow* aParentWindow, nsPIDOMWindow* aFrameWindow,
                   DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
  {}

protected:
  virtual void DoWork(AudioChannelService* aService, JSContext* aCx) override
  {
    bool muted = aService->GetAudioChannelMuted(mFrameWindow, mAudioChannel);

    JS::Rooted<JS::Value> value(aCx);
    if (!ToJSValue(aCx, muted, &value)) {
      mRequest->FireError(NS_ERROR_FAILURE);
      return;
    }

    mRequest->FireSuccess(value);
  }
};

class IsActiveRunnable final : public BaseRunnable
{
  bool mActive;
  bool mValueKnown;

public:
  IsActiveRunnable(nsPIDOMWindow* aParentWindow, nsPIDOMWindow* aFrameWindow,
                   DOMRequest* aRequest, AudioChannel aAudioChannel,
                   bool aActive)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
    , mActive(aActive)
    , mValueKnown(true)
  {}

  IsActiveRunnable(nsPIDOMWindow* aParentWindow, nsPIDOMWindow* aFrameWindow,
                   DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
    , mActive(true)
    , mValueKnown(false)
  {}

protected:
  virtual void DoWork(AudioChannelService* aService, JSContext* aCx) override
  {
    if (!mValueKnown) {
      mActive = aService->IsAudioChannelActive(mFrameWindow, mAudioChannel);
    }

    JS::Rooted<JS::Value> value(aCx);
    if (!ToJSValue(aCx, mActive, &value)) {
      mRequest->FireError(NS_ERROR_FAILURE);
      return;
    }

    mRequest->FireSuccess(value);
  }
};

class FireSuccessRunnable final : public BaseRunnable
{
public:
  FireSuccessRunnable(nsPIDOMWindow* aParentWindow, nsPIDOMWindow* aFrameWindow,
                      DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
  {}

protected:
  virtual void DoWork(AudioChannelService* aService, JSContext* aCx) override
  {
    JS::Rooted<JS::Value> value(aCx);
    mRequest->FireSuccess(value);
  }
};

class RespondSuccessHandler final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  explicit RespondSuccessHandler(DOMRequest* aRequest)
    : mDomRequest(aRequest)
  {};

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

private:
  ~RespondSuccessHandler() {};

  RefPtr<DOMRequest> mDomRequest;
};
NS_IMPL_ISUPPORTS0(RespondSuccessHandler);

void
RespondSuccessHandler::ResolvedCallback(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue)
{
  JS::Rooted<JS::Value> value(aCx);
  mDomRequest->FireSuccess(value);
}

void
RespondSuccessHandler::RejectedCallback(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue)
{
  mDomRequest->FireError(NS_ERROR_FAILURE);
}

} // anonymous namespace

already_AddRefed<dom::DOMRequest>
BrowserElementAudioChannel::GetVolume(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  if (!mFrameWindow) {
    nsCOMPtr<nsIDOMDOMRequest> request;
    aRv = mBrowserElementAPI->GetAudioChannelVolume((uint32_t)mAudioChannel,
                                                    getter_AddRefs(request));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return request.forget().downcast<DOMRequest>();
  }

  RefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

  nsCOMPtr<nsIRunnable> runnable =
    new GetVolumeRunnable(GetOwner(), mFrameWindow, domRequest, mAudioChannel);
  NS_DispatchToMainThread(runnable);

  return domRequest.forget();
}

already_AddRefed<dom::DOMRequest>
BrowserElementAudioChannel::SetVolume(float aVolume, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  if (!mFrameWindow) {
    nsCOMPtr<nsIDOMDOMRequest> request;
    aRv = mBrowserElementAPI->SetAudioChannelVolume((uint32_t)mAudioChannel,
                                                    aVolume,
                                                    getter_AddRefs(request));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return request.forget().downcast<DOMRequest>();
  }

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  MOZ_ASSERT(service);

  service->SetAudioChannelVolume(mFrameWindow, mAudioChannel, aVolume);

  RefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());
  nsCOMPtr<nsIRunnable> runnable = new FireSuccessRunnable(GetOwner(),
                                                           mFrameWindow,
                                                           domRequest,
                                                           mAudioChannel);
  NS_DispatchToMainThread(runnable);

  return domRequest.forget();
}

already_AddRefed<dom::DOMRequest>
BrowserElementAudioChannel::GetMuted(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  if (!mFrameWindow) {
    nsCOMPtr<nsIDOMDOMRequest> request;
    aRv = mBrowserElementAPI->GetAudioChannelMuted((uint32_t)mAudioChannel,
                                                   getter_AddRefs(request));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return request.forget().downcast<DOMRequest>();
  }

  RefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

  nsCOMPtr<nsIRunnable> runnable =
    new GetMutedRunnable(GetOwner(), mFrameWindow, domRequest, mAudioChannel);
  NS_DispatchToMainThread(runnable);

  return domRequest.forget();
}

already_AddRefed<dom::DOMRequest>
BrowserElementAudioChannel::SetMuted(bool aMuted, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  if (!mFrameWindow) {
    nsCOMPtr<nsIDOMDOMRequest> request;
    aRv = mBrowserElementAPI->SetAudioChannelMuted((uint32_t)mAudioChannel,
                                                   aMuted,
                                                   getter_AddRefs(request));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return request.forget().downcast<DOMRequest>();
  }

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  MOZ_ASSERT(service);

  service->SetAudioChannelMuted(mFrameWindow, mAudioChannel, aMuted);

  RefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());
  nsCOMPtr<nsIRunnable> runnable = new FireSuccessRunnable(GetOwner(),
                                                           mFrameWindow,
                                                           domRequest,
                                                           mAudioChannel);
  NS_DispatchToMainThread(runnable);

  return domRequest.forget();
}

already_AddRefed<dom::DOMRequest>
BrowserElementAudioChannel::IsActive(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  if (mState != eStateUnknown) {
    RefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

    nsCOMPtr<nsIRunnable> runnable =
      new IsActiveRunnable(GetOwner(), mFrameWindow, domRequest, mAudioChannel,
                           mState == eStateActive);
    NS_DispatchToMainThread(runnable);

    return domRequest.forget();
  }

  if (!mFrameWindow) {
    nsCOMPtr<nsIDOMDOMRequest> request;
    aRv = mBrowserElementAPI->IsAudioChannelActive((uint32_t)mAudioChannel,
                                                   getter_AddRefs(request));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return request.forget().downcast<DOMRequest>();
  }

  RefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

  nsCOMPtr<nsIRunnable> runnable =
    new IsActiveRunnable(GetOwner(), mFrameWindow, domRequest, mAudioChannel);
  NS_DispatchToMainThread(runnable);

  return domRequest.forget();
}

already_AddRefed<dom::DOMRequest>
BrowserElementAudioChannel::NotifyChannel(const nsAString& aEvent,
                                          ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!mFrameWindow) {
    nsCOMPtr<nsIDOMDOMRequest> request;
    aRv = mBrowserElementAPI->NotifyChannel(aEvent, mManifestURL,
                                            (uint32_t)mAudioChannel,
                                            getter_AddRefs(request));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return request.forget().downcast<DOMRequest>();
  }

  nsCOMPtr<nsISystemMessagesInternal> systemMessenger =
    do_GetService("@mozilla.org/system-message-internal;1");
  MOZ_ASSERT(systemMessenger);

  AutoJSAPI jsAPI;
  if (!jsAPI.Init(GetOwner())) {
    return nullptr;
  }

  JS::Rooted<JS::Value> value(jsAPI.cx());
  value.setInt32((uint32_t)mAudioChannel);

  nsCOMPtr<nsIURI> manifestURI;
  nsresult rv = NS_NewURI(getter_AddRefs(manifestURI), mManifestURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  // Since the pageURI of the app has been registered to the system messager,
  // when the app was installed. The system messager can only use the manifest
  // to send the message to correct page.
  nsCOMPtr<nsISupports> promise;
  rv = systemMessenger->SendMessage(aEvent, value, nullptr, manifestURI,
                                    JS::UndefinedHandleValue,
                                    getter_AddRefs(promise));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  RefPtr<Promise> promiseIns = static_cast<Promise*>(promise.get());
  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<RespondSuccessHandler> handler = new RespondSuccessHandler(request);
  promiseIns->AppendNativeHandler(handler);

  return request.forget();
}

NS_IMETHODIMP
BrowserElementAudioChannel::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData)
{
  nsAutoString name;
  AudioChannelService::GetAudioChannelString(mAudioChannel, name);

  nsAutoCString topic;
  topic.Assign("audiochannel-activity-");
  topic.Append(NS_ConvertUTF16toUTF8(name));

  if (strcmp(topic.get(), aTopic)) {
    return NS_OK;
  }

  // Message received from the child.
  if (!mFrameWindow) {
    if (mTabParent == aSubject) {
      ProcessStateChanged(aData);
    }

    return NS_OK;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
  if (NS_WARN_IF(!wrapper)) {
    return NS_ERROR_FAILURE;
  }

  uint64_t windowID;
  nsresult rv = wrapper->GetData(&windowID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (windowID != mFrameWindow->WindowID()) {
    return NS_OK;
  }

  ProcessStateChanged(aData);
  return NS_OK;
}

void
BrowserElementAudioChannel::ProcessStateChanged(const char16_t* aData)
{
  nsAutoString value(aData);
  mState = value.EqualsASCII("active") ? eStateActive : eStateInactive;
  DispatchTrustedEvent(NS_LITERAL_STRING("activestatechanged"));
}

} // dom namespace
} // mozilla namespace
