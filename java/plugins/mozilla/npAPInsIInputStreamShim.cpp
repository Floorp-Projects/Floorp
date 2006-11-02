/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "npAPInsIInputStreamShim.h"

#include "nsCRT.h"
#include "npapi.h"

#include "prlog.h"
#include "prlock.h"

#include "nsStreamUtils.h"

const uint32 npAPInsIInputStreamShim::INITIAL_BUFFER_LENGTH = 16532;
const uint32 npAPInsIInputStreamShim::buffer_increment = 5120;
const PRInt32 npAPInsIInputStreamShim::do_close_code = -524;

npAPInsIInputStreamShim::npAPInsIInputStreamShim(nsIPluginStreamListener *plugletListener, nsIPluginStreamInfo* streamInfo)
    : mPlugletListener(plugletListener),
      mStreamInfo(streamInfo),
      mBuffer(nsnull), mBufferLength(0), mCountFromPluginHost(0), 
      mCountFromPluglet(0), mAvailableForPluglet(0), 
      mNumWrittenFromPluginHost(0),
      mDoClose(PR_FALSE), mDidClose(PR_FALSE), mLock(PR_NewLock()), 
      mHasLock(PR_FALSE),
      mCloseStatus(NS_OK), mCallback(nsnull), mCallbackFlags(0)

{ 
    NS_INIT_ISUPPORTS();
}

npAPInsIInputStreamShim::~npAPInsIInputStreamShim()
{
    this->doLock();

    DoClose();

    mPlugletListener = nsnull;
    mStreamInfo = nsnull;

    NPN_MemFree(mBuffer);
    mBuffer = nsnull;
    mBufferLength = 0;

    this->doUnlock();

    mAvailableForPluglet = 0;
    mNumWrittenFromPluginHost = 0;
    mDoClose = PR_TRUE;
    mDidClose = PR_TRUE;
    PR_DestroyLock(mLock);
    mLock = nsnull;

}

PRStatus npAPInsIInputStreamShim::doLock(void)
{
    PRStatus result = PR_SUCCESS;

    if (!mHasLock) {
        PR_ASSERT(mLock);
        PR_Lock(mLock);
        mHasLock = PR_TRUE;
    }

    return result;
}

PRStatus npAPInsIInputStreamShim::doUnlock(void)
{
    PRStatus result = PR_SUCCESS;

    if (mHasLock) {
        PR_ASSERT(mLock);
        mHasLock = PR_FALSE;
        result = PR_Unlock(mLock);
    }

    return result;
}

//NS_IMPL_ISUPPORTS(npAPInsIInputStreamShim, NS_GET_IID(nsIInputStream))
NS_IMETHODIMP_(nsrefcnt) npAPInsIInputStreamShim::AddRef(void)                
{                                                            
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  
  // Note that we intentionally don't check for owning thread safety.
  ++mRefCnt;                                                 
  NS_LOG_ADDREF(this, mRefCnt, "npAPInsIInputStreamShim", sizeof(*this));      
  return mRefCnt;                                            
}

NS_IMETHODIMP_(nsrefcnt) npAPInsIInputStreamShim::Release(void)          
{                                                            
    NS_PRECONDITION(0 != mRefCnt, "dup release");              
    // Note that we intentionally don't check for owning thread safety.
    --mRefCnt;                                                 
    NS_LOG_RELEASE(this, mRefCnt, "npAPInsIInputStreamShim");
    if (mRefCnt == 0) {                                        
        mRefCnt = 1; /* stabilize */                             
        NS_DELETEXPCOM(this);                                    
        return 0;                                                
    }                                                          
    return mRefCnt;                                            
}

NS_IMPL_QUERY_INTERFACE2(npAPInsIInputStreamShim,
                         nsIInputStream,
                         nsIAsyncInputStream)

NS_IMETHODIMP
npAPInsIInputStreamShim::AllowStreamToReadFromBuffer(int32 len, void* buf, 
                                                     int32* outWritten)
{
    nsresult rv = NS_ERROR_NULL_POINTER;

    if (!mPlugletListener) {
        return rv;
    }

    this->doLock();

    rv = this->CopyFromPluginHostToBuffer(len, buf, outWritten);

    this->doUnlock();

    if (NS_SUCCEEDED(rv)) {
        //        rv = mPlugletListener->OnDataAvailable(mStreamInfo, this, 
        //                                               (PRUint32) outWritten);
    }

    return rv;
}

NS_IMETHODIMP
npAPInsIInputStreamShim::CopyFromPluginHostToBuffer(int32 len, void* buf, 
                                                    int32 * outWritten)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (nsnull == outWritten) {
        return rv;
    }

    if (mDoClose) {
        DoClose();
        return NS_ERROR_NOT_AVAILABLE;
    }

    rv = EnsureBuffer(len);
    if (NS_FAILED(rv)) {
        return rv;
    }
    
    mNumWrittenFromPluginHost = len;
    *outWritten = mNumWrittenFromPluginHost;
    if (0 < mNumWrittenFromPluginHost) {
        // copy the bytes the from the plugin host into our buffer
        memcpy(((char *) mBuffer) + mCountFromPluginHost, buf, 
               (size_t) mNumWrittenFromPluginHost);

        mCountFromPluginHost += mNumWrittenFromPluginHost;
        mAvailableForPluglet = mCountFromPluginHost - mCountFromPluglet;
    }

    // if we have bytes available for the pluglet, and they have
    // requested a callback when bytes are available.
    if (mCallback && 0 < mAvailableForPluglet
        && !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
        rv = mCallback->OnInputStreamReady(this);
        mCallback = nsnull;
        mCallbackFlags = nsnull;
    }

    rv = NS_OK;
    
    return rv;
}

NS_IMETHODIMP
npAPInsIInputStreamShim::EnsureBuffer(int32 requestedAdditionalLen)
{

    // if we don't have a buffer, create one
    if (!mBuffer) {
        if (0 < INITIAL_BUFFER_LENGTH) {
            mBuffer = NPN_MemAlloc(INITIAL_BUFFER_LENGTH);
            mBufferLength = INITIAL_BUFFER_LENGTH;
        }
        if (!mBuffer) {
            mBufferLength = 0;
            return NS_ERROR_FAILURE;
        }
    }
    else {
        
        // See if we need to grow our buffer.  If what we have plus what
        // we're about to get is greater than the current buffer size...
        int32 additionalLen = requestedAdditionalLen < buffer_increment ?
            buffer_increment : requestedAdditionalLen;

        if (mBufferLength < (mCountFromPluginHost + additionalLen)) {
            // create the new buffer
            void *tBuffer = NPN_MemAlloc(mBufferLength + additionalLen);
            if (!tBuffer) {
                return NS_ERROR_FAILURE;
            }
            // copy the old buffer into the new buffer
            memcpy(tBuffer, mBuffer, mBufferLength);
            // delete the old buffer
            NPN_MemFree(mBuffer);
            // update mBuffer;
            mBuffer = tBuffer;
            // update our bufferLength
            mBufferLength += additionalLen;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
npAPInsIInputStreamShim::DoClose(void)
{
    nsresult rv = NS_ERROR_FAILURE;
    PR_ASSERT(mDoClose);
    if (mDidClose) {
        return NS_OK;
    }
    if (mCallback && mAvailableForPluglet && 
        !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
        rv = mCallback->OnInputStreamReady(this);
        mCallback = nsnull;
        mCallbackFlags = nsnull;
    }

    mCloseStatus = NS_BASE_STREAM_CLOSED;
    mDidClose = PR_TRUE;

    //    if (mPlugletListener && mStreamInfo) {
    //        mPlugletListener->OnStopBinding(mStreamInfo, mCloseStatus);
    //    }

    return rv;
}

//
// nsIInputStream methods
// 

NS_IMETHODIMP
npAPInsIInputStreamShim::Available(PRUint32* aResult) 
{
    nsresult rv = NS_ERROR_FAILURE;

    if (!aResult) {
        return NS_ERROR_NULL_POINTER;
    }

    if (mDoClose) {
        if (mAvailableForPluglet) {
            *aResult = mAvailableForPluglet;
            return NS_OK;
        }
        return mCloseStatus;
    }
    this->doLock();
    *aResult = mAvailableForPluglet;
    // *aResult = PR_UINT32_MAX;
    rv = NS_OK;
    this->doUnlock();

    return rv;
}
  
NS_IMETHODIMP
npAPInsIInputStreamShim::Close() 
{
    nsresult rv = NS_ERROR_FAILURE;
    this->doLock();
    mDoClose = PR_TRUE;
    mCloseStatus = NS_BASE_STREAM_CLOSED;
    rv = NS_OK;
    this->doUnlock();

    return rv;
}

NS_IMETHODIMP
npAPInsIInputStreamShim::Read(char* aBuffer, PRUint32 aCount, PRUint32 *aNumRead)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!aBuffer || !aNumRead) {
        return NS_ERROR_NULL_POINTER;
    }

    if (mDoClose) {
        return mCloseStatus;
    }

    *aNumRead = 0;

    PR_ASSERT(mCountFromPluglet <= mCountFromPluginHost);

    this->doLock();

    if (mAvailableForPluglet) {
        if (aCount <= (mCountFromPluginHost - mCountFromPluglet)) {
            // what she's asking for is less than or equal to what we have
            memcpy(aBuffer, (((char *)mBuffer) + mCountFromPluglet), aCount);
            mCountFromPluglet += aCount;
            *aNumRead = aCount;
        }
        else {
            // what she's asking for is more than what we have
            memcpy(aBuffer, ((char *)mBuffer) + mCountFromPluglet, 
                          (mCountFromPluginHost - mCountFromPluglet));
            *aNumRead = (mCountFromPluginHost - mCountFromPluglet);
            
            mCountFromPluglet += (mCountFromPluginHost - mCountFromPluglet);
        }
        mAvailableForPluglet -= *aNumRead;
        rv = NS_OK;
    }
    else {
        rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
    
    this->doUnlock();

    return rv;
}

NS_IMETHODIMP 
npAPInsIInputStreamShim::ReadSegments(nsWriteSegmentFun writer, void * aClosure, 
                              PRUint32 aCount, PRUint32 *aNumRead)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!writer || !aNumRead) {
        return NS_ERROR_NULL_POINTER;
    }
    if (mDoClose && 0 == mAvailableForPluglet) {
        return mCloseStatus;
    }
    *aNumRead = 0;
    PR_ASSERT(mCountFromPluglet <= mCountFromPluginHost);

    this->doLock();

    PRInt32 bytesToWrite = mAvailableForPluglet;
    PRUint32 bytesWritten;
    PRUint32 totalBytesWritten = 0;

    if (bytesToWrite > aCount) {
        bytesToWrite = aCount;
    }

    if (0 == mAvailableForPluglet) {
        rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
    
    while (bytesToWrite) {
        // what she's asking for is less than or equal to what we have
        rv = writer(this, aClosure, 
                    (((char *)mBuffer) + mCountFromPluglet),
                    totalBytesWritten, bytesToWrite, &bytesWritten);
        if (NS_FAILED(rv)) {
            break;
        }
        
        bytesToWrite -= bytesWritten;
        totalBytesWritten += bytesWritten;
        mCountFromPluglet += bytesWritten;
    }
    
    *aNumRead = totalBytesWritten;
    mAvailableForPluglet -= totalBytesWritten;
    
    this->doUnlock();
}

NS_IMETHODIMP 
npAPInsIInputStreamShim::IsNonBlocking(PRBool *_retval)
{
    *_retval = PR_TRUE;
    return NS_OK;
}

//
// nsIAsyncInputStream methods
// 

NS_IMETHODIMP
npAPInsIInputStreamShim::CloseWithStatus(nsresult astatus) 
{
    this->Close();
    mCloseStatus = astatus;
    return NS_OK;
}

NS_IMETHODIMP
npAPInsIInputStreamShim::AsyncWait(nsIInputStreamCallback *aCallback, 
                           PRUint32 aFlags, PRUint32 aRequestedCount, 
                           nsIEventTarget *aEventTarget)
{
    this->doLock();
    
    mCallback = nsnull;
    mCallbackFlags = nsnull;
    
    nsCOMPtr<nsIInputStreamCallback> proxy;
    if (aEventTarget) {
        nsresult rv = NS_NewInputStreamReadyEvent(getter_AddRefs(proxy),
                                                  aCallback, aEventTarget);
        if (NS_FAILED(rv)) return rv;
        aCallback = proxy;
    }
    if (NS_FAILED(mCloseStatus) ||
        (mAvailableForPluglet && !(aFlags & WAIT_CLOSURE_ONLY))) {
        // stream is already closed or readable; post event.
        aCallback->OnInputStreamReady(this);
    }
    else {
        // queue up callback object to be notified when data becomes available
        mCallback = aCallback;
        mCallbackFlags = aFlags;
    }
    
    this->doUnlock();
    
    return NS_OK;
}

