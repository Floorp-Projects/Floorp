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
   IconGroup.h -- used to ease Toolbar icon stuff.
   Created: Chris Toshok <toshok@netscape.com>, 2-Nov-96
 */



#ifndef _xfe_icongroup_h
#define _xfe_icongroup_h

#include "rosetta.h"
#include "mozilla.h"
#include "xfe.h"
#include "icons.h"
#include "icondata.h"

typedef struct IconGroup
{
  const char *name;

  struct fe_icon_data *pixmap_data;
  struct fe_icon_data *pixmap_i_data;
  struct fe_icon_data *pixmap_mo_data;
  struct fe_icon_data *pixmap_md_data;

  fe_icon pixmap_icon;
  fe_icon pixmap_i_icon;
  fe_icon pixmap_mo_icon;
  fe_icon pixmap_md_icon;

} IconGroup;

#ifndef WANT_EXTERNS
#define WANT_EXTERNS
#endif

#ifdef WANT_GROUPS
#define ICONGROUP_VERBOSE(name, pixmap, pixmap_i, pixmap_mo, pixmap_md) \
        struct IconGroup name##_group = {"###", \
					 pixmap, pixmap_i, \
					 pixmap_mo, pixmap_md, \
					 { 0 }, { 0 }, { 0 }, { 0 } };
#define ICONGROUP(name) struct IconGroup name##_group = \
{ "####", &name, &name##_i, &name##_mo, &name##_md, \
  { 0 }, { 0 }, { 0 }, { 0 } };
#define ICONGROUP_NOMO(name) struct IconGroup name##_group = \
{ "####", &name, &name##_i, 0, 0, { 0 }, { 0 }, { 0 }, { 0 } };
#else
#ifdef WANT_EXTERNS
#define ICONGROUP(name) extern struct IconGroup name##_group;
#define ICONGROUP_VERBOSE(name, pixmap, pixmap_i, pixmap_mo, pixmap_md) extern struct IconGroup name##_group;
#define ICONGROUP_NOMO(name) extern struct IconGroup name##_group;
#endif
#endif

/* Browser Toolbar stuff. */
ICONGROUP(TB_Back)
ICONGROUP(TB_Forward)
ICONGROUP(TB_Home)
ICONGROUP(TB_Edit)
ICONGROUP(TB_Search)
ICONGROUP(TB_Places)
ICONGROUP(TB_LoadImages)
ICONGROUP(TB_Print)
ICONGROUP(TB_Reload)
ICONGROUP(TB_Stop)
HG29081

/* Dynamic ToolBox tab icons. */
ICONGROUP_VERBOSE(DTB_bottom,	&DTB_bottom,	NULL, &DTB_bottom_mo,	NULL)
ICONGROUP_VERBOSE(DTB_left,		&DTB_left,		NULL, &DTB_left_mo,		NULL)
ICONGROUP_VERBOSE(DTB_right,	&DTB_right,		NULL, &DTB_right_mo,	NULL)
ICONGROUP_VERBOSE(DTB_top,		&DTB_top,		NULL, &DTB_top_mo,		NULL)
ICONGROUP_VERBOSE(DTB_vertical,	&DTB_vertical,	NULL, &DTB_vertical_mo, NULL)
ICONGROUP_VERBOSE(DTB_horizontal,&DTB_horizontal,NULL,&DTB_horizontal_mo,NULL)

/* Bookmarks, URL bar */
ICONGROUP_VERBOSE(BM_Bookmark, &BM_Bookmark, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_MailBookmark, &BM_MailBookmark, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewsBookmark, &BM_NewsBookmark, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_Change, &BM_Change, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_PersonalFolder, &BM_PersonalFolder, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_PersonalFolderO, &BM_PersonalFolderO, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewPersonalFolder, &BM_NewPersonalFolder, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewPersonalFolderO, &BM_NewPersonalFolderO, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewPersonalMenu, &BM_NewPersonalMenu, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewPersonalMenuO, &BM_NewPersonalMenuO, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_Folder, &BM_Folder, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_FolderO, &BM_FolderO, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_MenuFolder, &BM_MenuFolder, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_MenuFolderO, &BM_MenuFolderO, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewAndMenuFolder, &BM_NewAndMenuFolder, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewAndMenuFolderO, &BM_NewAndMenuFolderO, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewFolder, &BM_NewFolder, NULL, NULL, NULL)
ICONGROUP_VERBOSE(BM_NewFolderO, &BM_NewFolderO, NULL, NULL, NULL)

ICONGROUP(BM_QFile)
ICONGROUP_VERBOSE(LocationProxy, &LocationProxy, NULL, &LocationProxy_mo, &LocationProxy_mo)

#if defined(MOZ_MAIL_NEWS) || !defined(WANT_GROUPS)

/* Mail/News Toolbar stuff. */
ICONGROUP(MNTB_GetMsg)
ICONGROUP(MNTB_Compose)
ICONGROUP(MNTB_AddGroup)
ICONGROUP_VERBOSE(MN_Mommy, &MN_BackToParent, NULL, NULL, NULL)

/* Message Window. */
ICONGROUP(MNTB_Reply)
ICONGROUP(MNTB_ReplyAll)
ICONGROUP(MNTB_Forward)
ICONGROUP(MNTB_File)
ICONGROUP(MNTB_Trash)
ICONGROUP(MNTB_Next)
ICONGROUP(MNTB_Prev)
ICONGROUP(MNTB_MarkRead)

/* Message Center */
ICONGROUP(MNTB_NewFolder)

HG10282

/* Compose Window. */
ICONGROUP(MNC_Send)
/* MNTB_File */
ICONGROUP(MNC_Quote)
ICONGROUP(MNC_Address)
ICONGROUP_VERBOSE(MNC_Attach, &MNC_Attach, NULL, NULL, NULL)
ICONGROUP(MNC_SpellCheck)
ICONGROUP(MNC_Save)
ICONGROUP(MNC_Directory)
/* Tab Icons */
ICONGROUP_VERBOSE(MNC_AddressSmall, &MNC_AddressSmall, NULL, NULL,NULL)
ICONGROUP_VERBOSE(MNC_AttachSmall,  &MNC_AttachSmall, NULL, NULL,NULL)
ICONGROUP_VERBOSE(MNC_Options, &MNC_Options, NULL,NULL,NULL)

ICONGROUP(MN_CollectSmall)
ICONGROUP(MN_Collect)
/* TB_Stop */


/* Address Book. */
ICONGROUP(MNAB_NewPerson)  // New Card.
ICONGROUP(MNAB_NewList)
ICONGROUP(MNAB_Properties)
/* MNTB_Compose */
/* MNC_Directory */
ICONGROUP(MNAB_Call)
/* MNTB_Trash */
#endif /* !MOZ_MAIL_NEWS || !WANT_GROUPS */

#if defined(EDITOR) || !defined(WANT_GROUPS)
/* Editor */
ICONGROUP(ed_new)
ICONGROUP(ed_open)
ICONGROUP(ed_browse)
ICONGROUP(ed_save)
ICONGROUP(ed_publish)
ICONGROUP(ed_cut)
ICONGROUP(ed_copy)
ICONGROUP(ed_paste)
ICONGROUP(ed_link)
ICONGROUP(ed_target)
ICONGROUP(ed_image)
ICONGROUP(ed_hrule)
ICONGROUP(ed_table)
ICONGROUP(ed_bold)
ICONGROUP(ed_italic)
ICONGROUP(ed_underline)
ICONGROUP(ed_bullet)
ICONGROUP(ed_number)
ICONGROUP(ed_indent)
ICONGROUP(ed_outdent)
ICONGROUP(ed_left)
ICONGROUP(ed_center)
ICONGROUP(ed_right)
ICONGROUP(ed_find)
ICONGROUP(ed_spellcheck)
ICONGROUP(ed_print)
ICONGROUP(ed_insert)
ICONGROUP(ed_clear)
#endif /* defined(EDITOR) || !defined(WANT_GROUPS)*/

/* TaskBar icons. */
#if defined(MOZ_TASKBAR) || !defined(WANT_GROUPS)
ICONGROUP(Task_MailN)
ICONGROUP(Task_MailY)
ICONGROUP(Task_MailU)
ICONGROUP(Task_Discussions)
ICONGROUP(Task_Browser)
ICONGROUP(Task_Composer)

ICONGROUP(TaskSm_MailN)
ICONGROUP(TaskSm_MailY)
ICONGROUP(TaskSm_MailU)
ICONGROUP(TaskSm_Discussions)
ICONGROUP(TaskSm_Browser)
ICONGROUP(TaskSm_Composer)
ICONGROUP_VERBOSE(TaskSm_Handle,  &TaskSm_Handle,  NULL, NULL, NULL)
#endif /* MOZ_TASKBAR || !WANT_GROUPS */

#ifdef __cplusplus
extern  "C" {
#endif

extern void
IconGroup_createAllIcons		(IconGroup *	group, 
								 Widget			toplevel, 
								 Pixel			foreground_pixel,
								 Pixel			transparent_pixel);
	
extern void
IconGroup_createOneIcon			(fe_icon *				icon,
								 struct fe_icon_data *	data,
								 Widget					toplevel,
								 Pixel					foreground_pixel,
								 Pixel					background_pixel);
	
extern IconGroup * 
IconGroup_findGroupForName		(char * name);
											
#ifdef __cplusplus
}
#endif

#endif /* _xfe_icongroup_h */
