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
/*   newsrc.c --- reading and writing newsrc files.
*/


/* This stuff is all going away (being replaced by newsset.{h,cpp}.)  So,
   for now, make it so that including this is a no-op. */


/* #ifndef _NEWSRC_H_ */
#ifdef NOTDEF

#define _NEWSRC_H_

#include "msg.h"

typedef struct msg_NewsRCSet  msg_NewsRCSet;
typedef struct msg_NewsRCFile msg_NewsRCFile;

XP_BEGIN_PROTOS


/* Allocates a set.  Shouldn't be used if this set comes from a newsrc, (which
   is the usual case); instead, get the set using msg_GetNewsRCSet(). */
extern msg_NewsRCSet * msg_MakeNewsrcSet(void);

/* Frees a set.  Should be a set explicitely allocated with
   msg_MakeNewsrcSet(); if this set came from a newsrc, you should not call
   this directly.
 */
extern void msg_FreeNewsrcSet (msg_NewsRCSet *set);


/* Parses a newsrc file and returns the result.
 */
extern struct msg_NewsRCFile *msg_ReadNewsRCFile (MWContext *context,
												  XP_File file);

/* Writes newsrc data to the given file.
 */
extern int msg_WriteNewsRCFile (MWContext *context,
								struct msg_NewsRCFile *data,
								XP_File file);

extern void msg_FreeNewsRCFile (struct msg_NewsRCFile *file);

extern struct msg_NewsRCFile *
msg_MakeDefaultNewsRCFileData (MWContext *context, XP_Bool default_host_p);

extern msg_NewsRCSet *
msg_GetNewsRCSet (MWContext *context, const char *group_name,
				  struct msg_NewsRCFile *file);

/* Copy one newsrc set to another one.  Pretty meaningless if both sets
   represent real newsrc lines; one or the other had better be for internal
   use only.  Note that it does not copy the name associated with the set;
   only the article number stuff. */

extern int msg_CopySet(msg_NewsRCSet* dest, msg_NewsRCSet* source);

/* Add a new entry to the newsrc file. */
extern msg_NewsRCSet *msg_AddNewsRCSet (const char *group_name,
										struct msg_NewsRCFile *file);


/* Return whether the given set has the subscribed bit on. */
extern XP_Bool msg_GetSubscribed(struct msg_NewsRCSet* set);

/* Turn on or off the subscribed bit. */
extern void msg_SetSubscribed(struct msg_NewsRCSet* set, XP_Bool value);


/* Returns the lowest non-member of the set greater than 0.
 */
extern int32 msg_FirstUnreadArt (msg_NewsRCSet *set);

/* This takes a `msg_NewsRCSet' object and converts it to a string suitable
   for printing in the newsrc file.

   The assumption is made that the `msg_NewsRCSet' is already compressed to
   an optimal sequence of ranges; no attempt is made to compress further.
 */
extern char *msg_OutputNewsRCSet (msg_NewsRCSet *set);

/* Re-compresses a `msg_NewsRCSet' object.

   The assumption is made that the `msg_NewsRCSet' is syntactically correct
   (all ranges have a length of at least 1, and all values are non-
   decreasing) but will optimize the compression, for example, merging
   consecutive literals or ranges into one range.

   Returns TRUE if successful, FALSE if there wasn't enough memory to
   allocate scratch space.
 */
extern XP_Bool msg_OptimizeNewsRCSet (msg_NewsRCSet *set);

/* Whether a number is a member of a set. */
extern XP_Bool msg_NewsRCIsRead (msg_NewsRCSet *set, int32 number);

/* Adds a number to a set.
   Returns negative if out of memory.
   Returns 1 if a change was made, 0 if it was already read.
 */
extern int msg_MarkArtAsRead (msg_NewsRCSet *set, int32 number);

/* Removes a number from a set.
   Returns negative if out of memory.
   Returns 1 if a change was made, 0 if it was already unread.
 */
extern int msg_MarkArtAsUnread (msg_NewsRCSet *set, int32 number);

/* Adds a range of numbers to a set.
   Returns negative if out of memory.
   Returns 1 if a change was made, 0 if they were all already read.
 */
extern int msg_MarkRangeAsRead (msg_NewsRCSet *set,
								int32 start, int32 end);

/* Remove a range of numbers from a set.
   Returns negative if out of memory.
   Returns 1 if a change was made, 0 if they were all already unread.
 */
extern int msg_MarkRangeAsUnread (msg_NewsRCSet *set,
								  int32 start, int32 end);

/* Given a range of articles, returns the number of articles in that
   range which are currently not marked as read.
 */
extern int32 msg_NewsRCUnreadInRange (msg_NewsRCSet *set,
									  int32 start, int32 end);

/* Given a set and an index into it, moves forward in that set until
   RANGE_SIZE non-members have been seen, and then returns that number.
   The idea here is that we want to request 100 articles from the news
   server (1-100), but if the user has already read 50 of them (say,
   numbers 20-80), we really want to request 150 of them (1-150) in
   order to get 100 *unread* articles.
 */
extern int32 msg_FindNewsRCSetNonmemberRange (msg_NewsRCSet *set, int32 start,
											  int32 range_size);

/* Given a set and an range in that set, find the first range of unread
   articles within the given range.  If none, return zeros. */

extern int msg_FirstUnreadRange(msg_NewsRCSet* set, int32 min, int32 max,
								int32* first, int32* last);

/* Given a set and an range in that set, find the last range of unread
   articles within the given range.  If none, return zeros. */

extern int msg_LastUnreadRange(msg_NewsRCSet* set, int32 min, int32 max,
							   int32* first, int32* last);


/* Remember the given set entry in the undo state off of the given mail/news
   context.  We're about to do a serious change to this newsset (i.e.,
   catchup newsgroup), and we want be able to get the old state back. */
extern int msg_PreserveSetInUndoHistory(MWContext* context,
										msg_NewsRCSet* set);


XP_END_PROTOS

#endif /* _NEWSRC_H_ */
