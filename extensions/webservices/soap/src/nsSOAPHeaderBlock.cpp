/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsSOAPHeaderBlock.h"
#include "nsSOAPUtils.h"
#include "nsIServiceManager.h"
#include "nsISOAPAttachments.h"
#include "nsISOAPMessage.h"
#include "nsSOAPException.h"

nsSOAPHeaderBlock::nsSOAPHeaderBlock()
{
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
  if (mElement) {
    if (mVersion == nsISOAPMessage::VERSION_UNKNOWN)
      return SOAP_EXCEPTION(NS_ERROR_NOT_AVAILABLE,"SOAP_HEADER_INIT", "Header has not been properly initialized.");
    return mElement->GetAttributeNS(*gSOAPStrings->kSOAPEnvURI[mVersion],
                                    gSOAPStrings->kActorAttribute,
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
  if (mElement) {
    if (mVersion == nsISOAPMessage::VERSION_UNKNOWN)
      return SOAP_EXCEPTION(NS_ERROR_NOT_AVAILABLE,"SOAP_HEADER_INIT", "Header has not been properly initialized.");
    nsAutoString m;
    nsresult
        rc =
        mElement->
        GetAttributeNS(*gSOAPStrings->kSOAPEnvURI[mVersion],
                       gSOAPStrings->kMustUnderstandAttribute, m);
    if (NS_FAILED(rc))
      return rc;
    if (m.IsEmpty())
      *aMustUnderstand = PR_FALSE;
    else if (m.Equals(gSOAPStrings->kTrue)
             || m.Equals(gSOAPStrings->kTrueA))
      *aMustUnderstand = PR_TRUE;
    else if (m.Equals(gSOAPStrings->kFalse)
             || m.Equals(gSOAPStrings->kFalseA))
      *aMustUnderstand = PR_FALSE;
    else
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_HEADER_MUSTUNDERSTAND", "Must understand value in header has an illegal value.");
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
