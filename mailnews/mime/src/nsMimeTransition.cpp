/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"

#include "nsMimeTransition.h"

#include "msgCore.h"
#include "mimebuf.h"

/* 
 * This is a transitional file that will help with the various message
 * ID definitions and the functions to retrieve the error string
 */

#if 0
extern "C" NET_cinfo *
NET_cinfo_find_type (char *uri)
{
  return NULL;  
}

extern "C" NET_cinfo *
NET_cinfo_find_info_by_type (char *uri)
{
  return NULL;  
}
#endif

/////////////////////////////////////////////////////////////////////
// THIS IS ONLY HERE UNTIL I CAN GET THE INTERNAL EMITTER OUT OF
// LIBMIME
/////////////////////////////////////////////////////////////////////
#include "nsIMimeEmitter.h"
#include "nsMailHeaders.h"

extern "C" PRBool
EmitThisHeaderForPrefSetting(PRInt32 dispType, const char *header)
{
	if (nsMimeHeaderDisplayTypes::AllHeaders == dispType)
    return PR_TRUE;

  if ((!header) || (!*header))
    return PR_FALSE;

  if (nsMimeHeaderDisplayTypes::MicroHeaders == dispType)
  {
    if (
          (!nsCRT::strcmp(header, HEADER_SUBJECT)) ||
          (!nsCRT::strcmp(header, HEADER_FROM)) ||
          (!nsCRT::strcmp(header, HEADER_DATE))
       )
      return PR_TRUE;
    else
      return PR_FALSE;
  }

  if (nsMimeHeaderDisplayTypes::NormalHeaders == dispType)
  {
    if (
        (!nsCRT::strcmp(header, HEADER_TO)) ||
        (!nsCRT::strcmp(header, HEADER_SUBJECT)) ||
        (!nsCRT::strcmp(header, HEADER_SENDER)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_TO)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_SENDER)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_FROM)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_CC)) ||
        (!nsCRT::strcmp(header, HEADER_REPLY_TO)) ||
        (!nsCRT::strcmp(header, HEADER_REFERENCES)) ||
        (!nsCRT::strcmp(header, HEADER_NEWSGROUPS)) ||
        (!nsCRT::strcmp(header, HEADER_MESSAGE_ID)) ||
        (!nsCRT::strcmp(header, HEADER_FROM)) ||
        (!nsCRT::strcmp(header, HEADER_FOLLOWUP_TO)) ||
        (!nsCRT::strcmp(header, HEADER_CC)) ||
        (!nsCRT::strcmp(header, HEADER_BCC))
       )
       return PR_TRUE;
    else
      return PR_FALSE;
  }

  return PR_TRUE;
}


#include "nsMimeRebuffer.h"
#include "prmem.h"

MimeRebuffer::MimeRebuffer(void)
{
  mSize = 0;
  mBuf = NULL;
}

MimeRebuffer::~MimeRebuffer(void)
{
  if (mBuf)
  {
    PR_FREEIF(mBuf);
    mBuf = NULL;
  }
}

PRUint32
MimeRebuffer::GetSize()
{
  return mSize;
}

PRUint32      
MimeRebuffer::IncreaseBuffer(const char *addBuf, PRUint32 size)
{
  if ( (!addBuf) || (size == 0) )
    return mSize;

  mBuf = (char *)PR_Realloc(mBuf, size + mSize);
  if (!mBuf)
  {
    mSize = 0;
    return mSize;
  }

  nsCRT::memcpy(mBuf+mSize, addBuf, size);
  mSize += size;
  return mSize;
}

PRUint32      
MimeRebuffer::ReduceBuffer(PRUint32 numBytes)
{
  if (numBytes == 0)
    return mSize;

  if (!mBuf)
  {
    mSize = 0;
    return mSize;
  }

  if (numBytes >= mSize)
  {
    PR_FREEIF(mBuf);
    mBuf = NULL;
    mSize = 0;
    return mSize;
  }

  nsCRT::memcpy(mBuf, mBuf+numBytes, (mSize - numBytes));
  mSize -= numBytes;
  return mSize;
}

char *
MimeRebuffer::GetBuffer()
{
  return mBuf;
}
