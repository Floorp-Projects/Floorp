/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Sean Echevarria <sean@beatnik.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Josh Aas <josh@mozilla.com>
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

class nsIChannel;

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
public nsINPAPIPluginStreamInfo,
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

  // nsINPAPIPluginStreamInfo interface
  NS_DECL_NSIPLUGINSTREAMINFO
  
  // Called by RequestRead
  void
  MakeByteRangeString(NPByteRange* aRangeList, nsACString &string, PRInt32 *numRequests);
  
  bool UseExistingPluginCacheFile(nsPluginStreamListenerPeer* psi);
  
  // Called by GetURL and PostURL (via NewStream)
  nsresult Initialize(nsIURI *aURL,
                      nsNPAPIPluginInstance *aInstance,
                      nsIPluginStreamListener *aListener);
  
  nsresult InitializeEmbedded(nsIURI *aURL,
                              nsNPAPIPluginInstance* aInstance,
                              nsIPluginInstanceOwner *aOwner = nsnull);
  
  nsresult InitializeFullPage(nsIURI* aURL, nsNPAPIPluginInstance *aInstance);

  nsresult OnFileAvailable(nsIFile* aFile);
  
  nsresult ServeStreamAsFile(nsIRequest *request, nsISupports *ctxt);
  
  nsNPAPIPluginInstance *GetPluginInstance() { return mPluginInstance; }
  
private:
  nsresult SetUpStreamListener(nsIRequest* request, nsIURI* aURL);
  nsresult SetupPluginCacheFile(nsIChannel* channel);
  nsresult GetInterfaceGlobal(const nsIID& aIID, void** result);

  nsCOMPtr<nsIURI> mURL;
  nsCString mURLSpec; // Have to keep this member because GetURL hands out char*
  nsCOMPtr<nsIPluginInstanceOwner> mOwner;
  nsRefPtr<nsNPAPIPluginStreamListener> mPStreamListener;

  // Set to PR_TRUE if we request failed (like with a HTTP response of 404)
  bool                    mRequestFailed;
  
  /*
   * Set to PR_TRUE after nsIPluginStreamListener::OnStartBinding() has
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
};

#endif // nsPluginStreamListenerPeer_h_
