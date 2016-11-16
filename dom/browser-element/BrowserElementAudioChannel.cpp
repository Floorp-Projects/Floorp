/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowserElementAudioChannel.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/BrowserElementAudioChannelBinding.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/ToJSValue.h"
#include "AudioChannelService.h"
#include "nsContentUtils.h"
#include "nsIBrowserElementAPI.h"
#include "nsIDocShell.h"
#include "nsIDOMDOMRequest.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsITabParent.h"
#include "nsPIDOMWindow.h"

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
BrowserElementAudioChannel::Create(nsPIDOMWindowInner* aWindow,
                                   nsIFrameLoader* aFrameLoader,
                                   nsIBrowserElementAPI* aAPI,
                                   AudioChannel aAudioChannel,
                                   ErrorResult& aRv)
{
  RefPtr<BrowserElementAudioChannel> ac =
    new BrowserElementAudioChannel(aWindow, aFrameLoader, aAPI, aAudioChannel);

  aRv = ac->Initialize();
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("BrowserElementAudioChannel, Create, channel = %p, type = %d\n",
          ac.get(), aAudioChannel));

  return ac.forget();
}

BrowserElementAudioChannel::BrowserElementAudioChannel(
						nsPIDOMWindowInner* aWindow,
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
    nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
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
    nsCOMPtr<nsPIDOMWindowOuter> window = docShell->GetWindow();
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
  return mAudioChannel;
}

namespace {

class BaseRunnable : public Runnable
{
protected:
  nsCOMPtr<nsPIDOMWindowInner> mParentWindow;
  nsCOMPtr<nsPIDOMWindowOuter> mFrameWindow;
  RefPtr<DOMRequest> mRequest;
  AudioChannel mAudioChannel;

  virtual void DoWork(AudioChannelService* aService,
                      JSContext* aCx) = 0;

public:
  BaseRunnable(nsPIDOMWindowInner* aParentWindow,
	       nsPIDOMWindowOuter* aFrameWindow,
               DOMRequest* aRequest, AudioChannel aAudioChannel)
    : mParentWindow(aParentWindow)
    , mFrameWindow(aFrameWindow)
    , mRequest(aRequest)
    , mAudioChannel(aAudioChannel)
  {}

  NS_IMETHOD Run() override
  {
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (!service) {
      return NS_OK;
    }

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
  GetVolumeRunnable(nsPIDOMWindowInner* aParentWindow,
		    nsPIDOMWindowOuter* aFrameWindow,
                    DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
  {}

protected:
  void DoWork(AudioChannelService* aService, JSContext* aCx) override
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
  GetMutedRunnable(nsPIDOMWindowInner* aParentWindow,
		   nsPIDOMWindowOuter* aFrameWindow,
                   DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
  {}

protected:
  void DoWork(AudioChannelService* aService, JSContext* aCx) override
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
  IsActiveRunnable(nsPIDOMWindowInner* aParentWindow,
		   nsPIDOMWindowOuter* aFrameWindow,
                   DOMRequest* aRequest, AudioChannel aAudioChannel,
                   bool aActive)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
    , mActive(aActive)
    , mValueKnown(true)
  {}

  IsActiveRunnable(nsPIDOMWindowInner* aParentWindow,
		   nsPIDOMWindowOuter* aFrameWindow,
                   DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
    , mActive(true)
    , mValueKnown(false)
  {}

protected:
  void DoWork(AudioChannelService* aService, JSContext* aCx) override
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
  FireSuccessRunnable(nsPIDOMWindowInner* aParentWindow,
		      nsPIDOMWindowOuter* aFrameWindow,
                      DOMRequest* aRequest, AudioChannel aAudioChannel)
    : BaseRunnable(aParentWindow, aFrameWindow, aRequest, aAudioChannel)
  {}

protected:
  void DoWork(AudioChannelService* aService, JSContext* aCx) override
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
  if (service) {
    service->SetAudioChannelVolume(mFrameWindow, mAudioChannel, aVolume);
  }

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
  if (service) {
    service->SetAudioChannelMuted(mFrameWindow, mAudioChannel, aMuted);
  }

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
  if (!wrapper) {
    bool isNested = false;
    nsresult rv = IsFromNestedFrame(aSubject, isNested);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isNested) {
      ProcessStateChanged(aData);
    }

    return NS_OK;
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
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("BrowserElementAudioChannel, ProcessStateChanged, this = %p, "
          "type = %d\n", this, mAudioChannel));

  nsAutoString value(aData);
  mState = value.EqualsASCII("active") ? eStateActive : eStateInactive;
  DispatchTrustedEvent(NS_LITERAL_STRING("activestatechanged"));
}

bool
BrowserElementAudioChannel::IsSystemAppWindow(nsPIDOMWindowOuter* aWindow) const
{
  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (!doc) {
    return false;
  }

  if (nsContentUtils::IsChromeDoc(doc)) {
    return true;
  }

  nsAdoptingCString systemAppUrl =
    mozilla::Preferences::GetCString("b2g.system_startup_url");
  if (!systemAppUrl) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  nsCOMPtr<nsIURI> uri;
  principal->GetURI(getter_AddRefs(uri));

  if (uri) {
    nsAutoCString spec;
    uri->GetSpec(spec);

    if (spec.Equals(systemAppUrl)) {
      return true;
    }
  }

  return false;
}

nsresult
BrowserElementAudioChannel::IsFromNestedFrame(nsISupports* aSubject,
                                              bool& aIsNested) const
{
  aIsNested = false;
  nsCOMPtr<nsITabParent> iTabParent = do_QueryInterface(aSubject);
  if (!iTabParent) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<TabParent> tabParent = TabParent::GetFrom(iTabParent);
  if (!tabParent) {
    return NS_ERROR_FAILURE;
  }

  Element* element = tabParent->GetOwnerElement();
  if (!element) {
    return NS_ERROR_FAILURE;
  }

  // Since the normal OOP processes are opened out from b2g process, the owner
  // of their tabParent are the same - system app window. Therefore, in order
  // to find the case of nested MozFrame, we need to exclude this situation.
  nsCOMPtr<nsPIDOMWindowOuter> window = element->OwnerDoc()->GetWindow();
  if (window == mFrameWindow && !IsSystemAppWindow(window)) {
    aIsNested = true;
    return NS_OK;
  }

  return NS_OK;
}

} // dom namespace
} // mozilla namespace
