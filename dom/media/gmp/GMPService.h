/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPService_h_
#define GMPService_h_

#include "GMPContentParent.h"
#include "GMPCrashHelper.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/Atomics.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/MozPromise.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIAsyncShutdownClient;
class nsIRunnable;
class nsISerialEventTarget;
class nsIThread;

template <class>
struct already_AddRefed;

namespace mozilla {

class GMPCrashHelper;
class MediaResult;

extern LogModule* GetGMPLog();

namespace gmp {

typedef MozPromise<RefPtr<GMPContentParent::CloseBlocker>, MediaResult,
                   /* IsExclusive = */ true>
    GetGMPContentParentPromise;
typedef MozPromise<RefPtr<ChromiumCDMParent>, MediaResult,
                   /* IsExclusive = */ true>
    GetCDMParentPromise;

class GeckoMediaPluginService : public mozIGeckoMediaPluginService,
                                public nsIObserver {
 public:
  static already_AddRefed<GeckoMediaPluginService> GetGeckoMediaPluginService();

  virtual nsresult Init();

  NS_DECL_THREADSAFE_ISUPPORTS

  RefPtr<GetCDMParentPromise> GetCDM(const NodeIdParts& aNodeIdParts,
                                     nsTArray<nsCString> aTags,
                                     GMPCrashHelper* aHelper);

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
  RefPtr<GetGMPContentParentPromise> GetContentParentForTest();
#endif

  // mozIGeckoMediaPluginService
  NS_IMETHOD GetThread(nsIThread** aThread) override;
  NS_IMETHOD GetGMPVideoDecoder(
      GMPCrashHelper* aHelper, nsTArray<nsCString>* aTags,
      const nsACString& aNodeId,
      UniquePtr<GetGMPVideoDecoderCallback>&& aCallback) override;
  NS_IMETHOD GetGMPVideoEncoder(
      GMPCrashHelper* aHelper, nsTArray<nsCString>* aTags,
      const nsACString& aNodeId,
      UniquePtr<GetGMPVideoEncoderCallback>&& aCallback) override;

  NS_IMETHOD RunPluginCrashCallbacks(uint32_t aPluginId,
                                     const nsACString& aPluginName) override;

  already_AddRefed<nsISerialEventTarget> GetGMPThread();

  void ConnectCrashHelper(uint32_t aPluginId, GMPCrashHelper* aHelper);
  void DisconnectCrashHelper(GMPCrashHelper* aHelper);

  bool XPCOMWillShutdownReceived() const { return mXPCOMWillShutdown; }

 protected:
  GeckoMediaPluginService();
  virtual ~GeckoMediaPluginService();

  virtual void InitializePlugins(nsISerialEventTarget* aGMPThread) = 0;

  virtual RefPtr<GetGMPContentParentPromise> GetContentParent(
      GMPCrashHelper* aHelper, const NodeIdVariant& aNodeIdVariant,
      const nsCString& aAPI, const nsTArray<nsCString>& aTags) = 0;

  nsresult GMPDispatch(nsIRunnable* event, uint32_t flags = NS_DISPATCH_NORMAL);
  nsresult GMPDispatch(already_AddRefed<nsIRunnable> event,
                       uint32_t flags = NS_DISPATCH_NORMAL);
  void ShutdownGMPThread();

  static nsCOMPtr<nsIAsyncShutdownClient> GetShutdownBarrier();

  Mutex mMutex;  // Protects mGMPThread, mPluginCrashHelpers,
                 // mGMPThreadShutdown and some members in derived classes.

  const nsCOMPtr<nsISerialEventTarget> mMainThread;

  nsCOMPtr<nsIThread> mGMPThread;
  bool mGMPThreadShutdown;
  bool mShuttingDownOnGMPThread;
  Atomic<bool> mXPCOMWillShutdown;

  nsClassHashtable<nsUint32HashKey, nsTArray<RefPtr<GMPCrashHelper>>>
      mPluginCrashHelpers;
};

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPService_h_
