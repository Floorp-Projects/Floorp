/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* felocale.h - header file for things exported by locale.c */


#ifdef	__cplusplus
extern "C" {
#endif

#include <libi18n.h>

#include <Xm/Xm.h>

#include "xp_core.h"
#include "fonts.h"

extern int16 fe_LocaleCharSetID;

extern char fe_LocaleCharSetName[];

XP_BEGIN_PROTOS

unsigned char *
fe_ConvertFromLocaleEncoding(int16, unsigned char *);

unsigned char *
fe_ConvertToLocaleEncoding(int16, unsigned char *);
#define INTL_CONVERT_BUF_TO_LOCALE(s,l) \
    (l) = (char*)fe_ConvertToLocaleEncoding(\
                INTL_DefaultWinCharSetID(NULL), \
                (unsigned char*)(s)); \
    if ((NULL!=(l)) && ((l) != (s))) \
      XP_STRNCPY_SAFE((s), (l), sizeof(s)), XP_FREE(l);


XmString
fe_ConvertToXmString(unsigned char *, int16, fe_Font, XmFontType, XmFontList *);

char *
fe_GetNormalizedLocaleName(void);

char *
fe_GetTextSelection(Widget);

char *
fe_GetTextField(Widget);

void
fe_InitCollation(void);

void
fe_SetTextField(Widget, const char *);

void
fe_SetTextFieldAndCallBack(Widget, const char *);

XP_END_PROTOS

#ifdef	__cplusplus
}
#endif

