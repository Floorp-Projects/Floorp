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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "prmem.h"
#include "plstr.h"
#include "nsMailHeaders.h"
#include "nsIMimeEmitter.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "prprf.h"
#include "nsSpecialSystemDirectory.h"

extern "C" PRBool
EmitThisHeaderForPrefSetting(PRInt32 dispType, const char *header)
{
	if (nsMimeHeaderDisplayTypes::AllHeaders == dispType)
    return PR_TRUE;

  if ((!header) || (!*header))
    return PR_FALSE;

  if (nsMimeHeaderDisplayTypes::MicroHeaders == dispType)
  {
    if (
          (!nsCRT::strcmp(header, HEADER_SUBJECT)) ||
          (!nsCRT::strcmp(header, HEADER_FROM)) ||
          (!nsCRT::strcmp(header, HEADER_DATE))
       )
      return PR_TRUE;
    else
      return PR_FALSE;
  }

  if (nsMimeHeaderDisplayTypes::NormalHeaders == dispType)
  {
    if (
        (!nsCRT::strcmp(header, HEADER_DATE)) ||
        (!nsCRT::strcmp(header, HEADER_TO)) ||
        (!nsCRT::strcmp(header, HEADER_SUBJECT)) ||
        (!nsCRT::strcmp(header, HEADER_SENDER)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_TO)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_SENDER)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_FROM)) ||
        (!nsCRT::strcmp(header, HEADER_RESENT_CC)) ||
        (!nsCRT::strcmp(header, HEADER_REPLY_TO)) ||
        (!nsCRT::strcmp(header, HEADER_REFERENCES)) ||
        (!nsCRT::strcmp(header, HEADER_NEWSGROUPS)) ||
        (!nsCRT::strcmp(header, HEADER_MESSAGE_ID)) ||
        (!nsCRT::strcmp(header, HEADER_FROM)) ||
        (!nsCRT::strcmp(header, HEADER_FOLLOWUP_TO)) ||
        (!nsCRT::strcmp(header, HEADER_CC)) ||
        (!nsCRT::strcmp(header, HEADER_ORGANIZATION)) ||
        (!nsCRT::strcmp(header, HEADER_REPLY_TO)) ||
        (!nsCRT::strcmp(header, HEADER_BCC))
       )
       return PR_TRUE;
    else
      return PR_FALSE;
  }

  return PR_TRUE;
}

