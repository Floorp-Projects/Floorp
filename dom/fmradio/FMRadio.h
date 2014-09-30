/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FMRadio_h
#define mozilla_dom_FMRadio_h

#include "AudioChannelAgent.h"
#include "FMRadioCommon.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/HalTypes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"

class nsPIDOMWindow;
class nsIScriptContext;

BEGIN_FMRADIO_NAMESPACE

class DOMRequest;

class FMRadio MOZ_FINAL : public DOMEventTargetHelper
                        , public hal::SwitchObserver
                        , public FMRadioEventObserver
                        , public nsSupportsWeakReference
                        , public nsIAudioChannelAgentCallback
                        , public nsIDOMEventListener

{
  friend class FMRadioRequest;

public:
  FMRadio();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

  /* hal::SwitchObserver */
  virtual void Notify(const hal::SwitchEvent& aEvent) MOZ_OVERRIDE;
  /* FMRadioEventObserver */
  virtual void Notify(const FMRadioEventType& aType) MOZ_OVERRIDE;

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static bool Enabled();

  bool RdsEnabled();

  bool AntennaAvailable() const;

  Nullable<double> GetFrequency() const;

  double FrequencyUpperBound() const;

  double FrequencyLowerBound() const;

  double ChannelWidth() const;

  uint32_t RdsGroupMask() const;

  void SetRdsGroupMask(uint32_t aRdsGroupMask);

  Nullable<unsigned short> GetPi() const;

  Nullable<uint8_t> GetPty() const;

  void GetPs(DOMString& aPsname) const;

  void GetRt(DOMString& aRadiotext) const;

  void GetRdsgroup(JSContext* cx, JS::MutableHandle<JSObject*> retval);

  already_AddRefed<DOMRequest> Enable(double aFrequency);

  already_AddRefed<DOMRequest> Disable();

  already_AddRefed<DOMRequest> SetFrequency(double aFrequency);

  already_AddRefed<DOMRequest> SeekUp();

  already_AddRefed<DOMRequest> SeekDown();

  already_AddRefed<DOMRequest> CancelSeek();

  already_AddRefed<DOMRequest> EnableRDS();

  already_AddRefed<DOMRequest> DisableRDS();

  IMPL_EVENT_HANDLER(enabled);
  IMPL_EVENT_HANDLER(disabled);
  IMPL_EVENT_HANDLER(rdsenabled);
  IMPL_EVENT_HANDLER(rdsdisabled);
  IMPL_EVENT_HANDLER(antennaavailablechange);
  IMPL_EVENT_HANDLER(frequencychange);
  IMPL_EVENT_HANDLER(pichange);
  IMPL_EVENT_HANDLER(ptychange);
  IMPL_EVENT_HANDLER(pschange);
  IMPL_EVENT_HANDLER(rtchange);
  IMPL_EVENT_HANDLER(newrdsgroup);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

private:
  ~FMRadio();

  void SetCanPlay(bool aCanPlay);
  void EnableAudioChannelAgent();

  hal::SwitchState mHeadphoneState;
  uint32_t mRdsGroupMask;
  bool mAudioChannelAgentEnabled;
  bool mHasInternalAntenna;
  bool mIsShutdown;

  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_FMRadio_h

