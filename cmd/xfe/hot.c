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
/* */
/* hot.c --- bookmarks dialogs
   Created: Terry Weissman <terry@netscape.com>, 20-Jul-95.
 */


#include "mozilla.h"
#include "xfe.h"
#include "bkmks.h" 

extern MWContext *fe_getBookmarkContext();

Boolean
fe_SaveBookmarks (void)
{
  if (fe_getBookmarkContext()) 
    if (BM_SaveBookmarks(fe_getBookmarkContext(), NULL) < 0)
      return False;
  return True;
}

void
fe_AddToBookmark (MWContext *context, const char *title, URL_Struct *url,
		  time_t time)
{
  BM_Entry *bm;
  const char *new_title;
  MWContext *bmcontext = NULL;


  if (!title || !*title) new_title = url->address;
  else new_title = title;
  bm = (BM_Entry*) BM_NewUrl(new_title, url->address, NULL, time);

  bmcontext = fe_getBookmarkContext();
  BM_AppendToHeader (bmcontext, BM_GetAddHeader(bmcontext), bm);
}

void
fe_AddBookmarkCallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  History_entry *h = SHIST_GetCurrent (&context->hist);
  BM_Entry *bm;
  MWContext *bmcontext = NULL;
  char *new_title;

  if (!h) return;

  if (!h->title || !*h->title) new_title = h->address;
  else new_title = h->title;
  bm = (BM_Entry*)BM_NewUrl( new_title, h->address, NULL, h->last_access);

  fe_UserActivity (context);

  bmcontext = fe_getBookmarkContext();
  BM_AppendToHeader (bmcontext, BM_GetAddHeader(bmcontext), bm);
}
