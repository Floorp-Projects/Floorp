/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include <cutils/properties.h>
#include <stdio.h>
#include <string.h>

#include "HalLog.h"
#include "nsITimer.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace hal_impl {

static const int sRetryInterval = 100; // ms

bool
SystemServiceIsRunning(const char* aSvcName)
{
  MOZ_ASSERT(NS_IsMainThread());

  char key[PROPERTY_KEY_MAX];
  auto res = snprintf(key, sizeof(key), "init.svc.%s", aSvcName);

  if (res < 0) {
    HAL_ERR("snprintf: %s", strerror(errno));
    return false;
  } else if (static_cast<size_t>(res) >= sizeof(key)) {
    HAL_ERR("snprintf: trunctated service name %s", aSvcName);
    return false;
  }

  char value[PROPERTY_VALUE_MAX];
  NS_WARN_IF(property_get(key, value, "") < 0);

  return !strcmp(value, "running");
}

class StartSystemServiceTimerCallback final : public nsITimerCallback
{
  NS_DECL_THREADSAFE_ISUPPORTS;

public:
  StartSystemServiceTimerCallback(const char* aSvcName, const char* aArgs)
    : mSvcName(aSvcName)
    , mArgs(aArgs)
  {
    MOZ_COUNT_CTOR_INHERITED(StartSystemServiceTimerCallback,
                             nsITimerCallback);
  }

  NS_IMETHOD Notify(nsITimer* aTimer) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    return StartSystemService(mSvcName.get(), mArgs.get());
  }

protected:
  ~StartSystemServiceTimerCallback()
  {
    MOZ_COUNT_DTOR_INHERITED(StartSystemServiceTimerCallback,
                             nsITimerCallback);
  }

private:
  nsCString mSvcName;
  nsCString mArgs;
};

NS_IMPL_ISUPPORTS0(StartSystemServiceTimerCallback);

nsresult
StartSystemService(const char* aSvcName, const char* aArgs)
{
  MOZ_ASSERT(NS_IsMainThread());

  char value[PROPERTY_VALUE_MAX];
  auto res = snprintf(value, sizeof(value), "%s:%s", aSvcName, aArgs);

  if (res < 0) {
    HAL_ERR("snprintf: %s", strerror(errno));
    return NS_ERROR_FAILURE;
  } else if (static_cast<size_t>(res) >= sizeof(value)) {
    HAL_ERR("snprintf: trunctated service name %s", aSvcName);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_WARN_IF(property_set("ctl.start", value) < 0)) {
    return NS_ERROR_FAILURE;
  }

  /* If the system service is not running, re-try later to start it.
   *
   * This condition happens when we restart a service immediately
   * after it crashed, as the service state remains 'stopping'
   * instead of 'stopped'. Due to the limitation of property service,
   * hereby add delay. See Bug 1143925 Comment 41.
   */
  if (!SystemServiceIsRunning(aSvcName)) {
    nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
    if (!timer) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<StartSystemServiceTimerCallback> timerCallback =
      new StartSystemServiceTimerCallback(aSvcName, aArgs);

    timer->InitWithCallback(timerCallback,
                            sRetryInterval,
                            nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

void
StopSystemService(const char* aSvcName)
{
  MOZ_ASSERT(NS_IsMainThread());

  Unused << NS_WARN_IF(property_set("ctl.stop", aSvcName));
}

} // namespace hal_impl
} // namespace mozilla
