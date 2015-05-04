/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsITimer.h"

class nsIGeolocationUpdate;
class nsIGeolocationProvider;

/*
 This class wraps the NetworkGeolocationProvider in a delayed startup.
 It is for providing a fallback to MLS when:
 1) using another provider as the primary provider, and
 2) that primary provider may fail to return a result (i.e. the error returned
 is indeterminate, or no error callback occurs)

 The intent is that the primary provider is started, then MLSFallback
 is started with sufficient delay that the primary provider will respond first
 if successful (in the majority of cases).

 MLS has an average response of 3s, so with the 2s default delay, a response can
 be expected in 5s.

 Telemetry is recommended to monitor that the primary provider is responding
 first when expected to do so.
*/
class MLSFallback : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  explicit MLSFallback(uint32_t delayMs = 2000);
  nsresult Startup(nsIGeolocationUpdate* aWatcher);
  nsresult Shutdown();

private:
  nsresult CreateMLSFallbackProvider();
  virtual ~MLSFallback();
  nsCOMPtr<nsITimer> mHandoffTimer;
  nsCOMPtr<nsIGeolocationProvider> mMLSFallbackProvider;
  nsCOMPtr<nsIGeolocationUpdate> mUpdateWatcher;
  const uint32_t mDelayMs;
};

