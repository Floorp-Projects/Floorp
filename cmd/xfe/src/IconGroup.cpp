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
   IconGroup.c -- functions dealing with icon groups.
   Created: Chris Toshok <toshok@netscape.com>, 13-Aug-96.
 */



#define WANT_GROUPS
#include "IconGroup.h"
#undef WANT_GROUPS

//////////////////////////////////////////////////////////////////////////
//
// Private functions
//
//////////////////////////////////////////////////////////////////////////
/* extern */ void
IconGroup_createOneIcon(fe_icon *				icon,
						struct fe_icon_data *	data,
						Widget					toplevel,
						Pixel					foreground_pixel,
						Pixel					background_pixel)
{
	// Create the icon only if the info and data are not NULL
    if (icon->pixmap == 0 && data)
	{
		fe_NewMakeIcon(toplevel,
					   foreground_pixel,
					   background_pixel,
					   icon,
					   NULL,
					   data->width,
					   data->height,
					   data->mono_bits,
					   data->color_bits,
					   data->mask_bits,
					   False);
	}
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Public functions
//
//////////////////////////////////////////////////////////////////////////
/* extern */ void
IconGroup_createAllIcons(IconGroup *group, 
						 Widget toplevel, 
						 Pixel foreground_pixel, 
						 Pixel transparent_pixel)
{
    // Normal icon.
	IconGroup_createOneIcon(&group->pixmap_icon,
							group->pixmap_data,
							toplevel,
							foreground_pixel,
							transparent_pixel);

    // Insensitive icon.
	IconGroup_createOneIcon(&group->pixmap_i_icon,
							group->pixmap_i_data,
							toplevel,
							foreground_pixel,
							transparent_pixel);

    // Mouse-over icon.
	IconGroup_createOneIcon(&group->pixmap_mo_icon,
							group->pixmap_mo_data,
							toplevel,
							foreground_pixel,
							transparent_pixel);
	
    // Mouse-down icon.
	IconGroup_createOneIcon(&group->pixmap_md_icon,
							group->pixmap_md_data,
							toplevel,
							foreground_pixel,
							transparent_pixel);
}
//////////////////////////////////////////////////////////////////////////


typedef struct 
{
	char *		name;
	IconGroup * group;
} _name_and_group_t;

static _name_and_group_t _iconGroups[] =
{
	// Browser
	{ "Back",			&TB_Back_group },
	{ "Forward",		&TB_Forward_group },
	{ "Home",			&TB_Home_group },
	{ "Search",			&TB_Search_group },
	{ "Places",			&TB_Places_group },
	{ "LoadImages",		&TB_LoadImages_group },
	{ "Print",			&TB_Print_group },
	{ "Reload",			&TB_Reload_group },
	{ "Stop",			&TB_Stop_group },
	{ "MixSecurity",	&TB_MixSecurity_group },
#ifdef MOZ_MAIL_NEWS
	// Mail/News
	{ "GetMsg",			&MNTB_GetMsg_group },
	{ "Compose",		&MNTB_Compose_group },
	{ "Addgroup",		&MNTB_AddGroup_group },
	{ "Reply",			&MNTB_Reply_group },
	{ "ReplyAll",		&MNTB_ReplyAll_group },
	{ "Forward",		&MNTB_Forward_group },
	{ "File",			&MNTB_File_group },
	{ "Trash",			&MNTB_Trash_group },
	{ "Next",			&MNTB_Next_group },
	{ "Prev",			&MNTB_Prev_group },
	{ "NewFolder",		&MNTB_NewFolder_group },
	{ "MarkRead",		&MNTB_MarkRead_group },

	// Compose
	{ "Send",			&MNC_Send_group },
	{ "Quote",			&MNC_Quote_group },
	{ "Address",		&MNC_Address_group },
	{ "Attach",			&MNC_Attach_group },
	{ "SpellCheck",		&MNC_SpellCheck_group },
	{ "Save",			&MNC_Save_group },
	{ "Directory",		&MNC_Directory_group },

	// Address Book
	{ "NewPerson",		&MNAB_NewPerson_group },
	{ "NewList",		&MNAB_NewList_group },
	{ "Properties",		&MNAB_Properties_group },
	{ "Call",			&MNAB_Call_group },
#endif
#ifdef EDITOR
	// Editor
	{ "New",			&ed_new_group },
	{ "Open",			&ed_open_group },
	{ "Browse",			&ed_browse_group },
	{ "Save",			&ed_save_group },
	{ "Publish",		&ed_publish_group },
	{ "Cut",			&ed_cut_group },
	{ "Copy",			&ed_copy_group },
	{ "Paste",			&ed_paste_group },
	{ "Link",			&ed_link_group },
	{ "Target",			&ed_target_group },
	{ "Image",			&ed_image_group },
	{ "Hrule",			&ed_hrule_group },
	{ "Table",			&ed_table_group },
	{ "Bold",			&ed_bold_group },
	{ "Italic",			&ed_italic_group },
	{ "Underline",		&ed_underline_group },
	{ "Bullet",			&ed_bullet_group },
	{ "Number",			&ed_number_group },
	{ "Indent",			&ed_indent_group },
	{ "Outdent",		&ed_outdent_group },
	{ "Left",			&ed_left_group },
	{ "Center",			&ed_center_group },
	{ "Right",			&ed_right_group },
	{ "Find",			&ed_find_group },
	{ "Spellcheck",		&ed_spellcheck_group },
	{ "Print",			&ed_print_group },
	{ "Insert",			&ed_insert_group },
	{ "Clear",			&ed_clear_group },
#endif /* EDITOR */
};

//////////////////////////////////////////////////////////////////////////
//
// Find the icon group for a name
//
//////////////////////////////////////////////////////////////////////////
IconGroup *
IconGroup_findGroupForName(char * name)
{
	Cardinal i;

	if (!name)
	{
		return NULL;
	}

	for(i = 0; i < XtNumber(_iconGroups); i++)
	{
		_name_and_group_t * pair = &_iconGroups[i];

		if (XP_STRCASECMP(name,pair->name) == 0)
		{
			return pair->group;
		}
	}

		// The home icon
	return &TB_Home_group;
}

