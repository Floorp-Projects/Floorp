/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradiochild_h__
#define mozilla_dom_fmradiochild_h__

#include "FMRadioCommon.h"
#include "FMRadioService.h"
#include "mozilla/dom/PFMRadioChild.h"
#include "mozilla/StaticPtr.h"

BEGIN_FMRADIO_NAMESPACE

/**
 * FMRadioChild plays two roles:
 *   - Kind of proxy of FMRadioService
 *     Redirect all the requests  coming from web content to FMRadioService
 *     in parent through IPC channel.
 *   - Child Actor of PFMRadio
 *     IPC channel to transfer the requests.
 */
class FMRadioChild MOZ_FINAL : public IFMRadioService
                             , public PFMRadioChild
{
public:
  static FMRadioChild* Singleton();
  ~FMRadioChild();

  void SendRequest(FMRadioReplyRunnable* aReplyRunnable,
                   FMRadioRequestArgs aArgs);

  /* IFMRadioService */
  virtual bool IsEnabled() const MOZ_OVERRIDE;
  virtual bool IsRDSEnabled() const MOZ_OVERRIDE;
  virtual double GetFrequency() const MOZ_OVERRIDE;
  virtual double GetFrequencyUpperBound() const MOZ_OVERRIDE;
  virtual double GetFrequencyLowerBound() const MOZ_OVERRIDE;
  virtual double GetChannelWidth() const MOZ_OVERRIDE;
  virtual Nullable<unsigned short> GetPi() const MOZ_OVERRIDE;
  virtual Nullable<uint8_t> GetPty() const MOZ_OVERRIDE;
  virtual bool GetPs(nsString& aPSName) MOZ_OVERRIDE;
  virtual bool GetRt(nsString& aRadiotext) MOZ_OVERRIDE;
  virtual bool GetRdsgroup(uint64_t& aRDSGroup) MOZ_OVERRIDE;

  virtual void Enable(double aFrequency,
                      FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void Disable(FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void SetFrequency(double frequency,
                            FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void Seek(mozilla::hal::FMRadioSeekDirection aDirection,
                    FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void CancelSeek(FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void SetRDSGroupMask(uint32_t aRDSGroupMask) MOZ_OVERRIDE;
  virtual void EnableRDS(FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void DisableRDS(FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;

  virtual void AddObserver(FMRadioEventObserver* aObserver) MOZ_OVERRIDE;
  virtual void RemoveObserver(FMRadioEventObserver* aObserver) MOZ_OVERRIDE;

  virtual void EnableAudio(bool aAudioEnabled) MOZ_OVERRIDE;

  /* PFMRadioChild */
  virtual bool
  Recv__delete__() MOZ_OVERRIDE;

  virtual bool
  RecvNotifyFrequencyChanged(const double& aFrequency) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyEnabledChanged(const bool& aEnabled,
                           const double& aFrequency) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyRDSEnabledChanged(const bool& aEnabled) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyPIChanged(const bool& aValid,
                      const uint16_t& aCode) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyPTYChanged(const bool& aValid,
                       const uint8_t& aPTY) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyPSChanged(const nsString& aPSName) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyRadiotextChanged(const nsString& aRadiotext) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyNewRDSGroup(const uint64_t& aGroup) MOZ_OVERRIDE;

  virtual PFMRadioRequestChild*
  AllocPFMRadioRequestChild(const FMRadioRequestArgs& aArgs) MOZ_OVERRIDE;

  virtual bool
  DeallocPFMRadioRequestChild(PFMRadioRequestChild* aActor) MOZ_OVERRIDE;

private:
  FMRadioChild();

  void Init();

  inline void NotifyFMRadioEvent(FMRadioEventType aType);

  bool mEnabled;
  bool mRDSEnabled;
  bool mRDSGroupSet;
  bool mPSNameSet;
  bool mRadiotextSet;
  double mFrequency;
  double mUpperBound;
  double mLowerBound;
  double mChannelWidth;
  Nullable<unsigned short> mPI;
  Nullable<uint8_t> mPTY;
  nsAutoString mPSName;
  nsAutoString mRadiotext;
  uint64_t mRDSGroup;
  uint32_t mRDSGroupMask;

  FMRadioEventObserverList mObserverList;

private:
  static StaticAutoPtr<FMRadioChild> sFMRadioChild;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradiochild_h__

