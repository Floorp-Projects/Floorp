/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Flags about a folder or a newsgroup.  Used in the MSG_FolderLine struct;
   also used internally in libmsg (the `flags' slot in MSG_Folder).  Note that
   these don't have anything to do with the above MSG_FLAG flags; they belong
   to different objects entirely.  */

#ifndef _msgFolderFlags_h_
#define _msgFolderFlags_h_


    /* These flags say what kind of folder this is:
       mail or news, directory or leaf.
     */
#define MSG_FOLDER_FLAG_NEWSGROUP   0x0001  /* The type of this folder. */
#define MSG_FOLDER_FLAG_NEWS_HOST   0x0002  /* Exactly one of these three */
#define MSG_FOLDER_FLAG_MAIL        0x0004  /* flags will be set. */

#define MSG_FOLDER_FLAG_DIRECTORY   0x0008  /* Whether this is a directory:
                                               NEWS_HOSTs are always
                                               directories; NEWS_GROUPs can be
                                               directories if we are in ``show
                                               all groups'' mode; MAIL folders
                                               will have this bit if they are
                                               really directories, not files.
                                               (Note that directories may have
                                               zero children.) */

#define MSG_FOLDER_FLAG_ELIDED      0x0010  /* Whether the children of this
                                               folder are currently hidden in
                                               the listing.  This will only
                                               be present if the DIRECTORY
                                               bit is on. */

    /* These flags only occur in folders which have
       the MSG_FOLDER_FLAG_NEWSGROUP bit set, and do
       not have the MSG_FOLDER_FLAG_DIRECTORY or
       MSG_FOLDER_FLAG_ELIDED bits set.
     */

#define MSG_FOLDER_FLAG_MODERATED   0x0020  /* Whether this folder represents
                                               a moderated newsgroup. */
#define MSG_FOLDER_FLAG_SUBSCRIBED  0x0040  /* Whether this folder represents
                                               a subscribed newsgroup. */
#define MSG_FOLDER_FLAG_NEW_GROUP   0x0080  /* A newsgroup which has just
                                               been added by the `Check
                                               New Groups' command. */


    /* These flags only occur in folders which have
       the MSG_FOLDER_FLAG_MAIL bit set, and do
       not have the MSG_FOLDER_FLAG_DIRECTORY or
       MSG_FOLDER_FLAG_ELIDED bits set.

	   The numeric order of these flags is important;
	   folders with these flags on get displayed first,
	   in reverse numeric order, before folders that have
	   none of these flags on.  (Note that if a folder is,
	   say, *both* inbox and sentmail, then its numeric value
	   will be even bigger, and so will bubble up to where the
	   inbox generally is.  What a hack!)
     */

#define MSG_FOLDER_FLAG_TRASH       0x0100  /* Whether this is the trash
                                               folder. */
#define MSG_FOLDER_FLAG_SENTMAIL	0x0200	/* Whether this is a folder that
											   sent mail gets delivered to.
											   This particular magic flag is
											   used only during sorting of
											   folders; we generally don't care
											   otherwise. */
#define MSG_FOLDER_FLAG_DRAFTS      0x0400	/* Whether this is the folder in
                                               which unfinised, unsent messages
                                               are saved for later editing. */
#define MSG_FOLDER_FLAG_QUEUE       0x0800  /* Whether this is the folder in
                                               which messages are queued for
                                               later delivery. */
#define MSG_FOLDER_FLAG_INBOX       0x1000  /* Whether this is the primary
                                               inbox folder. */
#define MSG_FOLDER_FLAG_IMAPBOX		0x2000	/* Whether this folder on online
											   IMAP */

#define MSG_FOLDER_FLAG_CAT_CONTAINER 0x4000 /* This group contains categories */

#define MSG_FOLDER_FLAG_PROFILE_GROUP 0x8000 /* This is a virtual newsgroup */

#define MSG_FOLDER_FLAG_CATEGORY	0x10000  /* this is a category */

#define MSG_FOLDER_FLAG_GOT_NEW		0x20000		/* folder got new msgs */

#define MSG_FOLDER_FLAG_IMAP_SERVER	0x40000		/* folder is an IMAP server */

#define MSG_FOLDER_FLAG_IMAP_PERSONAL	0x80000		/* folder is an IMAP personal folder */

#define MSG_FOLDER_FLAG_IMAP_PUBLIC		0x100000		/* folder is an IMAP public folder */

#define MSG_FOLDER_FLAG_IMAP_OTHER_USER	0x200000		/* folder is another user's IMAP folder */
														/* Think of it like a folder that someone would share. */
#define MSG_FOLDER_FLAG_TEMPLATES		0x400000	/* Whether this is the template folder */

#define MSG_FOLDER_FLAG_PERSONAL_SHARED	0x800000	/* This folder is one of your personal folders that
								`					   is shared with other users */

#define MSG_FOLDER_FLAG_IMAP_NOSELECT	0x1000000	/* This folder is an IMAP \Noselect folder */

#define MSG_FOLDER_FLAG_CREATED_OFFLINE 0x2000000	/* this folder created offline */

#define MSG_FOLDER_FLAG_IMAP_NOINFERIORS 0x4000000 /* this imap folder cannot have children :-( */

#define MSG_FOLDER_FLAG_OFFLINE 0x8000000 /* this folder configured for offline use */

#define MSG_FOLDER_FLAG_OFFLINEEVENTS 0x10000000 /* this folder has offline events to play back */

#endif
