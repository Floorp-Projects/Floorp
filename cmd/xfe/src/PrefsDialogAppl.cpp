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
   pref_helpers.c --- Preference->Helper dialogs.
   Created: Dora Hsu <dora@netscape.com>, 25-Mar-96.
*/

#define TRUE   1
#define FALSE  0

extern "C" {
#include "net.h"
}
#include "xp_list.h"
#include "xfe.h"
#include "PrefsDialogAppl.h"
#include "prefs.h"

#include <XmL/Grid.h>
#include <XmL/Folder.h>
#include "felocale.h"

#define TYPE			"type"
#define DESC			"desc"
#define ENC				"enc"
#define ICON			"icon"
#define LANG			"lang"
#define EXTS			"exts"
#define ALT				"alt"
#define SPACE			" "

extern int XFE_NO_PLUGINS;
extern int XFE_HELPERS_NETSCAPE;
extern int XFE_HELPERS_UNKNOWN;
extern int XFE_HELPERS_SAVE;
extern int XFE_HELPERS_PLUGIN;
extern int XFE_HELPERS_EMPTY_MIMETYPE;
extern int XFE_HELPERS_LIST_HEADER;
extern int XFE_HELPERS_PLUGIN_DESC_CHANGE;
extern int XFE_HELPERS_APP_TELNET;
extern int XFE_HELPERS_APP_TN3270;
extern int XFE_HELPERS_APP_RLOGIN;
extern int XFE_HELPERS_APP_RLOGIN_USER;

#define FE_MOZILLA_MIME_COMMENT	         "#mime types added by Netscape Helper"
#define FE_MOZILLA_MAILCAP_COMMENT       "#mailcap entry added by Netscape Helper"

#define FIELD_OFFSET(field) offsetof(struct _XFE_GlobalPrefs, field)
#define GET_FIELD(map, prefs) ( ((char*) prefs) + map.offset )

// Static applications

typedef struct _static_app_entry {
	int      *desc_label;
	size_t    prefs_offset;
} static_app_entry;

static static_app_entry static_apps[] = {
	{
		&XFE_HELPERS_APP_TELNET,
		FIELD_OFFSET(telnet_command)
	},
	{
		&XFE_HELPERS_APP_TN3270,
		FIELD_OFFSET(tn3270_command)
	},
	{
		&XFE_HELPERS_APP_RLOGIN,
		FIELD_OFFSET(rlogin_command)
	},
	{
		&XFE_HELPERS_APP_RLOGIN_USER,
		FIELD_OFFSET(rlogin_user_command)
	},
};

static int num_static_apps = sizeof(static_apps) / sizeof(static_app_entry);

extern "C" {
	char                   *XP_GetString(int i);
	XP_Bool                 fe_IsMailcapEntryPlugin(NET_mdataStruct *md_item);
	char                   *fe_GetPluginNameFromMailcapEntry(NET_mdataStruct *md_item);
	int                     fe_GetMimetypeInfoFromPlugin(char *pluginName, char *mimetype,
														 char **r_desc, char **r_ext);

	static char            *PrefsDialogAppl_appendKey (char *buf, char *key, char *val, Boolean hasSep);
	static void             PrefsDialogAppl_append_list_row(PrefsDataGeneralAppl *fep,
															int row, 
															char *data);
	static void             PrefsDialogAppl_add_list_row(PrefsDataGeneralAppl *fep,
														 char *data);
	static char            *PrefsDialogAppl_deleteKey (char *buf, char *key, Boolean hasSep);
	static int              PrefsDialogAppl_getToken(char *mess, char *token, int *n);
	static NET_cdataStruct *PrefsDialogAppl_get_exist_data(NET_cdataStruct *old_cd, int *old_pos);
	static char            *PrefsDialogAppl_new_string(char *old_str, char* type,  
													   char *value, char *del);
	static Boolean          PrefsDialogAppl_start_new_line( char *old_string, char *de, int n );
	static char*            PrefsDialogAppl_cleanString(char *newbuf );
	static void             PrefsDialogAppl_update_mailcap_entry (char *mimeType, NET_mdataStruct *md,
																  char *flag );
}

// ==================== Public Functions ====================

void xfe_prefsDialogAppl_build_mime_list(PrefsDataGeneralAppl *fep)
{
	int item_count = fep->static_apps_count; // static applications are listed first
	NET_mdataStruct *md;
	NET_cdataStruct *cd_item;

	/* Get beginning of the list */

	XP_List *infolist = cinfo_MasterListPointer();
	XtVaSetValues( fep->helpers_list, XmNlayoutFrozen, True, NULL);
 
	/* Get the file format from the object in order to
	   build the Helpers  */

	while ((cd_item = (NET_cdataStruct *)XP_ListNextObject(infolist))) {
		if ( cd_item->ci.type  && strlen(cd_item->ci.type) >0 ) {
			/* Add this item to the list table */
			if ( !xfe_prefsDialogAppl_is_deleted_type(cd_item) ) {
				md = xfe_prefsDialogAppl_get_mailcap_from_type(cd_item->ci.type);
				xfe_prefsDialogAppl_append_type_to_list(fep, cd_item,md, item_count);
				item_count++;
			}
		}
	}

	XtVaSetValues( fep->helpers_list, XmNlayoutFrozen, False, NULL);
}

NET_cdataStruct *xfe_prefsDialogAppl_get_mime_data_at_pos(int pos) /* Starting from 0 */
{
	int              count = 0;
	NET_cdataStruct *cd = NULL;
	XP_List         *infoList = cinfo_MasterListPointer();

	while((cd = (NET_cdataStruct *)XP_ListNextObject(infoList))) {
		if ( cd->ci.type  && *cd->ci.type 
			 && ! xfe_prefsDialogAppl_is_deleted_type(cd)) {
			if ( pos == count ) {
				break;
			}
			else
				count++;
		}
	}

	return cd;
}

Bool xfe_prefsDialogAppl_handle_by_netscape(char *type)
{
	Bool rightType = False;
	if (!strcmp(type, TEXT_HTML) ||
		!strcmp(type, TEXT_MDL)||
		!strcmp(type, TEXT_PLAIN)||
		!strcmp(type, IMAGE_GIF) ||
		!strcmp(type, IMAGE_JPG) ||
		!strcmp(type, IMAGE_PJPG) ||
		!strcmp(type, IMAGE_XBM) ||
		!strcmp(type, IMAGE_XBM2) ||
		!strcmp(type, IMAGE_XBM3) ||
		!strcmp(type, APPLICATION_NS_PROXY_AUTOCONFIG)
		) {
		rightType = True;
	}
	return rightType;
}

Bool xfe_prefsDialogAppl_handle_by_saveToDisk(char *type)
{
	Bool rightType = False;
	if ( !strcmp(type, APPLICATION_OCTET_STREAM) )
		rightType = True;

	return rightType;
}

NET_mdataStruct *xfe_prefsDialogAppl_get_mailcap_from_type(char *type)
{
	NET_mdataStruct *md_item= NULL, *new_item = NULL;
	/* Get beginning of the list
	 */
	XP_List *infolist = mailcap_MasterListPointer();

	while ((md_item = (NET_mdataStruct *)XP_ListNextObject(infolist)))
	  {
		if ( type && md_item->contenttype &&
			 !XP_STRCASECMP(type, md_item->contenttype))
		  {
			new_item = md_item;
			break;
		  }
	  }
	return new_item;
}

char *xfe_prefsDialogAppl_construct_new_mailcap_string(NET_mdataStruct *md, 
													   char* contenttype,
													   char *command, 
													   char *flag)
{
	char *str = NULL;
	char *buf0 = 0;
	char buf1[1024];
	char buf2[1024];
	int  len;

	memset(buf1, '\0', 1024);
	memset(buf2, '\0', 1024);

	if ( !md && !contenttype ) 
		return str;
	else if ( command && *command && flag && *flag ) {
		sprintf(buf1, "%s\n%s;%s", FE_MOZILLA_MAILCAP_COMMENT,
				md? md->contenttype: contenttype ,command );
   
		sprintf(buf2, ";\\\n\t%s=%s", NET_MOZILLA_FLAGS, flag);

		len = strlen(buf1)+strlen(buf2)+1;

		buf0 = (char*)malloc(len*sizeof(char));
		memset(buf0, '\0', len);
		strcpy(buf0, buf1);
		strcat(buf0, buf2);
	}
	else if ( command && *command ){
		/* APPLICATION MODE */
		sprintf(buf1, "%s\n%s;%s", FE_MOZILLA_MAILCAP_COMMENT,
				md ? md->contenttype: contenttype , command);

		len = strlen(buf1)+1;
		buf0 = (char*)malloc(len*sizeof(char));
		memset(buf0, '\0', len);

		strcpy(buf0, buf1);
	}
	else if ( flag && *flag ) {
		sprintf(buf1, "%s\n%s;", FE_MOZILLA_MAILCAP_COMMENT,
				md? md->contenttype: contenttype );
		sprintf(buf2, ";\\\n\t%s=%s", NET_MOZILLA_FLAGS, flag);

		len = strlen(buf1)+strlen(buf2)+1;

		buf0 = (char*)malloc(len*sizeof(char));
		memset(buf0, '\0', len);
		strcpy(buf0, buf1);
		strcat(buf0, buf2);
	}
	else {
		sprintf(buf1, "%s\n%s;", FE_MOZILLA_MAILCAP_COMMENT,
				md? md->contenttype: contenttype );
		sprintf(buf2, ";\\\n\t%s=%s", NET_MOZILLA_FLAGS, NET_COMMAND_UNKNOWN);

		len = strlen(buf1)+strlen(buf2)+1;

		buf0 = (char*)malloc(len*sizeof(char));
		memset(buf0, '\0', len);
		strcpy(buf0, buf1);
		strcat(buf0, buf2);
	}

	memset(buf1, '\0', strlen(buf1));
	memset(buf2, '\0', strlen(buf2));
	if (md &&  md->label && *md->label)
		sprintf(buf1, ";\\\n\t%s=%s", "description", md->label );
	if ( md && md->testcommand && *md->testcommand)
		sprintf(buf2, ";\\\n\t%s=%s", "test", md->testcommand );
	len = strlen(buf0)+strlen(buf1)+strlen(buf2)+strlen("\n")+1;
	str = (char*)malloc(len*sizeof(char));
	memset(str, '\0', len);
	strcpy(str, buf0);
	strcat(str, buf1);
	strcat(str, buf2);
	free(buf0);
	buf0 = 0;
	return str;
}

char*
xfe_prefsDialogAppl_addCommand(char *buf, char *command)
{
	char         *newbuf=0;
	int           len, i, n;
	unsigned int  j;

	/* validate inputs here */
	if (!buf || !command ||
		!strlen(buf) || !strlen(command))
		return 0;

	len = strlen(buf) + strlen(command) +
	  strlen(";") * 2 + strlen("\n") + 1;

	j = strlen(buf);
	newbuf = (char *)malloc (len);
	memset(newbuf, '\0', len);

	/* buf is the src string. Hence skip any comments that are preceding
	 * the actual specification. Then find the place of the ';'.
	 */
	n = 0;
	n += strspn(&buf[n], " \t\n");
	while (buf[n] && buf[n] == '#') 
	  {
		n++;
		/* Skip until and including the newline */
		while (buf[n] && buf[n] != '\n') n++;
		n += strspn(&buf[n], " \t\n");
	  }
	if (buf[n] == '\0')
		/* No entry. The entire entry was a comment. Punt. */
		return(0);
	n += strcspn (&buf[n], ";");        /* find the ';' */

	if ( buf[n] == ';' )
	  {
		j = n+1;

		if ( j < strlen(buf) )
		  {
			i = strcspn (buf+j, ";"); /* find the second ";" */

			/* If the second semicolon is missing, it is right for us
			 * to assume its presence at the end.
			 */
			j = j+i+1;
		  }
	  }

	strncpy(newbuf, buf, n);
	newbuf[n] = '\0';
	strcat (newbuf, ";");
	strcat (newbuf, command);
	if (j < strlen(buf))  /* more entries after command */
	  {
		strcat (newbuf, ";");
		strcat (newbuf, buf + j);
	  }
	/*
	  strcat (newbuf, "\n");
	  */

	return(newbuf);
}

char *xfe_prefsDialogAppl_deleteCommand(char *buf)
{
	char        *newbuf=0;
	int          len, i, n;
	unsigned int j;

	/* validate inputs here */
	if (!buf || !strlen(buf))
		return 0;

	len = strlen(buf) + 1;
	j = strlen(buf);

	newbuf = (char *)malloc (len);
	memset(newbuf, '\0', len);

	/* buf is the src string. Hence skip any comments that are preceding
	 * the actual specification. Then find the place of the ';'.
	 */
	n = 0;
	n += strspn(&buf[n], " \t\n");
	while (buf[n] && buf[n] == '#') {
		n++;
		/* Skip until and including the newline */
		while (buf[n] && buf[n] != '\n') n++;
		n += strspn(&buf[n], " \t\n");
	}
	if (buf[n] == '\0')
		/* No entry. The entire entry was a comment. Punt. */
		return(0);
	n += strcspn (&buf[n], ";");        /* find the ';' */

	if ( buf[n] == ';' ) {
		j = n+1;

		if ( j < strlen(buf) ) {
			i = strcspn (buf+j, ";"); /* find the second ";" */
			j = j +i;

			if ( j < strlen(buf) && buf[j] == ';' )
				j = j + 1;
		}
	}

	strncpy(newbuf, buf, n);
	newbuf[n] = '\0';
	if (j < strlen(buf))  /* more entries after command */ {
		strcat (newbuf, ";");
		strcat (newbuf, ";");
		strcat (newbuf, buf + j);
	}
	/*
	  strcat (newbuf, "\n");
	  */

	return newbuf;
}

char *xfe_prefsDialogAppl_updateKey(char *buf, char *key, char *newVal, Boolean hasSep)
{
	char *newbuf=NULL;
	char  tok[80];
	int   len, i, k=0, n;
	int   found = 0;
	int   quote = 0;
 
	/* validate inputs here */
	if (!buf || !key || !newVal ||
		!strlen(buf) || !strlen(key) || !strlen(newVal))
		return NULL;
 
	len = strlen(buf);
	i = 0;
 
	while (!found && i < len) {
		n = PrefsDialogAppl_getToken(buf, tok, &i);
		if (strcmp(tok, key) == 0) {
			n = strspn(buf+i, " \t\n");
			i += n;
			if (buf[i] == '=') {
				n = strspn(buf+i, " =\"\t\n");
				if ( buf[i+n-1]== '\"')
					quote = 1;
				k = i + n;
 
				if (hasSep)
					len = strcspn (buf+k, ";");
				else {
					if ( quote ) {
						len = strcspn(buf+k, "\""); /* end quote*/
						quote = 0;
					}
					else
						len = strcspn (buf+k, " \t\\");
				}
 
				strncpy(tok, buf+k, len);
				tok[len] = '\0';
				i = k + len;
				found = 1;
			}
		}
	}
  
	if (found) {
		len = strlen(buf) - strlen(tok) + strlen(newVal) + 3;
		newbuf = (char *)malloc (len);
		memset (newbuf, 0, len);
		strncpy(newbuf, buf, k);
		newbuf[k] = 0;
		if (tok[0] == '"')
			strcat (newbuf, "\"");
		strcat (newbuf, newVal);
		if (tok[0] == '"')
			strcat (newbuf, "\"");
		strcat (newbuf, buf+i);
	}
	return(newbuf);
}

void xfe_prefsDialogAppl_add_new_mailcap_data(char *contenttype, 
											  char* src_str,
											  char *command,
											  char *xmode,
											  Boolean isLocal)
{
	NET_mdataStruct *md = 0;

	md = NET_mdataCreate();
	if ( md ) {
		md->contenttype = 0;
		md->command = 0;
		md->label = 0;
		md->testcommand = 0;
		md->printcommand = 0;
		md->stream_buffer_size = 0;
		md->xmode = 0;
		md->src_string = 0;
		md->is_modified = 1;
		md->is_local = isLocal;
		if ( contenttype  && *contenttype )
			StrAllocCopy(md->contenttype, contenttype );
		if ( command && *command )
			StrAllocCopy(md->command, command);
		if ( xmode && *xmode )
			StrAllocCopy(md->xmode, xmode);
		if ( src_str && *src_str )
			StrAllocCopy(md->src_string, src_str);
		NET_mdataAdd(md);
	}
}

Boolean xfe_prefsDialogAppl_is_deleted_type (NET_cdataStruct *cd_item)
{
	NET_mdataStruct *md_item = 0;

	/* Check if this mime is deleted from the list */

	md_item = xfe_prefsDialogAppl_get_mailcap_from_type(cd_item->ci.type);
 
	if ( md_item && md_item->xmode && *md_item->xmode )
		if ( !XP_STRCASECMP(md_item->xmode, NET_COMMAND_DELETED))
			return True;
		else return False;
	else if (md_item && (!md_item->command|| !*md_item->command))
		return True;

	return False;
}

char *xfe_prefsDialogAppl_get_string_from_type(NET_cdataStruct *cd_item, 
											   NET_mdataStruct *md)
{
	char *desc = 0, *action = 0, *line = 0;
 
	if ( cd_item->ci.desc && strlen(cd_item->ci.desc) ) {
		cd_item->ci.desc = fe_StringTrim(cd_item->ci.desc);
		StrAllocCopy( desc, cd_item->ci.desc);
	}
	if ( cd_item->ci.type && strlen(cd_item->ci.type) ) {
		cd_item->ci.type = fe_StringTrim(cd_item->ci.type);
		if (desc == 0)
			StrAllocCopy( desc, cd_item->ci.type);
	}

	if ( md && md->xmode && *md->xmode ) {
		if ( !XP_STRCASECMP(md->xmode, NET_COMMAND_NETSCAPE ))
			StrAllocCopy(action, XP_GetString(XFE_HELPERS_NETSCAPE));
		else if (!strcmp(md->xmode, NET_COMMAND_SAVE_TO_DISK) ||
				 (!strcmp(md->xmode, NET_COMMAND_SAVE_BY_NETSCAPE)))
			StrAllocCopy(action, XP_GetString(XFE_HELPERS_SAVE));
		else if ( !XP_STRCASECMP(md->xmode, NET_COMMAND_UNKNOWN ))
			StrAllocCopy(action, XP_GetString(XFE_HELPERS_UNKNOWN));
		else if (fe_IsMailcapEntryPlugin(md)) {
			char *pluginName = fe_GetPluginNameFromMailcapEntry(md);
			action = PR_smprintf(XP_GetString(XFE_HELPERS_PLUGIN),
								 pluginName);
		}
	}
	else if (md && md->command && *md->command) {
		md->command = fe_StringTrim(md->command);
		StrAllocCopy( action, md->command);
	}
	else if (xfe_prefsDialogAppl_handle_by_netscape(cd_item->ci.type) )
		StrAllocCopy(action, XP_GetString(XFE_HELPERS_NETSCAPE));
	else if (xfe_prefsDialogAppl_handle_by_saveToDisk(cd_item->ci.type))
		StrAllocCopy(action, XP_GetString(XFE_HELPERS_SAVE));
	else
		StrAllocCopy(action, XP_GetString(XFE_HELPERS_UNKNOWN));

	if ( desc && action ) {
		line = PR_smprintf("%s|%s", desc, action); 
	}

	XP_FREEIF(desc);
	XP_FREEIF(action);
	return line;
}
 
void xfe_prefsDialogAppl_append_type_to_list(PrefsDataGeneralAppl *fep, 
											 NET_cdataStruct *cd_item, 
											 NET_mdataStruct *md,
											 int pos)
{
	char *line = xfe_prefsDialogAppl_get_string_from_type(cd_item, md);

	if (line) {
		if (pos <= 0)
			PrefsDialogAppl_add_list_row(fep, line);
		else 
			PrefsDialogAppl_append_list_row(fep, pos, line);
	}

	XP_FREEIF(line);
}
 
void xfe_prefsDialogAppl_insert_type_at_pos(PrefsDataGeneralAppl *fep, 
											NET_cdataStruct *cd_item, 
											NET_mdataStruct *md,
											int pos)
{
	char *line = xfe_prefsDialogAppl_get_string_from_type(cd_item, md);
	if (! line) return;

	int total ;
	XtVaGetValues( fep->helpers_list, XmNrows, &total, 0 );

	if ( pos < total ) {
		XmLGridAddRows(fep->helpers_list, XmCONTENT, pos, 1);
		XmLGridSetStringsPos(fep->helpers_list, XmCONTENT, pos, 
							 XmCONTENT, 0, line);
	}
	else {
		/* append to the end */
		PrefsDialogAppl_append_list_row(fep, pos, line);
	}

	XP_FREEIF(line);
}
 
void xfe_prefsDialogAppl_build_exts(char *ext, NET_cdataStruct *cd)
{
	int n = cd->num_exts;
 
	cd->exts = (char **) (n ? XP_REALLOC(cd->exts, (n+1)*sizeof(char *))
						  : XP_ALLOC(sizeof(char *)));
 
	if(!cd->exts)
		return;
 
	cd->exts[n] = 0;
	StrAllocCopy(cd->exts[n], ext);
	cd->num_exts = ++n;
}

NET_cdataStruct *xfe_prefsDialogAppl_can_combine ( NET_cdataStruct *new_cd, 
												   int             *old_pos)
{
	NET_cdataStruct *old_cd = NULL;

	old_cd = PrefsDialogAppl_get_exist_data(new_cd, old_pos);

	return old_cd;
}
 
Boolean
xfe_prefsDialogAppl_get_mime_data_real_pos(NET_cdataStruct *old_cd, int *real_pos)
{
	Boolean found = False; 
	XP_List *infoList = cinfo_MasterListPointer();
	NET_cdataStruct *cd;

	if (!infoList) return found;
	*real_pos = 0;

	while ((cd=(NET_cdataStruct*)XP_ListNextObject(infoList)))
	  {
		if ( !xfe_prefsDialogAppl_is_deleted_type(cd) )
		  {
			if ( cd->ci.type ) /* should skip the encoding ones which type = null */
			  {
				if ( old_cd == cd )
				  {
					found = True;
					break;
				  }
				else (*real_pos)++;
			  }
		  }
	  }
	return found;
}

char *
xfe_prefsDialogAppl_replace_new_mime_string(NET_cdataStruct *old_cd, NET_cdataStruct *new_cd)
{
	int n;
	char old_exts[200];
	char new_exts[200];
	char *str = NULL;
	char *old_str = NULL;



	if ( !old_cd->src_string ) return NULL;
	str = strdup(old_cd->src_string);
	memset(old_exts, '\0', 200);
	memset(new_exts, '\0', 200);

	if (old_cd->ci.type && *old_cd->ci.type )
	  {
		if (new_cd->ci.type && *new_cd->ci.type ) 
		  {
			old_str = str;
			str = xfe_prefsDialogAppl_updateKey(old_str, TYPE, new_cd->ci.type, 0 );

			if ( !str )
			  {
				str = PrefsDialogAppl_new_string(old_str, TYPE,
												 new_cd->ci.type, SPACE);
			  }
			XP_FREE(old_str);
		  }
		else 
		  {
			old_str = str;
			str = PrefsDialogAppl_deleteKey(str, TYPE, 0 );
			XP_FREE(old_str);
		  }
	
	  }
	else
	  {
		old_str = str;
		str = PrefsDialogAppl_new_string(str, TYPE,
									new_cd->ci.type, SPACE);
		XP_FREE(old_str);
	  }



	if (old_cd->ci.desc && *old_cd->ci.desc )
	  {
		if (new_cd->ci.desc && *new_cd->ci.desc ) 
		  {
			old_str = str;
			str = xfe_prefsDialogAppl_updateKey(str, DESC, new_cd->ci.desc, 0 );
			
			if ( !str )
			  {
				str = PrefsDialogAppl_new_string(old_str, DESC,
											new_cd->ci.desc, SPACE);
			  }
			XP_FREE(old_str);
		  }
		else 
		  {
			old_str = str;
			str = PrefsDialogAppl_deleteKey(str, DESC,0 );
			XP_FREE(old_str);
		  }
	  }
	else
	  {
		old_str = str;
		str = PrefsDialogAppl_new_string(str, DESC,
									new_cd->ci.desc, SPACE);
		XP_FREE(old_str);
	  }

	if ( old_cd->exts && *old_cd->exts )
	  {
		for (n= 0; n < old_cd->num_exts; n++ )
		  {
			if ( n > 0 )
				strcat( old_exts, ",");
			if ( n == 0 )
				strcpy(old_exts, old_cd->exts[n]);
			else
				strcat(old_exts, old_cd->exts[n]);
		  }

		if ( new_cd->num_exts > 0 )
		  { 

			for (n= 0; n < new_cd->num_exts; n++ )
			  {
				if ( n > 0 )
					strcat( new_exts, ",");
				if ( n == 0 )
					strcpy(new_exts, new_cd->exts[n]);
				else
					strcat(new_exts, new_cd->exts[n]);
			  }
			old_str = str;
			str = xfe_prefsDialogAppl_updateKey(str, EXTS, new_exts, 0 );
			if ( !str )
			  {
				str = PrefsDialogAppl_new_string(old_str, EXTS,
											new_exts, SPACE);
			  }
			XP_FREE(old_str);
		  }
		else
		  {
			old_str = str;
			str = PrefsDialogAppl_deleteKey(str, EXTS, 0);
			XP_FREE(old_str);
		  }
	  }
	else
	  {
		if ( new_cd->num_exts > 0 )
		  { 

			for (n= 0; n < new_cd->num_exts; n++ )
			  {
				if ( n > 0 )
					strcat( new_exts, ",");
				strcat(new_exts, new_cd->exts[n]);
			  }
			old_str = str;
			str = PrefsDialogAppl_new_string(str, EXTS,
										new_exts, SPACE);
			XP_FREE(old_str);
		  }
	  }

	return str;
}

char *
xfe_prefsDialogAppl_construct_new_mime_string(NET_cdataStruct *new_cd)
{
	int n;
	int len;
	char buffer1[1024];
	char buffer2[1024];
	char *string = 0;
	char *str = 0;
	char *old_str = NULL;
	XP_List *masterList = cinfo_MasterListPointer();

	strcpy(buffer1, FE_MOZILLA_MIME_COMMENT);
	if ( NET_IsOldMimeTypes(masterList) )
	  {
		sprintf(buffer2, "%s", new_cd->ci.type); 
		if ( new_cd->num_exts > 0 ) 
			for ( n = 0; n < new_cd->num_exts; n++ )
			  {
				if (n == 0 ) 
					strcat(buffer2, SPACE);
				else 
					strcat(buffer2, SPACE);
				strcat(buffer2, new_cd->exts[n]);
			  }
		len = strlen(buffer1)+strlen(buffer2)+ 2*strlen("\n")+1;
		string = (char*)malloc(len *sizeof(char));
		strcpy(string, buffer1);
		strcat(string, "\n");
		strcat(string, buffer2);
	  }
	else
	  {
		n = 0;
		if ( new_cd->ci.type &&  *new_cd->ci.type )
		  {
			str = PrefsDialogAppl_new_string(old_str, TYPE, new_cd->ci.type, SPACE);
			XP_FREE(old_str);
		  }
		if ( new_cd->ci.encoding && *new_cd->ci.encoding )
		  {
			old_str = str;
			str = PrefsDialogAppl_new_string(old_str, ENC, new_cd->ci.encoding, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.language && *new_cd->ci.language)
		  {
			old_str = str;
			str = PrefsDialogAppl_new_string(old_str, LANG, new_cd->ci.language, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.desc && *new_cd->ci.desc)
		  {
			old_str = str;
			str = PrefsDialogAppl_new_string(old_str, DESC, 
										new_cd->ci.desc, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.alt_text && *new_cd->ci.alt_text)
		  {
			old_str = str;
			str = PrefsDialogAppl_new_string(old_str, ALT, new_cd->ci.alt_text, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.icon && *new_cd->ci.icon)
		  {
			old_str = str;
			str = PrefsDialogAppl_new_string(old_str, ICON, new_cd->ci.icon, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->exts && *new_cd->exts)
		  {
			char exts[1024];

			memset(exts, '\0', 1024);
			for (n= 0; n < new_cd->num_exts; n++ )
			  {
				if ( n > 0 )
					strcat( exts, ",");
				strcat(exts, new_cd->exts[n]);
			  }
			
			old_str = str;
			str = PrefsDialogAppl_new_string(old_str, EXTS, exts, SPACE);
			XP_FREE(old_str);
		  }
		len = strlen(buffer1)+strlen(str)+ 2 *strlen("\n") + 1;
		string = (char*)malloc(len *sizeof(char));
		memset(string, '\0', len);
		strcpy(string, buffer1);
		strcat(string, "\n");
		strcat(string, str);
	  }

	return string;
}

void
xfe_prefsDialogAppl_build_handle(PrefsDataApplEdit    *fep)
{
	NET_mdataStruct *old_md = NULL;
	char *src_str;
	char *oldStr = NULL;

	old_md = xfe_prefsDialogAppl_get_mailcap_from_type(fep->cd->ci.type);

	/* old_md might be marked "Deleted", but that's okay */
	/* because we will replace it with the new md data value */

	if ( XmToggleButtonGetState(fep->navigator_toggle) )
	  {
		PrefsDialogAppl_update_mailcap_entry(fep->cd->ci.type, old_md,
											 NET_COMMAND_NETSCAPE);
			
	  }
	else if (XmToggleButtonGetState(fep->plugin_toggle))
	  {
		char *value;
		int n = 0;
		XtVaGetValues(fep->plugin_combo, XmNuserData, &n, 0);
		value = PR_smprintf("%s%s", NET_COMMAND_PLUGIN, fep->plugins[n]);
		PrefsDialogAppl_update_mailcap_entry(fep->cd->ci.type, old_md, value);
		XP_FREE(value);
	  }
	else if (XmToggleButtonGetState(fep->app_toggle))
	  {
		char * text = fe_GetTextField(fep->app_text);
		if ( text && *text)
		  {
			text = fe_StringTrim(text);

			if( old_md )
			  {
				oldStr = old_md->src_string;
				old_md->src_string = 0;
			  }

			if ( !oldStr || !strlen(oldStr) ) 
				src_str = xfe_prefsDialogAppl_construct_new_mailcap_string
					(old_md, fep->cd->ci.type, text, NULL);
			else 
			  {
				src_str = xfe_prefsDialogAppl_addCommand (oldStr, text);
				XP_FREE(oldStr);

				oldStr = src_str;
				src_str = PrefsDialogAppl_deleteKey(oldStr, NET_MOZILLA_FLAGS, 1);
				if ( src_str )
					XP_FREE(oldStr);
				else src_str = oldStr;

			  }

			if ( old_md )
			  {
				StrAllocCopy(old_md->command, text);
				if ( old_md->src_string )
					XP_FREE(old_md->src_string);
				old_md->src_string = 0;
				if ( src_str && *src_str )
					old_md->src_string = src_str;
				if (old_md->xmode) XP_FREE(old_md->xmode);
				old_md->xmode = 0;
				old_md->is_modified = 1;
			  }
			else
			  {
				xfe_prefsDialogAppl_add_new_mailcap_data(fep->cd->ci.type,
														 src_str,text, NULL, 1);
				XP_FREE(src_str);
			  }
		  }
		else
		  {
			if ( old_md )
			  {
				oldStr = old_md->src_string;
				old_md->src_string = 0;
			  }
			if ( !oldStr || !strlen(oldStr) ) 
			  {
				src_str = xfe_prefsDialogAppl_construct_new_mailcap_string
					(old_md, fep->cd->ci.type, NULL, NET_COMMAND_UNKNOWN);
			  }
			else 
			  {
				src_str = xfe_prefsDialogAppl_deleteCommand(oldStr); 
				XP_FREE(oldStr);

				oldStr = src_str;
				src_str = xfe_prefsDialogAppl_updateKey(oldStr, NET_MOZILLA_FLAGS,
														NET_COMMAND_UNKNOWN, True);
				if ( !src_str )
					src_str = PrefsDialogAppl_appendKey ( oldStr, NET_MOZILLA_FLAGS,
														  NET_COMMAND_UNKNOWN, 1);

				XP_FREE(oldStr);
			  }
			if ( old_md )
			  {
				StrAllocCopy(old_md->command, text);
				if ( old_md->src_string ) 
					XP_FREE(old_md->src_string);
				old_md->src_string = 0;
				if ( src_str && *src_str )
					old_md->src_string = src_str;
				if (old_md->xmode) XP_FREE(old_md->xmode);
				old_md->xmode = strdup(NET_COMMAND_UNKNOWN);
				old_md->is_modified = 1;
			  }
			else
			  {
				xfe_prefsDialogAppl_add_new_mailcap_data(fep->cd->ci.type,
														 src_str,NULL,
														 NET_COMMAND_UNKNOWN, 1);
				XP_FREE(src_str);
			  }
		  }
		if (text) XtFree(text);
	  }
	else if (XmToggleButtonGetState(fep->save_toggle))
	  {
		  PrefsDialogAppl_update_mailcap_entry(fep->cd->ci.type, old_md,
											  NET_COMMAND_SAVE_TO_DISK);
	  }
	else /* Everything else, we set it to be unknown */
	  {
		PrefsDialogAppl_update_mailcap_entry(fep->cd->ci.type, old_md,
											 NET_COMMAND_UNKNOWN);
	  }
		
}

int xfe_prefsAppl_get_static_app_count()
{
	return num_static_apps;
}

void xfe_prefsAppl_set_static_app_command(int index, char *command)
{
	char *field;

	if (index >= num_static_apps) {
		/* shouldn't happen */
		return;
	}

	field = (char *)&fe_globalPrefs + static_apps[index].prefs_offset;
	XP_FREEIF(*(char **)field);
	*(char **)field = XP_STRDUP(command);
}

char *xfe_prefsAppl_get_static_app_command(int index)
{
	/* Caller shouldn't free up the string returned from this function */
	char *field;
	if (index >= num_static_apps) {
		/* shouldn't happen */
		return "";
	}
	else {
		field = (char *)&fe_globalPrefs + static_apps[index].prefs_offset;
		return *(char **)field;
	}
}

char *xfe_prefsAppl_get_static_app_desc(int index)
{
	/* Caller shouldn't free up the string returned from this function */
	if (index >= num_static_apps) {
		/* shouldn't happen */
		return "";
	}
	else {
		return XP_GetString(*static_apps[index].desc_label);
	}
}

void xfe_prefsDialogAppl_build_static_list(PrefsDataGeneralAppl *fep)
{
	// Build the static apps first. Instead of using static mime 
	// libprefs API which I cannot find, use the regular prefs
    // for now. This is not ideal, but at least we can get this
    // functionality in.

	XtVaSetValues(fep->helpers_list, XmNlayoutFrozen, True, NULL);
 
	char *desc = 0;
	char *action = 0;
	char *line = 0;

	for (int i = 0; i < num_static_apps; i++) {
		StrAllocCopy(desc, xfe_prefsAppl_get_static_app_desc(i));
		StrAllocCopy(action, xfe_prefsAppl_get_static_app_command(i));

		if ( desc && action ) {
			line = PR_smprintf("%s|%s", desc, action); 
			if (i == 0)
				PrefsDialogAppl_add_list_row(fep, line);
			else 
				PrefsDialogAppl_append_list_row(fep, i, line);
		}
		XP_FREEIF(desc);
		XP_FREEIF(action);
		XP_FREEIF(line);
	}

	XtVaSetValues(fep->helpers_list, XmNlayoutFrozen, False, NULL);
	
	fep->static_apps_count = num_static_apps; 
}

// ==================== Static Functions ====================

static void PrefsDialogAppl_add_list_row(PrefsDataGeneralAppl *fep, char *data)
{
	int total ;
	XtVaGetValues( fep->helpers_list, XmNrows, &total, 0 );
	XmLGridAddRows(fep->helpers_list, XmCONTENT, -1, 1);
	if ( total > 0 )
		XmLGridMoveRows(fep->helpers_list, 1, 0, total);
	XmLGridSetStringsPos(fep->helpers_list, XmCONTENT, 0, 
						 XmCONTENT, 0, data);
} 

static void PrefsDialogAppl_append_list_row(PrefsDataGeneralAppl *fep, int row, char *data)
{
	int total ;
	XtVaGetValues( fep->helpers_list, XmNrows, &total, 0 );
	if ( row >= total )
		XmLGridAddRows(fep->helpers_list, XmCONTENT, -1, 1);

	XmLGridSetStringsPos(fep->helpers_list, XmCONTENT, row, 
						 XmCONTENT, 0, data);
} 

static int /* returns len of token */
PrefsDialogAppl_getToken (char *mess, char *token, int *n) /* n: next position to search*/
{
	int len, i, j;
 
	if (mess == NULL)
		return(0);
 
	if ((unsigned int)*n > strlen(mess))
		return(0);
 
	i = *n;
	len = strspn(mess+i, " =;\\\t\n\0");
	i += len;
 
	j = i;
	if (mess[i] == '"')
	  {
		i++; j++;
		for (len = 0; mess[j] != '\n' && mess[j++] != '"'; len++);
	  }
	else
		len = strcspn(mess+i, " =;\\\t\n\0");
	strncpy(token, mess+i, len);
	token[len] = '\0';
 
	if (j > 0 && mess[j-1] == '"') i++;
 
	*n = i + len;
 
	return(len);
}

static NET_cdataStruct *
PrefsDialogAppl_get_exist_data(NET_cdataStruct *old_cd, int *old_pos)
{
	NET_cdataStruct *found_cd = NULL;
	NET_cdataStruct *cd = NULL;
	XP_List *infoList = cinfo_MasterListPointer();

	*old_pos = 0;
 
	if ( !infoList )
		return found_cd;
 
	while ((cd= (NET_cdataStruct *)XP_ListNextObject(infoList)))
	  {
		if ( old_cd->ci.type && cd->ci.type )
		  {
			if( !XP_STRCASECMP(old_cd->ci.type, cd->ci.type))
			  {
				/* found matching type regardless whether it is deleted
				 * one or not. caller needs to decide what they want to
				 *  with this old_cd 
				 */
				found_cd = cd;
				break;
			  }
			else (*old_pos)++;
		  }
	  }
	return found_cd;
}


static char *
PrefsDialogAppl_new_string(char *old_str, char* type,  char *value, char *del)
{
	char *str = 0 ;
	int len = 0;
	Boolean newline = False;
	Boolean quoted = False;


	if ( !value || !*value)
	  {
		if ( old_str )
		  {
			str = (char*)malloc((strlen(old_str)+1)*sizeof(char));
			strcpy(str, old_str);
			return str;
		  }
		else return NULL;
	  }

	if (!old_str)
	  {
		if ( !strcmp(type, DESC) || !strcmp(type, EXTS) ) 
		  {
			len = strlen(type) + strlen("=") + strlen(value) + 2 * strlen("\"") 
			  + strlen(del) + 1;

			str = (char*)malloc(len *sizeof(char));
			memset(str, '\0', len);
			strcpy(str, type);
			strcat(str, "=");
			strcat(str, "\"");
			strcat(str, value);
			strcat(str, "\"");
			strcat(str, del);
		  }
		else 
		  {
			len = strlen(type) +strlen("=") + strlen(value) + 
			  strlen(del) +1;

			str = (char*)malloc(len *sizeof(char));
			memset(str, '\0', len);
			strcpy(str, type);
			strcat(str, "=");
			strcat(str, value);
			strcat(str, del);
		  }
	  }
	else 
	  {
		if ( old_str[strlen(old_str)-1] == '\n')
			old_str[strlen(old_str)-1] = '\0';


		if (PrefsDialogAppl_start_new_line(old_str, del, 2) )
		  {
			newline = True;
			len = strlen(" \\\n");
		  }

		if ( !strcmp(type, DESC) || !strcmp(type, EXTS) ) 
		  {
			quoted = True;
			len = len + 2 * strlen("\"") ;
		  }


		len = len + strlen(old_str)+ strlen("\t\t")+ 
		  strlen(type) + strlen("=") + strlen(value)+ 
		  strlen(del) +   1;

		str = (char*)malloc(len*sizeof(char));
		memset(str, '\0', len);
		strcpy(str, old_str);

		if ( newline )
			strcat(str, " \\\n");
		else
			strcat(str, "\t\t");
		strcat(str, type);
		strcat(str, "=");
		if (quoted)
			strcat(str, "\"");
		strcat(str, value);
		if (quoted)
			strcat(str, "\"");
		strcat(str, del);

	  }

	return str;
}

static Boolean 
PrefsDialogAppl_start_new_line( char *old_string, char *de, int n )
{
	Boolean yes = False;
	int i = 1;
	char *str = 0;
	char *string = strdup(old_string);

	while (1) {

	  while ((*string) && XP_IS_SPACE(*string)) ++string;

	  if ( *string && (*string != '#') ) 
		{
		  if ( string && de && n > 0)
			{
			  str =  XP_STRTOK( string, de);

			  while (  i < n || str )
				{
				  str = XP_STRTOK(NULL, de);
				  i++;
				}   

			  if ( i >= n ) /* String ended */
				{
				  yes = True;
				  break;
				}
			}
		}
	  else
		{
		  while ((*string) && (*string != '\n')) ++string;
		  if (*string && *string == '\n') string++;
		  else break;
		}
	}

	return yes;
} 

static char*
PrefsDialogAppl_deleteKey (char *buf, char *key, Boolean hasSep)
{
	char *newbuf=NULL;
	char  tok[80];
	int   len, i, j = 0, k = 0, n;
	int   found = 0;
	int   quote = 0;
 
	/* validate inputs here */
	if (!buf || !key ||
		!strlen(buf) || !strlen(key))
		return NULL;
 
	len = strlen(buf);
	i = 0;
	while (!found && i < len)
	  {
		j = i;
		n = PrefsDialogAppl_getToken(buf, tok, &i);
		if (strcmp(tok, key) == 0)
		  {
			n = strspn(buf+i, " \t\n");
			i += n;
			if (buf[i] == '=')
			  {
				n = strspn(buf+i, " =\"\t\n");
				if ( buf[i+n-1] == '\"')
					quote = 1;
				k = i + n;
				if (hasSep)
					len = strcspn (buf+k, ";");
				else
				  {
					if ( quote )
					  {
						n = strcspn(buf+k, "\""); /* end quote*/
						if ( buf[k+n] == '\"')
							k = k + n;
						quote = 0;
					  }
					len = strcspn (buf+k, " \t\\");
				  }
				i = k + len;
				found = 1;
			  }
		  }
		else
		  {
			n = strspn(buf+i, " ;\\\t\n");
			i += n;
		  }
	  }
	if (found)
	  {
		newbuf = strdup (buf);
		newbuf[j] = '\0';
		found = 0;
		newbuf = PrefsDialogAppl_cleanString(newbuf);
		strcat (newbuf, buf+i);
		newbuf = PrefsDialogAppl_cleanString(newbuf);
 
		/*
		  if( newbuf[strlen(newbuf)-1] != '\n' ) strcat(newbuf, "\n");
		  */
	  }
	return(newbuf);
}

static char* PrefsDialogAppl_cleanString(char *newbuf )
{
	int i = 0;
	int found =  0;
 
	while (!found && strlen(newbuf) > 0)
	  {
		if ( newbuf[strlen(newbuf)-1] == ' ' ||
			 newbuf[strlen(newbuf)-1] == '\\' ||
			 newbuf[strlen(newbuf)-1] == ';' ||
			 newbuf[strlen(newbuf)-1] == '\n' ||
			 newbuf[strlen(newbuf)-1] == '\t' )
		  {
			newbuf[strlen(newbuf)-1] = '\0';
		  }
		else found = 1;
	  }
 
	found = 0;
	i = 0;
	while (!found && strlen(newbuf) > 0 && ((unsigned int)i) < strlen(newbuf))
	  {
		if ( newbuf[i] == '\\' ||
			 newbuf[i] == ';' )
		  {
			strncpy( newbuf, newbuf+i+1, strlen(newbuf)-i);
		  }
		else found = 1;
		i++;
	  }
 
	return newbuf;
}

static void
PrefsDialogAppl_update_mailcap_entry (char *mimeType, NET_mdataStruct *md,
									  char *flag )
{
	char *oldStr = NULL;
	char *src_str = 0;

	if (md)
	  {
		oldStr = md->src_string;
		md->src_string = 0;
	  }

	if ( !oldStr || !strlen(oldStr) ) 
		src_str = xfe_prefsDialogAppl_construct_new_mailcap_string (md, mimeType, 	
																	md? md->command: (char *)NULL,
																	flag);
	else 
	  {
		src_str = xfe_prefsDialogAppl_updateKey(oldStr, NET_MOZILLA_FLAGS, flag, True);

		if ( !src_str )
			src_str = PrefsDialogAppl_appendKey(oldStr, NET_MOZILLA_FLAGS, flag, 1);
		
		XP_FREE(oldStr);
	  }

	if ( md )
	  {
		if ( md->src_string ) XP_FREE(md->src_string);
		if ( src_str && *src_str)
			md->src_string = src_str;
		if ( md->xmode ) XP_FREE(md->xmode);
		md->xmode = strdup(flag);
		md->is_modified = 1;
	  }
	else {
	  xfe_prefsDialogAppl_add_new_mailcap_data(mimeType, src_str, NULL, flag, 1);
	  XP_FREE(src_str);
	}
} 

static char*
PrefsDialogAppl_appendKey (char *buf, char *key, char *val, Boolean hasSep)
{
	char *newbuf=NULL;
	char  str[80];
	int   len, i;
	
	/* validate inputs here */
	if (!buf || !key || !val ||
		!strlen(buf) || !strlen(key) || !strlen(val))
		return NULL;
	
	if (hasSep)
		sprintf(str, "; \\\n%s=%s", key, val);
	else
		sprintf(str, " \\\n%s=%s", key, val);
	len = strlen(buf);
	for (i = len-1;
		 buf[i] == '\n' || buf[i] == '\\' || buf[i] == ' ' || buf[i] == '\t';
		 i--);
	i++;
 
	len = strlen(buf) + strlen(str) + 1 + 1;
	newbuf = (char *)malloc(len);
	memset (newbuf, 0, len);
	strncpy (newbuf, buf, i);  /* i-1 */
	newbuf[i] = 0;
	if (newbuf[i-1] == ';') newbuf[i-1] = 0;
	strcat (newbuf, str);
 
	return(newbuf);
}
 


