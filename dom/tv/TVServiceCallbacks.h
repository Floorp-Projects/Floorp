/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVServiceCallbacks_h
#define mozilla_dom_TVServiceCallbacks_h

#include "nsITVService.h"
// Include TVSourceBinding.h since enum TVSourceType can't be forward declared.
#include "mozilla/dom/TVSourceBinding.h"

namespace mozilla {
namespace dom {

class Promise;
class TVChannel;
class TVManager;
class TVTuner;
class TVSource;

class TVServiceSourceSetterCallback MOZ_FINAL : public nsITVServiceCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITVSERVICECALLBACK
  NS_DECL_CYCLE_COLLECTION_CLASS(TVServiceSourceSetterCallback)

  TVServiceSourceSetterCallback(TVTuner* aTuner,
                                Promise* aPromise,
                                TVSourceType aSourceType);

private:
  ~TVServiceSourceSetterCallback();

  nsRefPtr<TVTuner> mTuner;
  nsRefPtr<Promise> mPromise;
  TVSourceType mSourceType;
};

class TVServiceChannelScanCallback MOZ_FINAL : public nsITVServiceCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITVSERVICECALLBACK
  NS_DECL_CYCLE_COLLECTION_CLASS(TVServiceChannelScanCallback)

  TVServiceChannelScanCallback(TVSource* aSource,
                               Promise* aPromise,
                               bool aIsScanning);

private:
  ~TVServiceChannelScanCallback();

  nsRefPtr<TVSource> mSource;
  nsRefPtr<Promise> mPromise;
  bool mIsScanning;
};

class TVServiceChannelSetterCallback MOZ_FINAL : public nsITVServiceCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITVSERVICECALLBACK
  NS_DECL_CYCLE_COLLECTION_CLASS(TVServiceChannelSetterCallback)

  TVServiceChannelSetterCallback(TVSource* aSource,
                                 Promise* aPromise,
                                 const nsAString& aChannelNumber);

private:
  ~TVServiceChannelSetterCallback();

  nsRefPtr<TVSource> mSource;
  nsRefPtr<Promise> mPromise;
  nsString mChannelNumber;
};

class TVServiceTunerGetterCallback MOZ_FINAL : public nsITVServiceCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITVSERVICECALLBACK
  NS_DECL_CYCLE_COLLECTION_CLASS(TVServiceTunerGetterCallback)

  explicit TVServiceTunerGetterCallback(TVManager* aManager);

private:
  ~TVServiceTunerGetterCallback();

  nsRefPtr<TVManager> mManager;
};

class TVServiceChannelGetterCallback MOZ_FINAL : public nsITVServiceCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITVSERVICECALLBACK
  NS_DECL_CYCLE_COLLECTION_CLASS(TVServiceChannelGetterCallback)

  TVServiceChannelGetterCallback(TVSource* aSource,
                                 Promise* aPromise);

private:
  ~TVServiceChannelGetterCallback();

  nsRefPtr<TVSource> mSource;
  nsRefPtr<Promise> mPromise;
};

class TVServiceProgramGetterCallback MOZ_FINAL : public nsITVServiceCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITVSERVICECALLBACK
  NS_DECL_CYCLE_COLLECTION_CLASS(TVServiceProgramGetterCallback)

  // |aIsSingular| is set when the promise is expected to be resolved as a
  // TVProgram for |TVChannel::GetCurrentProgram()|, instead of a sequence of
  // TVProgram for |TVChannel::GetPrograms()|.
  TVServiceProgramGetterCallback(TVChannel* aChannel,
                                 Promise* aPromise,
                                 bool aIsSingular);

private:
  ~TVServiceProgramGetterCallback();

  nsRefPtr<TVChannel> mChannel;
  nsRefPtr<Promise> mPromise;
  bool mIsSingular;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVServiceCallbacks_h
