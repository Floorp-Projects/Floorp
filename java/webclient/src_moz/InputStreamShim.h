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

#ifndef InputStreamShim_h
#define InputStreamShim_h

#include "nsIAsyncInputStream.h"
#include "nsCOMPtr.h"

#include <jni.h>



class InputStreamShimActionEvent;
struct PRLock; 

class InputStreamShim : public nsIAsyncInputStream
{
public:
    InputStreamShim(jobject yourJavaStreamRef,
                    PRInt32 contentLength);
    virtual ~InputStreamShim();

    /**

     * Called on NativeEventThread from
     * wsLoadFromStreamEvent::handleEvent()

     * When called, this method will read as up to mContentLength data
     * from the Java input stream.  Each read will update the number of
     * bytes available on the stream.

     * if the ivar mDoClose's value is PR_TRUE, the java
     * InputStream should be closed.

     * @return NS_ERROR_NOT_AVAILABLE when there is no more data.
    
     * @return NS_OK when there is more data
     
     * @return NS_ERROR_FAILURE when there has been an unrecoverable
     * error
     

     */

    nsresult doReadFromJava(void);
    PRInt32 getContentLength() const { return mContentLength; };

private:

    // called from doReadFromJava.  These methods assume they execute
    // within PR_Lock(mLock)

    nsresult doAvailable(void);
    nsresult doRead(void);
    nsresult doClose(void);

private:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIInputStream methods
    NS_DECL_NSIINPUTSTREAM
    
    // nsIAsyncInputStream
    NS_DECL_NSIASYNCINPUTSTREAM
protected:

    /**

     * A global jobject reference for the java InputStream

     * MUST be ::util_deleteGlobalRef'd in destructor.

     */ 

    jobject mJavaStream;

    /**

     * the number of bytes we expect to read.

     */

    PRInt32 mContentLength;

    
    /**

     * a dynamically allocated buffer, written by NativeEventThread
     * calling out doReadFromJava(), read by Mozilla calling our Read().

     * deleting this buffer signifies the java InputStream should be
     * closed.

     * MUST be deallocated in the destructor.

     */
     
    char *mBuffer;
    
    /**
       
     * the length of the buffer
     
     */
    
    PRUint32 mBufferLength;
    

    /**
     
     * Running sum of the total number of bytes read so far from java.

     */ 

    PRUint32 mCountFromJava;

    /**
     
     * Running sum of the total number of bytes read so far by mozilla

     */ 

    PRUint32 mCountFromMozilla;

    /**

     * The number of bytes available on the java InputStream

     */

    PRInt32 mAvailable;

    /**

     * The number of bytes available in the buffer that haven't yet been
     * read from Mozilla.

     */

    PRInt32 mAvailableForMozilla;

    /**

     * The number of bytes read on the last read from the java
     * InputStream

     */

    PRInt32 mNumRead;

    PRBool mDoClose;

    PRBool mDidClose;

    /**

     * Provides mutex

     */

    PRLock *mLock;

    nsresult mCloseStatus;

    nsCOMPtr<nsIInputStreamCallback> mCallback;

    PRUint32 mCallbackFlags;
};

#endif // InputStreamShim_h
