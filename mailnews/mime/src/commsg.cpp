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

/* 
 * nsmsg.cpp - external libmsg calls
 */

#include "xp.h"
#include "xp_time.h"
#include "xplocale.h"
#include "prmem.h"
#include "plstr.h"
#include "msgcom.h"
#include "mimemoz2.h"

extern "C" MSG_Pane * MSG_FindPane(MWContext* context, MSG_PaneType type) 
{
  return NULL;

  /* return MSG_Pane::FindPane(context, type, PR_FALSE); */
}

extern "C" XP_Bool
MSG_ShouldRot13Message(MSG_Pane* messagepane)
{
  return PR_FALSE;
  /*  return CastMessagePane(messagepane)->ShouldRot13Message(); */
}

size_t XP_StrfTime(MWContext* context, char *result, size_t maxsize, int format,
                   const struct tm *timeptr)
{
    /* MOZ_FUNCTION_STUB; */
    return 0;
}

extern "C"
const char* MSG_FormatDateFromContext(MWContext *context, time_t date) 
{
  /* fix i18n.  Well, maybe.  Isn't strftime() supposed to be i18n? */
  /* ftong- Well.... strftime() in Mac and Window is not really i18n 		*/
  /* We need to use XP_StrfTime instead of strftime 						*/
  static char result[40];	/* 30 probably not enough */
  time_t now = time ((time_t *) 0);

  PRInt32 offset = XP_LocalZoneOffset() * 60L; /* Number of seconds between
											 local and GMT. */

  PRInt32 secsperday = 24L * 60L * 60L;

  PRInt32 nowday = (now + offset) / secsperday;
  PRInt32 day = (date + offset) / secsperday;

  if (day == nowday) {
	XP_StrfTime(context, result, sizeof(result), XP_TIME_FORMAT,
				localtime(&date));
  } else if (day < nowday && day > nowday - 7) {
	XP_StrfTime(context, result, sizeof(result), XP_WEEKDAY_TIME_FORMAT,
				localtime(&date));
  } else {
#if defined (XP_WIN)
  if (date < 0 || date > 0x7FFFFFFF)
	date = 0x7FFFFFFF;
#endif
  XP_StrfTime(context, result, sizeof(result), XP_DATE_TIME_FORMAT,
				localtime(&date));
  }

  return result;
}

extern "C" MSG_MessagePaneCallbacks*
MSG_GetMessagePaneCallbacks(MSG_Pane* messagepane,
                            void** closure)
{
    return NULL;
    /* return CastMessagePane(messagepane)->GetMessagePaneCallbacks(closure); */
}
