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
#ifndef BKMKS_H
#define BKMKS_H

#include "xp_mcom.h"
#include "ntypes.h"

XP_BEGIN_PROTOS

#define BM_LAST_CELL			0xFFFF /* See BMFE_RefreshCells */

typedef time_t BM_Date;

typedef enum {
  BM_Cmd_Invalid,				/* XFE in particular likes to have an invalid
								   ID code as one of the entries here...*/

  BM_Cmd_Open,					/* Open a new bookmark file. */

  BM_Cmd_ImportBookmarks,		/* Import entries from another bookmark
								   file. */

  BM_Cmd_SaveAs,				/* Save these bookmarks into another file. */

  BM_Cmd_Close,					/* Close the bookmarks window.  (NYI ###) */

  BM_Cmd_Undo,					/* Undo the last command. */
  BM_Cmd_Redo,					/* Redo the last undone command. */

  BM_Cmd_Cut,					/* Cut the selected bookmarks. */
  BM_Cmd_Copy,					/* Copy the selected bookmarks. */
  BM_Cmd_Paste,					/* Paste in the most recent cut/copy. */
  BM_Cmd_Delete,				/* Delete the selected bookmarks without
								   affecting the clipboard. */

  BM_Cmd_SelectAllBookmarks,	/* Select every bookmark in the window. */

  BM_Cmd_Find,					/* Find a string. */

  BM_Cmd_FindAgain,				/* Find the same string again. */

  BM_Cmd_BookmarkProps,			/* Bring up the properties window.  */

  BM_Cmd_GotoBookmark,			/* Load the selected bookmark in a browser
								   window. */

                                /* Sort the bookmarks.  NYI### */
  BM_Cmd_Sort_Name,
  BM_Cmd_Sort_Name_Asc,
  
  BM_Cmd_Sort_Address,
  BM_Cmd_Sort_Address_Asc,
  
  BM_Cmd_Sort_AddDate,
  BM_Cmd_Sort_AddDate_Asc,
  
  BM_Cmd_Sort_LastVisit,
  BM_Cmd_Sort_LastVisit_Asc,      
  
  BM_Cmd_Sort_Natural,        
  

  BM_Cmd_InsertBookmark,		/* Insert a new bookmark (or a new address if
								   in addressbook). */

  BM_Cmd_InsertHeader,			/* Insert a new header. */

  BM_Cmd_InsertSeparator,		/* Insert a separator. */

  BM_Cmd_MakeAlias,				/* Make an alias of this bookmark. */

  BM_Cmd_SetAddHeader,			/* Make the current header be the one where new
								   bookmarks are added. */

  BM_Cmd_SetMenuHeader			/* Make the current header be the one which the
								   pulldown menu is created from. */

} BM_CommandType;
#define BM_Cmd_SortBookmarks  BM_Cmd_Sort_Name

typedef enum
{
   BM_Sort_NONE = -1,
   
   BM_Sort_Name,
   BM_Sort_Name_Asc,
   
   BM_Sort_Address,
   BM_Sort_Address_Asc,
   
   BM_Sort_AddDate,
   BM_Sort_AddDate_Asc,
   
   BM_Sort_LastVisit,
   BM_Sort_LastVisit_Asc,
   
   BM_Sort_Natural
   
} BM_SortType;

/* The various types of bookmark entries (as returned by BM_GetType). */

typedef uint16 BM_Type;

#define BM_TYPE_HEADER		0x0001
#define BM_TYPE_URL			0x0002
#define BM_TYPE_ADDRESS		0x0004
#define BM_TYPE_SEPARATOR	0x0008
#define BM_TYPE_ALIAS		0x0010 


/* This represents one line in the bookmarks window -- a URL, a
   separater, an addressbook entry, a header, etc.  It's also already
   defined in ntypes.h, and some compilers bitch and moan about seeing it
   twice, so I've commented it out here. */
/* typedef struct BM_Entry_struct BM_Entry; */



/* information for the find dialog */
typedef struct BM_FindInfo {
  char* textToFind;
  XP_Bool checkNickname;		/* Meaningful only in addressbook */
  XP_Bool checkName;
  XP_Bool checkLocation;
  XP_Bool checkDescription;
  XP_Bool matchCase;
  XP_Bool matchWholeWord;
	
  BM_Entry*	lastEntry;
} BM_FindInfo;

struct BM_Entry_Focus {
  BM_Entry* saveFocus;
  XP_Bool foundSelection;
};

extern XP_Bool BM_IsHeader(BM_Entry* entry);
extern XP_Bool BM_IsUrl(BM_Entry* entry);
extern XP_Bool BM_IsAddress(BM_Entry* entry);
extern XP_Bool BM_IsSeparator(BM_Entry* entry);
extern XP_Bool BM_IsAlias(BM_Entry* entry);
extern XP_Bool BM_IsFolded(BM_Entry* entry);
extern XP_Bool BM_IsSelected(BM_Entry* entry);


/* Returns the state of whether this is a "changed URL"; that is, a URL whose
   contents have apparently changed since we last visited it. */

#define BM_CHANGED_YES			1
#define BM_CHANGED_NO			0
#define BM_CHANGED_UNKNOWN		-1

extern int32 BM_GetChangedState(BM_Entry* entry);



/* Get/Set the header that is to be the root of the pulldown menu. */
extern BM_Entry* BM_GetMenuHeader(MWContext* context);
extern void BM_SetMenuHeader(MWContext* context, BM_Entry* entry);

/* Get/Set the header that is to be the container of new items added from
   browser windows. */
extern BM_Entry* BM_GetAddHeader(MWContext* context);
extern void BM_SetAddHeader(MWContext* context, BM_Entry* entry);


typedef void (*EntryFunc)(MWContext* context, BM_Entry* entry, void* closure);


/* Executes the given function on every single entry in the bookmark file. */
extern void BM_EachEntryDo(MWContext* context, EntryFunc func, void* closure);

/* Executes the given function for every entry that is selected. */
extern void BM_EachSelectedEntryDo(MWContext* context, EntryFunc func,
								   void* closure);

/* Executes the given function for every entry that is selected and is not a
   descendent of a header that is not folded or selected. */
extern void BM_EachProperSelectedEntryDo(MWContext* context, EntryFunc func,
							void* closure, struct BM_Entry_Focus* bmFocus);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Front end stubs -- each FE needs to implement these. */

/* Refresh each cell between and including first and last in the bookmarks
   widget (if now is TRUE, the FE is expected to redraw them BEFORE returning,
   otherwise the FE can simply invalidate them and wait for the redraw to
   happen).  If BM_LAST_CELL is passed in as last, then it means paint from
   the first to the end. */
extern void BMFE_RefreshCells(MWContext* context, int32 first, int32 last,
							  XP_Bool now);


/* Resize the widget to accomodate "visibleCount" number of entries vertically
   and the width of widest entry the actual widget should NOT change size, just
   the size of the scrollable area under it */
extern void BMFE_SyncDisplay(MWContext* context);


/* measure the item and assign the width and height required to draw it into
   the widget into width and height.  This is used only by BM_WidestEntry(); if
   you don't need that call, you can just make this an empty stub. */
extern void BMFE_MeasureEntry(MWContext* context, BM_Entry* entry,
							  uint32* width, uint32* height);

/* Save the given bucket o' bits as the clipboard.  This same bucket needs to
   be returned later if BMFE_GetClipContents() is called. */
extern void BMFE_SetClipContents(MWContext* context, void* buffer,
								 int32 length);


/* return the clipboard contents */
extern void* BMFE_GetClipContents(MWContext* context, int32* length);


/* Copy the selected items from a history window to the clipboard (as bookmarks) */
extern char *BM_ClipCopyHistorySelection( void *pHistCsr, uint32 *pSelections, int iCount, int *pSize, XP_Bool bLongFormat );

/* Insert the block into the list */
extern void BM_DropBlockL( MWContext *pContext, char *pData, BM_Entry *firstSelected );

/* Create the bookmarks property window.  If one already exists, just bring it
   to the front.  This will always be immediately followed by a call to
   BMFE_EditItem(). */
extern void BMFE_OpenBookmarksWindow(MWContext* context);


/* Edit the given item in the bookmarks property window.  If there is no
   bookmarks property window currently, then the FE should ignore this call.
   If the bookmarks property window is currently displaying some other entry,
   then it should save any changes made to that entry (by calling BM_SetName,
   etc.) before loading up this entry. */
extern void BMFE_EditItem(MWContext* context, BM_Entry* entry);

/* The given entry is no longer valid (i.e., the user just deleted it).  So,
   the given pointer is about to become invalid, and the FE should remove any
   references to it it may have.  In particular, if it is the one being edited
   in the bookmarks property window, then the FE should clear that window. */
extern void BMFE_EntryGoingAway(MWContext* context, BM_Entry* entry);


/* The user has requested to view the given url.  Show it to him in, using some
   appropriate context.  Url may be targeted to a different window */
extern void BMFE_GotoBookmark(MWContext* context,
		const char* url, const char* target);
							

/* Create the find dialog, and fill it in as specified in the given
   structure.  When the user hits the "Find" button in the dialog, call
   BM_DoFindBookmark. */
extern void* BMFE_OpenFindWindow(MWContext* context, BM_FindInfo* findInfo);
				

/* Make sure that the given entry is visible. */
extern void BMFE_ScrollIntoView(MWContext* context, BM_Entry* entry);


/* The list of bookmarks has changed somehow, so any "bookmarks" menu needs to
   be recreated.  This should be a cheap call, just setting a flag in the FE so
   that it knows to recreate the menu later (like, when the user tries to view
   it).  Recreating it immediately would be bad, because this can get called
   much more often than is reasonable. */
extern void BMFE_BookmarkMenuInvalid(MWContext* context);


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

extern void BMFE_UpdateWhatsChanged(MWContext* context,
									const char* url, /* If NULL, just display
														"Checking..." */
									int32 done, int32 total,
									const char* totaltime);



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

extern void BMFE_FinishedWhatsChanged(MWContext* context, int32 totalchecked,
									  int32 numreached, int32 numchanged);

#ifdef XP_WIN
/* The current bookmarks file is about to change */
extern void BMFE_ChangingBookmarksFile(void);

/* The current bookmarks file has been changed */
extern void BMFE_ChangedBookmarksFile(void);
#endif

#ifdef XP_UNIX
/* Use these to know when to allow refresh */
extern void BMFE_StartBatch(MWContext* context);
extern void BMFE_EndBatch(MWContext* context);
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */



/* Initialize a new bookmarks context.  (returns negative on failure) */
extern int BM_InitializeBookmarksContext(MWContext* context);


/* Prepare to destroy a bookmarks context.  Will save any changes that have
   been made. */
extern void BM_CleanupBookmarksContext(MWContext* context);


/* Set some FE data to associate with a bookmarks context. */
extern void BM_SetFEData(MWContext* context, void* data);

/* Get the FE data previously associated with a context. */
extern void* BM_GetFEData(MWContext* context);


/* Create a new url entry.  */
extern BM_Entry* BM_NewUrl(const char* name, const char* address,
						   const char* content_type, BM_Date last_visit);

/* Create a new header. */
extern BM_Entry* BM_NewHeader(const char* name);


/* Create a copy of an existing bookmark.  If the bookmark is a folder
   it will also contain a copy of all of the bookmarks in the folder.*/
extern BM_Entry* BM_CopyBookmark(MWContext* context, BM_Entry* original);

/* Frees an entry.  This should never be called unless you are sure the entry
   has not been added to some header, and is not the object of some alias.  In
   other words, be sure nothing could possibly have a pointer to this it.
   If the object is a header, this will also free all of its descendents. */
extern void BM_FreeEntry(MWContext* context, BM_Entry* entry);


/* Get the type of this bookmark.  (Returns one of the BM_TYPE_* values.) */
extern BM_Type BM_GetType(BM_Entry* entry);

extern char* BM_GetName(BM_Entry* entry);

extern char* BM_GetAddress(BM_Entry* entry);
extern char* BM_GetTarget(BM_Entry* entry, XP_Bool recurse);
extern char* BM_GetDescription(BM_Entry* entry);

extern char* BM_GetNickName(BM_Entry* entry); /* Only meaningful in address
												 book. */

/* Get the full name and address of the given entry (which must be an
   address book entry or header).  Result is returned in a newly allocated
   string; free it with XP_FREE() when through. */
extern char* BM_GetFullAddress(MWContext* context, BM_Entry* entry);

/* These return prettily formated info about the bookmark.  They each
   return the result in a staticly allocated string, so if you call the
   same function twice the results get stomped. */
extern char* BM_PrettyLastVisitedDate(BM_Entry* entry);
extern char* BM_PrettyAddedOnDate(BM_Entry* entry);
extern char* BM_PrettyAliasCount(MWContext* context, BM_Entry* entry);


/* Get the root node of a context. */
extern BM_Entry* BM_GetRoot(MWContext* context);
   

/* Given a node, return the first of its children (if any). */
extern BM_Entry* BM_GetChildren(BM_Entry* entry);

/* Given a node, return the next node that has the same parent (if any). */
extern BM_Entry* BM_GetNext(BM_Entry* entry);

/* Given a node, returns its parent (or NULL if this is the root) */
extern BM_Entry* BM_GetParent(BM_Entry* entry);

/* Does this node have a sibling somewhere below it? */
extern XP_Bool BM_HasNext(BM_Entry* entry);

/* Does this node have a sibling somewhere above it?  Note that there is *not*
   an API to actually get that sibling, as this is not a simple call.  However,
   determing the existance of such a child is easy, and that's what Windows
   needs to paint its pipes. */
extern XP_Bool BM_HasPrev(BM_Entry* entry);



/* Routines to change the name/address/description of an entry.  These routines
   will take care of updating the display as necessary. */
extern void BM_SetName(MWContext* context, BM_Entry* entry,
					   const char* newName);
extern void BM_SetAddress(MWContext* context, BM_Entry* entry,
						  const char* newAddress);
extern void BM_SetTarget(MWContext* context, BM_Entry* entry,
						  const char* newTarget);
extern void BM_SetDescription(MWContext* context, BM_Entry* entry,
							  const char* newDesc);

/* Only meaningful in addressbook (on an address or header or alias to same) */
extern XP_Bool BM_SetNickName(MWContext* context, BM_Entry* entry,
						   const char* newName);

/* The user just hit "Cancel" on the properties window, which was editing
   the given item.  This gives the bookmarks code a chance to delete the
   item if it was just newly created. */
extern void BM_CancelEdit(MWContext* context, BM_Entry* entry);


/* fold or unfold the header according to "fold" and all its subfolders if
	foldAll is TRUE */
extern void BM_FoldHeader(MWContext* context, BM_Entry* entry, XP_Bool fold,
						  XP_Bool refresh, XP_Bool foldAll);

							
/* clear all the selection flags, and cause a redisplay if refresh is set. */
extern void BM_ClearAllSelection(MWContext* context, XP_Bool refresh);

/* Clear all the child selections of the passes header and redisplay if refresh
   is set. */
extern void BM_ClearAllChildSelection(MWContext* context, BM_Entry* at,
									  XP_Bool refresh);

/* Select everything. */
extern void BM_SelectAll(MWContext* context, XP_Bool refresh);


/* selects the item, call BMFE_Refresh on it if "refresh" and calls
	BM_ClearSelection first if extend is FALSE */
extern void BM_SelectItem(MWContext* context, BM_Entry* item, XP_Bool refresh,
						  XP_Bool extend, XP_Bool select);

/* Adds a range of bookmarks to the current selection.  To be called by the FE
   when the user Shift-Clicks in the window.  This will unselect everything and
   then select a range of bookmarks, from the last bookmark that was selected
   with BM_SelectItem to the one given here. */
extern void BM_SelectRangeTo(MWContext* context, BM_Entry* item);

/* toggles the selected state of the item, call BMFE_Refresh on it if "refresh"
	and calls BM_ClearSelection if extend is FALSE */
extern void BM_ToggleItem(MWContext* context, BM_Entry* item, XP_Bool refresh,
						  XP_Bool extend );

/* returns the first selected entry */
extern BM_Entry* BM_FirstSelectedItem(MWContext* context);


extern BM_Date BM_GetLastVisited(BM_Entry *);
extern BM_Date BM_GetAdditionDate(BM_Entry *);

/* return the number of bookmarks */
extern int32 BM_GetCount(MWContext* context);

/* return the number of visible bookmarks */
extern int32 BM_GetVisibleCount(MWContext* context);

/* returns an integer index of the item in the list */
extern int32 BM_GetIndex(MWContext* context, BM_Entry* item);

/* returns an integer index of the item in the list and does not pay attention
   to the is_folded value */
extern int32 BM_GetUnfoldedIndex(MWContext* context, BM_Entry* item);

/* returns the object associated with the index returned by BM_GetIndex() */
extern BM_Entry* BM_AtIndex(MWContext* context, int32 index);

/* returns the object associated with the index returned by
   BM_GetUnfoldedIndex() */
extern BM_Entry* BM_AtUnfoldedIndex(MWContext* context, int32 index);


/* returns an integer depth of the item in the list starting
	at zero */
extern int32 BM_GetDepth(MWContext* context, BM_Entry* item);


/* Execute a find operation according to the data in the given structure.  This
   will cause the appropriate entry to be selected and made visible. */
extern void BM_DoFindBookmark(MWContext* context, BM_FindInfo* findInfo);

/* For addressbook only: find and return the entry (if any) for the e-mail
   address within the given mailto: url. (used by libmsg) */
extern BM_Entry* BM_FindAddress(MWContext* context, const char* mailtourl);


/* For addressbook only: edit the entry for the e-mail address within the
   given mailto: url, creating the entry if necessary.  (used by libmsg) */
extern void BM_EditAddress(MWContext* context, const char* mailtourl);


/* For addressbook only: if the given list of addresses includes a nickname
   from the addresses, then return a new string with the addresses expanded.
   If expandfull is True, then all mailing list entries are expanded too,
   otherwise, mailing list entries are only expanded to include their name.
   (used by libmsg). */
extern char* BM_ExpandHeaderString(MWContext* context, const char* value,
								   XP_Bool expandfull);
								   

/* Checks every bookmark context for a url and updates the last accessed
   time.  (For use by global history code; FE's probably don't need this.) */
extern void BM_UpdateBookmarksTime(URL_Struct* URL_s, BM_Date cur_time);

/* returns TRUE if the second argument is a direct descendent of the first
   argument, returns FALSE otherwise */
extern XP_Bool BM_IsDescendent(MWContext* context, BM_Entry* parent,
							   BM_Entry* possible_child);


/* The front end can use this call to determine what the indentation depth is
   needed to display all the icons in the bookmarks.  The XFE uses this to
   dynamically resize the icon column. In true C style, the number returned is
   actually one bigger than the biggest depth the FE will ever get. */
extern int BM_GetMaxDepth(MWContext* context);


/* This is called during a drag operation.  The user is dragging some bookmarks
   and is currently pointing at the given line.  (If under is True, then the
   user is really pointing between the given line and the next line.)  This
   returns TRUE if the FE should draw a box around the given line, and FALSE if
   it should underline it. */
extern XP_Bool BM_IsDragEffectBox(MWContext* context, int line, XP_Bool under);


/* Actually do a drop in a drag-n-drop reordering operation.  The arguments are
   the same as BM_IsDragEffectBox(); the selected items will be moved. */
extern void BM_DoDrop(MWContext* ctnx, int line, XP_Bool under);




/* find and return the widest visible entry in the bookmarks tree.  This makes
   sense only if your FE has defined a meaningful BMFE_MeasureEntry(). */
extern BM_Entry* BM_WidestEntry(MWContext* context);



/* insert an item after another item in the bm if the insert_after item is NULL
   or not found the item will be inserted at the end of the list */
extern void BM_InsertItemAfter(MWContext* context, BM_Entry* insert_after,
							   BM_Entry* insertee);

/* insert an item in a header if "insert_after" is a Header type, or after the
   item if "insert after" is not a header type. if the insert_after item is
   NULL or not found the item will be inserted at the end of the bm */
extern void BM_InsertItemInHeaderOrAfterItem(MWContext* context,
											 BM_Entry* insert_after,
											 BM_Entry* insertee );

/* Cause the given item to be inserted at the end of the given header.  The
   header passed in here is usually BM_GetAddHeader(). */
extern void BM_AppendToHeader(MWContext* context, BM_Entry* header,
							  BM_Entry* entry);

extern void BM_PrependChildToHeader(MWContext* context, BM_Entry* parent, BM_Entry* child);

/* Removes the given item from bookmarks without deleting it*/
extern void BM_RemoveChildFromHeader(MWContext* context, BM_Entry* parent, BM_Entry* child);

/* Cause the given bookmark to be displayed in a browser window somewhere.
   The given item must be an URL (BM_IsUrl must return TRUE). */
extern void BM_GotoBookmark(MWContext* context, BM_Entry* item);


/* Reads the hostlist from disk, what else?  Pass in the bm filename and a
   relative URL which represents the original location of the html file.  If
   you are reading the default bm you should pass in a file URL of the form
   file://localhost/PATH */
extern void BM_ReadBookmarksFromDisk(MWContext* context, const char* filename,
									 const char* relative_url);

/* saves the bm to a file.  If the given filename is NULL, then save it back to
   where it was loaded from and only if changes have been made.  Returns
   negative on failure. */
extern int32 BM_SaveBookmarks(MWContext* context, const char* filename);


/* Returns the filename that the bookmarks are saved in. */
extern const char* BM_GetFileName(MWContext* context);

/* read in a new bookmarks file (esp for LI use). */
extern void BM_Open_File(MWContext* context, char* newFile);

/* ability to set the modified to true or false (esp for LI use). */
extern void BM_SetModified(MWContext* context, XP_Bool mod);

/* Whether the file will be written when Save is called. */
extern XP_Bool BM_Modified(MWContext* context);

/* Convert a number of selections in a bm list into a block of memory that the
   user can use for cut and paste operations */
extern char* BM_ConvertSelectionsToBlock(MWContext* context,
										 XP_Bool bLongFormat,
										 int32* lTotalLen);

/* Take a block of memory and insert the bm items it represents into the
   current bm */
extern void BM_InsertBlockAt(MWContext* context, char* pOriginalBlock,
							 BM_Entry* addTo, XP_Bool bLongFormat,
							 int32 lTotalLen);


/* select all aliases for an entry */
extern void BM_SelectAliases(MWContext* context, BM_Entry* forEntry );


/* Count how many aliases there are to the given entry. */
extern int32 BM_CountAliases(MWContext* context, BM_Entry* forEntry);


/* Make an alias for each of the currently selected entries. */
extern void BM_MakeAliases(MWContext* context);


/* Returns the real item that an alias points to.  The given entry must be an
   alias (BM_IsAlias must return TRUE on it). */
extern BM_Entry* BM_GetAliasOriginal(BM_Entry*);



/* Start a What's Changed operation.  Before calling this routine, it's up to
   the FE to present a dialog like this when What's Changed starts:

   			 Look for documents that have changed on:
             (o) All bookmarks
             ( ) Selected bookmarks 

             [ Start Checking ] [ Cancel ]

   Then the FE calls this routine when the user clicks Start Checking.  The FE
   will immediately get called via BMFE_UpdateWhatsChanged(), and the FE should
   change the dialog box's appearance to match. */
extern int BM_StartWhatsChanged(MWContext* context,
								XP_Bool do_only_selected);


/* Cancel a running What's Changed operation. */
extern int BM_CancelWhatsChanged(MWContext* context);



/* Returns whether the given command can be executed right now.  Should be
   used to decide whether to disable a menu item for this command. */
extern XP_Bool BM_FindCommandStatus(MWContext* context,
									BM_CommandType command);

/* Execute the given command. */
extern void BM_ObeyCommand(MWContext* context, BM_CommandType command );

/* Clean up the undo queue */
void BM_ResetUndo(MWContext * context);

XP_END_PROTOS

#endif /* BMLIST_H */
