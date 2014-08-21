/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVSource_h__
#define mozilla_dom_TVSource_h__

#include "mozilla/DOMEventTargetHelper.h"
// Include TVSourceBinding.h since enum TVSourceType can't be forward declared.
#include "mozilla/dom/TVSourceBinding.h"

namespace mozilla {
namespace dom {

class Promise;
class TVChannel;
class TVTuner;

class TVSource MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TVSource, DOMEventTargetHelper)

  explicit TVSource(nsPIDOMWindow* aWindow);

  // WebIDL (internal functions)

  virtual JSObject* WrapObject(JSContext *aCx) MOZ_OVERRIDE;

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
  ~TVSource();

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVSource_h__
