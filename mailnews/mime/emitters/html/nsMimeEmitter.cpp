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
#include "rebuffer.h"
#include "nsMimeEmitter.h"
#include "plstr.h"
#include "nsEmitterUtils.h"

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult NS_NewMimeEmitter(nsIMimeEmitter ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take 
     a string describing the assertion */
	nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMimeEmitter *obj = new nsMimeEmitter();
		if (obj)
			return obj->QueryInterface(nsIMimeEmitter::GetIID(), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ADDREF(nsMimeEmitter)
NS_IMPL_RELEASE(nsMimeEmitter)
NS_IMPL_QUERY_INTERFACE(nsMimeEmitter, nsIMimeEmitter::GetIID()); /* we need to pass in the interface ID of this interface */

/*
 * nsIMimeEmitter definitions....
 */
nsMimeEmitter::nsMimeEmitter()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
  
  mOutStream = NULL;
  mBufferMgr = NULL;
  mTotalWritten = 0;
  mTotalRead = 0;
  mDocHeader = PR_FALSE;
}

nsMimeEmitter::~nsMimeEmitter(void)
{
  if (mBufferMgr)
    delete mBufferMgr;
}

// Set the output stream for processed data.
nsresult
nsMimeEmitter::SetOutputStream(nsINetOStream *outStream)
{
  return NS_OK;
}

// Header handling routines.
nsresult
nsMimeEmitter::StartHeader(PRBool rootMailHeader)
{
  mDocHeader = rootMailHeader;

  if (mDocHeader)
    UtilityWrite("<table BORDER=0 BGCOLOR=\"#99FF99\" >");
  else
    UtilityWrite("<table BORDER=0 BGCOLOR=\"#CCCCCC\" >");
  return NS_OK;
}

nsresult
nsMimeEmitter::AddHeaderField(const char *field, const char *value)
{
  char  *newValue = nsEscapeHTML(value);

  if (!newValue)
    return NS_OK;

  UtilityWrite("<TR>");

  UtilityWrite("<td>");
  UtilityWrite("<div align=right>");
  UtilityWrite("<B>");
  UtilityWrite(field);
  UtilityWrite(":");
  UtilityWrite("</B>");
  UtilityWrite("</div>");
  UtilityWrite("</td>");

  UtilityWrite("<td>");
  UtilityWrite(newValue);
  UtilityWrite("</td>");

  UtilityWrite("</TR>");

  PR_FREEIF(newValue);
  return NS_OK;
}

nsresult
nsMimeEmitter::EndHeader()
{
  UtilityWrite("</TABLE>");
  return NS_OK;
}

// Note - these is setup only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult       
nsMimeEmitter::Initialize(nsINetOStream *outStream)
{
  mOutStream = outStream;

  // Create rebuffering object
  mBufferMgr = new MimeRebuffer();

  // Counters for output stream
  mTotalWritten = 0;
  mTotalRead = 0;

  return NS_OK;
}

// Note - this is teardown only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult
nsMimeEmitter::Complete()
{
  // If we are here and still have data to write, we should try
  // to flush it...if we try and fail, we should probably return
  // an error!
  PRUint32      written; 
  if (mBufferMgr->GetSize() > 0)
    Write("", 0, &written);

  printf("TOTAL WRITTEN = %d\n", mTotalWritten);
  printf("LEFTOVERS     = %d\n", mBufferMgr->GetSize());

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// These are routines necessary for the C based routines in libmime
// to access the new world streams.
////////////////////////////////////////////////////////////////////////////////
nsresult
nsMimeEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  unsigned int        written = 0;
  PRUint32            rc, aReadyCount = 0;

  //
  // Make sure that the buffer we are "pushing" into has enough room
  // for the write operation. If not, we have to buffer, return, and get
  // it on the next time through
  //
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

// Attachment handling routines
nsresult
nsMimeEmitter::StartAttachment()
{
  UtilityWrite("<table BORDER=2>");
  return NS_OK;
}

nsresult
nsMimeEmitter::AddAttachmentField(const char *field, const char *value)
{
  char  *newValue = nsEscapeHTML(value);

  UtilityWrite("<TR>");

  UtilityWrite("<td>");
  UtilityWrite("<div align=right>");
  UtilityWrite("<B>");
  UtilityWrite(field);
  UtilityWrite(":");
  UtilityWrite("</B>");
  UtilityWrite("</div>");
  UtilityWrite("</td>");
  UtilityWrite("<td>");
  UtilityWrite(newValue);
  UtilityWrite("</td>");
  UtilityWrite("</TR>");

  PR_FREEIF(newValue);
  return NS_OK;
}

nsresult
nsMimeEmitter::EndAttachment()
{
  UtilityWrite("</TABLE>");
  UtilityWrite("<BR>");
  return NS_OK;
}

nsresult
nsMimeEmitter::UtilityWrite(const char *buf)
{
  PRInt32     tmpLen = PL_strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeEmitter::StartBody()
{
#ifdef rhp_DEBUG
  UtilityWrite("<BR>");
  UtilityWrite("<B>============== STARTING BODY ==============</B>");
  UtilityWrite("<BR>");
#endif

  return NS_OK;
}

nsresult
nsMimeEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{

  Write(buf, size, amountWritten);
  return NS_OK;
}

nsresult
nsMimeEmitter::EndBody()
{
#ifdef rhp_DEBUG
  UtilityWrite("<BR>");
  UtilityWrite("<B>============== ENDING BODY ==============</B>");
  UtilityWrite("<BR>");
#endif
  return NS_OK;
}


/*************
#include "prprf.h"
//  PR_smprintf(buf, 

  * First, let's import some style sheet information and
   * JavaScript!
#define MHTML_STYLE_IMPORT       "<HTML><LINK REL=\"stylesheet\" HREF=\"resource:/res/mail.css\">"
#define MHTML_JS_IMPORT          "<SCRIPT LANGUAGE=\"JavaScript1.2\" SRC=\"resource:/res/mail.js\"></SCRIPT>"

  status = MimeHeaders_write(opt, MHTML_STYLE_IMPORT, PL_strlen(MHTML_STYLE_IMPORT));
  if (status < 0) 
  {
    return status;
  }

  status = MimeHeaders_write(opt, MHTML_JS_IMPORT, PL_strlen(MHTML_JS_IMPORT));
  if (status < 0) 
  {
    return status;
  }
******************/
