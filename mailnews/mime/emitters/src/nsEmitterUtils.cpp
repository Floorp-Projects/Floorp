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
#include "prmem.h"
#include "plstr.h"
#include "nsMailHeaders.h"
#include "nsIMimeEmitter.h"

extern "C" PRBool
EmitThisHeaderForPrefSetting(PRInt32 dispType, const char *header)
{
  if (AllHeaders == dispType)
    return PR_TRUE;

  if ((!header) || (!*header))
    return PR_FALSE;

  if (MicroHeaders == dispType)
  {
    if (
          (!PL_strcmp(header, HEADER_SUBJECT)) ||
          (!PL_strcmp(header, HEADER_FROM)) ||
          (!PL_strcmp(header, HEADER_DATE))
       )
      return PR_TRUE;
    else
      return PR_FALSE;
  }

  if (NormalHeaders == dispType)
  {
    if (
        (!PL_strcmp(header, HEADER_TO)) ||
        (!PL_strcmp(header, HEADER_SUBJECT)) ||
        (!PL_strcmp(header, HEADER_SENDER)) ||
        (!PL_strcmp(header, HEADER_RESENT_TO)) ||
        (!PL_strcmp(header, HEADER_RESENT_SENDER)) ||
        (!PL_strcmp(header, HEADER_RESENT_FROM)) ||
        (!PL_strcmp(header, HEADER_RESENT_CC)) ||
        (!PL_strcmp(header, HEADER_REPLY_TO)) ||
        (!PL_strcmp(header, HEADER_REFERENCES)) ||
        (!PL_strcmp(header, HEADER_NEWSGROUPS)) ||
        (!PL_strcmp(header, HEADER_MESSAGE_ID)) ||
        (!PL_strcmp(header, HEADER_FROM)) ||
        (!PL_strcmp(header, HEADER_FOLLOWUP_TO)) ||
        (!PL_strcmp(header, HEADER_CC)) ||
        (!PL_strcmp(header, HEADER_BCC))
       )
       return PR_TRUE;
    else
      return PR_FALSE;
  }

  return PR_TRUE;
}
