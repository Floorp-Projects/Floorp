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

#ifndef HOTLIST_H
#define HOTLIST_H

#include "xp_mcom.h"
#include "ntypes.h"

XP_BEGIN_PROTOS

typedef enum HOT_Type {
  HOT_URLType, HOT_HeaderType, HOT_SeparatorType } HOT_Type;

typedef struct HotlistStruct_ {
    HOT_Type  type;
    Bool      is_folded;        /* if it's a header is it folded? */
    XP_List  *children;         /* a list of children, only headers have these */
    char     *name;             /* a title */
    char     *address;          /* the URL address */
    char     *description;      /* random text */
    time_t    last_visit;
    time_t    addition_date;
    char     *content_type;
    struct HotlistStruct_  *parent; /* My hotlist parent */
    XP_List  *lParent;          /* The XP_List object that points to my parent or NULL */
} HotlistStruct;

/* tell the hotlist code that the hotlist has been modified
 * so that it gets saved the next time SaveHotlist is called
 */
extern void HOT_SetModified(void);

/* changes a entry to a header type from a non header type
 * and vice versa.  If the object was a header and
 * has children, the children will be blown away. (very bad)
 */
extern void HOT_ChangeEntryType(HotlistStruct * entry, HOT_Type new_type);

/* Fold or unfold a hotlist header
 * 
 * set the Boolean to True to fold the list and
 * False to unfold
 */
extern void HOT_FoldHeader(HotlistStruct * item, Bool fold);

/* checks the hotlist for a url and updates the last accessed
 * time
 */
extern void HOT_UpdateHotlistTime(URL_Struct *URL_s, time_t cur_time);

/* Performs regular expression match on hotlist name and address
 * fields.  Returns the found object, or NULL if not
 * found.
 *
 * start_obj specifies the object to start searching
 * on.  The start_num object WILL NOT be searched but all those
 * after it will be.
 * To search the whole list give NULL as start_obj.
 *
 * If headers became unfolded because of the search then redisplay_all
 * will be set to TRUE
 */
extern HotlistStruct * HOT_SearchHotlist(char * search_string, 
										HotlistStruct * start_obj, 
										Bool * redisplay_all);

/* returns TRUE if the second argument is a direct
 * descendent of the first argument.
 *
 * returns FALSE otherwise
 */
extern Bool HOT_IsDescendent(HotlistStruct *parent, HotlistStruct *possible_child);


/* Reads the hostlist from disk, what else?
 *
 * pass in the hotlist filename and a relative URL which represents
 * the original location of the html file.  If you are reading the
 * default hotlist you should pass in a file URL of the form
 * file://localhost/PATH
 *
 */
extern void HOT_ReadHotlistFromDisk (char * filename, char * relative_url);

/* returns an integer index of the item in the list
 */
extern int HOT_GetIndex(HotlistStruct * item);

/* returns an integer index of the item in the list
 * and does not pay attention to the is_folded value
 */
extern int HOT_GetUnfoldedIndex(HotlistStruct * item);

/* returns the object associated with the index returned by 
 * HOT_GetIndex()
 */
extern HotlistStruct * HOT_IndexOf(int index);

/* returns the object associated with the index returned by 
 * HOT_GetUnfoldedIndex()
 */
extern HotlistStruct * HOT_UnfoldedIndexOf(int index);

/* returns an integer depth of the item in the list starting at zero
 */
extern int HOT_GetDepth(HotlistStruct * item);

/* return a pointer to the main hotlist list
 *
 * returns NULL if nothing has ever been
 * added to the hotlist
 */
extern XP_List * HOT_GetHotlistList(void);

/* saves the hotlist to a configuration file
 */
extern int HOT_SaveHotlist (char * filename);

/* Free's the entire hotlist
 */
extern void HOT_FreeHotlist (void);


/* create a hotlist entry struct and fill it in with
 * the passed in data
 *
 * returns NULL on out of memory error.
 */
extern HotlistStruct *
HOT_CreateEntry(HOT_Type type,
                const char *name,
                const char *address,
                const char *content_type,
                time_t      last_visit);

/* free's a hotlist entry
 */
extern void HOT_FreeEntry(HotlistStruct * entry);

/* create a completely new copy of the entry passed in
 */
extern HotlistStruct * HOT_CopyEntry(HotlistStruct * entry);

/* insert an item before another item in the hotlist
 *
 * if the insert_before item is NULL or not found the item
 * will be inserted at the begining of the list
 */
extern void HOT_InsertItemBefore(HotlistStruct * insert_before, HotlistStruct * insertee);

/* insert an item after another item in the hotlist
 *
 * if the insert_after item is NULL or not found the item
 * will be inserted at the end of the list
 */
extern void HOT_InsertItemAfter(HotlistStruct * insert_after, HotlistStruct * insertee);

/* insert an item in a header if "insert_after" is a
 * Header type, or after the item if "insert after" is
 * not a header type.
 *
 * if the insert_after item is NULL or not found the item
 * will be inserted at the end of the hotlist
 */
extern void
HOT_InsertItemInHeaderOrAfterItem(HotlistStruct * insert_after,
                                  HotlistStruct * insertee);

/* remove an item from the hotlist and free's it
 *
 * returns TRUE on success, FALSE if not found
 */
extern Bool HOT_RemoveItem(HotlistStruct * old_item);

/* remove an item from the hotlist and doesn't free it
 *
 * returns TRUE on success, FALSE if not found
 */
extern Bool HOT_RemoveItemFromList(HotlistStruct * old_item);

/* move an item up in the list
 */
extern void HOT_MoveObjectUp(HotlistStruct * item);

/* move an item down in the list
 */
extern void HOT_MoveObjectDown(HotlistStruct * item);

/* returns True if the object can be moved Up
 * False if the object cannot be moved Up or if
 * it cannot be found in the list
 */
extern Bool HOT_ObjectCanGoUp(HotlistStruct * item);

/* returns True if the object can be moved down
 * False if the object cannot be moved down or if
 * it cannot be found in the list
 */
extern Bool HOT_ObjectCanGoDown(HotlistStruct * item);

/* Whether the file will be written when Save is called. */
extern Bool HOT_Modified(void);

/*
 *	Gets the top node of the hotlist
*/
extern HotlistStruct*
HOT_GetHotlist (void);


/*
 * Convert a number of selections in a hotlist list into a block of
 *   memory that the user can use for cut and paste operations
 */
extern char *
HOT_ConvertSelectionsToBlock(HotlistStruct ** list, 
                             int iCount, 
                             int bLongFormat, 
                             int32 * lTotalLen);
/*
 * Take a block of memory and insert the hotlist items it represents into
 *   the current hotlist
 */
extern void
HOT_InsertBlockAt(char * pOriginalBlock, 
                  HotlistStruct * item, 
                  int bLongFormat, 
                  int32 lTotalLen);

XP_END_PROTOS

#endif /* HOTLIST_H */


