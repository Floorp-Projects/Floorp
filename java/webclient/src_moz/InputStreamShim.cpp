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

#include "InputStreamShim.h"

#include "jni_util.h"

#include "nsCRT.h"
#include "prlock.h"
#include "prlog.h"
#include "prthread.h"

#include "ns_globals.h"

#include "nsIEventTarget.h"
#include "nsStreamUtils.h"

static const PRInt32 buffer_increment = 5120;
static const PRInt32 do_close_code = -524;

InputStreamShim::InputStreamShim(jobject yourJavaStreamRef, 
                                 PRInt32 yourContentLength) : 
    mJavaStream(yourJavaStreamRef), mContentLength(yourContentLength),
    mBuffer(nsnull), mBufferLength(0), mCountFromJava(0), 
    mCountFromMozilla(0), mAvailable(0), mAvailableForMozilla(0), mNumRead(0),
    mDoClose(PR_FALSE), mDidClose(PR_FALSE), mLock(nsnull), 
    mCloseStatus(NS_OK), mCallback(nsnull), mCallbackFlags(0)
{ 
    NS_INIT_ISUPPORTS();
    mLock = PR_NewLock();
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
}

InputStreamShim::~InputStreamShim()
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    ::util_DeleteGlobalRef(env, mJavaStream);
    mJavaStream = nsnull;

    mContentLength = -1;

    PR_Lock(mLock);

    delete [] mBuffer;
    mBuffer = nsnull;
    mBufferLength = 0;

    PR_Unlock(mLock);

    mAvailable = 0;
    mAvailableForMozilla = 0;
    mNumRead = 0;
    mDoClose = PR_TRUE;
    mDidClose = PR_TRUE;
    PR_DestroyLock(mLock);
    mLock = nsnull;
}

//NS_IMPL_ISUPPORTS(InputStreamShim, NS_GET_IID(nsIInputStream))
NS_IMETHODIMP_(nsrefcnt) InputStreamShim::AddRef(void)                
{                                                            
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  
  // Note that we intentionally don't check for owning thread safety.
  ++mRefCnt;                                                 
  NS_LOG_ADDREF(this, mRefCnt, "InputStreamShim", sizeof(*this));      
  return mRefCnt;                                            
}

NS_IMETHODIMP_(nsrefcnt) InputStreamShim::Release(void)          
{                                                            
    NS_PRECONDITION(0 != mRefCnt, "dup release");              
    // Note that we intentionally don't check for owning thread safety.
    --mRefCnt;                                                 
    NS_LOG_RELEASE(this, mRefCnt, "InputStreamShim");
    if (mRefCnt == 0) {                                        
        mRefCnt = 1; /* stabilize */                             
        NS_DELETEXPCOM(this);                                    
        return 0;                                                
    }                                                          
    return mRefCnt;                                            
}

NS_IMPL_QUERY_INTERFACE2(InputStreamShim,
                         nsIInputStream,
                         nsIAsyncInputStream)

nsresult InputStreamShim::doReadFromJava()
{
    nsresult rv = NS_ERROR_FAILURE;
    PR_ASSERT(mLock);

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::doReadFromJava: entering\n"));

    PR_Lock(mLock);

    // first, see how much data is available
    if (NS_FAILED(rv = doAvailable())) {
        goto DRFJ_CLEANUP;
    }

    // if we have all our data, give the error appropriate result
    if (mAvailable <= 0 ||
        (0 != mCountFromJava && 
         (((PRUint32)mContentLength) == mCountFromJava))) {
        mDoClose = PR_TRUE;
        rv = doClose();
        if (0 == mAvailableForMozilla) {
            rv = NS_ERROR_NOT_AVAILABLE;
        }
        goto DRFJ_CLEANUP;
    }
    
    if (NS_FAILED(doRead())) {
        rv = NS_ERROR_FAILURE;
        goto DRFJ_CLEANUP;
    }
    rv = NS_OK;

    // if we have bytes available for mozilla, and they it has requested
    // a callback when bytes are available.
    if (mCallback && 0 < mAvailableForMozilla 
        && !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
        rv = mCallback->OnInputStreamReady(this);
        mCallback = nsnull;
        mCallbackFlags = nsnull;
        if (NS_FAILED(rv)) {
            goto DRFJ_CLEANUP;
        }
    }

    // finally, do another check for available bytes
    if (NS_FAILED(doAvailable())) {
        rv = NS_ERROR_FAILURE;
        goto DRFJ_CLEANUP;
    }
    if (mAvailable <= 0 ||
        (0 != mCountFromJava && 
         (((PRUint32)mContentLength) == mCountFromJava))) {
        mDoClose = PR_TRUE;
        rv = doClose();
        if (0 == mAvailableForMozilla) {
            rv = NS_ERROR_NOT_AVAILABLE;
        }

        goto DRFJ_CLEANUP;
    }

 DRFJ_CLEANUP:
    
    PR_Unlock(mLock);

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::doReadFromJava: exiting\n"));
    return rv;
}

// 
// Helper methods called from doReadFromJava
// 

nsresult
InputStreamShim::doAvailable(void)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (mDidClose) {
        mAvailable = 0;
        return NS_OK;
    }
#ifdef BAL_INTERFACE
#else  
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jclass streamClass = nsnull;
    jmethodID mid = nsnull;
    
    if (!(streamClass = env->GetObjectClass(mJavaStream))) {
        return rv;
    }
    if (!(mid = env->GetMethodID(streamClass, "available", "()I"))) {
        return rv;
    }
    mAvailable = (PRInt32) env->CallIntMethod(mJavaStream, mid);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return NS_ERROR_FAILURE;
    }
    rv = NS_OK;
#endif
    
    return rv;
}

nsresult
InputStreamShim::doRead(void)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (mDoClose) {
        doClose();
        return NS_ERROR_NOT_AVAILABLE;
    }

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::doRead: entering\n"));

    PR_ASSERT(0 != mAvailable);

    // if we don't have a buffer, create one
    if (!mBuffer) {
        if (0 < mContentLength) {
            mBuffer = new char[mContentLength];
            mBufferLength = mContentLength;
        }
        else {

            // make sure we allocate enough buffer to store what is
            // currently available.
            if (mAvailable < buffer_increment) {
                mBufferLength = buffer_increment;
            }
            else {
                PRInt32 bufLengthCalc = mAvailable / buffer_increment;
                mBufferLength = buffer_increment + 
                    (bufLengthCalc * buffer_increment);
            }
            mBuffer = new char[mBufferLength];
                
        }
        if (!mBuffer) {
            mBufferLength = 0;
            return NS_ERROR_FAILURE;
        }
    }
    else {
        
        // See if we need to grow our buffer.  If what we have plus what
        // we're about to get is greater than the current buffer size...

        if (mBufferLength < (mCountFromJava + mAvailable)) {
            // create the new buffer
            char *tBuffer = new char[mBufferLength + buffer_increment];
            if (!tBuffer) {
                return NS_ERROR_FAILURE;
            }
            // copy the old buffer into the new buffer
            memcpy(tBuffer, mBuffer, mBufferLength);
            // delete the old buffer
            delete [] mBuffer;
            // update mBuffer;
            mBuffer = tBuffer;
            // update our bufferLength
            mBufferLength += buffer_increment;
        }
    }
    
#ifdef BAL_INTERFACE
#else  
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jmethodID mid = nsnull;
    jclass streamClass = nsnull;
    jbyteArray javaByteArray = nsnull;
    
    if (!(streamClass = env->GetObjectClass(mJavaStream))) {
        return rv;
    }
    if (!(mid = env->GetMethodID(streamClass, "read", "([BII)I"))) {
        return rv;
    }
    
    if (!(javaByteArray = env->NewByteArray((jsize) mAvailable))) {
        return rv;
    }
    
    mNumRead = env->CallIntMethod(mJavaStream, mid, javaByteArray, (jint) 0, 
                                  (jint) mAvailable);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return NS_ERROR_FAILURE;
    }
    if (0 < mNumRead) {
        // copy the bytes from java into our buffer
        env->GetByteArrayRegion(javaByteArray, 0, (jint) mNumRead, 
                                (jbyte *) (mBuffer + mCountFromJava));
        mCountFromJava += mNumRead;
        mAvailableForMozilla = mCountFromJava - mCountFromMozilla;
    }
    rv = NS_OK;
#endif

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::doRead: exiting\n"));

    return rv;
}

nsresult
InputStreamShim::doClose(void)
{
    nsresult rv = NS_ERROR_FAILURE;
    PR_ASSERT(mDoClose);
    if (mDidClose) {
        return NS_OK;
    }

#ifdef BAL_INTERFACE
#else  
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jclass streamClass = nsnull;
    jmethodID mid = nsnull;
    
    if (!(streamClass = env->GetObjectClass(mJavaStream))) {
        return rv;
    }
    if (!(mid = env->GetMethodID(streamClass, "close", "()V"))) {
        return rv;
    }
    env->CallVoidMethod(mJavaStream, mid);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return NS_ERROR_FAILURE;
    }
    rv = NS_OK;
#endif
    if (mCallback && mAvailableForMozilla && 
        !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
        rv = mCallback->OnInputStreamReady(this);
        mCallback = nsnull;
        mCallbackFlags = nsnull;
    }

    mCloseStatus = NS_BASE_STREAM_CLOSED;
    mDidClose = PR_TRUE;
    return rv;
}

//
// nsIInputStream methods
// 

NS_IMETHODIMP
InputStreamShim::Available(PRUint32* aResult) 
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    if (mDoClose) {
        if (mAvailableForMozilla) {
            *aResult = mAvailableForMozilla;
            return NS_OK;
        }
        return mCloseStatus;
    }
    PR_ASSERT(mLock);
    PR_Lock(mLock);
    // *aResult = mAvailableForMozilla;
    *aResult = PR_UINT32_MAX;
    rv = NS_OK;
    PR_Unlock(mLock);

    return rv;
}
  
NS_IMETHODIMP
InputStreamShim::Close() 
{
    nsresult rv = NS_ERROR_FAILURE;
    PR_ASSERT(mLock);
    PR_Lock(mLock);
    mDoClose = PR_TRUE;
    mCloseStatus = NS_BASE_STREAM_CLOSED;
    rv = NS_OK;
    PR_Unlock(mLock);

    return rv;
}

NS_IMETHODIMP
InputStreamShim::Read(char* aBuffer, PRUint32 aCount, PRUint32 *aNumRead)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!aBuffer || !aNumRead) {
        return NS_ERROR_NULL_POINTER;
    }
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::Read: entering\n"));
    if (mDoClose) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	       ("InputStreamShim::Read: exiting, already closed\n"));
        return mCloseStatus;
    }
    *aNumRead = 0;
    PR_ASSERT(mLock);
    PR_ASSERT(mCountFromMozilla <= mCountFromJava);

    PR_Lock(mLock);

    if (mAvailableForMozilla) {
        if (aCount <= (mCountFromJava - mCountFromMozilla)) {
            // what she's asking for is less than or equal to what we have
            memcpy(aBuffer, (mBuffer + mCountFromMozilla), aCount);
            mCountFromMozilla += aCount;
            *aNumRead = aCount;
        }
        else {
            // what she's asking for is more than what we have
            memcpy(aBuffer, (mBuffer + mCountFromMozilla), 
                          (mCountFromJava - mCountFromMozilla));
            *aNumRead = (mCountFromJava - mCountFromMozilla);
            
            mCountFromMozilla += (mCountFromJava - mCountFromMozilla);
        }
        mAvailableForMozilla -= *aNumRead;
        rv = NS_OK;
    }
    else {
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("InputStreamShim::Read: exiting, would block\n"));
        rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
    
    PR_Unlock(mLock);
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::Read: exiting\n"));

    return rv;
}

NS_IMETHODIMP 
InputStreamShim::ReadSegments(nsWriteSegmentFun writer, void * aClosure, 
                              PRUint32 aCount, PRUint32 *aNumRead)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!writer || !aNumRead) {
        return NS_ERROR_NULL_POINTER;
    }
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::ReadSegments: entering\n"));
    if (mDoClose && 0 == mAvailableForMozilla) {
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("InputStreamShim::ReadSegments: exiting, already closed\n"));
        return mCloseStatus;
    }
    *aNumRead = 0;
    PR_ASSERT(mLock);
    PR_ASSERT(mCountFromMozilla <= mCountFromJava);

    PR_Lock(mLock);

    PRInt32 bytesToWrite = mAvailableForMozilla;
    PRUint32 bytesWritten;
    PRUint32 totalBytesWritten = 0;

    if (bytesToWrite > aCount) {
        bytesToWrite = aCount;
    }

    if (0 == mAvailableForMozilla) {
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("InputStreamShim::Read: exiting, would block\n"));
        rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
    
    while (bytesToWrite) {
        // what she's asking for is less than or equal to what we have
        rv = writer(this, aClosure, 
                    (mBuffer + mCountFromMozilla),
                    totalBytesWritten, bytesToWrite, &bytesWritten);
        if (NS_FAILED(rv)) {
            break;
        }
        
        bytesToWrite -= bytesWritten;
        totalBytesWritten += bytesWritten;
        mCountFromMozilla += bytesWritten;
    }
    
    *aNumRead = totalBytesWritten;
    mAvailableForMozilla -= totalBytesWritten;
    
    PR_Unlock(mLock);
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("InputStreamShim::ReadSegments: exiting\n"));
}

NS_IMETHODIMP 
InputStreamShim::IsNonBlocking(PRBool *_retval)
{
    *_retval = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::CloseWithStatus(nsresult astatus) 
{
    this->Close();
    mCloseStatus = astatus;
    return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::AsyncWait(nsIInputStreamCallback *aCallback, 
                           PRUint32 aFlags, PRUint32 aRequestedCount, 
                           nsIEventTarget *aEventTarget)
{
    PR_Lock(mLock);
    
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
        (mAvailableForMozilla && !(aFlags & WAIT_CLOSURE_ONLY))) {
        // stream is already closed or readable; post event.
        aCallback->OnInputStreamReady(this);
    }
    else {
        // queue up callback object to be notified when data becomes available
        mCallback = aCallback;
        mCallbackFlags = aFlags;
    }
    
    PR_Unlock(mLock);
    
    return NS_OK;
}
