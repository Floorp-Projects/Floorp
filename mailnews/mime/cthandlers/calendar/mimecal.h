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
#ifndef _MIMECAL_H_
#define _MIMECAL_H_

#include "mimetext.h"

/* The MimeInlineTextCalendar class implements the text/calendar MIME
   content types.
 */

typedef struct MimeInlineTextCalendarClass MimeInlineTextCalendarClass;
typedef struct MimeInlineTextCalendar      MimeInlineTextCalendar;

struct MimeInlineTextCalendarClass {
    MimeInlineTextClass text;
    char* buffer;
    PRInt32 bufferlen;
    PRInt32 buffermax;
};

extern MimeInlineTextCalendarClass mimeInlineTextCalendarClass;

struct MimeInlineTextCalendar {
  MimeInlineText text;
};

#endif /* _MIMECAL_H_ */
