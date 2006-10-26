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

#ifndef npAPInsIInputStreamShim_h
#define npAPInsIInputStreamShim_h

#include "nsIPluginStreamListener.h"
#include "nsIPluginStreamInfo.h"

#include "nsIInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsCOMPtr.h"

struct PRLock; 

class npAPInsIInputStreamShim : public nsIAsyncInputStream
{
public:
    npAPInsIInputStreamShim(nsIPluginStreamListener *plugletListener,
                            nsIPluginStreamInfo *streamInfo);
    virtual ~npAPInsIInputStreamShim();

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIInputStream methods
    NS_DECL_NSIINPUTSTREAM

    // nsIAsyncInputStream
    NS_DECL_NSIASYNCINPUTSTREAM

    NS_IMETHOD AllowStreamToReadFromBuffer(int32 len, void* buf, int32* outWritten);

private:

    NS_IMETHOD DoClose(void);

    NS_IMETHOD CopyFromPluginHostToBuffer(int32 len, void* buf, 
                                          int32* outWritten);

    nsCOMPtr<nsIPluginStreamListener> mPlugletListener;
    nsCOMPtr<nsIPluginStreamInfo> mStreamInfo;

    /**

     * a dynamically allocated buffer, written by nppluglet.cpp's
     * nsPluginInstance::Write(), read by
     * PlugletStreamListener::OnDataAvailable calling our Read().

     * deleting this buffer signifies the nppluglet's InputStream should
     * be closed.

     * MUST be deallocated in the destructor.

     */
     
    char *mBuffer;
    
    /**
       
     * the length of the buffer
     
     */
    
    PRUint32 mBufferLength;
    

    /**
     
     * Running sum of the total number of bytes read so far from the
     * npapi plugin.

     */ 

    PRUint32 mCountFromPluginHost;

    /**
     
     * Running sum of the total number of bytes read so far by pluglet

     */ 

    PRUint32 mCountFromPluglet;

    /**

     * The number of bytes available on the plugin's InputStream

     */

    PRInt32 mAvailable;

    /**

     * The number of bytes available in the buffer that haven't yet been
     * read by the pluglet.

     */

    PRInt32 mAvailableForPluglet;

    /**

     * The number of bytes written to our internal buffer on the last
     * call to AllowStreamToReadFromBuffer.

     */

    PRInt32 mNumWrittenFromPluginHost;

    PRBool mDoClose;

    PRBool mDidClose;

    /**

     * Provides mutex

     */

    PRLock *mLock;

    nsresult mCloseStatus;

    nsCOMPtr<nsIInputStreamCallback> mCallback;

    PRUint32 mCallbackFlags;

    static const PRUint32 INITIAL_BUFFER_LENGTH;
    static const PRInt32 buffer_increment;
    static const PRInt32 do_close_code;


};

#endif // npAPInsIInputStreamShim_h
