/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "stdio.h"
#include "nsMimeBaseEmitter.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsEmitterUtils.h"
#include "nsFileStream.h"
#include "nsMimeStringResources.h"
#include "msgCore.h"
#include "nsIMsgHeaderParser.h"
#include "nsIComponentManager.h"
#include "nsEmitterUtils.h"
#include "nsFileSpec.h"
#include "nsIRegistry.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

NS_IMPL_ISUPPORTS2(nsMimeBaseEmitter, nsIMimeEmitter, nsIPipeObserver)

nsMimeBaseEmitter::nsMimeBaseEmitter()
{
  NS_INIT_REFCNT(); 

#ifdef DEBUG_rhp
  mLogFile = nsnull;
#endif

  mBufferMgr = NULL;
  mTotalWritten = 0;
  mTotalRead = 0;
  mDocHeader = PR_FALSE;
  mAttachContentType = NULL;

  mInputStream = nsnull;
  mOutStream = nsnull;
  mOutListener = nsnull;
  mURL = nsnull;
  mHeaderDisplayType = nsMimeHeaderDisplayTypes::NormalHeaders;

  nsresult rv = nsServiceManager::GetService(kPrefCID, nsIPref::GetIID(), (nsISupports**)&(mPrefs));
  if (! (mPrefs && NS_SUCCEEDED(rv)))
    return;

  if ((mPrefs && NS_SUCCEEDED(rv)))
    mPrefs->GetIntPref("mail.show_headers", &mHeaderDisplayType);
}

nsMimeBaseEmitter::~nsMimeBaseEmitter(void)
{
  if (mBufferMgr)
    delete mBufferMgr;

  // Release the prefs service
  if (mPrefs)
    nsServiceManager::ReleaseService(kPrefCID, mPrefs);
}

NS_IMETHODIMP
nsMimeBaseEmitter::SetPipe(nsIInputStream * aInputStream, nsIOutputStream *outStream)
{
  mInputStream = aInputStream;
  mOutStream = outStream;
  return NS_OK;
}

// Note - these is setup only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
NS_IMETHODIMP       
nsMimeBaseEmitter::Initialize(nsIURI *url, nsIChannel * aChannel)
{
  // set the url
  mURL = url;
  mChannel = aChannel;

  // Create rebuffering object
  mBufferMgr = new MimeRebuffer();

  // Counters for output stream
  mTotalWritten = 0;
  mTotalRead = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::SetOutputListener(nsIStreamListener *listener)
{
  mOutListener = listener;
  return NS_OK;
}

// Note - this is teardown only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
NS_IMETHODIMP
nsMimeBaseEmitter::Complete()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_FALSE;
#endif

  // If we are here and still have data to write, we should try
  // to flush it...if we try and fail, we should probably return
  // an error!
  PRUint32      written; 
  if ( (mBufferMgr) && (mBufferMgr->GetSize() > 0))
    Write("", 0, &written);

  if (mOutListener)
  {
      PRUint32 bytesInStream;
      mInputStream->Available(&bytesInStream);
      mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, bytesInStream);
  }

#ifdef DEBUG_rhp
  if (mLogFile) 
    PR_Close(mLogFile);
#endif

  return NS_OK;
}

// Header handling routines.
NS_IMETHODIMP
nsMimeBaseEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                           const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::AddHeaderField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::EndHeader()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

// Attachment handling routines
NS_IMETHODIMP
nsMimeBaseEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::AddAttachmentField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::EndAttachment()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

// body handling routines
NS_IMETHODIMP
nsMimeBaseEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsMimeBaseEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::EndBody()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_FALSE;
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsMimeBaseEmitter::UtilityWrite(const char *buf)
{
  PRInt32     tmpLen = nsCRT::strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::UtilityWriteCRLF(const char *buf)
{
  PRInt32     tmpLen = nsCRT::strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
  Write(CRLF, 2, &written);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// These are routines necessary for the C based routines in libmime
// to access the new world streams.
////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsMimeBaseEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  unsigned int        written = 0;
  PRUint32            rc = 0;
  PRUint32            needToWrite;

#ifdef DEBUG_rhp
  if ((mLogFile) && (mReallyOutput))
    PR_Write(mLogFile, buf, size);
#endif

  //
  // Make sure that the buffer we are "pushing" into has enough room
  // for the write operation. If not, we have to buffer, return, and get
  // it on the next time through
  //
  *amountWritten = 0;

  needToWrite = mBufferMgr->GetSize();
  // First, handle any old buffer data...
  if (needToWrite > 0)
  {
    rc += mOutStream->Write(mBufferMgr->GetBuffer(), 
                            needToWrite, &written);

    mTotalWritten += written;
    mBufferMgr->ReduceBuffer(written);
//    mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, written);
    *amountWritten = written;

    // if we couldn't write all the old data, buffer the new data
    // and return
    if (mBufferMgr->GetSize() > 0)
    {
      mBufferMgr->IncreaseBuffer(buf, size);
      return NS_OK;
    }
  }


  // if we get here, we are dealing with new data...try to write
  // and then do the right thing...
  rc = mOutStream->Write(buf, size, &written);
  *amountWritten = written;
  mTotalWritten += written;

  if (written < size)
    mBufferMgr->IncreaseBuffer(buf+written, (size-written));

//  if (mOutListener)
//    mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, written);

  return rc;
}


NS_IMETHODIMP nsMimeBaseEmitter::OnWrite(nsIPipe* aPipe, PRUint32 aCount)
{
  return NS_OK;
}

NS_IMETHODIMP nsMimeBaseEmitter::OnEmpty(nsIPipe* aPipe)
{
  return NS_OK;
}


NS_IMETHODIMP nsMimeBaseEmitter::OnFull(nsIPipe* /* aPipe */)
{
  // the pipe is full so we should flush our data to the converter's listener
  // in order to make more room. 

  // since we only have one pipe per mime emitter, i can ignore the pipe param and use
  // my outStream object directly (it will be the same thing as what we'd get from aPipe.

  nsresult rv = NS_OK;
  if (mOutListener && mInputStream)
  {
      PRUint32 bytesAvailable = 0;
      mInputStream->Available(&bytesAvailable);
      rv = mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, bytesAvailable);
  }
  else 
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}
