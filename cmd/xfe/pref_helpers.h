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

#ifndef __xfe_pref_helpers_h
#define __xfe_pref_helpers_h

#include <X11/Intrinsic.h>
#include "net.h"

struct fe_prefs_helpers_data
{
  MWContext *context;
 
  /* Helpers page */
  Widget helpers_selector, helpers_page;
  Widget helpers_list;
 
  /* New Helper and Plugin stuff */
  Widget mime_types_desc_text;
  Widget mime_types_text;
  Widget mime_types_suffix_text;
 
  /* Editor */
  Widget navigator_b;
  Widget plugin_b;
  Widget plugin_option;
  Widget plugin_pulldown;
  Widget app_b;
  Widget app_text;
  Widget app_browse;
  Widget save_b;
  Widget unknown_b;

  Widget editor;
  Widget edit_b;
  Widget new_b;
  Widget delete_b;
 
  /* Data Stuff */
  int pos;
  NET_cdataStruct *cd;
  char **plugins;
};

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

EXTERN void
fe_helpers_build_mime_list(struct fe_prefs_helpers_data *fep);

EXTERN struct fe_prefs_helpers_data *
fe_helpers_make_helpers_page (MWContext *context, Widget parent);

EXTERN void
fe_helpers_prepareForDestroy(struct fe_prefs_helpers_data *fep);

/* Given a mimetype string, will return the mailcap entry (md_list)
 * associated with this mimetype. If the mimetype has no mailcap entry
 * associated with this, returns NULL.
 */
EXTERN NET_mdataStruct *
fe_helpers_get_mailcap_from_type(char *type);

/* Adds a new entry into the mailcap list (md_list) */
EXTERN void
fe_helpers_add_new_mailcap_data(char *contenttype, char* src_str,
				char *command, char *xmode, Boolean isLocal);

/* Update the xmode of a mailcap entry. Also the src_string is updated.
 * If the mailcap entry is NULL, this will create a new one.
 */
EXTERN void
fe_helpers_update_mailcap_entry(char *contenttype, NET_mdataStruct *md,
				char *xmode);

/* From plugins.c */
EXTERN XP_Bool fe_IsMailcapEntryPlugin(NET_mdataStruct *md_item);
EXTERN char *fe_GetPluginNameFromMailcapEntry(NET_mdataStruct *md_item);
EXTERN int fe_GetMimetypeInfoFromPlugin(char *pluginName, char *mimetype,
					char **r_desc, char **r_ext);

#endif
