/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsNPAPIPluginStreamListener_h_
#define nsNPAPIPluginStreamListener_h_

#include "nscore.h"
#include "nsIPluginStreamListener.h"
#include "nsIPluginStreamInfo.h"
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

class nsINPAPIPluginStreamInfo;
class nsPluginStreamListenerPeer;

// nsINPAPIPluginStreamInfo is an internal helper interface that exposes
// the underlying necko request to consumers of nsIPluginStreamInfo's.
#define NS_INPAPIPLUGINSTREAMINFO_IID       \
{ 0x097fdaaa, 0xa2a3, 0x49c2, \
{0x91, 0xee, 0xeb, 0xc5, 0x7d, 0x6c, 0x9c, 0x97} }

class nsINPAPIPluginStreamInfo : public nsIPluginStreamInfo
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INPAPIPLUGINSTREAMINFO_IID)

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

protected:
  friend class nsPluginByteRangeStreamListener;
  
  nsCOMArray<nsIRequest> mRequests;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINPAPIPluginStreamInfo,
                              NS_INPAPIPLUGINSTREAMINFO_IID)

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
  nsCOMPtr<nsILocalFile> mTempFile;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsIPluginInstanceOwner* mOwner;
};

class nsNPAPIPluginStreamListener : public nsIPluginStreamListener,
                                    public nsITimerCallback,
                                    public nsIHTTPHeaderListener
{
private:
  typedef mozilla::PluginLibrary PluginLibrary;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINSTREAMLISTENER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIHTTPHEADERLISTENER

  nsNPAPIPluginStreamListener(nsNPAPIPluginInstance* inst, void* notifyData,
                              const char* aURL);
  virtual ~nsNPAPIPluginStreamListener();

  bool IsStarted();
  nsresult CleanUpStream(NPReason reason);
  void CallURLNotify(NPReason reason);
  void SetCallNotify(bool aCallNotify) { mCallNotify = aCallNotify; }
  void SuspendRequest();
  void ResumeRequest();
  nsresult StartDataPump();
  void StopDataPump();
  bool PluginInitJSLoadInProgress();

  void* GetNotifyData() { return mNPStream.notifyData; }
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
  nsPluginStreamListenerPeer* mStreamListenerPeer;
  NPStream mNPStream;
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
  nsCOMPtr<nsIPluginStreamInfo> mStreamInfo;
};

#endif // nsNPAPIPluginStreamListener_h_
