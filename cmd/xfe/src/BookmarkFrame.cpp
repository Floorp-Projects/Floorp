/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   BookmarkFrame.cpp -- Bookmark window stuff
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#include "BookmarkFrame.h"
#include "xpassert.h"

#if DEBUG_dora
#define D(x) x
#else
#define D(x)
#endif

// The singleton bookmark frame
/* static */ XFE_BookmarkFrame * XFE_BookmarkFrame::m_bookmarkFrame = NULL;

MWContext *XFE_BookmarkFrame::main_bm_context = NULL;

XFE_BookmarkFrame::XFE_BookmarkFrame(Widget toplevel)
  : XFE_Frame("bookmarks", toplevel,
			  NULL,  //parent_frame
			  FRAME_BOOKMARK, 
			  NULL, //chromespec
			  False,
			  True,
			  False,
			  True,
			  False) // m_destroyOnClose, always keep one Bookmark instance
{
  D(printf("XFE_BookmarkFrame SUB-SYSTEM INSTANCIATING\n");)

  XP_ASSERT( m_bookmarkFrame == NULL );

  main_bm_context = m_context;

  m_bookmarkFrame = this;
}

XFE_BookmarkFrame::~XFE_BookmarkFrame()
{
	D(printf("XFE_BookmarkFrame SUB-SYSTEM DESTRUCTING\n");)

	m_bookmarkFrame = NULL;
	main_bm_context = NULL;
}

extern "C" MWContext* fe_getBookmarkContext()
{
    if (XFE_BookmarkFrame::main_bm_context == NULL)
        XFE_BookmarkFrame::createBookmarkFrame(FE_GetToplevelWidget());
	return XFE_BookmarkFrame::main_bm_context;
}

//////////////////////////////////////////////////////////////////////////

/* static */ void 
XFE_BookmarkFrame::createBookmarkFrame(Widget		toplevel)
{
	if (m_bookmarkFrame == NULL)
	{
		m_bookmarkFrame = new XFE_BookmarkFrame(toplevel);
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ XFE_BookmarkFrame * 
XFE_BookmarkFrame::getBookmarkFrame()
{
	XP_ASSERT( m_bookmarkFrame != NULL );

	return m_bookmarkFrame;
}
//////////////////////////////////////////////////////////////////////////

