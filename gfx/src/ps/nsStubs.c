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

// STOLEN for mozilla/include/libi18n.h
// XXX: Need a real implementation for this

#include "prtypes.h"
#include "structs.h"

void 
INTL_CharSetIDToName(int16 csid, char  *charset)
{
	//XXX: Fill me in with code from mozilla/lib/libi18n
}

/*
** Here's a lovely hack.  This routine draws the header or footer for a page.
*/

/* XXX: Added this from print.c */

void
xl_annotate_page(MWContext *cx, char *template, int y, int delta_dir, int pn)
{
}

