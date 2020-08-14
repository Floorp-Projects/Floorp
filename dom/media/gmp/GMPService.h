/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPService_h_
#define GMPService_h_

#include "ChromiumCDMParent.h"
#include "GMPContentParent.h"
#include "GMPCrashHelper.h"
#include "MediaResult.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/Document.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsIThread.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

template <class>
struct already_AddRefed;

namespace mozilla {

class GMPCrashHelper;

extern LogModule* GetGMPLog();

namespace gmp {

struct NodeId {
  NodeId(const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
         const nsAString& aGMPName)
      : mOrigin(aOrigin),
        mTopLevelOrigin(aTopLevelOrigin),
        mGMPName(aGMPName) {}
  nsString mOrigin;
  nsString mTopLevelOrigin;
  nsString mGMPName;
};

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

  RefPtr<GetCDMParentPromise> GetCDM(const NodeId& aNodeId,
                                     nsTArray<nsCString> aTags,
                                     GMPCrashHelper* aHelper);

  // mozIGeckoMediaPluginService
  NS_IMETHOD GetThread(nsIThread** aThread) override;
  NS_IMETHOD GetDecryptingGMPVideoDecoder(
      GMPCrashHelper* aHelper, nsTArray<nsCString>* aTags,
      const nsACString& aNodeId,
      UniquePtr<GetGMPVideoDecoderCallback>&& aCallback,
      uint32_t aDecryptorId) override;
  NS_IMETHOD GetGMPVideoEncoder(
      GMPCrashHelper* aHelper, nsTArray<nsCString>* aTags,
      const nsACString& aNodeId,
      UniquePtr<GetGMPVideoEncoderCallback>&& aCallback) override;

  // Helper for backwards compatibility with WebRTC/tests.
  NS_IMETHOD
  GetGMPVideoDecoder(
      GMPCrashHelper* aHelper, nsTArray<nsCString>* aTags,
      const nsACString& aNodeId,
      UniquePtr<GetGMPVideoDecoderCallback>&& aCallback) override {
    return GetDecryptingGMPVideoDecoder(aHelper, aTags, aNodeId,
                                        std::move(aCallback), 0);
  }

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
      GMPCrashHelper* aHelper, const nsACString& aNodeIdString,
      const nsCString& aAPI, const nsTArray<nsCString>& aTags) = 0;

  virtual RefPtr<GetGMPContentParentPromise> GetContentParent(
      GMPCrashHelper* aHelper, const NodeId& aNodeId, const nsCString& aAPI,
      const nsTArray<nsCString>& aTags) = 0;

  nsresult GMPDispatch(nsIRunnable* event, uint32_t flags = NS_DISPATCH_NORMAL);
  nsresult GMPDispatch(already_AddRefed<nsIRunnable> event,
                       uint32_t flags = NS_DISPATCH_NORMAL);
  void ShutdownGMPThread();

  Mutex mMutex;  // Protects mGMPThread, mPluginCrashHelpers,
                 // mGMPThreadShutdown and some members in derived classes.
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
