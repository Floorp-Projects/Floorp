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

#include "nsMsgComposeParams.h"

nsMsgComposeParams::nsMsgComposeParams() :
  mType(nsIMsgCompType::New),
  mFormat(nsIMsgCompFormat::Default),
  mBodyIsLink(PR_FALSE)
{
	NS_INIT_REFCNT();
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS1(nsMsgComposeParams, nsIMsgComposeParams);

nsMsgComposeParams::~nsMsgComposeParams()
{
}

/* attribute MSG_ComposeType type; */
NS_IMETHODIMP nsMsgComposeParams::GetType(MSG_ComposeType *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  
  *aType = mType;
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeParams::SetType(MSG_ComposeType aType)
{
  mType = aType;
  return NS_OK;
}

/* attribute MSG_ComposeFormat format; */
NS_IMETHODIMP nsMsgComposeParams::GetFormat(MSG_ComposeFormat *aFormat)
{
  NS_ENSURE_ARG_POINTER(aFormat);
  
  *aFormat = mFormat;
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeParams::SetFormat(MSG_ComposeFormat aFormat)
{
  mFormat = aFormat;
  return NS_OK;
}

/* attribute wstring originalMsgURI; */
NS_IMETHODIMP nsMsgComposeParams::GetOriginalMsgURI(PRUnichar * *aOriginalMsgURI)
{
  NS_ENSURE_ARG_POINTER(aOriginalMsgURI);
  
  *aOriginalMsgURI = mOriginalMsgUri.ToNewUnicode();
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeParams::SetOriginalMsgURI(const PRUnichar * aOriginalMsgURI)
{
  mOriginalMsgUri = aOriginalMsgURI;
  return NS_OK;
}

/* attribute nsIMsgIdentity identity; */
NS_IMETHODIMP nsMsgComposeParams::GetIdentity(nsIMsgIdentity * *aIdentity)
{
  NS_ENSURE_ARG_POINTER(aIdentity);
  
  if (mIdentity)
  {
     *aIdentity = mIdentity;
     NS_ADDREF(*aIdentity);
  }
  else
    *aIdentity = nsnull;
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeParams::SetIdentity(nsIMsgIdentity * aIdentity)
{
  mIdentity = aIdentity;
  return NS_OK;
}

/* attribute nsIMsgCompFields composeFields; */
NS_IMETHODIMP nsMsgComposeParams::GetComposeFields(nsIMsgCompFields * *aComposeFields)
{
  NS_ENSURE_ARG_POINTER(aComposeFields);
  
  if (mComposeFields)
  {
     *aComposeFields = mComposeFields;
     NS_ADDREF(*aComposeFields);
  }
  else
    *aComposeFields = nsnull;
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeParams::SetComposeFields(nsIMsgCompFields * aComposeFields)
{
  mComposeFields = aComposeFields;
  return NS_OK;
}

/* attribute boolean bodyIsLink; */
NS_IMETHODIMP nsMsgComposeParams::GetBodyIsLink(PRBool *aBodyIsLink)
{
  NS_ENSURE_ARG_POINTER(aBodyIsLink);
  
  *aBodyIsLink = mBodyIsLink;
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeParams::SetBodyIsLink(PRBool aBodyIsLink)
{
  mBodyIsLink = aBodyIsLink;
  return NS_OK;
}
