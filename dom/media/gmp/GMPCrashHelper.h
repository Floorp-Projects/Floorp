/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GMPCrashHelper_h_)
#define GMPCrashHelper_h_

#include "nsPIDOMWindow.h"
#include "nsISupportsImpl.h"

namespace mozilla {

// For every GMP actor requested, the caller can specify a crash helper,
// which is an object which supplies the nsPIDOMWindowInner to which we'll
// dispatch the PluginCrashed event if the GMP crashes.
// GMPCrashHelper has threadsafe refcounting. Its release method ensures
// that instances are destroyed on the main thread.
class GMPCrashHelper
{
public:
  NS_METHOD_(MozExternalRefCountType) AddRef(void);
  NS_METHOD_(MozExternalRefCountType) Release(void);

  // Called on the main thread.
  virtual already_AddRefed<nsPIDOMWindowInner> GetPluginCrashedEventTarget() = 0;

protected:
  virtual ~GMPCrashHelper()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }
  void Destroy();
  mozilla::ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

} // namespace mozilla

#endif // GMPCrashHelper_h_
