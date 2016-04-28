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
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIWeakReference.h"
#include "mozilla/AbstractThread.h"

template <class> struct already_AddRefed;

namespace mozilla {

extern LogModule* GetGMPLog();

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

  void RunPluginCrashCallbacks(const uint32_t aPluginId,
                               const nsACString& aPluginName);

  // Sets the window to which 'PluginCrashed' chromeonly event is dispatched.
  // Note: if the plugin has crashed before the target window has been set,
  // the 'PluginCrashed' event is dispatched as soon as a target window is set.
  void AddPluginCrashedEventTarget(const uint32_t aPluginId,
                                   nsPIDOMWindowInner* aParentWindow);

  RefPtr<AbstractThread> GetAbstractGMPThread();

protected:
  GeckoMediaPluginService();
  virtual ~GeckoMediaPluginService();

  void RemoveObsoletePluginCrashCallbacks(); // Called from add/run.

  virtual void InitializePlugins() = 0;
  virtual bool GetContentParentFrom(const nsACString& aNodeId,
                                    const nsCString& aAPI,
                                    const nsTArray<nsCString>& aTags,
                                    UniquePtr<GetGMPContentParentCallback>&& aCallback) = 0;

  nsresult GMPDispatch(nsIRunnable* event, uint32_t flags = NS_DISPATCH_NORMAL);
  nsresult GMPDispatch(already_AddRefed<nsIRunnable> event, uint32_t flags = NS_DISPATCH_NORMAL);
  void ShutdownGMPThread();

  Mutex mMutex; // Protects mGMPThread and mGMPThreadShutdown and some members
                // in derived classes.
  nsCOMPtr<nsIThread> mGMPThread;
  RefPtr<AbstractThread> mAbstractGMPThread;
  bool mGMPThreadShutdown;
  bool mShuttingDownOnGMPThread;

  class GMPCrashCallback
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(GMPCrashCallback)

    GMPCrashCallback(const uint32_t aPluginId,
                     nsPIDOMWindowInner* aParentWindow,
                     nsIDocument* aDocument);
    void Run(const nsACString& aPluginName);
    bool IsStillValid();
    uint32_t GetPluginId() const { return mPluginId; }
  private:
    virtual ~GMPCrashCallback() { MOZ_ASSERT(NS_IsMainThread()); }

    bool GetParentWindowAndDocumentIfValid(nsCOMPtr<nsPIDOMWindowInner>& parentWindow,
                                           nsCOMPtr<nsIDocument>& document);
    const uint32_t mPluginId;
    nsWeakPtr mParentWindowWeakPtr;
    nsWeakPtr mDocumentWeakPtr;
  };

  struct PluginCrash
  {
    PluginCrash(uint32_t aPluginId,
                const nsACString& aPluginName)
      : mPluginId(aPluginId)
      , mPluginName(aPluginName)
    {
    }
    uint32_t mPluginId;
    nsCString mPluginName;

    bool operator==(const PluginCrash& aOther) const {
      return mPluginId == aOther.mPluginId &&
             mPluginName == aOther.mPluginName;
    }
  };

  static const size_t MAX_PLUGIN_CRASHES = 100;
  nsTArray<PluginCrash> mPluginCrashes;

  nsTArray<RefPtr<GMPCrashCallback>> mPluginCrashCallbacks;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPService_h_
