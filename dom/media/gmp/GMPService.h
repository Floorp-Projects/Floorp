/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPService_h_
#define GMPService_h_

#include "nsString.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsIDocument.h"
#include "nsIWeakReference.h"
#include "mozilla/AbstractThread.h"
#include "nsClassHashtable.h"
#include "nsISupportsImpl.h"
#include "mozilla/MozPromise.h"

template <class> struct already_AddRefed;

namespace mozilla {

class GMPCrashHelper;

extern LogModule* GetGMPLog();

namespace gmp {

class GMPContentParent;
typedef MozPromise<RefPtr<GMPContentParent>, nsresult, /* IsExclusive = */ true> GetGMPContentParentPromise;

class GeckoMediaPluginService : public mozIGeckoMediaPluginService
                              , public nsIObserver
{
public:
  static already_AddRefed<GeckoMediaPluginService> GetGeckoMediaPluginService();

  virtual nsresult Init();

  NS_DECL_THREADSAFE_ISUPPORTS

  // mozIGeckoMediaPluginService
  NS_IMETHOD GetThread(nsIThread** aThread) override;
  NS_IMETHOD GetDecryptingGMPVideoDecoder(GMPCrashHelper* aHelper,
                                          nsTArray<nsCString>* aTags,
                                          const nsACString& aNodeId,
                                          UniquePtr<GetGMPVideoDecoderCallback>&& aCallback,
                                          uint32_t aDecryptorId)
    override;
  NS_IMETHOD GetGMPVideoEncoder(GMPCrashHelper* aHelper,
                                nsTArray<nsCString>* aTags,
                                const nsACString& aNodeId,
                                UniquePtr<GetGMPVideoEncoderCallback>&& aCallback)
    override;
  NS_IMETHOD GetGMPAudioDecoder(GMPCrashHelper* aHelper,
                                nsTArray<nsCString>* aTags,
                                const nsACString& aNodeId,
                                UniquePtr<GetGMPAudioDecoderCallback>&& aCallback)
    override;
  NS_IMETHOD GetGMPDecryptor(GMPCrashHelper* aHelper,
                             nsTArray<nsCString>* aTags,
                             const nsACString& aNodeId,
                             UniquePtr<GetGMPDecryptorCallback>&& aCallback)
    override;

  // Helper for backwards compatibility with WebRTC/tests.
  NS_IMETHOD
  GetGMPVideoDecoder(GMPCrashHelper* aHelper,
                     nsTArray<nsCString>* aTags,
                     const nsACString& aNodeId,
                     UniquePtr<GetGMPVideoDecoderCallback>&& aCallback) override
  {
    return GetDecryptingGMPVideoDecoder(aHelper, aTags, aNodeId, Move(aCallback), 0);
  }

  int32_t AsyncShutdownTimeoutMs();

  NS_IMETHOD RunPluginCrashCallbacks(uint32_t aPluginId,
                                     const nsACString& aPluginName) override;

  RefPtr<AbstractThread> GetAbstractGMPThread();

  void ConnectCrashHelper(uint32_t aPluginId, GMPCrashHelper* aHelper);
  void DisconnectCrashHelper(GMPCrashHelper* aHelper);

protected:
  GeckoMediaPluginService();
  virtual ~GeckoMediaPluginService();

  virtual void InitializePlugins(AbstractThread* aAbstractGMPThread) = 0;

  virtual RefPtr<GetGMPContentParentPromise>
  GetContentParent(GMPCrashHelper* aHelper,
                   const nsACString& aNodeId,
                   const nsCString& aAPI,
                   const nsTArray<nsCString>& aTags) = 0;

  nsresult GMPDispatch(nsIRunnable* event, uint32_t flags = NS_DISPATCH_NORMAL);
  nsresult GMPDispatch(already_AddRefed<nsIRunnable> event, uint32_t flags = NS_DISPATCH_NORMAL);
  void ShutdownGMPThread();

  Mutex mMutex; // Protects mGMPThread, mAbstractGMPThread, mPluginCrashHelpers,
                // mGMPThreadShutdown and some members in derived classes.
  nsCOMPtr<nsIThread> mGMPThread;
  RefPtr<AbstractThread> mAbstractGMPThread;
  bool mGMPThreadShutdown;
  bool mShuttingDownOnGMPThread;

  nsClassHashtable<nsUint32HashKey, nsTArray<RefPtr<GMPCrashHelper>>> mPluginCrashHelpers;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPService_h_
