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
#include "nsMemory.h"

static const PRInt32 buffer_increment = 5120;
static const PRInt32 do_close_code = -524;

InputStreamShim::InputStreamShim(jobject yourJavaStreamRef, 
                                 PRInt32 yourContentLength) : 
    mJavaStream(yourJavaStreamRef), mContentLength(yourContentLength),
    mBuffer(nsnull), mBufferLength(0), mCountFromJava(0), 
    mCountFromMozilla(0), mAvailable(0), mAvailableForMozilla(0), mNumRead(0),
    mDoClose(PR_FALSE), mDidClose(PR_FALSE), mLock(nsnull) 
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

    nsMemory::Free(mBuffer);
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

NS_IMPL_QUERY_INTERFACE1(InputStreamShim, nsIInputStream)

nsresult InputStreamShim::doReadFromJava()
{
    nsresult rv = NS_ERROR_FAILURE;
    PR_ASSERT(mLock);


    PR_Lock(mLock);

    // first, see how much data is available
    if (NS_FAILED(rv = doAvailable())) {
        goto DRFJ_CLEANUP;
    }

    // if we have all our data, give the error appropriate result
    if (0 == mAvailable ||
        (0 != mCountFromJava && 
         (((PRUint32)mContentLength) == mCountFromJava))) {
        mDoClose = PR_TRUE;
        doClose();
        rv = NS_ERROR_NOT_AVAILABLE;
        goto DRFJ_CLEANUP;
    }
    
    if (NS_FAILED(doRead())) {
        rv = NS_ERROR_FAILURE;
        goto DRFJ_CLEANUP;
    }
    rv = NS_OK;

    // finally, do another check for available bytes
    if (NS_FAILED(doAvailable())) {
        rv = NS_ERROR_FAILURE;
        goto DRFJ_CLEANUP;
    }
    if (0 == mAvailable ||
        (0 != mCountFromJava && 
         (((PRUint32)mContentLength) == mCountFromJava))) {
        mDoClose = PR_TRUE;
        doClose();
        rv = NS_ERROR_NOT_AVAILABLE;
        goto DRFJ_CLEANUP;
    }

 DRFJ_CLEANUP:
    
    PR_Unlock(mLock);

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
    mAvailable = (PRUint32) env->CallIntMethod(mJavaStream, mid);
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

    PR_ASSERT(0 != mAvailable);

    // if we don't have a buffer, create one
    if (!mBuffer) {
        if (0 < mContentLength) {
            mBuffer = (char *) nsMemory::Alloc(mContentLength);
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
            mBuffer = (char *) nsMemory::Alloc(mBufferLength);
                
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
            char *tBuffer = (char *) nsMemory::Alloc(mBufferLength + 
                                                     buffer_increment);
            if (!tBuffer) {
                tBuffer = (char *) malloc(mBufferLength + 
                                          buffer_increment);
                if (!tBuffer) {
                    mDoClose = PR_TRUE;
                    return NS_ERROR_NOT_AVAILABLE;
                }
            }
            // copy the old buffer into the new buffer
            memcpy(tBuffer, mBuffer, mBufferLength);
            // delete the old buffer
            
            nsMemory::Free(mBuffer);

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
    PR_ASSERT(mLock);
    PR_Lock(mLock);
    //    *aResult = mAvailableForMozilla;
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

    *aNumRead = 0;
    PR_ASSERT(mLock);
    PR_ASSERT(mCountFromMozilla <= mCountFromJava);

    PR_Lock(mLock);

    // wait for java to load the buffer with some data
    do {
        if (mAvailableForMozilla == 0) {
            PR_Unlock(mLock);
            PR_Sleep(PR_INTERVAL_MIN);
            PR_Lock(mLock);
            if (mDoClose) {
                PR_Unlock(mLock);
                return NS_OK;
            }
        }
        else {
            break;
        }
    } while ((!mDidClose) && (-1 != mNumRead));

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
    }

    rv = NS_OK;
    PR_Unlock(mLock);

    return rv;
}

NS_IMETHODIMP 
InputStreamShim::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
InputStreamShim::IsNonBlocking(PRBool *_retval)
{
//    NS_NOTREACHED("IsNonBlocking");
//    return NS_ERROR_NOT_IMPLEMENTED;

    *_retval = PR_FALSE;
    return NS_OK;
}
