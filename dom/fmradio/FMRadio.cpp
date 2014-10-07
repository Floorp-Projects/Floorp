/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FMRadio.h"
#include "nsContentUtils.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/FMRadioBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PFMRadioChild.h"
#include "mozilla/dom/FMRadioService.h"
#include "mozilla/dom/TypedArray.h"
#include "DOMRequest.h"
#include "nsDOMClassInfo.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAudioManager.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadio", args)

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

using namespace mozilla::hal;
using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

class FMRadioRequest MOZ_FINAL : public FMRadioReplyRunnable
                               , public DOMRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  FMRadioRequest(nsPIDOMWindow* aWindow, FMRadio* aFMRadio)
    : DOMRequest(aWindow)
    , mType(FMRadioRequestArgs::T__None)
  {
    // |FMRadio| inherits from |nsIDOMEventTarget| and |nsISupportsWeakReference|
    // which both inherits from nsISupports, so |nsISupports| is an ambiguous
    // base of |FMRadio|, we have to cast |aFMRadio| to one of the base classes.
    mFMRadio = do_GetWeakReference(static_cast<nsIDOMEventTarget*>(aFMRadio));
  }

  FMRadioRequest(nsPIDOMWindow* aWindow, FMRadio* aFMRadio,
    FMRadioRequestArgs::Type aType)
    : DOMRequest(aWindow)
  {
    MOZ_ASSERT(aType >= FMRadioRequestArgs::T__None &&
               aType <= FMRadioRequestArgs::T__Last,
               "Wrong FMRadioRequestArgs in FMRadioRequest");

    mFMRadio = do_GetWeakReference(static_cast<nsIDOMEventTarget*>(aFMRadio));
    mType = aType;
  }

  ~FMRadioRequest() { }

  NS_IMETHODIMP
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryReferent(mFMRadio);
    if (!target) {
      return NS_OK;
    }

    FMRadio* fmRadio = static_cast<FMRadio*>(
      static_cast<nsIDOMEventTarget*>(target));

    if (fmRadio->mIsShutdown) {
      return NS_OK;
    }

    switch (mResponseType.type()) {
      case FMRadioResponseType::TErrorResponse:
        FireError(mResponseType.get_ErrorResponse().error());
        break;
      case FMRadioResponseType::TSuccessResponse:
        if (mType == FMRadioRequestArgs::TEnableRequestArgs) {
          fmRadio->EnableAudioChannelAgent();
        }

        FireSuccess(JS::UndefinedHandleValue);
        break;
      default:
        MOZ_CRASH();
    }

    return NS_OK;
  }

private:
  FMRadioRequestArgs::Type mType;
  nsWeakPtr mFMRadio;
};

NS_IMPL_ISUPPORTS_INHERITED0(FMRadioRequest, DOMRequest)

FMRadio::FMRadio()
  : mHeadphoneState(SWITCH_STATE_OFF)
  , mRdsGroupMask(0)
  , mAudioChannelAgentEnabled(false)
  , mHasInternalAntenna(false)
  , mIsShutdown(false)
{
  LOG("FMRadio is initialized.");
}

FMRadio::~FMRadio()
{
}

void
FMRadio::Init(nsPIDOMWindow *aWindow)
{
  BindToOwner(aWindow);

  IFMRadioService::Singleton()->AddObserver(this);

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);
  if (mHasInternalAntenna) {
    LOG("We have an internal antenna.");
  } else {
    mHeadphoneState = GetCurrentSwitchState(SWITCH_HEADPHONES);
    RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  }

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);
  target->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"), this,
                                 /* useCapture = */ true,
                                 /* wantsUntrusted = */ false);


  // All of the codes below are for AudioChannel. We can directly return here
  // if preferences doesn't enable AudioChannelService.
  NS_ENSURE_TRUE_VOID(Preferences::GetBool("media.useAudioChannelService"));

  nsCOMPtr<nsIAudioChannelAgent> audioChannelAgent =
    do_CreateInstance("@mozilla.org/audiochannelagent;1");
  NS_ENSURE_TRUE_VOID(audioChannelAgent);

  audioChannelAgent->InitWithWeakCallback(
    GetOwner(),
    nsIAudioChannelAgent::AUDIO_AGENT_CHANNEL_CONTENT,
    this);

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(docshell);

  bool isActive = false;
  docshell->GetIsActive(&isActive);
  audioChannelAgent->SetVisibilityState(isActive);

  // Once all necessary resources are got successfully, we just enabled
  // mAudioChannelAgent.
  mAudioChannelAgent = audioChannelAgent;
}

void
FMRadio::Shutdown()
{
  IFMRadioService::Singleton()->RemoveObserver(this);

  if (!mHasInternalAntenna) {
    UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
  }

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);
  target->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"), this,
                                    /* useCapture = */ true);

  mIsShutdown = true;
}

JSObject*
FMRadio::WrapObject(JSContext* aCx)
{
  return FMRadioBinding::Wrap(aCx, this);
}

void
FMRadio::Notify(const SwitchEvent& aEvent)
{
  MOZ_ASSERT(!mHasInternalAntenna);

  if (mHeadphoneState != aEvent.status()) {
    mHeadphoneState = aEvent.status();

    DispatchTrustedEvent(NS_LITERAL_STRING("antennaavailablechange"));
  }
}

void
FMRadio::Notify(const FMRadioEventType& aType)
{
  switch (aType) {
    case FrequencyChanged:
      DispatchTrustedEvent(NS_LITERAL_STRING("frequencychange"));
      break;
    case EnabledChanged:
      if (Enabled()) {
        DispatchTrustedEvent(NS_LITERAL_STRING("enabled"));
      } else {
        if (mAudioChannelAgentEnabled) {
          mAudioChannelAgent->StopPlaying();
          mAudioChannelAgentEnabled = false;
        }

        DispatchTrustedEvent(NS_LITERAL_STRING("disabled"));
      }
      break;
    case RDSEnabledChanged:
      if (RdsEnabled()) {
        DispatchTrustedEvent(NS_LITERAL_STRING("rdsenabled"));
      } else {
        DispatchTrustedEvent(NS_LITERAL_STRING("rdsdisabled"));
      }
      break;
    case PIChanged:
      DispatchTrustedEvent(NS_LITERAL_STRING("pichange"));
      break;
    case PSChanged:
      DispatchTrustedEvent(NS_LITERAL_STRING("pschange"));
      break;
    case RadiotextChanged:
      DispatchTrustedEvent(NS_LITERAL_STRING("rtchange"));
      break;
    case PTYChanged:
      DispatchTrustedEvent(NS_LITERAL_STRING("ptychange"));
      break;
    case NewRDSGroup:
      DispatchTrustedEvent(NS_LITERAL_STRING("newrdsgroup"));
      break;
    default:
      MOZ_CRASH();
  }
}

/* static */
bool
FMRadio::Enabled()
{
  return IFMRadioService::Singleton()->IsEnabled();
}

bool
FMRadio::RdsEnabled()
{
  return IFMRadioService::Singleton()->IsRDSEnabled();
}

bool
FMRadio::AntennaAvailable() const
{
  return mHasInternalAntenna ? true : (mHeadphoneState != SWITCH_STATE_OFF) &&
    (mHeadphoneState != SWITCH_STATE_UNKNOWN);
}

Nullable<double>
FMRadio::GetFrequency() const
{
  return Enabled() ?
    Nullable<double>(IFMRadioService::Singleton()->GetFrequency()) :
    Nullable<double>();
}

double
FMRadio::FrequencyUpperBound() const
{
  return IFMRadioService::Singleton()->GetFrequencyUpperBound();
}

double
FMRadio::FrequencyLowerBound() const
{
  return IFMRadioService::Singleton()->GetFrequencyLowerBound();
}

double
FMRadio::ChannelWidth() const
{
  return IFMRadioService::Singleton()->GetChannelWidth();
}

uint32_t
FMRadio::RdsGroupMask() const
{
  return mRdsGroupMask;
}

void
FMRadio::SetRdsGroupMask(uint32_t aRdsGroupMask)
{
  mRdsGroupMask = aRdsGroupMask;
  IFMRadioService::Singleton()->SetRDSGroupMask(aRdsGroupMask);
}

Nullable<unsigned short>
FMRadio::GetPi() const
{
  return IFMRadioService::Singleton()->GetPi();
}

Nullable<uint8_t>
FMRadio::GetPty() const
{
  return IFMRadioService::Singleton()->GetPty();
}

void
FMRadio::GetPs(DOMString& aPsname) const
{
  if (!IFMRadioService::Singleton()->GetPs(aPsname)) {
    aPsname.SetNull();
  }
}

void
FMRadio::GetRt(DOMString& aRadiotext) const
{
  if (!IFMRadioService::Singleton()->GetRt(aRadiotext)) {
    aRadiotext.SetNull();
  }
}

void
FMRadio::GetRdsgroup(JSContext* cx, JS::MutableHandle<JSObject*> retval)
{
  uint64_t group;
  if (!IFMRadioService::Singleton()->GetRdsgroup(group)) {
    return;
  }

  JSObject *rdsgroup = Uint16Array::Create(cx, this, 4);
  uint16_t *data = JS_GetUint16ArrayData(rdsgroup);
  data[3] = group & 0xFFFF;
  group >>= 16;
  data[2] = group & 0xFFFF;
  group >>= 16;
  data[1] = group & 0xFFFF;
  group >>= 16;
  data[0] = group & 0xFFFF;

  JS::ExposeObjectToActiveJS(rdsgroup);
  retval.set(rdsgroup);
}

already_AddRefed<DOMRequest>
FMRadio::Enable(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r =
    new FMRadioRequest(win, this, FMRadioRequestArgs::TEnableRequestArgs);
  IFMRadioService::Singleton()->Enable(aFrequency, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::Disable()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->Disable(r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SetFrequency(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->SetFrequency(aFrequency, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekUp()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->Seek(FM_RADIO_SEEK_DIRECTION_UP, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekDown()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->Seek(FM_RADIO_SEEK_DIRECTION_DOWN, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::CancelSeek()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->CancelSeek(r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::EnableRDS()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->EnableRDS(r);
  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::DisableRDS()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  FMRadioService::Singleton()->DisableRDS(r);
  return r.forget();
}

NS_IMETHODIMP
FMRadio::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  if (!type.EqualsLiteral("visibilitychange")) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE(docshell, NS_ERROR_FAILURE);

  bool isActive = false;
  docshell->GetIsActive(&isActive);

  mAudioChannelAgent->SetVisibilityState(isActive);
  return NS_OK;
}

void
FMRadio::EnableAudioChannelAgent()
{
  NS_ENSURE_TRUE_VOID(mAudioChannelAgent);

  int32_t playingState = 0;
  mAudioChannelAgent->StartPlaying(&playingState);
  SetCanPlay(playingState == AudioChannelState::AUDIO_CHANNEL_STATE_NORMAL);

  mAudioChannelAgentEnabled = true;
}

NS_IMETHODIMP
FMRadio::CanPlayChanged(int32_t aCanPlay)
{
  SetCanPlay(aCanPlay == AudioChannelState::AUDIO_CHANNEL_STATE_NORMAL);
  return NS_OK;
}

NS_IMETHODIMP
FMRadio::WindowVolumeChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
FMRadio::SetCanPlay(bool aCanPlay)
{
  IFMRadioService::Singleton()->EnableAudio(aCanPlay);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FMRadio)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FMRadio, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FMRadio, DOMEventTargetHelper)

END_FMRADIO_NAMESPACE

