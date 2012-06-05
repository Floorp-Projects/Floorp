/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginStreamListenerPeer_h_
#define nsPluginStreamListenerPeer_h_

#include "nscore.h"
#include "nsIFile.h"
#include "nsIStreamListener.h"
#include "nsIProgressEventSink.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsWeakReference.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsHashtable.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIObjectLoadingContent.h"

class nsIChannel;
class nsObjectLoadingContent;

/**
 * When a plugin requests opens multiple requests to the same URL and
 * the request must be satified by saving a file to disk, each stream
 * listener holds a reference to the backing file: the file is only removed
 * when all the listeners are done.
 */
class CachedFileHolder
{
public:
  CachedFileHolder(nsIFile* cacheFile);
  ~CachedFileHolder();
  
  void AddRef();
  void Release();
  
  nsIFile* file() const { return mFile; }
  
private:
  nsAutoRefCnt mRefCnt;
  nsCOMPtr<nsIFile> mFile;
};

class nsPluginStreamListenerPeer : public nsIStreamListener,
public nsIProgressEventSink,
public nsIHttpHeaderVisitor,
public nsSupportsWeakReference,
public nsIInterfaceRequestor,
public nsIChannelEventSink
{
public:
  nsPluginStreamListenerPeer();
  virtual ~nsPluginStreamListenerPeer();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROGRESSEVENTSINK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIHTTPHEADERVISITOR
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  // Called by RequestRead
  void
  MakeByteRangeString(NPByteRange* aRangeList, nsACString &string, PRInt32 *numRequests);
  
  bool UseExistingPluginCacheFile(nsPluginStreamListenerPeer* psi);
  
  // Called by GetURL and PostURL (via NewStream)
  nsresult Initialize(nsIURI *aURL,
                      nsNPAPIPluginInstance *aInstance,
                      nsNPAPIPluginStreamListener *aListener);
  
  nsresult InitializeEmbedded(nsIURI *aURL,
                              nsNPAPIPluginInstance* aInstance,
                              nsObjectLoadingContent *aContent);
  
  nsresult InitializeFullPage(nsIURI* aURL, nsNPAPIPluginInstance *aInstance);

  nsresult OnFileAvailable(nsIFile* aFile);
  
  nsresult ServeStreamAsFile(nsIRequest *request, nsISupports *ctxt);
  
  nsNPAPIPluginInstance *GetPluginInstance() { return mPluginInstance; }
  
  nsresult RequestRead(NPByteRange* rangeList);
  nsresult GetLength(PRUint32* result);
  nsresult GetURL(const char** result);
  nsresult GetLastModified(PRUint32* result);
  nsresult IsSeekable(bool* result);
  nsresult GetContentType(char** result);
  nsresult GetStreamOffset(PRInt32* result);
  nsresult SetStreamOffset(PRInt32 value);

  void TrackRequest(nsIRequest* request)
  {
    mRequests.AppendObject(request);
  }

  void ReplaceRequest(nsIRequest* oldRequest, nsIRequest* newRequest)
  {
    PRInt32 i = mRequests.IndexOfObject(oldRequest);
    if (i == -1) {
      NS_ASSERTION(mRequests.Count() == 0,
                   "Only our initial stream should be unknown!");
      mRequests.AppendObject(oldRequest);
    }
    else {
      mRequests.ReplaceObjectAt(newRequest, i);
    }
  }
  
  void CancelRequests(nsresult status)
  {
    // Copy the array to avoid modification during the loop.
    nsCOMArray<nsIRequest> requestsCopy(mRequests);
    for (PRInt32 i = 0; i < requestsCopy.Count(); ++i)
      requestsCopy[i]->Cancel(status);
  }

  void SuspendRequests() {
    nsCOMArray<nsIRequest> requestsCopy(mRequests);
    for (PRInt32 i = 0; i < requestsCopy.Count(); ++i)
      requestsCopy[i]->Suspend();
  }

  void ResumeRequests() {
    nsCOMArray<nsIRequest> requestsCopy(mRequests);
    for (PRInt32 i = 0; i < requestsCopy.Count(); ++i)
      requestsCopy[i]->Resume();
  }

private:
  nsresult SetUpStreamListener(nsIRequest* request, nsIURI* aURL);
  nsresult SetupPluginCacheFile(nsIChannel* channel);
  nsresult GetInterfaceGlobal(const nsIID& aIID, void** result);

  nsCOMPtr<nsIURI> mURL;
  nsCString mURLSpec; // Have to keep this member because GetURL hands out char*
  nsCOMPtr<nsIObjectLoadingContent> mContent;
  nsRefPtr<nsNPAPIPluginStreamListener> mPStreamListener;

  // Set to true if we request failed (like with a HTTP response of 404)
  bool                    mRequestFailed;
  
  /*
   * Set to true after nsNPAPIPluginStreamListener::OnStartBinding() has
   * been called.  Checked in ::OnStopRequest so we can call the
   * plugin's OnStartBinding if, for some reason, it has not already
   * been called.
   */
  bool              mStartBinding;
  bool              mHaveFiredOnStartRequest;
  // these get passed to the plugin stream listener
  PRUint32                mLength;
  PRInt32                 mStreamType;
  
  // local cached file, we save the content into local cache if browser cache is not available,
  // or plugin asks stream as file and it expects file extension until bug 90558 got fixed
  nsRefPtr<CachedFileHolder> mLocalCachedFileHolder;
  nsCOMPtr<nsIOutputStream> mFileCacheOutputStream;
  nsHashtable             *mDataForwardToRequest;
  
  nsCString mContentType;
  bool mSeekable;
  PRUint32 mModified;
  nsRefPtr<nsNPAPIPluginInstance> mPluginInstance;
  PRInt32 mStreamOffset;
  bool mStreamComplete;
  
public:
  bool                    mAbort;
  PRInt32                 mPendingRequests;
  nsWeakPtr               mWeakPtrChannelCallbacks;
  nsWeakPtr               mWeakPtrChannelLoadGroup;
  nsCOMArray<nsIRequest> mRequests;
};

#endif // nsPluginStreamListenerPeer_h_
