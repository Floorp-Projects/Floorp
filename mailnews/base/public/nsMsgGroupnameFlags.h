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

#ifndef _msgGroupnameFlags_h_
#define _msgGroupnameFlags_h_


/* Flags in the subscribe pane (used inside of MSG_GroupNameLine).  Where
   the flags overlap with the MSG_FOLDER_FLAG_* flags, it has the same value,
   to reduce the chance of someone using the wrong constant. */

#define MSG_GROUPNAME_FLAG_ELIDED		0x0010  /* Whether the children of this
												   group are currently hidden
												   in the listing.  This will
												   only be present if it has
												   any children. */

#define MSG_GROUPNAME_FLAG_MODERATED	0x0020  /* Whether this folder
												   represents a moderated
												   newsgroup. */
#define MSG_GROUPNAME_FLAG_SUBSCRIBED	0x0040  /* Whether this folder
												   represents a subscribed
												   newsgroup. */
#define MSG_GROUPNAME_FLAG_NEW_GROUP	0x0080  /* A newsgroup which has just
												   been added by the `Check
												   New Groups' command. */
#define MSG_GROUPNAME_FLAG_HASCHILDREN	0x40000 /* Whether there are children
												  of this group.  Whether those
												  chilren are visible in this
												  list is determined by the
												  above "ELIDED" flag. 
												  Setting this to the same value
												  as a MSG_FOLDER_FLAG_* IMAP server,
												  since an IMAP _server_ will never
												  appear in the subscribe pane.  */
#define MSG_GROUPNAME_FLAG_IMAP_PERSONAL	0x80000		/* folder is an IMAP personal folder */

#define MSG_GROUPNAME_FLAG_IMAP_PUBLIC		0x100000		/* folder is an IMAP public folder */

#define MSG_GROUPNAME_FLAG_IMAP_OTHER_USER	0x200000		/* folder is another user's IMAP folder */

#define MSG_GROUPNAME_FLAG_IMAP_NOSELECT	0x400000		/* A \NoSelect IMAP folder */

#define MSG_GROUPNAME_FLAG_PERSONAL_SHARED	0x800000	/* whether or not this folder is one of your personal folders that
								`					       is shared with other users */

#endif
