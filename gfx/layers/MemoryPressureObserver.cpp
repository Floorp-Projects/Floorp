/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryPressureObserver.h"

#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsDependentString.h"
#include "nsIObserverService.h"
#include "nsLiteralString.h"

namespace mozilla {
namespace layers {

MemoryPressureObserver::MemoryPressureObserver(
    MemoryPressureListener* aListener)
    : mListener(aListener) {}

MemoryPressureObserver::~MemoryPressureObserver() {
  // If this assertion is hit we probably forgot to unregister the observer.
  MOZ_ASSERT(!mListener);
}

already_AddRefed<MemoryPressureObserver> MemoryPressureObserver::Create(
    MemoryPressureListener* aListener) {
  nsCOMPtr<nsIObserverService> service = services::GetObserverService();

  if (!service) {
    return nullptr;
  }

  RefPtr<MemoryPressureObserver> observer =
      new MemoryPressureObserver(aListener);

  bool useWeakRef = false;
  service->AddObserver(observer, "memory-pressure", useWeakRef);

  return observer.forget();
}

void MemoryPressureObserver::Unregister() {
  if (!mListener) {
    return;
  }

  nsCOMPtr<nsIObserverService> service = services::GetObserverService();
  if (service) {
    service->RemoveObserver(this, "memory-pressure");
  }

  mListener = nullptr;
}

NS_IMETHODIMP
MemoryPressureObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  if (mListener && strcmp(aTopic, "memory-pressure") == 0) {
    MemoryPressureReason reason = MemoryPressureReason::LOW_MEMORY;
    auto reason_string = nsDependentString(aData);
    if (StringBeginsWith(reason_string,
                         NS_LITERAL_STRING("low-memory-ongoing"))) {
      reason = MemoryPressureReason::LOW_MEMORY_ONGOING;
    } else if (StringBeginsWith(reason_string,
                                NS_LITERAL_STRING("heap-minimize"))) {
      reason = MemoryPressureReason::HEAP_MINIMIZE;
    }
    mListener->OnMemoryPressure(reason);
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(MemoryPressureObserver, nsIObserver)

}  // namespace layers
}  // namespace mozilla
