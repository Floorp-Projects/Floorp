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
#include <string.h>
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

  memcpy(mBuf+mSize, addBuf, size);
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

  memcpy(mBuf, mBuf+numBytes, (mSize - numBytes));
  mSize -= numBytes;
  return mSize;
}

char *
MimeRebuffer::GetBuffer()
{
  return mBuf;
}
