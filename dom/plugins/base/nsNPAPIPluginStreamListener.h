/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNPAPIPluginStreamListener_h_
#define nsNPAPIPluginStreamListener_h_

#include "nscore.h"
#include "nsIHTTPHeaderListener.h"
#include "nsIRequest.h"
#include "nsITimer.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsIOutputStream.h"
#include "nsIPluginInstanceOwner.h"
#include "nsString.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/PluginLibrary.h"

#define MAX_PLUGIN_NECKO_BUFFER 16384

class nsPluginStreamListenerPeer;
class nsNPAPIPluginStreamListener;

class nsNPAPIStreamWrapper
{
public:
  nsNPAPIStreamWrapper(nsIOutputStream *outputStream,
                       nsNPAPIPluginStreamListener *streamListener);
  ~nsNPAPIStreamWrapper();

  nsIOutputStream* GetOutputStream() { return mOutputStream.get(); }
  nsNPAPIPluginStreamListener* GetStreamListener() { return mStreamListener; }

  NPStream                              mNPStream;
protected:
  nsCOMPtr<nsIOutputStream>             mOutputStream; // only valid if not browser initiated
  nsNPAPIPluginStreamListener*          mStreamListener; // only valid if browser initiated
};

// Used to handle NPN_NewStream() - writes the stream as received by the plugin
// to a file and at completion (NPN_DestroyStream), tells the browser to load it into
// a plugin-specified target
class nsPluginStreamToFile : public nsIOutputStream
{
public:
  nsPluginStreamToFile(const char* target, nsIPluginInstanceOwner* owner);
  virtual ~nsPluginStreamToFile();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
protected:
  char* mTarget;
  nsCString mFileURL;
  nsCOMPtr<nsIFile> mTempFile;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsIPluginInstanceOwner* mOwner;
};

class nsNPAPIPluginStreamListener : public nsITimerCallback,
                                    public nsIHTTPHeaderListener
{
private:
  typedef mozilla::PluginLibrary PluginLibrary;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIHTTPHEADERLISTENER

  nsNPAPIPluginStreamListener(nsNPAPIPluginInstance* inst, void* notifyData,
                              const char* aURL);
  virtual ~nsNPAPIPluginStreamListener();

  nsresult OnStartBinding(nsPluginStreamListenerPeer* streamPeer);
  nsresult OnDataAvailable(nsPluginStreamListenerPeer* streamPeer,
                           nsIInputStream* input,
                           PRUint32 length);
  nsresult OnFileAvailable(nsPluginStreamListenerPeer* streamPeer, 
                           const char* fileName);
  nsresult OnStopBinding(nsPluginStreamListenerPeer* streamPeer, 
                         nsresult status);
  nsresult GetStreamType(PRInt32 *result);

  bool IsStarted();
  nsresult CleanUpStream(NPReason reason);
  void CallURLNotify(NPReason reason);
  void SetCallNotify(bool aCallNotify) { mCallNotify = aCallNotify; }
  void SuspendRequest();
  void ResumeRequest();
  nsresult StartDataPump();
  void StopDataPump();
  bool PluginInitJSLoadInProgress();

  void* GetNotifyData();
  nsPluginStreamListenerPeer* GetStreamListenerPeer() { return mStreamListenerPeer; }
  void SetStreamListenerPeer(nsPluginStreamListenerPeer* aPeer) { mStreamListenerPeer = aPeer; }

  // Returns true if the redirect will be handled by NPAPI, false otherwise.
  bool HandleRedirectNotification(nsIChannel *oldChannel, nsIChannel *newChannel,
                                  nsIAsyncVerifyRedirectCallback* callback);
  void URLRedirectResponse(NPBool allow);

protected:
  char* mStreamBuffer;
  char* mNotifyURL;
  nsRefPtr<nsNPAPIPluginInstance> mInst;
  nsNPAPIStreamWrapper *mNPStreamWrapper;
  PRUint32 mStreamBufferSize;
  PRInt32 mStreamBufferByteCount;
  PRInt32 mStreamType;
  bool mStreamStarted;
  bool mStreamCleanedUp;
  bool mCallNotify;
  bool mIsSuspended;
  bool mIsPluginInitJSStream;
  bool mRedirectDenied;
  nsCString mResponseHeaders;
  char* mResponseHeaderBuf;
  nsCOMPtr<nsITimer> mDataPumpTimer;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mHTTPRedirectCallback;

public:
  nsRefPtr<nsPluginStreamListenerPeer> mStreamListenerPeer;
};

#endif // nsNPAPIPluginStreamListener_h_
