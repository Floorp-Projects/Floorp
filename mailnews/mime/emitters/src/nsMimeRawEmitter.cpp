/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "stdio.h"
#include "nsMimeRebuffer.h"
#include "nsMimeRawEmitter.h"
#include "plstr.h"
#include "nsEmitterUtils.h"
#include "nsMailHeaders.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nscore.h"

// For the new pref API's
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsresult NS_NewMimeRawEmitter(const nsIID& iid, void **result)
{
	nsMimeRawEmitter *obj = new nsMimeRawEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ADDREF(nsMimeRawEmitter)
NS_IMPL_RELEASE(nsMimeRawEmitter)
NS_IMPL_QUERY_INTERFACE(nsMimeRawEmitter, nsIMimeEmitter::GetIID()); /* we need to pass in the interface ID of this interface */

/*
 * nsIMimeEmitter definitions....
 */
nsMimeRawEmitter::nsMimeRawEmitter()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
  
  mOutStream = NULL;
  mBufferMgr = NULL;
  mTotalWritten = 0;
  mTotalRead = 0;

#ifdef DEBUG_rhp
  mLogFile = NULL;    /* Temp file to put generated XML into. */
  mReallyOutput = PR_FALSE;
#endif

  nsresult rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&(mPrefs));
  if (! (mPrefs && NS_SUCCEEDED(rv)))
    return;
}

nsMimeRawEmitter::~nsMimeRawEmitter(void)
{
  if (mBufferMgr)
    delete mBufferMgr;

  // Release the prefs service
  if (mPrefs)
    nsServiceManager::ReleaseService(kPrefCID, mPrefs);
}

// Set the output stream for processed data.
nsresult
nsMimeRawEmitter::SetOutputStream(nsINetOStream *outStream)
{
  return NS_OK;
}

// Note - these is setup only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult       
nsMimeRawEmitter::Initialize(nsINetOStream *outStream)
{
  mOutStream = outStream;

  // Create rebuffering object
  mBufferMgr = new MimeRebuffer();

  // Counters for output stream
  mTotalWritten = 0;
  mTotalRead = 0;

#ifdef DEBUG_rhp
  PR_Delete("C:\\mail.raw");
  mLogFile = PR_Open("C:\\mail.raw", PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 493);
#endif /* DEBUG */

  return NS_OK;
}

// Note - this is teardown only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult
nsMimeRawEmitter::Complete()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // If we are here and still have data to write, we should try
  // to flush it...if we try and fail, we should probably return
  // an error!
  PRUint32      written; 
  if (mBufferMgr->GetSize() > 0)
    Write("", 0, &written);

  printf("TOTAL WRITTEN = %d\n", mTotalWritten);
  printf("LEFTOVERS     = %d\n", mBufferMgr->GetSize());

#ifdef DEBUG_rhp
  if (mLogFile) 
    PR_Close(mLogFile);
#endif

  return NS_OK;
}

// Header handling routines.
nsresult
nsMimeRawEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                           const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

nsresult
nsMimeRawEmitter::AddHeaderField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

nsresult
nsMimeRawEmitter::EndHeader()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeRawEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

nsresult
nsMimeRawEmitter::AddAttachmentField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

nsresult
nsMimeRawEmitter::EndAttachment()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeRawEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

nsresult
nsMimeRawEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  Write(buf, size, amountWritten);
  return NS_OK;
}

nsresult
nsMimeRawEmitter::EndBody()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// These are routines necessary for the C based routines in libmime
// to access the new world streams.
////////////////////////////////////////////////////////////////////////////////
nsresult
nsMimeRawEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  unsigned int        written = 0;
  PRUint32            rc, aReadyCount = 0;

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
  rc = mOutStream->WriteReady(&aReadyCount);

  // First, handle any old buffer data...
  if (mBufferMgr->GetSize() > 0)
  {
    if (aReadyCount >= mBufferMgr->GetSize())
    {
      rc += mOutStream->Write(mBufferMgr->GetBuffer(), 
                            mBufferMgr->GetSize(), &written);
      mTotalWritten += written;
      mBufferMgr->ReduceBuffer(written);
    }
    else
    {
      rc += mOutStream->Write(mBufferMgr->GetBuffer(),
                             aReadyCount, &written);
      mTotalWritten += written;
      mBufferMgr->ReduceBuffer(written);
      mBufferMgr->IncreaseBuffer(buf, size);
      return rc;
    }
  }

  // Now, deal with the new data the best way possible...
  rc = mOutStream->WriteReady(&aReadyCount);
  if (aReadyCount >= size)
  {
    rc += mOutStream->Write(buf, size, &written);
    mTotalWritten += written;
    *amountWritten = written;
    return rc;
  }
  else
  {
    rc += mOutStream->Write(buf, aReadyCount, &written);
    mTotalWritten += written;
    mBufferMgr->IncreaseBuffer(buf+written, (size-written));
    *amountWritten = written;
    return rc;
  }
}

nsresult
nsMimeRawEmitter::UtilityWrite(const char *buf)
{
  PRInt32     tmpLen = PL_strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
#ifdef DEBUG
//  Write("\r\n", 2, &written);
#endif

  return NS_OK;
}
