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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 */

#include "nsMsgComposeProgressParams.h"


NS_IMPL_ISUPPORTS1(nsMsgComposeProgressParams, nsIMsgComposeProgressParams)

nsMsgComposeProgressParams::nsMsgComposeProgressParams() :
  m_deliveryMode(nsIMsgCompDeliverMode::Now)
{
  NS_INIT_ISUPPORTS();
}

nsMsgComposeProgressParams::~nsMsgComposeProgressParams()
{
}

/* attribute wstring subject; */
NS_IMETHODIMP nsMsgComposeProgressParams::GetSubject(PRUnichar * *aSubject)
{
  NS_ENSURE_ARG(aSubject);
  
  *aSubject = m_subject.ToNewUnicode();
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeProgressParams::SetSubject(const PRUnichar * aSubject)
{
  m_subject = aSubject;
  return NS_OK;
}

/* attribute MSG_DeliverMode deliveryMode; */
NS_IMETHODIMP nsMsgComposeProgressParams::GetDeliveryMode(MSG_DeliverMode *aDeliveryMode)
{
  NS_ENSURE_ARG(aDeliveryMode);
  
  *aDeliveryMode = m_deliveryMode;
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeProgressParams::SetDeliveryMode(MSG_DeliverMode aDeliveryMode)
{
  m_deliveryMode = aDeliveryMode;
  return NS_OK;
}
