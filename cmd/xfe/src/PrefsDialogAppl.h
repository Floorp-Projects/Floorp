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
/* ** */
#ifndef __xfe_prefsdialogappl_h
#define __xfe_prefsdialogappl_h

#include <X11/Intrinsic.h>
#include "xeditor.h"
#include "PrefsData.h"

#ifdef	__cplusplus
extern "C" {
#endif

extern char   *xfe_prefsDialogAppl_addCommand(char *buf, char *command);
extern void   xfe_prefsDialogAppl_add_new_mailcap_data(char *contenttype,
						 char* src_str,
						 char *command, 
						 char *xmode,
						 Boolean isLocal);
extern void   xfe_prefsDialogAppl_insert_type_at_pos(PrefsDataGeneralAppl *fep,
						 NET_cdataStruct *cd_item, 
						 NET_mdataStruct *md,
						 int pos);
extern void   xfe_prefsDialogAppl_append_type_to_list(PrefsDataGeneralAppl *fep,
						 NET_cdataStruct *cd_item, 
						 NET_mdataStruct *md,
						 int pos);
extern void     xfe_prefsDialogAppl_build_exts(char *ext, NET_cdataStruct *cd);
extern void     xfe_prefsDialogAppl_build_handle(PrefsDataApplEdit *fep);
extern void     xfe_prefsDialogAppl_build_mime_list(PrefsDataGeneralAppl *fep);
extern NET_cdataStruct *xfe_prefsDialogAppl_can_combine(NET_cdataStruct *new_cd, int* old_pos);
extern char     *xfe_prefsDialogAppl_construct_new_mailcap_string(
						 NET_mdataStruct *md,
						 char* contenttype,
						 char *command,
						 char *flag);
extern char            *xfe_prefsDialogAppl_construct_new_mime_string(NET_cdataStruct *new_cd);
extern char            *xfe_prefsDialogAppl_deleteCommand(char *buf);
extern NET_mdataStruct *xfe_prefsDialogAppl_get_mailcap_from_type(char *type);
extern NET_cdataStruct *xfe_prefsDialogAppl_get_mime_data_at_pos(int pos);
extern Boolean          xfe_prefsDialogAppl_get_mime_data_real_pos(
						 NET_cdataStruct *old_cd, 
						 int *real_pos);
extern Bool             xfe_prefsDialogAppl_handle_by_netscape(char *type);
extern Bool             xfe_prefsDialogAppl_handle_by_saveToDisk(char *type);
extern Boolean          xfe_prefsDialogAppl_is_deleted_type(NET_cdataStruct *cd_item);
extern char            *xfe_prefsDialogAppl_replace_new_mime_string(
						 NET_cdataStruct *old_cd, 
						 NET_cdataStruct *new_cd);
extern char            *xfe_prefsDialogAppl_updateKey(char *buf,
						      char *key,
						      char *newVal,
						      Boolean hasSep);

extern void             xfe_prefsDialogAppl_build_static_list(PrefsDataGeneralAppl *fep);
extern char            *xfe_prefsAppl_get_static_app_command(int index);
extern void             xfe_prefsAppl_set_static_app_command(int index, char *command);
extern char            *xfe_prefsAppl_get_static_app_desc(int index);
extern int              xfe_prefsAppl_get_static_app_count();

#ifdef	__cplusplus
}
#endif

#endif


