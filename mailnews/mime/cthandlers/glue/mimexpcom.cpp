/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIComponentManager.h" 
#include "nsMimeObjectClassAccess.h"
#include "nsMsgMimeCID.h"
#include "nsCOMPtr.h"
static NS_DEFINE_CID(kMimeObjectClassAccessCID, NS_MIME_OBJECT_CLASS_ACCESS_CID); 

/*
 * These calls are necessary to expose the object class heirarchy 
 * to externally developed content type handlers.
 */
extern "C" void *
COM_GetmimeInlineTextClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeInlineTextClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeLeafClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeLeafClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeObjectClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeObjectClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeContainerClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeContainerClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeMultipartClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeMultipartClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeMultipartSignedClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeMultipartSignedClass(&ptr);

  return ptr;
}

extern "C" int  
COM_MimeObject_write(void *mimeObject, char *data, PRInt32 length, 
                     PRBool user_visible_p)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  PRInt32                             rc=-1;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
  { 
    if (NS_SUCCEEDED(objAccess->MimeObjectWrite(mimeObject, data, length, user_visible_p)))
      rc = length;
    else
      rc = -1;
  } 

  return rc;
}
