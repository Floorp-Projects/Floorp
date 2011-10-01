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

#include "nsNPAPIPluginStreamListener.h"
#include "plstr.h"
#include "prmem.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsPluginHost.h"
#include "nsNPAPIPlugin.h"
#include "nsPluginSafety.h"
#include "nsPluginLogging.h"
#include "nsPluginStreamListenerPeer.h"

NS_IMPL_ISUPPORTS1(nsPluginStreamToFile, nsIOutputStream)

nsPluginStreamToFile::nsPluginStreamToFile(const char* target,
                                           nsIPluginInstanceOwner* owner)
: mTarget(PL_strdup(target)),
mOwner(owner)
{
  nsresult rv;
  nsCOMPtr<nsIFile> pluginTmp;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(pluginTmp));
  if (NS_FAILED(rv)) return;
  
  mTempFile = do_QueryInterface(pluginTmp, &rv);
  if (NS_FAILED(rv)) return;
  
  // need to create a file with a unique name - use target as the basis
  rv = mTempFile->AppendNative(nsDependentCString(target));
  if (NS_FAILED(rv)) return;
  
  // Yes, make it unique.
  rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0700); 
  if (NS_FAILED(rv)) return;
  
  // create the file
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(mOutputStream), mTempFile, -1, 00600);
  if (NS_FAILED(rv))
    return;
	
  // construct the URL we'll use later in calls to GetURL()
  NS_GetURLSpecFromFile(mTempFile, mFileURL);
  
#ifdef NS_DEBUG
  printf("File URL = %s\n", mFileURL.get());
#endif
}

nsPluginStreamToFile::~nsPluginStreamToFile()
{
  // should we be deleting mTempFile here?
  if (nsnull != mTarget)
    PL_strfree(mTarget);
}

NS_IMETHODIMP
nsPluginStreamToFile::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::Write(const char* aBuf, PRUint32 aCount,
                            PRUint32 *aWriteCount)
{
  mOutputStream->Write(aBuf, aCount, aWriteCount);
  mOutputStream->Flush();
  mOwner->GetURL(mFileURL.get(), mTarget, nsnull, nsnull, 0);
  
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::WriteFrom(nsIInputStream *inStr, PRUint32 count,
                                PRUint32 *_retval)
{
  NS_NOTREACHED("WriteFrom");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::WriteSegments(nsReadSegmentFun reader, void * closure,
                                    PRUint32 count, PRUint32 *_retval)
{
  NS_NOTREACHED("WriteSegments");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::IsNonBlocking(bool *aNonBlocking)
{
  *aNonBlocking = false;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::Close(void)
{
  mOutputStream->Close();
  mOwner->GetURL(mFileURL.get(), mTarget, nsnull, nsnull, 0);
  return NS_OK;
}

// nsNPAPIPluginStreamListener Methods

NS_IMPL_ISUPPORTS3(nsNPAPIPluginStreamListener, nsIPluginStreamListener,
                   nsITimerCallback, nsIHTTPHeaderListener)

nsNPAPIPluginStreamListener::nsNPAPIPluginStreamListener(nsNPAPIPluginInstance* inst, 
                                                         void* notifyData,
                                                         const char* aURL)
: mStreamBuffer(nsnull),
mNotifyURL(aURL ? PL_strdup(aURL) : nsnull),
mInst(inst),
mStreamListenerPeer(nsnull),
mStreamBufferSize(0),
mStreamBufferByteCount(0),
mStreamType(NP_NORMAL),
mStreamStarted(false),
mStreamCleanedUp(false),
mCallNotify(notifyData ? true : false),
mIsSuspended(false),
mIsPluginInitJSStream(mInst->mInPluginInitCall &&
                      aURL && strncmp(aURL, "javascript:",
                                      sizeof("javascript:") - 1) == 0),
mRedirectDenied(false),
mResponseHeaderBuf(nsnull)
{
  memset(&mNPStream, 0, sizeof(mNPStream));
  mNPStream.notifyData = notifyData;
}

nsNPAPIPluginStreamListener::~nsNPAPIPluginStreamListener()
{
  // remove this from the plugin instance's stream list
  nsTArray<nsNPAPIPluginStreamListener*> *streamListeners = mInst->StreamListeners();
  streamListeners->RemoveElement(this);

  // For those cases when NewStream is never called, we still may need
  // to fire a notification callback. Return network error as fallback
  // reason because for other cases, notify should have already been
  // called for other reasons elsewhere.
  CallURLNotify(NPRES_NETWORK_ERR);
  
  // lets get rid of the buffer
  if (mStreamBuffer) {
    PR_Free(mStreamBuffer);
    mStreamBuffer=nsnull;
  }
  
  if (mNotifyURL)
    PL_strfree(mNotifyURL);
  
  if (mResponseHeaderBuf)
    PL_strfree(mResponseHeaderBuf);
}

nsresult
nsNPAPIPluginStreamListener::CleanUpStream(NPReason reason)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  if (mStreamCleanedUp)
    return NS_OK;
  
  mStreamCleanedUp = true;
  
  StopDataPump();

  // Release any outstanding redirect callback.
  if (mHTTPRedirectCallback) {
    mHTTPRedirectCallback->OnRedirectVerifyCallback(NS_ERROR_FAILURE);
    mHTTPRedirectCallback = nsnull;
  }

  // Seekable streams have an extra addref when they are created which must
  // be matched here.
  if (NP_SEEK == mStreamType)
    NS_RELEASE_THIS();
  
  if (!mInst || !mInst->CanFireNotifications())
    return rv;
  
  mStreamInfo = NULL;
  
  PluginDestructionGuard guard(mInst);

  nsNPAPIPlugin* plugin = mInst->GetPlugin();
  if (!plugin || !plugin->GetLibrary())
    return rv;

  NPPluginFuncs* pluginFunctions = plugin->PluginFuncs();

  NPP npp;
  mInst->GetNPP(&npp);

  if (mStreamStarted && pluginFunctions->destroystream) {
    NPPAutoPusher nppPusher(npp);

    NPError error;
    NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->destroystream)(npp, &mNPStream, reason), mInst);
    
    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                   ("NPP DestroyStream called: this=%p, npp=%p, reason=%d, return=%d, url=%s\n",
                    this, npp, reason, error, mNPStream.url));
    
    if (error == NPERR_NO_ERROR)
      rv = NS_OK;
  }
  
  mStreamStarted = false;
  
  // fire notification back to plugin, just like before
  CallURLNotify(reason);
  
  return rv;
}

void
nsNPAPIPluginStreamListener::CallURLNotify(NPReason reason)
{
  if (!mCallNotify || !mInst || !mInst->CanFireNotifications())
    return;
  
  PluginDestructionGuard guard(mInst);
  
  mCallNotify = false; // only do this ONCE and prevent recursion

  nsNPAPIPlugin* plugin = mInst->GetPlugin();
  if (!plugin || !plugin->GetLibrary())
    return;

  NPPluginFuncs* pluginFunctions = plugin->PluginFuncs();

  if (pluginFunctions->urlnotify) {
    NPP npp;
    mInst->GetNPP(&npp);
    
    NS_TRY_SAFE_CALL_VOID((*pluginFunctions->urlnotify)(npp, mNotifyURL, reason, mNPStream.notifyData), mInst);
    
    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                   ("NPP URLNotify called: this=%p, npp=%p, notify=%p, reason=%d, url=%s\n",
                    this, npp, mNPStream.notifyData, reason, mNotifyURL));
  }
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnStartBinding(nsIPluginStreamInfo* pluginInfo)
{
  if (!mInst || !mInst->CanFireNotifications())
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(mInst);

  nsNPAPIPlugin* plugin = mInst->GetPlugin();
  if (!plugin || !plugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = plugin->PluginFuncs();

  if (!pluginFunctions->newstream)
    return NS_ERROR_FAILURE;

  NPP npp;
  mInst->GetNPP(&npp);

  bool seekable;
  char* contentType;
  PRUint16 streamType = NP_NORMAL;
  NPError error;
  
  mNPStream.ndata = (void*) this;
  pluginInfo->GetURL(&mNPStream.url);
  
  pluginInfo->GetLength((PRUint32*)&(mNPStream.end));
  pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));
  pluginInfo->IsSeekable(&seekable);
  pluginInfo->GetContentType(&contentType);
  
  if (!mResponseHeaders.IsEmpty()) {
    mResponseHeaderBuf = PL_strdup(mResponseHeaders.get());
    mNPStream.headers = mResponseHeaderBuf;
  }
  
  mStreamInfo = pluginInfo;
  
  NPPAutoPusher nppPusher(npp);
  
  NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->newstream)(npp, (char*)contentType, &mNPStream, seekable, &streamType), mInst);
  
  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPP NewStream called: this=%p, npp=%p, mime=%s, seek=%d, type=%d, return=%d, url=%s\n",
                  this, npp, (char *)contentType, seekable, streamType, error, mNPStream.url));
  
  if (error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;
  
  switch(streamType)
  {
    case NP_NORMAL:
      mStreamType = NP_NORMAL; 
      break;
    case NP_ASFILEONLY:
      mStreamType = NP_ASFILEONLY; 
      break;
    case NP_ASFILE:
      mStreamType = NP_ASFILE; 
      break;
    case NP_SEEK:
      mStreamType = NP_SEEK; 
      // Seekable streams should continue to exist even after OnStopRequest
      // is fired, so we AddRef ourself an extra time and Release when the
      // plugin calls NPN_DestroyStream (CleanUpStream). If the plugin never
      // calls NPN_DestroyStream the stream will be destroyed before the plugin
      // instance is destroyed.
      NS_ADDREF_THIS();
      break;
    default:
      return NS_ERROR_FAILURE;
  }
  
  mStreamStarted = true;
  return NS_OK;
}

void
nsNPAPIPluginStreamListener::SuspendRequest()
{
  NS_ASSERTION(!mIsSuspended,
               "Suspending a request that's already suspended!");
  
  nsCOMPtr<nsINPAPIPluginStreamInfo> pluginInfoNPAPI =
  do_QueryInterface(mStreamInfo);
  
  if (!pluginInfoNPAPI) {
    return;
  }
  
  nsresult rv = StartDataPump();
  if (NS_FAILED(rv))
    return;
  
  mIsSuspended = true;
  
  pluginInfoNPAPI->SuspendRequests();
}

void
nsNPAPIPluginStreamListener::ResumeRequest()
{
  nsCOMPtr<nsINPAPIPluginStreamInfo> pluginInfoNPAPI =
  do_QueryInterface(mStreamInfo);
  
  pluginInfoNPAPI->ResumeRequests();
  
  mIsSuspended = false;
}

nsresult
nsNPAPIPluginStreamListener::StartDataPump()
{
  nsresult rv;
  mDataPumpTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Start pumping data to the plugin every 100ms until it obeys and
  // eats the data.
  return mDataPumpTimer->InitWithCallback(this, 100,
                                          nsITimer::TYPE_REPEATING_SLACK);
}

void
nsNPAPIPluginStreamListener::StopDataPump()
{
  if (mDataPumpTimer) {
    mDataPumpTimer->Cancel();
    mDataPumpTimer = nsnull;
  }
}

// Return true if a javascript: load that was started while the plugin
// was being initialized is still in progress.
bool
nsNPAPIPluginStreamListener::PluginInitJSLoadInProgress()
{
  if (!mInst)
    return false;

  nsTArray<nsNPAPIPluginStreamListener*> *streamListeners = mInst->StreamListeners();
  for (unsigned int i = 0; i < streamListeners->Length(); i++) {
    if (streamListeners->ElementAt(i)->mIsPluginInitJSStream) {
      return true;
    }
  }
  
  return false;
}

// This method is called when there's more data available off the
// network, but it's also called from our data pump when we're feeding
// the plugin data that we already got off the network, but the plugin
// was unable to consume it at the point it arrived. In the case when
// the plugin pump calls this method, the input argument will be null,
// and the length will be the number of bytes available in our
// internal buffer.
NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
                                             nsIInputStream* input,
                                             PRUint32 length)
{
  if (!length || !mInst || !mInst->CanFireNotifications())
    return NS_ERROR_FAILURE;
  
  PluginDestructionGuard guard(mInst);
  
  // Just in case the caller switches plugin info on us.
  mStreamInfo = pluginInfo;

  nsNPAPIPlugin* plugin = mInst->GetPlugin();
  if (!plugin || !plugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = plugin->PluginFuncs();

  // check out if plugin implements NPP_Write call
  if (!pluginFunctions->write)
    return NS_ERROR_FAILURE; // it'll cancel necko transaction 
  
  if (!mStreamBuffer) {
    // To optimize the mem usage & performance we have to allocate
    // mStreamBuffer here in first ODA when length of data available
    // in input stream is known.  mStreamBuffer will be freed in DTOR.
    // we also have to remember the size of that buff to make safe
    // consecutive Read() calls form input stream into our buff.
    
    PRUint32 contentLength;
    pluginInfo->GetLength(&contentLength);
    
    mStreamBufferSize = NS_MAX(length, contentLength);
    
    // Limit the size of the initial buffer to MAX_PLUGIN_NECKO_BUFFER
    // (16k). This buffer will grow if needed, as in the case where
    // we're getting data faster than the plugin can process it.
    mStreamBufferSize = NS_MIN(mStreamBufferSize,
                               PRUint32(MAX_PLUGIN_NECKO_BUFFER));
    
    mStreamBuffer = (char*) PR_Malloc(mStreamBufferSize);
    if (!mStreamBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  // prepare NPP_ calls params
  NPP npp;
  mInst->GetNPP(&npp);
  
  PRInt32 streamPosition;
  pluginInfo->GetStreamOffset(&streamPosition);
  PRInt32 streamOffset = streamPosition;
  
  if (input) {
    streamOffset += length;
    
    // Set new stream offset for the next ODA call regardless of how
    // following NPP_Write call will behave we pretend to consume all
    // data from the input stream.  It's possible that current steam
    // position will be overwritten from NPP_RangeRequest call made
    // from NPP_Write, so we cannot call SetStreamOffset after
    // NPP_Write.
    //
    // Note: there is a special case when data flow should be
    // temporarily stopped if NPP_WriteReady returns 0 (bug #89270)
    pluginInfo->SetStreamOffset(streamOffset);
    
    // set new end in case the content is compressed
    // initial end is less than end of decompressed stream
    // and some plugins (e.g. acrobat) can fail. 
    if ((PRInt32)mNPStream.end < streamOffset)
      mNPStream.end = streamOffset;
  }
  
  nsresult rv = NS_OK;
  while (NS_SUCCEEDED(rv) && length > 0) {
    if (input && length) {
      if (mStreamBufferSize < mStreamBufferByteCount + length && mIsSuspended) {
        // We're in the ::OnDataAvailable() call that we might get
        // after suspending a request, or we suspended the request
        // from within this ::OnDataAvailable() call while there's
        // still data in the input, and we don't have enough space to
        // store what we got off the network. Reallocate our internal
        // buffer.
        mStreamBufferSize = mStreamBufferByteCount + length;
        char *buf = (char*)PR_Realloc(mStreamBuffer, mStreamBufferSize);
        if (!buf)
          return NS_ERROR_OUT_OF_MEMORY;
        
        mStreamBuffer = buf;
      }
      
      PRUint32 bytesToRead =
      NS_MIN(length, mStreamBufferSize - mStreamBufferByteCount);
      
      PRUint32 amountRead = 0;
      rv = input->Read(mStreamBuffer + mStreamBufferByteCount, bytesToRead,
                       &amountRead);
      NS_ENSURE_SUCCESS(rv, rv);
      
      if (amountRead == 0) {
        NS_NOTREACHED("input->Read() returns no data, it's almost impossible "
                      "to get here");
        
        break;
      }
      
      mStreamBufferByteCount += amountRead;
      length -= amountRead;
    } else {
      // No input, nothing to read. Set length to 0 so that we don't
      // keep iterating through this outer loop any more.
      
      length = 0;
    }
    
    // Temporary pointer to the beginning of the data we're writing as
    // we loop and feed the plugin data.
    char *ptrStreamBuffer = mStreamBuffer;
    
    // it is possible plugin's NPP_Write() returns 0 byte consumed. We
    // use zeroBytesWriteCount to count situation like this and break
    // the loop
    PRInt32 zeroBytesWriteCount = 0;
    
    // mStreamBufferByteCount tells us how many bytes there are in the
    // buffer. WriteReady returns to us how many bytes the plugin is
    // ready to handle.
    while (mStreamBufferByteCount > 0) {
      PRInt32 numtowrite;
      if (pluginFunctions->writeready) {
        NPPAutoPusher nppPusher(npp);
        
        NS_TRY_SAFE_CALL_RETURN(numtowrite, (*pluginFunctions->writeready)(npp, &mNPStream), mInst);
        NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                       ("NPP WriteReady called: this=%p, npp=%p, "
                        "return(towrite)=%d, url=%s\n",
                        this, npp, numtowrite, mNPStream.url));
        
        if (!mStreamStarted) {
          // The plugin called NPN_DestroyStream() from within
          // NPP_WriteReady(), kill the stream.
          
          return NS_BINDING_ABORTED;
        }
        
        // if WriteReady returned 0, the plugin is not ready to handle
        // the data, suspend the stream (if it isn't already
        // suspended).
        //
        // Also suspend the stream if the stream we're loading is not
        // a javascript: URL load that was initiated during plugin
        // initialization and there currently is such a stream
        // loading. This is done to work around a Windows Media Player
        // plugin bug where it can't deal with being fed data for
        // other streams while it's waiting for data from the
        // javascript: URL loads it requests during
        // initialization. See bug 386493 for more details.
        
        if (numtowrite <= 0 ||
            (!mIsPluginInitJSStream && PluginInitJSLoadInProgress())) {
          if (!mIsSuspended) {
            SuspendRequest();
          }
          
          // Break out of the inner loop, but keep going through the
          // outer loop in case there's more data to read from the
          // input stream.
          
          break;
        }
        
        numtowrite = NS_MIN(numtowrite, mStreamBufferByteCount);
      } else {
        // if WriteReady is not supported by the plugin, just write
        // the whole buffer
        numtowrite = mStreamBufferByteCount;
      }
      
      NPPAutoPusher nppPusher(npp);
      
      PRInt32 writeCount = 0; // bytes consumed by plugin instance
      NS_TRY_SAFE_CALL_RETURN(writeCount, (*pluginFunctions->write)(npp, &mNPStream, streamPosition, numtowrite, ptrStreamBuffer), mInst);
      
      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                     ("NPP Write called: this=%p, npp=%p, pos=%d, len=%d, "
                      "buf=%s, return(written)=%d,  url=%s\n",
                      this, npp, streamPosition, numtowrite,
                      ptrStreamBuffer, writeCount, mNPStream.url));
      
      if (!mStreamStarted) {
        // The plugin called NPN_DestroyStream() from within
        // NPP_Write(), kill the stream.
        return NS_BINDING_ABORTED;
      }
      
      if (writeCount > 0) {
        NS_ASSERTION(writeCount <= mStreamBufferByteCount,
                     "Plugin read past the end of the available data!");
        
        writeCount = NS_MIN(writeCount, mStreamBufferByteCount);
        mStreamBufferByteCount -= writeCount;
        
        streamPosition += writeCount;
        
        zeroBytesWriteCount = 0;
        
        if (mStreamBufferByteCount > 0) {
          // This alignment code is most likely bogus, but we'll leave
          // it in for now in case it matters for some plugins on some
          // architectures. Who knows...
          if (writeCount % sizeof(PRWord)) {
            // memmove will take care  about alignment 
            memmove(mStreamBuffer, ptrStreamBuffer + writeCount,
                    mStreamBufferByteCount);
            ptrStreamBuffer = mStreamBuffer;
          } else {
            // if aligned we can use ptrStreamBuffer += to eliminate
            // memmove()
            ptrStreamBuffer += writeCount;
          }
        }
      } else if (writeCount == 0) {
        // if NPP_Write() returns writeCount == 0 lets say 3 times in
        // a row, suspend the request and continue feeding the plugin
        // the data we got so far. Once that data is consumed, we'll
        // resume the request.
        if (mIsSuspended || ++zeroBytesWriteCount == 3) {
          if (!mIsSuspended) {
            SuspendRequest();
          }
          
          // Break out of the for loop, but keep going through the
          // while loop in case there's more data to read from the
          // input stream.
          
          break;
        }
      } else {
        // Something's really wrong, kill the stream.
        rv = NS_ERROR_FAILURE;
        
        break;
      }  
    } // end of inner while loop
    
    if (mStreamBufferByteCount && mStreamBuffer != ptrStreamBuffer) {
      memmove(mStreamBuffer, ptrStreamBuffer, mStreamBufferByteCount);
    }
  }
  
  if (streamPosition != streamOffset) {
    // The plugin didn't consume all available data, or consumed some
    // of our cached data while we're pumping cached data. Adjust the
    // plugin info's stream offset to match reality, except if the
    // plugin info's stream offset was set by a re-entering
    // NPN_RequestRead() call.
    
    PRInt32 postWriteStreamPosition;
    pluginInfo->GetStreamOffset(&postWriteStreamPosition);
    
    if (postWriteStreamPosition == streamOffset) {
      pluginInfo->SetStreamOffset(streamPosition);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnFileAvailable(nsIPluginStreamInfo* pluginInfo, 
                                             const char* fileName)
{
  if (!mInst || !mInst->CanFireNotifications())
    return NS_ERROR_FAILURE;
  
  PluginDestructionGuard guard(mInst);

  nsNPAPIPlugin* plugin = mInst->GetPlugin();
  if (!plugin || !plugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = plugin->PluginFuncs();

  if (!pluginFunctions->asfile)
    return NS_ERROR_FAILURE;

  NPP npp;
  mInst->GetNPP(&npp);
  
  NS_TRY_SAFE_CALL_VOID((*pluginFunctions->asfile)(npp, &mNPStream, fileName), mInst);
  
  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPP StreamAsFile called: this=%p, npp=%p, url=%s, file=%s\n",
                  this, npp, mNPStream.url, fileName));
  
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnStopBinding(nsIPluginStreamInfo* pluginInfo, 
                                           nsresult status)
{
  StopDataPump();
  
  if (NS_FAILED(status)) {
    // The stream was destroyed, or died for some reason. Make sure we
    // cancel the underlying request.
    nsCOMPtr<nsINPAPIPluginStreamInfo> pluginInfoNPAPI =
    do_QueryInterface(mStreamInfo);
    
    if (pluginInfoNPAPI) {
      pluginInfoNPAPI->CancelRequests(status);
    }
  }
  
  if (!mInst || !mInst->CanFireNotifications())
    return NS_ERROR_FAILURE;

  // check if the stream is of seekable type and later its destruction
  // see bug 91140
  nsresult rv = NS_OK;
  NPReason reason = NS_FAILED(status) ? NPRES_NETWORK_ERR : NPRES_DONE;
  if (mRedirectDenied) {
    reason = NPRES_USER_BREAK;
  }
  if (mStreamType != NP_SEEK ||
      (NP_SEEK == mStreamType && NS_BINDING_ABORTED == status)) {
    rv = CleanUpStream(reason);
  }

  return rv;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::GetStreamType(PRInt32 *result)
{
  *result = mStreamType;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::Notify(nsITimer *aTimer)
{
  NS_ASSERTION(aTimer == mDataPumpTimer, "Uh, wrong timer?");
  
  PRInt32 oldStreamBufferByteCount = mStreamBufferByteCount;
  
  nsresult rv = OnDataAvailable(mStreamInfo, nsnull, mStreamBufferByteCount);
  
  if (NS_FAILED(rv)) {
    // We ran into an error, no need to keep firing this timer then.
    aTimer->Cancel();
    return NS_OK;
  }
  
  if (mStreamBufferByteCount != oldStreamBufferByteCount &&
      ((mStreamStarted && mStreamBufferByteCount < 1024) ||
       mStreamBufferByteCount == 0)) {
        // The plugin read some data and we've got less than 1024 bytes in
        // our buffer (or its empty and the stream is already
        // done). Resume the request so that we get more data off the
        // network.
        ResumeRequest();
        // Necko will pump data now that we've resumed the request.
        StopDataPump();
      }
  
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::StatusLine(const char* line)
{
  mResponseHeaders.Append(line);
  mResponseHeaders.Append('\n');
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::NewResponseHeader(const char* headerName,
                                               const char* headerValue)
{
  mResponseHeaders.Append(headerName);
  mResponseHeaders.Append(": ");
  mResponseHeaders.Append(headerValue);
  mResponseHeaders.Append('\n');
  return NS_OK;
}

bool
nsNPAPIPluginStreamListener::HandleRedirectNotification(nsIChannel *oldChannel, nsIChannel *newChannel,
                                                        nsIAsyncVerifyRedirectCallback* callback)
{
  nsCOMPtr<nsIHttpChannel> oldHttpChannel = do_QueryInterface(oldChannel);
  nsCOMPtr<nsIHttpChannel> newHttpChannel = do_QueryInterface(newChannel);
  if (!oldHttpChannel || !newHttpChannel) {
    return false;
  }

  if (!mInst || !mInst->CanFireNotifications()) {
    return false;
  }

  nsNPAPIPlugin* plugin = mInst->GetPlugin();
  if (!plugin || !plugin->GetLibrary()) {
    return false;
  }

  NPPluginFuncs* pluginFunctions = plugin->PluginFuncs();
  if (!pluginFunctions->urlredirectnotify) {
    return false;
  }

  // A non-null closure is required for redirect handling support.
  if (mNPStream.notifyData) {
    PRUint32 status;
    if (NS_SUCCEEDED(oldHttpChannel->GetResponseStatus(&status))) {
      nsCOMPtr<nsIURI> uri;
      if (NS_SUCCEEDED(newHttpChannel->GetURI(getter_AddRefs(uri))) && uri) {
        nsCAutoString spec;
        if (NS_SUCCEEDED(uri->GetAsciiSpec(spec))) {
          // At this point the plugin will be responsible for making the callback
          // so save the callback object.
          mHTTPRedirectCallback = callback;

          NPP npp;
          mInst->GetNPP(&npp);
#if defined(XP_WIN) || defined(XP_OS2)
          NS_TRY_SAFE_CALL_VOID((*pluginFunctions->urlredirectnotify)(npp, spec.get(), static_cast<int32_t>(status), mNPStream.notifyData), mInst);
#else
          (*pluginFunctions->urlredirectnotify)(npp, spec.get(), static_cast<int32_t>(status), mNPStream.notifyData);
#endif
          return true;
        }
      }
    }
  }

  callback->OnRedirectVerifyCallback(NS_ERROR_FAILURE);
  return true;
}

void
nsNPAPIPluginStreamListener::URLRedirectResponse(NPBool allow)
{
  if (mHTTPRedirectCallback) {
    mHTTPRedirectCallback->OnRedirectVerifyCallback(allow ? NS_OK : NS_ERROR_FAILURE);
    mRedirectDenied = allow ? false : true;
    mHTTPRedirectCallback = nsnull;
  }
}
