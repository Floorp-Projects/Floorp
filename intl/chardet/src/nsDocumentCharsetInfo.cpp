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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCharDetDll.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsDocumentCharsetInfo.h"
#include "nsCOMPtr.h"

// XXX doc me

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDocumentCharsetInfo, nsIDocumentCharsetInfo);

nsDocumentCharsetInfo::nsDocumentCharsetInfo() 
{
  NS_INIT_REFCNT();
}

nsDocumentCharsetInfo::~nsDocumentCharsetInfo() 
{
}

NS_IMETHODIMP nsDocumentCharsetInfo::SetForcedCharset(nsIAtom * aCharset)
{
  mForcedCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::GetForcedCharset(nsIAtom ** aResult)
{
  *aResult = mForcedCharset;
  if (mForcedCharset) NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::SetForcedDetector(PRBool aForced)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::GetForcedDetector(PRBool * aResult)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::SetParentCharset(nsIAtom * aCharset)
{
  mParentCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::GetParentCharset(nsIAtom ** aResult)
{
  *aResult = mParentCharset;
  if (mParentCharset) NS_ADDREF(*aResult);
  return NS_OK;
}


