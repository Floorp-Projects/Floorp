/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "stdio.h"
#include "mimecom.h"
#include "nscore.h"
#include "nsMimeObjectClassAccess.h"

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ISUPPORTS1(nsMimeObjectClassAccess, nsIMimeObjectClassAccess)

/*
 * nsMimeObjectClassAccess definitions....
 */

/* 
 * Inherited methods for nsMimeObjectClassAccess
 */
nsMimeObjectClassAccess::nsMimeObjectClassAccess()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsMimeObjectClassAccess::~nsMimeObjectClassAccess()
{
}

nsresult
nsMimeObjectClassAccess::MimeObjectWrite(void *mimeObject, 
                                char *data, 
                                PRInt32 length, 
                                PRBool user_visible_p)
{
  int rc = XPCOM_MimeObject_write(mimeObject, data, length, user_visible_p);
  if (rc < 0)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeInlineTextClass(void **ptr)
{
  *ptr = XPCOM_GetmimeInlineTextClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeLeafClass(void **ptr)
{
  *ptr = XPCOM_GetmimeLeafClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeObjectClass(void **ptr)
{
  *ptr = XPCOM_GetmimeObjectClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeContainerClass(void **ptr)
{
  *ptr = XPCOM_GetmimeContainerClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeMultipartClass(void **ptr)
{
  *ptr = XPCOM_GetmimeMultipartClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeMultipartSignedClass(void **ptr)
{
  *ptr = XPCOM_GetmimeMultipartSignedClass();
  return NS_OK;
}
