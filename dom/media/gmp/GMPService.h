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

template <class> struct already_AddRefed;

namespace mozilla {

extern PRLogModuleInfo* GetGMPLog();

namespace gmp {

class GetGMPContentParentCallback;

#define GMP_DEFAULT_ASYNC_SHUTDONW_TIMEOUT 3000

class GeckoMediaPluginService : public mozIGeckoMediaPluginService
                              , public nsIObserver
{
public:
  static already_AddRefed<GeckoMediaPluginService> GetGeckoMediaPluginService();

  virtual nsresult Init();

  NS_DECL_THREADSAFE_ISUPPORTS

  // mozIGeckoMediaPluginService
  NS_IMETHOD GetThread(nsIThread** aThread) override;
  NS_IMETHOD HasPluginForAPI(const nsACString& aAPI, nsTArray<nsCString>* aTags,
                             bool *aRetVal) override;
  NS_IMETHOD GetGMPVideoDecoder(nsTArray<nsCString>* aTags,
                                const nsACString& aNodeId,
                                UniquePtr<GetGMPVideoDecoderCallback>&& aCallback)
    override;
  NS_IMETHOD GetGMPVideoEncoder(nsTArray<nsCString>* aTags,
                                const nsACString& aNodeId,
                                UniquePtr<GetGMPVideoEncoderCallback>&& aCallback)
    override;
  NS_IMETHOD GetGMPAudioDecoder(nsTArray<nsCString>* aTags,
                                const nsACString& aNodeId,
                                UniquePtr<GetGMPAudioDecoderCallback>&& aCallback)
    override;
  NS_IMETHOD GetGMPDecryptor(nsTArray<nsCString>* aTags,
                             const nsACString& aNodeId,
                             UniquePtr<GetGMPDecryptorCallback>&& aCallback)
    override;

  int32_t AsyncShutdownTimeoutMs();

  class PluginCrashCallback
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(PluginCrashCallback)

    PluginCrashCallback(const uint32_t aPluginId)
      : mPluginId(aPluginId)
    {
      MOZ_ASSERT(NS_IsMainThread());
    }
    const uint32_t PluginId() const { return mPluginId; }
    virtual void Run(const nsACString& aPluginName) = 0;
    virtual bool IsStillValid() = 0; // False if callback has become useless.
  protected:
    virtual ~PluginCrashCallback()
    {
      MOZ_ASSERT(NS_IsMainThread());
    }
  private:
    const uint32_t mPluginId;
  };
  void RemoveObsoletePluginCrashCallbacks(); // Called from add/remove/run.
  void AddPluginCrashCallback(nsRefPtr<PluginCrashCallback> aPluginCrashCallback);
  void RemovePluginCrashCallbacks(const uint32_t aPluginId);
  void RunPluginCrashCallbacks(const uint32_t aPluginId,
                               const nsACString& aPluginName);

protected:
  GeckoMediaPluginService();
  virtual ~GeckoMediaPluginService();

  virtual void InitializePlugins() = 0;
  virtual bool GetContentParentFrom(const nsACString& aNodeId,
                                    const nsCString& aAPI,
                                    const nsTArray<nsCString>& aTags,
                                    UniquePtr<GetGMPContentParentCallback>&& aCallback) = 0;

  nsresult GMPDispatch(nsIRunnable* event, uint32_t flags = NS_DISPATCH_NORMAL);
  void ShutdownGMPThread();

protected:
  Mutex mMutex; // Protects mGMPThread and mGMPThreadShutdown and some members
                // in derived classes.
  nsCOMPtr<nsIThread> mGMPThread;
  bool mGMPThreadShutdown;
  bool mShuttingDownOnGMPThread;

  nsTArray<nsRefPtr<PluginCrashCallback>> mPluginCrashCallbacks;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPService_h_
