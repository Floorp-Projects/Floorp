/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
class FMRadioChild final : public IFMRadioService
                         , public PFMRadioChild
{
public:
  static FMRadioChild* Singleton();
  ~FMRadioChild();

  void SendRequest(FMRadioReplyRunnable* aReplyRunnable,
                   FMRadioRequestArgs aArgs);

  /* IFMRadioService */
  virtual bool IsEnabled() const override;
  virtual bool IsRDSEnabled() const override;
  virtual double GetFrequency() const override;
  virtual double GetFrequencyUpperBound() const override;
  virtual double GetFrequencyLowerBound() const override;
  virtual double GetChannelWidth() const override;
  virtual Nullable<unsigned short> GetPi() const override;
  virtual Nullable<uint8_t> GetPty() const override;
  virtual bool GetPs(nsString& aPSName) override;
  virtual bool GetRt(nsString& aRadiotext) override;
  virtual bool GetRdsgroup(uint64_t& aRDSGroup) override;

  virtual void Enable(double aFrequency,
                      FMRadioReplyRunnable* aReplyRunnable) override;
  virtual void Disable(FMRadioReplyRunnable* aReplyRunnable) override;
  virtual void SetFrequency(double frequency,
                            FMRadioReplyRunnable* aReplyRunnable) override;
  virtual void Seek(mozilla::hal::FMRadioSeekDirection aDirection,
                    FMRadioReplyRunnable* aReplyRunnable) override;
  virtual void CancelSeek(FMRadioReplyRunnable* aReplyRunnable) override;
  virtual void SetRDSGroupMask(uint32_t aRDSGroupMask) override;
  virtual void EnableRDS(FMRadioReplyRunnable* aReplyRunnable) override;
  virtual void DisableRDS(FMRadioReplyRunnable* aReplyRunnable) override;

  virtual void AddObserver(FMRadioEventObserver* aObserver) override;
  virtual void RemoveObserver(FMRadioEventObserver* aObserver) override;

  virtual void EnableAudio(bool aAudioEnabled) override;

  /* PFMRadioChild */
  virtual bool
  Recv__delete__() override;

  virtual bool
  RecvNotifyFrequencyChanged(const double& aFrequency) override;

  virtual bool
  RecvNotifyEnabledChanged(const bool& aEnabled,
                           const double& aFrequency) override;

  virtual bool
  RecvNotifyRDSEnabledChanged(const bool& aEnabled) override;

  virtual bool
  RecvNotifyPIChanged(const bool& aValid,
                      const uint16_t& aCode) override;

  virtual bool
  RecvNotifyPTYChanged(const bool& aValid,
                       const uint8_t& aPTY) override;

  virtual bool
  RecvNotifyPSChanged(const nsString& aPSName) override;

  virtual bool
  RecvNotifyRadiotextChanged(const nsString& aRadiotext) override;

  virtual bool
  RecvNotifyNewRDSGroup(const uint64_t& aGroup) override;

  virtual PFMRadioRequestChild*
  AllocPFMRadioRequestChild(const FMRadioRequestArgs& aArgs) override;

  virtual bool
  DeallocPFMRadioRequestChild(PFMRadioRequestChild* aActor) override;

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

