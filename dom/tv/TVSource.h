/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVSource_h
#define mozilla_dom_TVSource_h

#include "mozilla/DOMEventTargetHelper.h"
// Include TVScanningStateChangedEventBinding.h since enum TVScanningState can't
// be forward declared.
#include "mozilla/dom/TVScanningStateChangedEventBinding.h"
// Include TVSourceBinding.h since enum TVSourceType can't be forward declared.
#include "mozilla/dom/TVSourceBinding.h"

class nsITVChannelData;
class nsITVProgramData;
class nsITVService;

namespace mozilla {
namespace dom {

class Promise;
class TVChannel;
class TVProgram;
class TVTuner;

class TVSource MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TVSource, DOMEventTargetHelper)

  static already_AddRefed<TVSource> Create(nsPIDOMWindow* aWindow,
                                           TVSourceType aType,
                                           TVTuner* aTuner);

  // WebIDL (internal functions)

  virtual JSObject* WrapObject(JSContext *aCx) MOZ_OVERRIDE;

  nsresult SetCurrentChannel(nsITVChannelData* aChannelData);

  nsresult UnsetCurrentChannel();

  void SetIsScanning(bool aIsScanning);

  nsresult DispatchTVEvent(nsIDOMEvent* aEvent);

  nsresult NotifyChannelScanned(nsITVChannelData* aChannelData);

  nsresult NotifyChannelScanComplete();

  nsresult NotifyChannelScanStopped();

  nsresult NotifyEITBroadcasted(nsITVChannelData* aChannelData,
                                nsITVProgramData** aProgramDataList,
                                uint32_t aCount);

  // WebIDL (public APIs)

  already_AddRefed<Promise> GetChannels(ErrorResult& aRv);

  already_AddRefed<Promise> SetCurrentChannel(const nsAString& aChannelNumber,
                                              ErrorResult& aRv);

  already_AddRefed<Promise> StartScanning(const TVStartScanningOptions& aOptions,
                                          ErrorResult& aRv);

  already_AddRefed<Promise> StopScanning(ErrorResult& aRv);

  already_AddRefed<TVTuner> Tuner() const;

  TVSourceType Type() const;

  bool IsScanning() const;

  already_AddRefed<TVChannel> GetCurrentChannel() const;

  IMPL_EVENT_HANDLER(currentchannelchanged);
  IMPL_EVENT_HANDLER(eitbroadcasted);
  IMPL_EVENT_HANDLER(scanningstatechanged);

private:
  TVSource(nsPIDOMWindow* aWindow,
           TVSourceType aType,
           TVTuner* aTuner);

  ~TVSource();

  bool Init();

  void Shutdown();

  nsresult DispatchCurrentChannelChangedEvent(TVChannel* aChannel);

  nsresult DispatchScanningStateChangedEvent(TVScanningState aState,
                                             TVChannel* aChannel);

  nsresult DispatchEITBroadcastedEvent(const Sequence<OwningNonNull<TVProgram>>& aPrograms);

  nsCOMPtr<nsITVService> mTVService;
  nsRefPtr<TVTuner> mTuner;
  nsRefPtr<TVChannel> mCurrentChannel;
  TVSourceType mType;
  bool mIsScanning;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVSource_h
