/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   stub_bm.c --- stub functions dealing with front-end
                 bookmarks handling.
*/

#include "structs.h"
#include "ntypes.h"
#include "bkmks.h"

/* The list of bookmarks has changed somehow, so any "bookmarks" menu needs to
   be recreated.  This should be a cheap call, just setting a flag in the FE so
   that it knows to recreate the menu later (like, when the user tries to view
   it).  Recreating it immediately would be bad, because this can get called
   much more often than is reasonable. */
void
BMFE_BookmarkMenuInvalid(MWContext* context)
{
}


/* Edit the given item in the bookmarks property window.  If there is no
   bookmarks property window currently, then the FE should ignore this call.
   If the bookmarks property window is currently displaying some other entry,
   then it should save any changes made to that entry (by calling BM_SetName,
   etc.) before loading up this entry. */
void
BMFE_EditItem(MWContext* context,
	      BM_Entry* entry)
{
}

/* Use these to know when to allow refresh */
void
BMFE_StartBatch(MWContext* context)
{
}

void
BMFE_EndBatch(MWContext* context)
{
}

/* The given entry is no longer valid (i.e., the user just deleted it).  So,
   the given pointer is about to become invalid, and the FE should remove any
   references to it it may have.  In particular, if it is the one being edited
   in the bookmarks property window, then the FE should clear that window. */
void
BMFE_EntryGoingAway(MWContext* context,
		    BM_Entry* entry)
{
}

/* We've finished processing What's Changed.  The What's Changed window should
   change to display the summary of what happened.   It should look something
   like this:

             Done checking <157> Bookmarks. 
             <134> documents were reached.
             <27> documents have changed and are marked in blue.

             [ OK ]

   When the user clicks on the OK, the FE should just take down the window.
   (It doesn't matter if the FE calls BM_CancelWhatsChanged(); it will be a
   no-op in this situtation.) */

void
BMFE_FinishedWhatsChanged(MWContext* context,
			  int32 totalchecked,
			  int32 numreached,
			  int32 numchanged)
{
}

/* return the clipboard contents */
void*
BMFE_GetClipContents(MWContext* context,
		     int32* length)
{
}

/* The user has requested to view the given url.  Show it to him in, using some
   appropriate context.  Url may be targeted to a different window */
void
BMFE_GotoBookmark(MWContext* context,
		  const char* url,
		  const char* target)
{
}

/* measure the item and assign the width and height required to draw it into
   the widget into width and height.  This is used only by BM_WidestEntry(); if
   you don't need that call, you can just make this an empty stub. */
void
BMFE_MeasureEntry(MWContext* context,
		  BM_Entry* entry,
		  uint32* width,
		  uint32* height)
{
}

/* Create the bookmarks property window.  If one already exists, just bring it
   to the front.  This will always be immediately followed by a call to
   BMFE_EditItem(). */
void
BMFE_OpenBookmarksWindow(MWContext* context)
{
}

/* Create the find dialog, and fill it in as specified in the given
   structure.  When the user hits the "Find" button in the dialog, call
   BM_DoFindBookmark. */
void*
BMFE_OpenFindWindow(MWContext* context,
		    BM_FindInfo* findInfo)
{
}

/* Refresh each cell between and including first and last in the bookmarks
   widget (if now is TRUE, the FE is expected to redraw them BEFORE returning,
   otherwise the FE can simply invalidate them and wait for the redraw to
   happen).  If BM_LAST_CELL is passed in as last, then it means paint from
   the first to the end. */
void
BMFE_RefreshCells(MWContext* context,
		  int32 first,
		  int32 last,
		  XP_Bool now)
{
}

/* Make sure that the given entry is visible. */
void
BMFE_ScrollIntoView(MWContext* context,
		    BM_Entry* entry)
{
}

/* Save the given bucket o' bits as the clipboard.  This same bucket needs to
   be returned later if BMFE_GetClipContents() is called. */
void
BMFE_SetClipContents(MWContext* context,
		     void* buffer,
		     int32 length)
{
}

/* Resize the widget to accomodate "visibleCount" number of entries vertically
   and the width of widest entry the actual widget should NOT change size, just
   the size of the scrollable area under it */
void
BMFE_SyncDisplay(MWContext* context)
{
}

/* We're in the process of doing a What's Changed operation.  The What's
   Changed window should update to display the URL, the percentage (calculate
   as done*100/total), and the total estimated time (given here as a
   pre-formatted string).  The What's Changed window should end up looking
   something like this:

             Checking <URL>... (<13> left)
             {=====================    } (progress bar)

             Estimated time remaining: <2 hours 13 minutes>
             (Remaining time depends on the sites selected and 
             the network traffic).


             [ Cancel ]

   It's up to the FE to notice the first time this is called and change its
   window to display the info instead of the initial What's Changed screen.

   If the user ever hits Cancel (or does something equivilant, like destroys
   the window), the FE must call BM_CancelWhatsChanged(). */

void
BMFE_UpdateWhatsChanged(MWContext* context,
			const char* url, /* If NULL, just display
					    "Checking..." */
			int32 done,
			int32 total,
			const char* totaltime)
{
}


