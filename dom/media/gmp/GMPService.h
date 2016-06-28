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
#include "nsClassHashtable.h"

template <class> struct already_AddRefed;

// For every GMP actor requested, the caller can specify a crash helper,
// which is an object which supplies the nsPIDOMWindowInner to which we'll
// dispatch the PluginCrashed event if the GMP crashes.
class GMPCrashHelper
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPCrashHelper)
  virtual already_AddRefed<nsPIDOMWindowInner> GetPluginCrashedEventTarget() = 0;
protected:
  virtual ~GMPCrashHelper() {}
};

namespace mozilla {

extern LogModule* GetGMPLog();

namespace gmp {

class GetGMPContentParentCallback;

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
  NS_IMETHOD GetGMPVideoDecoder(GMPCrashHelper* aHelper,
                                nsTArray<nsCString>* aTags,
                                const nsACString& aNodeId,
                                UniquePtr<GetGMPVideoDecoderCallback>&& aCallback)
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

  int32_t AsyncShutdownTimeoutMs();

  NS_IMETHOD RunPluginCrashCallbacks(uint32_t aPluginId,
                                     const nsACString& aPluginName) override;

  // Sets the window to which 'PluginCrashed' chromeonly event is dispatched.
  // Note: if the plugin has crashed before the target window has been set,
  // the 'PluginCrashed' event is dispatched as soon as a target window is set.
  void AddPluginCrashedEventTarget(const uint32_t aPluginId,
                                   nsPIDOMWindowInner* aParentWindow);

  RefPtr<AbstractThread> GetAbstractGMPThread();

  void ConnectCrashHelper(uint32_t aPluginId, GMPCrashHelper* aHelper);
  void DisconnectCrashHelper(GMPCrashHelper* aHelper);

protected:
  GeckoMediaPluginService();
  virtual ~GeckoMediaPluginService();

  void RemoveObsoletePluginCrashCallbacks(); // Called from add/run.

  virtual void InitializePlugins(AbstractThread* aAbstractGMPThread) = 0;
  virtual bool GetContentParentFrom(GMPCrashHelper* aHelper,
                                    const nsACString& aNodeId,
                                    const nsCString& aAPI,
                                    const nsTArray<nsCString>& aTags,
                                    UniquePtr<GetGMPContentParentCallback>&& aCallback) = 0;

  nsresult GMPDispatch(nsIRunnable* event, uint32_t flags = NS_DISPATCH_NORMAL);
  nsresult GMPDispatch(already_AddRefed<nsIRunnable> event, uint32_t flags = NS_DISPATCH_NORMAL);
  void ShutdownGMPThread();

  Mutex mMutex; // Protects mGMPThread, mAbstractGMPThread, mPluginCrashHelpers,
                // mGMPThreadShutdown and some members in derived classes.
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

  nsClassHashtable<nsUint32HashKey, nsTArray<RefPtr<GMPCrashHelper>>> mPluginCrashHelpers;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPService_h_
