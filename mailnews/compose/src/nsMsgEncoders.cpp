/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsMsgComposeStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIMimeConverter.h"

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

extern "C" MimeEncoderData *
MIME_B64EncoderInit(int (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                           nsCOMTypeInfo<nsIMimeConverter>::GetIID(), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->B64EncoderInit(output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

extern "C" MimeEncoderData *	
MIME_QPEncoderInit(int (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                           nsCOMTypeInfo<nsIMimeConverter>::GetIID(), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->QPEncoderInit(output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

extern "C" MimeEncoderData *	
MIME_UUEncoderInit(char *filename, int (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                           nsCOMTypeInfo<nsIMimeConverter>::GetIID(), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->UUEncoderInit(filename, output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

extern "C" nsresult
MIME_EncoderDestroy(MimeEncoderData *data, PRBool abort_p) 
{
  //MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                           nsCOMTypeInfo<nsIMimeConverter>::GetIID(), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->EncoderDestroy(data, abort_p);
    NS_RELEASE(converter);
  }

  return NS_SUCCEEDED(res) ? 0 : -1;
}
