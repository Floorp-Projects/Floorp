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
#include "plugin_inst.h"

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
  OutStream = NULL;
  first_write = PR_FALSE;
}

MimePluginInstance::~MimePluginInstance(void)
{
}

NS_METHOD
MimePluginInstance::Initialize(nsINetOStream* stream)
{
    OutStream = stream;
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

NS_METHOD
MimePluginInstance::Destroy(void)
{
    return NS_OK;
}

nsresult MimePluginInstance::Complete(void)
{
	nsINetOStream *stream = OutStream;

	if (stream)
	{
		if (first_write)
		{
			unsigned int written;
			stream->Write("</pre>", 0, 6, &written);
		}
		return stream->Complete();
	}
	return NS_OK;
}

nsresult MimePluginInstance::Abort(PRInt32 status)
{
	nsINetOStream *stream = OutStream;

	if (stream)
	{
		return stream->Abort(status);
	}
	return NS_OK;
}

nsresult MimePluginInstance::WriteReady(PRUint32 *aReadyCount)
{
	nsINetOStream *stream = OutStream;

	if (stream)
	{
		return stream->WriteReady(aReadyCount);
	}
	return NS_OK;
}

nsresult MimePluginInstance::Write(const char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aWriteCount)
{
	nsINetOStream *stream = OutStream;

	if (stream)
	{
		if (!first_write)
		{
			unsigned int written;
			stream->Write("<pre>", 0, 5, &written);
			first_write = PR_TRUE;
		}
		return stream->Write(aBuf, aOffset, aCount, aWriteCount);
	}
	return NS_OK;
}

nsresult MimePluginInstance::Close(void)
{
	nsINetOStream *stream = OutStream;

	if (stream)
	{
		return stream->Close();
	}
	return NS_OK;
}
