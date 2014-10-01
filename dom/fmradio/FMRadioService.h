/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradioservice_h__
#define mozilla_dom_fmradioservice_h__

#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PFMRadioRequest.h"
#include "FMRadioCommon.h"
#include "mozilla/Hal.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "nsIObserver.h"
#include "nsXULAppAPI.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioReplyRunnable : public nsRunnable
{
public:
  FMRadioReplyRunnable() : mResponseType(SuccessResponse()) {}
  virtual ~FMRadioReplyRunnable() {}

  void
  SetReply(const FMRadioResponseType& aResponseType)
  {
    mResponseType = aResponseType;
  }

protected:
  FMRadioResponseType mResponseType;
};

/**
 * The FMRadio Service Interface for FMRadio.
 *
 * There are two concrete classes which implement this interface:
 *  - FMRadioService
 *    It's used in the main process, implements all the logics about FM Radio.
 *
 *  - FMRadioChild
 *    It's used in subprocess. It's a kind of proxy which just sends all
 *    the requests to main process through IPC channel.
 *
 * All the requests coming from the content page will be redirected to the
 * concrete class object.
 *
 * Consider navigator.mozFMRadio.enable(). Here is the call sequence:
 *  - OOP
 *    Child:
 *      (1) Call navigator.mozFMRadio.enable().
 *      (2) Return a DOMRequest object, and call FMRadioChild.Enable() with a
 *          FMRadioReplyRunnable object.
 *      (3) Send IPC message to main process.
 *    Parent:
 *      (4) Call FMRadioService::Enable() with a FMRadioReplyRunnable object.
 *      (5) Call hal::EnableFMRadio().
 *      (6) Notify FMRadioService object when FM radio HW is enabled.
 *      (7) Dispatch the FMRadioReplyRunnable object created in (4).
 *      (8) Send IPC message back to child process.
 *    Child:
 *      (9) Dispatch the FMRadioReplyRunnable object created in (2).
 *     (10) Fire success callback of the DOMRequest Object created in (2).
 *                     _ _ _ _ _ _ _ _ _ _ _ _ _ _
 *                    |            OOP            |
 *                    |                           |
 *   Page  FMRadio    |    FMRadioChild       IPC |    FMRadioService   Hal
 *    | (1)  |        |          |             |  |           |          |
 *    |----->|    (2) |          |             |  |           |          |
 *    |      |--------|--------->|      (3)    |  |           |          |
 *    |      |        |          |-----------> |  |   (4)     |          |
 *    |      |        |          |             |--|---------->|  (5)     |
 *    |      |        |          |             |  |           |--------->|
 *    |      |        |          |             |  |           |  (6)     |
 *    |      |        |          |             |  |   (7)     |<---------|
 *    |      |        |          |      (8)    |<-|-----------|          |
 *    |      |    (9) |          |<----------- |  |           |          |
 *    | (10) |<-------|----------|             |  |           |          |
 *    |<-----|        |          |             |  |           |          |
 *                    |                           |
 *                    |_ _ _ _ _ _ _ _ _ _ _ _ _ _|
 *  - non-OOP
 *    In non-OOP model, we don't need to send messages between processes, so
 *    the call sequences are much more simpler, it almost just follows the
 *    sequences presented in OOP model: (1) (2) (5) (6) (9) and (10).
 *
 */
class IFMRadioService
{
protected:
  virtual ~IFMRadioService() { }

public:
  virtual bool IsEnabled() const = 0;
  virtual bool IsRDSEnabled() const = 0;
  virtual double GetFrequency() const = 0;
  virtual double GetFrequencyUpperBound() const = 0;
  virtual double GetFrequencyLowerBound() const = 0;
  virtual double GetChannelWidth() const = 0;
  virtual Nullable<unsigned short> GetPi() const = 0;
  virtual Nullable<uint8_t> GetPty() const = 0;
  virtual bool GetPs(nsString& aPsname) = 0;
  virtual bool GetRt(nsString& aRadiotext) = 0;
  virtual bool GetRdsgroup(uint64_t& aRDSGroup) = 0;

  virtual void Enable(double aFrequency, FMRadioReplyRunnable* aReplyRunnable) = 0;
  virtual void Disable(FMRadioReplyRunnable* aReplyRunnable) = 0;
  virtual void SetFrequency(double aFrequency, FMRadioReplyRunnable* aReplyRunnable) = 0;
  virtual void Seek(mozilla::hal::FMRadioSeekDirection aDirection,
                    FMRadioReplyRunnable* aReplyRunnable) = 0;
  virtual void CancelSeek(FMRadioReplyRunnable* aReplyRunnable) = 0;
  virtual void SetRDSGroupMask(uint32_t aRDSGroupMask) = 0;
  virtual void EnableRDS(FMRadioReplyRunnable* aReplyRunnable) = 0;
  virtual void DisableRDS(FMRadioReplyRunnable* aReplyRunnable) = 0;

  /**
   * Register handler to receive the FM Radio events, including:
   *   - StateChangedEvent
   *   - FrequencyChangedEvent
   *
   * Called by FMRadio and FMRadioParent.
   */
  virtual void AddObserver(FMRadioEventObserver* aObserver) = 0;
  virtual void RemoveObserver(FMRadioEventObserver* aObserver) = 0;

  // Enable/Disable FMRadio
  virtual void EnableAudio(bool aAudioEnabled) = 0;

  /**
   * Static method to return the singleton instance. If it's in the child
   * process, we will get an object of FMRadioChild.
   */
  static IFMRadioService* Singleton();
};

enum FMRadioState
{
  Disabled,
  Disabling,
  Enabling,
  Enabled,
  Seeking
};

class FMRadioService MOZ_FINAL : public IFMRadioService
                               , public hal::FMRadioObserver
                               , public hal::FMRadioRDSObserver
                               , public nsIObserver
{
  friend class ReadAirplaneModeSettingTask;
  friend class EnableRunnable;
  friend class DisableRunnable;
  friend class NotifyRunnable;

public:
  static FMRadioService* Singleton();
  virtual ~FMRadioService();

  NS_DECL_ISUPPORTS

  virtual bool IsEnabled() const MOZ_OVERRIDE;
  virtual bool IsRDSEnabled() const MOZ_OVERRIDE;
  virtual double GetFrequency() const MOZ_OVERRIDE;
  virtual double GetFrequencyUpperBound() const MOZ_OVERRIDE;
  virtual double GetFrequencyLowerBound() const MOZ_OVERRIDE;
  virtual double GetChannelWidth() const MOZ_OVERRIDE;
  virtual Nullable<unsigned short> GetPi() const MOZ_OVERRIDE;
  virtual Nullable<uint8_t> GetPty() const MOZ_OVERRIDE;
  virtual bool GetPs(nsString& aPsname) MOZ_OVERRIDE;
  virtual bool GetRt(nsString& aRadiotext) MOZ_OVERRIDE;
  virtual bool GetRdsgroup(uint64_t& aRDSGroup) MOZ_OVERRIDE;

  virtual void Enable(double aFrequency,
                      FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void Disable(FMRadioReplyRunnable* aReplyRunnable) MOZ_OVERRIDE;
  virtual void SetFrequency(double aFrequency,
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

  /* FMRadioObserver */
  void Notify(const hal::FMRadioOperationInformation& aInfo) MOZ_OVERRIDE;
  /* FMRadioRDSObserver */
  void Notify(const hal::FMRadioRDSGroup& aRDSGroup) MOZ_OVERRIDE;

  NS_DECL_NSIOBSERVER

protected:
  FMRadioService();

private:
  int32_t RoundFrequency(double aFrequencyInMHz);

  void NotifyFMRadioEvent(FMRadioEventType aType);
  void DoDisable();
  void TransitionState(const FMRadioResponseType& aResponse, FMRadioState aState);
  void SetState(FMRadioState aState);
  void UpdatePowerState();
  void UpdateFrequency();

private:
  bool mEnabled;

  int32_t mPendingFrequencyInKHz;

  FMRadioState mState;

  bool mHasReadAirplaneModeSetting;
  bool mAirplaneModeEnabled;
  bool mRDSEnabled;

  uint32_t mUpperBoundInKHz;
  uint32_t mLowerBoundInKHz;
  uint32_t mChannelWidthInKHz;
  uint32_t mPreemphasis;

  nsCOMPtr<nsIThread> mTuneThread;
  nsRefPtr<FMRadioReplyRunnable> mPendingRequest;

  FMRadioEventObserverList mObserverList;

  static StaticRefPtr<FMRadioService> sFMRadioService;

  uint32_t mRDSGroupMask;

  uint16_t mLastPI;
  uint16_t mLastPTY;
  Atomic<uint32_t> mPI;
  Atomic<uint32_t> mPTY;
  Atomic<bool> mPISet;
  Atomic<bool> mPTYSet;

  /* Protects mPSName, mRadiotext, and mRDSGroup */
  Mutex mRDSLock;
  char16_t mPSName[9];
  char16_t mRadiotext[65];
  uint64_t mRDSGroup;

  uint8_t mPSNameState;
  uint16_t mRadiotextState;
  uint16_t mTempPSName[8];
  uint16_t mTempRadiotext[64];
  bool mRadiotextAB;
  bool mRDSGroupSet;
  bool mPSNameSet;
  bool mRadiotextSet;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradioservice_h__

