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
#include <stdio.h>
#include <string.h>
#include "mimemoz2.h"
#include "plugin_inst.h"
#include "rebuffer.h"

static NS_DEFINE_IID(kINetPluginInstanceIID,  NS_INETPLUGININSTANCE_IID);
static NS_DEFINE_IID(kINetOStreamIID,         NS_INETOSTREAM_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

nsresult 
NS_NewMimePluginInstance(MimePluginInstance **aInstancePtrResult)
{
	/* Note this new macro for assertions...they can take 
   * a string describing the assertion */
  nsresult res = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
    MimePluginInstance* inst = new MimePluginInstance();
    if (inst == NULL)
      return NS_ERROR_OUT_OF_MEMORY;

    res = inst->QueryInterface(nsINetPluginInstance::GetIID(), (void **)aInstancePtrResult);
    if (res != NS_OK)
    {
      *aInstancePtrResult = NULL;
      delete inst;
    }

    return res;
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_METHOD
MimePluginInstance::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER; 
  
  // Always NULL result, in case of failure   
  *aInstancePtr = NULL;   
  
  if (aIID.Equals(kINetPluginInstanceIID))
    *aInstancePtr = (void*) this; 
  else if (aIID.Equals(kISupportsIID))
    *aInstancePtr = (void*) ((nsISupports*)(nsINetPluginInstance *)this); 
  else if (aIID.Equals(kINetOStreamIID))
    *aInstancePtr = (void *)((nsINetOStream *)this);
  
  if (*aInstancePtr == NULL)
    return NS_NOINTERFACE;
  
  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}

NS_IMPL_ADDREF(MimePluginInstance);
NS_IMPL_RELEASE(MimePluginInstance);

////////////////////////////////////////////////////////////////////////////////
// MimePluginInstance Methods
////////////////////////////////////////////////////////////////////////////////

MimePluginInstance::MimePluginInstance(void)
{
  NS_INIT_REFCNT();  
  mOutStream = NULL;
  mBridgeStream = NULL;
  mRebuffer = NULL;
}

MimePluginInstance::~MimePluginInstance(void)
{
}

NS_METHOD
MimePluginInstance::Initialize(nsINetOStream* stream, const char *stream_name)
{
  mOutStream = stream;
  mTotalWritten = 0;
  mTotalRead = 0;

  mBridgeStream = mime_bridge_create_stream(this, stream_name, FO_NGLAYOUT);
  if (!mBridgeStream)
    return NS_ERROR_OUT_OF_MEMORY;

  // Create rebuffering object
  mRebuffer = new MimeRebuffer();
  return NS_OK;
}

NS_METHOD
MimePluginInstance::Destroy(void)
{
  return NS_OK;
}

NS_METHOD
MimePluginInstance::GetMIMEOutput(const char* *result)
{
  *result = "text/html";
  return NS_OK;
}

NS_METHOD
MimePluginInstance::Start(void)
{
    return NS_OK;
}

NS_METHOD
MimePluginInstance::Stop(void)
{
    return NS_OK;
}

nsresult MimePluginInstance::Close(void)
{
	nsINetOStream *stream = mOutStream;
  nsresult      rc;

  if (!stream)
    return NS_ERROR_FAILURE;

  rc = stream->Close();
  return rc;
}

extern "C" int
sconverter_write_data(void *streamObj, const char *buf, PRUint32 size);

nsresult MimePluginInstance::Complete(void)
{

  nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  //
  // Now complete the stream!
  //
  mime_display_stream_complete(mBridgeStream);

  // Make sure to do necessary cleanup!
  InternalCleanup();

  return stream->Complete();
}

nsresult MimePluginInstance::InternalCleanup(void)
{
  // If we are here and still have data to write, we should try
  // to flush it...if we try and fail, we should probably return
  // an error!
  if (mRebuffer->GetSize() > 0)
    sconverter_write_data(this, "", 0);

  printf("TOTAL READ    = %d\n", mTotalRead);
  printf("TOTAL WRITTEN = %d\n", mTotalWritten);
  printf("LEFTOVERS = %d\n", mRebuffer->GetSize());

  // 
  // Now do necessary cleanup!
  //
  mime_bridge_destroy_stream(mBridgeStream);
  if (mRebuffer)
    delete mRebuffer;

  Close();
  return NS_OK;
}

nsresult MimePluginInstance::Abort(PRInt32 status)
{
	nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  mime_display_stream_abort (mBridgeStream, status);

  // Make sure to do necessary cleanup!
  InternalCleanup();

  return stream->Abort(status);
}

nsresult MimePluginInstance::WriteReady(PRUint32 *aReadyCount)
{
	nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  return stream->WriteReady(aReadyCount);
}

nsresult MimePluginInstance::Write(const char* aBuf, PRUint32 aCount, 
                                   PRUint32 *aWriteCount)
{
  nsresult        rc;
	nsINetOStream   *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  // SHERRY mTotalWritten = 0;
  mTotalRead += aCount;
  rc = mime_display_stream_write(mBridgeStream, aBuf, aCount);
  // *aWriteCount = mTotalWritten;
  if (mRebuffer->GetSize() > 0)
    *aWriteCount = 0;
  else
    *aWriteCount = aCount;
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// These are routines necessary for the C based routines in libmime
// to access the new world streams.
////////////////////////////////////////////////////////////////////////////////
extern "C" int
sconverter_write_data(void *streamObj, const char *buf, PRUint32 size)
{
  unsigned int        written = 0;
  MimePluginInstance  *obj = (MimePluginInstance *)streamObj;
  nsINetOStream       *newStream;
  PRUint32            rc, aReadyCount = 0;

  if (!obj)
    return NS_ERROR_FAILURE;
  newStream = (nsINetOStream *)(obj->mOutStream);

  //
  // Make sure that the buffer we are "pushing" into has enough room
  // for the write operation. If not, we have to buffer, return, and get
  // it on the next time through
  //
  rc = newStream->WriteReady(&aReadyCount);

  // First, handle any old buffer data...
  if (obj->mRebuffer->GetSize() > 0)
  {
    if (aReadyCount >= obj->mRebuffer->GetSize())
    {
      rc += newStream->Write(obj->mRebuffer->GetBuffer(), 
                            obj->mRebuffer->GetSize(), &written);
      obj->mTotalWritten += written;
      obj->mRebuffer->ReduceBuffer(written);
    }
    else
    {
      rc += newStream->Write(obj->mRebuffer->GetBuffer(),
                             aReadyCount, &written);
      obj->mTotalWritten += written;
      obj->mRebuffer->ReduceBuffer(written);
      obj->mRebuffer->IncreaseBuffer(buf, size);
      return rc;
    }
  }

  // Now, deal with the new data the best way possible...
  rc = newStream->WriteReady(&aReadyCount);
  if (aReadyCount >= size)
  {
    rc += newStream->Write(buf, size, &written);
    obj->mTotalWritten += written;
    return rc;
  }
  else
  {
    rc += newStream->Write(buf, aReadyCount, &written);
    obj->mTotalWritten += written;
    obj->mRebuffer->IncreaseBuffer(buf+written, (size-written));
    return rc;
  }
}
