/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsSOAPHeaderBlock.h"
#include "nsSOAPUtils.h"
#include "nsIServiceManager.h"
#include "nsISOAPAttachments.h"
#include "nsISOAPMessage.h"

nsSOAPHeaderBlock::nsSOAPHeaderBlock()
{
  NS_INIT_ISUPPORTS();
}

NS_IMPL_CI_INTERFACE_GETTER2(nsSOAPHeaderBlock, nsISOAPBlock,
			     nsISOAPHeaderBlock)
    NS_IMPL_ADDREF_INHERITED(nsSOAPHeaderBlock, nsSOAPBlock)
    NS_IMPL_RELEASE_INHERITED(nsSOAPHeaderBlock, nsSOAPBlock)
    NS_INTERFACE_MAP_BEGIN(nsSOAPHeaderBlock)
    NS_INTERFACE_MAP_ENTRY(nsISOAPHeaderBlock)
    NS_IMPL_QUERY_CLASSINFO(nsSOAPHeaderBlock)
    NS_INTERFACE_MAP_END_INHERITING(nsSOAPBlock) nsSOAPHeaderBlock::
    ~nsSOAPHeaderBlock()
{
}

/* attribute AString actorURI; */
NS_IMETHODIMP nsSOAPHeaderBlock::GetActorURI(nsAString & aActorURI)
{
  NS_ENSURE_ARG_POINTER(&aActorURI);
  if (mElement) {
    if (mVersion == nsISOAPMessage::VERSION_UNKNOWN)
      return NS_ERROR_NOT_AVAILABLE;
    return mElement->GetAttributeNS(*nsSOAPUtils::kSOAPEnvURI[mVersion],
				    nsSOAPUtils::kActorAttribute,
				    aActorURI);
  } else {
    aActorURI.Assign(mActorURI);
  }
  return NS_OK;
}

NS_IMETHODIMP nsSOAPHeaderBlock::SetActorURI(const nsAString & aActorURI)
{
  nsresult rc = SetElement(nsnull);
  if (NS_FAILED(rc))
    return rc;
  mActorURI.Assign(aActorURI);
  return NS_OK;
}

/* attribute AString mustUnderstand; */
NS_IMETHODIMP nsSOAPHeaderBlock::GetMustUnderstand(PRBool *
						   aMustUnderstand)
{
  NS_ENSURE_ARG_POINTER(&aMustUnderstand);
  if (mElement) {
    if (mVersion == nsISOAPMessage::VERSION_UNKNOWN)
      return NS_ERROR_NOT_AVAILABLE;
    nsAutoString m;
    nsresult
	rc =
	mElement->
	GetAttributeNS(*nsSOAPUtils::kSOAPEnvURI[mVersion],
		       nsSOAPUtils::kMustUnderstandAttribute, m);
    if (NS_FAILED(rc))
      return rc;
    if (m.Length() == 0)
      *aMustUnderstand = PR_FALSE;
    else if (m.Equals(nsSOAPUtils::kTrue)
	     || m.Equals(nsSOAPUtils::kTrueA))
      *aMustUnderstand = PR_TRUE;
    else if (m.Equals(nsSOAPUtils::kFalse)
	     || m.Equals(nsSOAPUtils::kFalseA))
      *aMustUnderstand = PR_FALSE;
    else
      return NS_ERROR_ILLEGAL_VALUE;
    return NS_OK;
  } else {
    *aMustUnderstand = mMustUnderstand;
  }
  return NS_OK;
}

NS_IMETHODIMP nsSOAPHeaderBlock::SetMustUnderstand(PRBool aMustUnderstand)
{
  nsresult rc = SetElement(nsnull);
  if (NS_FAILED(rc))
    return rc;
  mMustUnderstand = aMustUnderstand;
  return NS_OK;
}
