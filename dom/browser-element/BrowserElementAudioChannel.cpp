/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowserElementAudioChannel.h"

#include "mozilla/Services.h"
#include "mozilla/dom/BrowserElementAudioChannelBinding.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/ToJSValue.h"
#include "AudioChannelService.h"
#include "nsIBrowserElementAPI.h"
#include "nsIDocShell.h"
#include "nsIDOMDOMRequest.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsITabParent.h"
#include "nsPIDOMWindow.h"

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

BrowserElementAudioChannel::BrowserElementAudioChannel(
                                                   nsPIDOMWindow* aWindow,
                                                   nsIFrameLoader* aFrameLoader,
                                                   nsIBrowserElementAPI* aAPI,
                                                   AudioChannel aAudioChannel)
  : DOMEventTargetHelper(aWindow)
  , mFrameLoader(aFrameLoader)
  , mBrowserElementAPI(aAPI)
  , mAudioChannel(aAudioChannel)
  , mState(eStateUnknown)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();
  MOZ_ASSERT(mFrameLoader);

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

    nsCOMPtr<nsIDOMWindow> topWindow;
    window->GetScriptableTop(getter_AddRefs(topWindow));

    mFrameWindow = do_QueryInterface(topWindow);
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
  nsRefPtr<DOMRequest> mRequest;
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
    nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
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

  nsRefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

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

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  MOZ_ASSERT(service);

  service->SetAudioChannelVolume(mFrameWindow, mAudioChannel, aVolume);

  nsRefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());
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

  nsRefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

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

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  MOZ_ASSERT(service);

  service->SetAudioChannelMuted(mFrameWindow, mAudioChannel, aMuted);

  nsRefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());
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
    nsRefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

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

  nsRefPtr<DOMRequest> domRequest = new DOMRequest(GetOwner());

  nsCOMPtr<nsIRunnable> runnable =
    new IsActiveRunnable(GetOwner(), mFrameWindow, domRequest, mAudioChannel);
  NS_DispatchToMainThread(runnable);

  return domRequest.forget();
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
