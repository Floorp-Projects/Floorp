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
#include "nsRepository.h" 
#include "nsMimeObjectClassAccess.h"

static NS_DEFINE_IID(kIMimeObjectClassAccessIID, NS_IMIME_OBJECT_CLASS_ACCESS_IID);
static NS_DEFINE_CID(kMimeObjectClassAccessCID, NS_MIME_OBJECT_CLASS_ACCESS_CID); 

/*
 * These calls are necessary to expose the object class heirarchy 
 * to externally developed content type handlers.
 */
extern "C" void *
COM_GetmimeInlineTextClass(void)
{
  nsMimeObjectClassAccess	*objAccess;
  void                    *ptr = NULL;

  int res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                                      NULL, kIMimeObjectClassAccessIID, 
                                      (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
    objAccess->GetmimeInlineTextClass(&ptr);
    objAccess->Release(); 
  } 

  return ptr;
}

extern "C" void *
COM_GetmimeLeafClass(void)
{
  nsMimeObjectClassAccess	*objAccess;
  void                    *ptr = NULL;

  int res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                                      NULL, kIMimeObjectClassAccessIID, 
                                      (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
    objAccess->GetmimeLeafClass(&ptr);
    objAccess->Release(); 
  } 

  return ptr;
}

extern "C" void *
COM_GetmimeObjectClass(void)
{
  nsMimeObjectClassAccess	*objAccess;
  void                    *ptr = NULL;

  int res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                                      NULL, kIMimeObjectClassAccessIID, 
                                      (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
    objAccess->GetmimeObjectClass(&ptr);
    objAccess->Release(); 
  } 

  return ptr;
}

extern "C" void *
COM_GetmimeContainerClass(void)
{
  nsMimeObjectClassAccess	*objAccess;
  void                    *ptr = NULL;

  int res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                                      NULL, kIMimeObjectClassAccessIID, 
                                      (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
    objAccess->GetmimeContainerClass(&ptr);
    objAccess->Release(); 
  } 

  return ptr;
}

extern "C" void *
COM_GetmimeMultipartClass(void)
{
  nsMimeObjectClassAccess	*objAccess;
  void                    *ptr = NULL;

  int res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                                      NULL, kIMimeObjectClassAccessIID, 
                                      (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
    objAccess->GetmimeMultipartClass(&ptr);
    objAccess->Release(); 
  } 

  return ptr;
}

extern "C" void *
COM_GetmimeMultipartSignedClass(void)
{
  nsMimeObjectClassAccess	*objAccess;
  void                    *ptr = NULL;

  int res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                                      NULL, kIMimeObjectClassAccessIID, 
                                      (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
    objAccess->GetmimeMultipartSignedClass(&ptr);
    objAccess->Release(); 
  } 

  return ptr;
}

extern "C" int  
COM_MimeObject_write(void *mimeObject, char *data, PRInt32 length, 
                     PRBool user_visible_p)
{
  nsMimeObjectClassAccess	*objAccess;
  int                     rc = -1;

  int res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                                      NULL, kIMimeObjectClassAccessIID, 
                                      (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
    if (objAccess->MimeObjectWrite(mimeObject, data, length, user_visible_p) == NS_OK)
      rc = length;
    else
      rc = -1;
    objAccess->Release(); 
  } 

  return rc;
}






  
/**  
  // NOTE: Need to do these in the global registry
  // register our dll
  nsRepository::RegisterFactory(kRFC822toHTMLStreamConverterCID, 
                                "mime.dll", PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kMimeObjectClassAccessCID,
                                "mime.dll", PR_FALSE, PR_FALSE);
  

*/