/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/FMRadioRequestChild.h"

using namespace mozilla::hal;

BEGIN_FMRADIO_NAMESPACE

StaticAutoPtr<FMRadioChild> FMRadioChild::sFMRadioChild;

FMRadioChild::FMRadioChild()
  : mEnabled(false)
  , mRDSEnabled(false)
  , mRDSGroupSet(false)
  , mPSNameSet(false)
  , mRadiotextSet(false)
  , mFrequency(0)
  , mRDSGroup(0)
  , mRDSGroupMask(0)
  , mObserverList(FMRadioEventObserverList())
{
  MOZ_COUNT_CTOR(FMRadioChild);

  ContentChild::GetSingleton()->SendPFMRadioConstructor(this);

  StatusInfo statusInfo;
  SendGetStatusInfo(&statusInfo);

  mEnabled = statusInfo.enabled();
  mFrequency = statusInfo.frequency();
  mUpperBound = statusInfo.upperBound();
  mLowerBound= statusInfo.lowerBound();
  mChannelWidth = statusInfo.channelWidth();
}

FMRadioChild::~FMRadioChild()
{
  MOZ_COUNT_DTOR(FMRadioChild);
}

bool
FMRadioChild::IsEnabled() const
{
  return mEnabled;
}

bool
FMRadioChild::IsRDSEnabled() const
{
  return mRDSEnabled;
}

double
FMRadioChild::GetFrequency() const
{
  return mFrequency;
}


double
FMRadioChild::GetFrequencyUpperBound() const
{
  return mUpperBound;
}

double
FMRadioChild::GetFrequencyLowerBound() const
{
  return mLowerBound;
}

double
FMRadioChild::GetChannelWidth() const
{
  return mChannelWidth;
}

Nullable<unsigned short>
FMRadioChild::GetPi() const
{
  return mPI;
}

Nullable<uint8_t>
FMRadioChild::GetPty() const
{
  return mPTY;
}

bool
FMRadioChild::GetPs(nsString& aPSName)
{
  if (mPSNameSet) {
    aPSName = mPSName;
  }
  return mPSNameSet;
}

bool
FMRadioChild::GetRt(nsString& aRadiotext)
{
  if (mRadiotextSet) {
    aRadiotext = mRadiotext;
  }
  return mRadiotextSet;
}

bool
FMRadioChild::GetRdsgroup(uint64_t& aRDSGroup)
{
  aRDSGroup = mRDSGroup;
  return mRDSGroupSet;
}

void
FMRadioChild::Enable(double aFrequency, FMRadioReplyRunnable* aReplyRunnable)
{
  SendRequest(aReplyRunnable, EnableRequestArgs(aFrequency));
}

void
FMRadioChild::Disable(FMRadioReplyRunnable* aReplyRunnable)
{
  SendRequest(aReplyRunnable, DisableRequestArgs());
}

void
FMRadioChild::SetFrequency(double aFrequency,
                           FMRadioReplyRunnable* aReplyRunnable)
{
  SendRequest(aReplyRunnable, SetFrequencyRequestArgs(aFrequency));
}

void
FMRadioChild::Seek(FMRadioSeekDirection aDirection,
                   FMRadioReplyRunnable* aReplyRunnable)
{
  SendRequest(aReplyRunnable, SeekRequestArgs(aDirection));
}

void
FMRadioChild::CancelSeek(FMRadioReplyRunnable* aReplyRunnable)
{
  SendRequest(aReplyRunnable, CancelSeekRequestArgs());
}

void
FMRadioChild::SetRDSGroupMask(uint32_t aRDSGroupMask)
{
  mRDSGroupMask = aRDSGroupMask;
  SendSetRDSGroupMask(aRDSGroupMask);
}

void
FMRadioChild::EnableRDS(FMRadioReplyRunnable* aReplyRunnable)
{
  SendRequest(aReplyRunnable, EnableRDSArgs());
}

void
FMRadioChild::DisableRDS(FMRadioReplyRunnable* aReplyRunnable)
{
  SendRequest(aReplyRunnable, DisableRDSArgs());
}

inline void
FMRadioChild::NotifyFMRadioEvent(FMRadioEventType aType)
{
  mObserverList.Broadcast(aType);
}

void
FMRadioChild::AddObserver(FMRadioEventObserver* aObserver)
{
  mObserverList.AddObserver(aObserver);
}

void
FMRadioChild::RemoveObserver(FMRadioEventObserver* aObserver)
{
  mObserverList.RemoveObserver(aObserver);
}

void
FMRadioChild::SendRequest(FMRadioReplyRunnable* aReplyRunnable,
                          FMRadioRequestArgs aArgs)
{
  PFMRadioRequestChild* childRequest = new FMRadioRequestChild(aReplyRunnable);
  SendPFMRadioRequestConstructor(childRequest, aArgs);
}

bool
FMRadioChild::RecvNotifyFrequencyChanged(const double& aFrequency)
{
  mFrequency = aFrequency;
  NotifyFMRadioEvent(FrequencyChanged);

  if (!mPI.IsNull()) {
    mPI.SetNull();
    NotifyFMRadioEvent(PIChanged);
  }
  if (!mPTY.IsNull()) {
    mPTY.SetNull();
    NotifyFMRadioEvent(PTYChanged);
  }
  if (mPSNameSet) {
    mPSNameSet = false;
    mPSName.Truncate();
    NotifyFMRadioEvent(PSChanged);
  }
  if (mRadiotextSet) {
    mRadiotextSet = false;
    mRadiotext.Truncate();
    NotifyFMRadioEvent(RadiotextChanged);
  }
  mRDSGroupSet = false;
  return true;
}

bool
FMRadioChild::RecvNotifyEnabledChanged(const bool& aEnabled,
                                       const double& aFrequency)
{
  mEnabled = aEnabled;
  mFrequency = aFrequency;
  if (!mEnabled) {
    mPI.SetNull();
    mPTY.SetNull();
    mPSName.Truncate();
    mRadiotext.Truncate();
    mRDSGroupSet = false;
    mPSNameSet = false;
    mRadiotextSet = false;
  }
  NotifyFMRadioEvent(EnabledChanged);
  return true;
}

bool
FMRadioChild::RecvNotifyRDSEnabledChanged(const bool& aEnabled)
{
  mRDSEnabled = aEnabled;
  NotifyFMRadioEvent(RDSEnabledChanged);
  return true;
}

bool
FMRadioChild::RecvNotifyPIChanged(const bool& aValid,
                                  const uint16_t& aCode)
{
  if (aValid) {
    mPI.SetValue(aCode);
  } else {
    mPI.SetNull();
  }
  NotifyFMRadioEvent(PIChanged);
  return true;
}

bool
FMRadioChild::RecvNotifyPTYChanged(const bool& aValid,
                                   const uint8_t& aPTY)
{
  if (aValid) {
    mPTY.SetValue(aPTY);
  } else {
    mPTY.SetNull();
  }
  NotifyFMRadioEvent(PTYChanged);
  return true;
}

bool
FMRadioChild::RecvNotifyPSChanged(const nsString& aPSName)
{
  mPSNameSet = true;
  mPSName = aPSName;
  NotifyFMRadioEvent(PSChanged);
  return true;
}

bool
FMRadioChild::RecvNotifyRadiotextChanged(const nsString& aRadiotext)
{
  mRadiotextSet = true;
  mRadiotext = aRadiotext;
  NotifyFMRadioEvent(RadiotextChanged);
  return true;
}

bool
FMRadioChild::RecvNotifyNewRDSGroup(const uint64_t& aGroup)
{
  uint16_t grouptype = (aGroup >> 43) & 0x1F;
  if (!(mRDSGroupMask & (1 << grouptype))) {
    return true;
  }

  mRDSGroupSet = true;
  mRDSGroup = aGroup;
  NotifyFMRadioEvent(NewRDSGroup);
  return true;
}

bool
FMRadioChild::Recv__delete__()
{
  return true;
}

PFMRadioRequestChild*
FMRadioChild::AllocPFMRadioRequestChild(const FMRadioRequestArgs& aArgs)
{
  MOZ_CRASH();
  return nullptr;
}

bool
FMRadioChild::DeallocPFMRadioRequestChild(PFMRadioRequestChild* aActor)
{
  delete aActor;
  return true;
}

void
FMRadioChild::EnableAudio(bool aAudioEnabled)
{
  SendEnableAudio(aAudioEnabled);
}

// static
FMRadioChild*
FMRadioChild::Singleton()
{
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sFMRadioChild) {
    sFMRadioChild = new FMRadioChild();
  }

  return sFMRadioChild;
}

END_FMRADIO_NAMESPACE

