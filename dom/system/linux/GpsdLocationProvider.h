/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GpsdLocationProvider_h
#define GpsdLocationProvider_h

#include "nsCOMPtr.h"
#include "nsGeolocation.h"
#include "nsIGeolocationProvider.h"

class MLSFallback;

namespace mozilla {

class LazyIdleThread;

namespace dom {

class GpsdLocationProvider final : public nsIGeolocationProvider
{
  class MLSGeolocationUpdate;
  class NotifyErrorRunnable;
  class PollRunnable;
  class UpdateRunnable;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER

  GpsdLocationProvider();

private:
  ~GpsdLocationProvider();

  void Update(nsIDOMGeoPosition* aPosition);
  void NotifyError(int aError);

  static const uint32_t GPSD_POLL_THREAD_TIMEOUT_MS;

  nsCOMPtr<nsIGeolocationUpdate> mCallback;
  RefPtr<LazyIdleThread> mPollThread;
  RefPtr<PollRunnable> mPollRunnable;
  RefPtr<MLSFallback> mMLSProvider;
};

} // namespace dom
} // namespace mozilla

#endif /* GpsLocationProvider_h */
