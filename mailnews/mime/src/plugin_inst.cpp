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
#include "nsIMimeEmitter.h"
#include "nsRepository.h"

// Need this for FO_NGLAYOUT
#include "net.h"

/* net.h includes xp_core.h which has trouble with "Bool" */
#ifdef XP_UNIX
#undef Bool
#endif

static NS_DEFINE_IID(kINetPluginInstanceIID,  NS_INETPLUGININSTANCE_IID);
static NS_DEFINE_IID(kINetOStreamIID,         NS_INETOSTREAM_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

extern "C" nsresult 
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
  mTotalRead = 0;
}

MimePluginInstance::~MimePluginInstance(void)
{
}

static NS_DEFINE_IID(kMimeEmitterCID, NS_MIME_EMITTER_CID);

NS_METHOD
MimePluginInstance::Initialize(nsINetOStream* stream, const char *stream_name)
{
  int         format_out = FO_NGLAYOUT;

  mOutStream = stream;

  // I really think there needs to be a way to handle multiple output formats
  // We will need to find an emitter in the repository that supports the appropriate
  // output format. 
  nsresult res = nsRepository::CreateInstance(kMimeEmitterCID, 
                                  NULL, nsIMimeEmitter::GetIID(), 
                                  (void **) &mEmitter); 
  if ((NS_OK != res) || (!mEmitter))
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mBridgeStream = mime_bridge_create_stream(this, mEmitter, stream_name, format_out);
  if (!mBridgeStream)
  {
    mEmitter->Release();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mEmitter->Initialize(stream);
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
//  *result = "image/jpeg";
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

nsresult MimePluginInstance::Complete(void)
{

  nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  //
  // Now complete the stream!
  //
  mime_display_stream_complete((nsMIMESession *)mBridgeStream);

  // Make sure to do necessary cleanup!
  InternalCleanup();

  return stream->Complete();
}

nsresult MimePluginInstance::InternalCleanup(void)
{
  // 
  // Now complete the emitter and do necessary cleanup!
  //
  printf("TOTAL READ    = %d\n", mTotalRead);
  mEmitter->Complete();

  if (mEmitter)
    mEmitter->Release();

  mime_bridge_destroy_stream(mBridgeStream);

  Close();
  return NS_OK;
}

nsresult MimePluginInstance::Abort(PRInt32 status)
{
	nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  mime_display_stream_abort ((nsMIMESession *)mBridgeStream, status);

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

  mTotalRead += aCount;
  rc = mime_display_stream_write((nsMIMESession *) mBridgeStream, aBuf, aCount);
  // SHERRY - returning the amount read here?
  *aWriteCount = aCount;
  return rc;
}

