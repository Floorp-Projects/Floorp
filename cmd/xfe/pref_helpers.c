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
/* 
   pref_helpers.c --- Preference->Helper dialogs.
   Created: Dora Hsu <dora@netscape.com>, 25-Mar-96.
*/


#include "mozilla.h"
#include "xlate.h"
#include "xfe.h"
#include "nslocks.h"
#include "secnav.h"
#include "mozjava.h"
#include "prnetdb.h"
#include "e_kit.h"
#include "pref_helpers.h"
#include "np.h"

#include <Xm/Label.h>
#include <Xfe/Xfe.h>
#include "felocale.h"

#ifndef _STDC_
#define _STDC_ 1
#define HACKED_STDC 1
#endif

#include <XmL/Grid.h>
#include <XmL/Folder.h>

#ifdef HACKED_STDC
#undef HACKED_STDC
#undef _STDC_
#endif

#include "msgcom.h"

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_NO_PLUGINS;
extern int XFE_HELPERS_NETSCAPE;
extern int XFE_HELPERS_UNKNOWN;
extern int XFE_HELPERS_SAVE;
extern int XFE_HELPERS_PLUGIN;
extern int XFE_HELPERS_EMPTY_MIMETYPE;
extern int XFE_HELPERS_LIST_HEADER;
extern int XFE_HELPERS_PLUGIN_DESC_CHANGE;

#define DEFAULT_COLUMN_WIDTH 	35     
#define MAX_COLUMN_WIDTH 	100	

#define FE_MOZILLA_MIME_COMMENT		"#mime types added by Netscape Helper"
#define FE_MOZILLA_MAILCAP_COMMENT	"#mailcap entry added by Netscape Helper"
#define TYPE				"type"
#define DESC				"desc"
#define ENC				"enc"
#define ICON				"icon"
#define LANG				"lang"
#define EXTS				"exts"
#define ALT				"alt"
#define SPACE				" "

/* For sys_errlist and sys_nerr */
#include "prprf.h"
#include "libi18n.h"
#include "fonts.h"

/* External Methods */
void fe_browse_file_of_text (MWContext *context, Widget text_field,
                             Boolean dirp);

void fe_UnmanageChild_safe (Widget w);

/* From prefdialogs.c */
void fe_MarkHelpersModified(MWContext *context);

/* Internal Methods */
static void
fe_helpers_application_browse_cb(Widget widget, XtPointer closure,
								 XtPointer call_data);
static void
fe_helpers_set_handle_by_cb (Widget w, XtPointer closure, 
							 XtPointer call_data );

static void
fe_helpers_append_type_to_list(struct fe_prefs_helpers_data *fep,
							   NET_cdataStruct *cd_item, 
							   NET_mdataStruct *md, int pos);

/*===================== A pile of string manipulation=======================*/
static int /* returns len of token */
fe_GetToken (char *mess, char *token, int *n) /* n: next position to search*/
{
	int len, i, j;
 
	if (mess == NULL)
		return(0);
 
	if (*n > (int)strlen(mess))
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

static char*
fe_helpers_appendKey (char *buf, char *key, char *val, Boolean hasSep)
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
 
/*----------------------------------------------------------
 
-----------------------------------------------------------*/
 
static char* cleanString(char *newbuf )
{
	int i = 0;
	int found =  0;
 
	while (!found && (int)strlen(newbuf) > 0)
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
	while (!found && (int)strlen(newbuf) > 0 && i < (int)strlen(newbuf))
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


static char*
fe_helpers_deleteKey (char *buf, char *key, Boolean hasSep)
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
		n = fe_GetToken(buf, tok, &i);
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
		newbuf = cleanString(newbuf);
		strcat (newbuf, buf+i);
		newbuf = cleanString(newbuf);
 
		/*
		  if( newbuf[strlen(newbuf)-1] != '\n' ) strcat(newbuf, "\n");
		  */
	  }
	return(newbuf);
}
/*----------------------------------------------------------
 
-----------------------------------------------------------*/
 
static char*
fe_helpers_updateKey (char *buf, char *key, char *newVal, Boolean hasSep)
{
	char *newbuf=NULL;
	char  tok[80];
	int   len, i, k, n;
	int   found = 0;
	int   quote = 0;
 
	/* validate inputs here */
	if (!buf || !key || !newVal ||
		!strlen(buf) || !strlen(key) || !strlen(newVal))
		return NULL;
 
	len = strlen(buf);
	i = 0;
 
	while (!found && i < len)
	  {
		n = fe_GetToken(buf, tok, &i);
		if (strcmp(tok, key) == 0)
		  {
			n = strspn(buf+i, " \t\n");
			i += n;
			if (buf[i] == '=')
			  {
				n = strspn(buf+i, " =\"\t\n");
				if ( buf[i+n-1]== '\"')
					quote = 1;
				k = i + n;
 
				if (hasSep)
					len = strcspn (buf+k, ";");
				else
				  {
					if ( quote )
					  {
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
 
 
	if (found)
	  {
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

/*----------------------------------------------------------
-----------------------------------------------------------*/
static char*
fe_helpers_addCommand(char *buf, char *command)
{
	char *newbuf=0;
	int   len, i, n, j;

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

		if ( j < (int)strlen(buf) )
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
	if (j < (int)strlen(buf))  /* more entries after command */
	  {
		strcat (newbuf, ";");
		strcat (newbuf, buf + j);
	  }
	/*
	  strcat (newbuf, "\n");
	  */

	return(newbuf);
}

static char*
fe_helpers_deleteCommand(char *buf)
{
	char *newbuf=0;
	int   len, i, n, j;

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

		if ( j < (int)strlen(buf) )
		  {
			i = strcspn (buf+j, ";"); /* find the second ";" */
			j = j +i;

			if ( j < (int)strlen(buf) && buf[j] == ';' )
				j = j + 1;
		  }
	  }

	strncpy(newbuf, buf, n);
	newbuf[n] = '\0';
	if (j < (int)strlen(buf))  /* more entries after command */
	  {
		strcat (newbuf, ";");
		strcat (newbuf, ";");
		strcat (newbuf, buf + j);
	  }
	/*
	  strcat (newbuf, "\n");
	  */

	return newbuf;
}

/*======================== A pile of callbacks  ========================*/
void
fe_helpers_add_new_mailcap_data(char *contenttype, char* src_str,
								char *command, char *xmode, Boolean isLocal)
{
	NET_mdataStruct *md = 0;

	md = NET_mdataCreate();
	if ( md )
	  {
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

static Boolean
fe_helpers_is_deleted_type (NET_cdataStruct *cd_item)
{
	NET_mdataStruct *md_item = 0;

	/* Check if this mime is deleted from the list */

	md_item = fe_helpers_get_mailcap_from_type(cd_item->ci.type);
 
	if ( md_item && md_item->xmode && *md_item->xmode )
		if ( !XP_STRCASECMP(md_item->xmode, NET_COMMAND_DELETED))
			return True;
		else return False;
	else if (md_item && (!md_item->command|| !*md_item->command))
		return True;

	return False;
}

static void
fe_helpers_plugin_selected(struct fe_prefs_helpers_data *fep, char *pluginName)
{
	/* Change the description and extensions to what the plugins says */
	char *mimetype, *desc=NULL, *ext=NULL;
	char *cur_desc=NULL, *cur_ext=NULL;
	XP_Bool differ = True;

	mimetype = fe_GetTextField(fep->mime_types_text);

	if (fe_GetMimetypeInfoFromPlugin(pluginName, mimetype, &desc, &ext) >= 0) {
	  /* If either the description or the extension is different from what is
	   * already being displayed, ask the user whether to revert to what the
	   * plugin says.
       */
	  cur_desc = fe_GetTextField(fep->mime_types_desc_text);
	  cur_ext = fe_GetTextField(fep->mime_types_suffix_text);
     
	  if (desc) differ = strcmp(desc, cur_desc);
	  else differ = *cur_desc;
     
	  if (!differ)
		  if (ext) differ = strcmp(ext, cur_ext);
		  else differ = *cur_ext;

	  if (differ) {
		char *buf;
		buf = PR_smprintf(XP_GetString(XFE_HELPERS_PLUGIN_DESC_CHANGE),
						  mimetype, desc?desc:"", ext?ext:"");
		if (FE_Confirm(fep->context, buf)) {
		  XtVaSetValues(fep->mime_types_desc_text, XmNcursorPosition, 0, 0);
		  fe_SetTextField(fep->mime_types_desc_text, desc);
		  XtVaSetValues(fep->mime_types_suffix_text, XmNcursorPosition, 0, 0);
		  fe_SetTextField(fep->mime_types_suffix_text, ext);
		}
		if (buf) XP_FREE(buf);
	  }
	}

	if (mimetype) XP_FREE(mimetype);
	if (cur_desc) XP_FREE(cur_desc);
	if (cur_ext) XP_FREE(cur_ext);
}

static void
fe_helpers_plugin_sel_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	struct fe_prefs_helpers_data *fep =
	  (struct fe_prefs_helpers_data *) closure;
	int n = 0;
	char *pluginName;

	if (!XmToggleButtonGetState(fep->plugin_b)) return;

	XtVaGetValues(widget, XmNuserData, &n, 0);
	pluginName = fep->plugins[n];
	fe_helpers_plugin_selected(fep, pluginName);
}

static void
fe_helpers_build_plugin_list(struct fe_prefs_helpers_data *fep,
							 char *mime_type_to_find)
{
	int i;
	Cardinal ac;
	Arg av[20];
	Widget button = 0, current = 0;
	char namebuf[64];
	char **plugins = NULL, *current_plugin = NULL;
	NET_mdataStruct *md = NULL;
  
	if (!fep) return;
	if (mime_type_to_find  && *mime_type_to_find)
		plugins = NPL_FindPluginsForType(mime_type_to_find);
  
	/* Destroy all existing children */
	if (fep->plugin_pulldown) {
	  int nkids = 0;
	  Widget *kids;
	  XtVaGetValues(fep->plugin_pulldown, XmNchildren, &kids,
					XmNnumChildren, &nkids, 0);
	  if (nkids > 0) {
		/* Conditions for not destroy are
		 *	1. If we didn't have plugins previously and dont have any now
		 * in all other cases, we destroy.
		 */
		if (nkids == 1 && !strcmp(XtName(kids[0]), "plugin0"))
			if (!plugins)
				return;
		XfeDestroyMenuWidgetTree(kids, nkids,False);
	  }
	}
  
	if (plugins && plugins[0]) {
	  md = fe_helpers_get_mailcap_from_type(mime_type_to_find);
	  if (fe_IsMailcapEntryPlugin(md))
		  current_plugin = fe_GetPluginNameFromMailcapEntry(md);
    
	  i = 0;
	  while (plugins[i]) {
		sprintf(namebuf, "plugin%d", i+1);
		ac = 0;
		XtSetArg (av[ac], XmNuserData, i); ac++;
		button =  XmCreatePushButtonGadget(fep->plugin_pulldown, namebuf,
										   av, ac);
		fe_SetString(button, XmNlabelString, plugins[i]);
		XtAddCallback (button, XmNactivateCallback,
					   fe_helpers_plugin_sel_cb, fep);
		XtManageChild(button);
		if (current_plugin && !strcmp(current_plugin, plugins[i]))
			current = button;
		i++;
	  }
	  fep->plugins = plugins;
	}
	if (!button) {
	  /* No plugins installed for this mime type */
	  ac = 0;
	  XtSetArg (av[ac], XmNsensitive, False); ac++;
	  button =  XmCreatePushButtonGadget(fep->plugin_pulldown, "plugin0",
										 av, ac);
	  fe_SetString(button, XmNlabelString, XP_GetString(XFE_NO_PLUGINS));
	  XtManageChild(button);
	  XtVaSetValues(fep->plugin_option, XmNsensitive, False, 0);
	  XtVaSetValues(fep->plugin_b, XmNsensitive, False, 0);
	  current = button;
	}
	else {
	  XtVaSetValues(fep->plugin_b, XmNsensitive, True, 0);
	  if (!current) current = button;
	}
	XtVaSetValues(fep->plugin_option, XmNmenuHistory, current, 0);	
}

static void
fe_helpers_build_exts(char *ext, NET_cdataStruct *cd)
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
static char *
fe_helpers_construct_new_mailcap_string(NET_mdataStruct *md, char* contenttype,
										char *command, char *flag)
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
	else if ( command && *command && flag && *flag )
	  {
		sprintf(buf1, "%s\n%s;%s", FE_MOZILLA_MAILCAP_COMMENT,
				md? md->contenttype: contenttype ,command );
   
		sprintf(buf2, ";\\\n\t%s=%s", NET_MOZILLA_FLAGS, flag);

		len = strlen(buf1)+strlen(buf2)+1;

		buf0 = (char*)malloc(len*sizeof(char));
		memset(buf0, '\0', len);
		strcpy(buf0, buf1);
		strcat(buf0, buf2);
	  }
	else if ( command && *command )
	  { /* APPLICATION MODE */
		sprintf(buf1, "%s\n%s;%s", FE_MOZILLA_MAILCAP_COMMENT,
				md ? md->contenttype: contenttype , command);

		len = strlen(buf1)+1;
		buf0 = (char*)malloc(len*sizeof(char));
		memset(buf0, '\0', len);

		strcpy(buf0, buf1);
	  }
	else if ( flag && *flag ) 
	  {
		sprintf(buf1, "%s\n%s;", FE_MOZILLA_MAILCAP_COMMENT,
				md? md->contenttype: contenttype );
		sprintf(buf2, ";\\\n\t%s=%s", NET_MOZILLA_FLAGS, flag);

		len = strlen(buf1)+strlen(buf2)+1;

		buf0 = (char*)malloc(len*sizeof(char));
		memset(buf0, '\0', len);
		strcpy(buf0, buf1);
		strcat(buf0, buf2);
	  }
	else
	  {
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

void
fe_helpers_update_mailcap_entry (char *mimeType, NET_mdataStruct *md,
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
		src_str = fe_helpers_construct_new_mailcap_string (md, mimeType, 	
														   md? md->command: NULL,flag);
	else 
	  {
		src_str = fe_helpers_updateKey(oldStr, NET_MOZILLA_FLAGS, flag, True);

		if ( !src_str )
			src_str = fe_helpers_appendKey(oldStr, NET_MOZILLA_FLAGS, flag, 1);
		
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
	  fe_helpers_add_new_mailcap_data(mimeType, src_str, NULL, flag, 1);
	  XP_FREE(src_str);
	}
} 

static void
fe_helpers_build_handle(struct fe_prefs_helpers_data *fep)
{
	NET_mdataStruct *old_md = NULL;
	char *src_str;
	char *oldStr = NULL;

	old_md = fe_helpers_get_mailcap_from_type(fep->cd->ci.type);

	/* old_md might be marked "Deleted", but that's okay */
	/* because we will replace it with the new md data value */

	if ( XmToggleButtonGetState(fep->navigator_b) )
	  {
		fe_helpers_update_mailcap_entry(fep->cd->ci.type, old_md,
										NET_COMMAND_NETSCAPE);
			
	  }
	else if (XmToggleButtonGetState(fep->plugin_b))
	  {
		Widget w;
		char *value;
		int n = 0;
		XtVaGetValues(fep->plugin_option, XmNmenuHistory, &w, 0);
		XtVaGetValues(w, XmNuserData, &n, 0);
		value = PR_smprintf("%s%s", NET_COMMAND_PLUGIN,
							fep->plugins[n]);
		fe_helpers_update_mailcap_entry(fep->cd->ci.type, old_md, value);
		XP_FREE(value);
	  }
	else if (XmToggleButtonGetState(fep->app_b))
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
				src_str = fe_helpers_construct_new_mailcap_string
				  (old_md, fep->cd->ci.type, text, NULL);
			else 
			  {
				src_str = fe_helpers_addCommand (oldStr, text);
				XP_FREE(oldStr);

				oldStr = src_str;
				src_str = fe_helpers_deleteKey(oldStr, NET_MOZILLA_FLAGS, 1);
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
				fe_helpers_add_new_mailcap_data(fep->cd->ci.type,
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
				src_str = fe_helpers_construct_new_mailcap_string
				  (old_md, fep->cd->ci.type, NULL, NET_COMMAND_UNKNOWN);
			  }
			else 
			  {
				src_str = fe_helpers_deleteCommand(oldStr); 
				XP_FREE(oldStr);

				oldStr = src_str;
				src_str = fe_helpers_updateKey(oldStr, NET_MOZILLA_FLAGS,
											   NET_COMMAND_UNKNOWN, True);
				if ( !src_str )
					src_str = fe_helpers_appendKey ( oldStr, NET_MOZILLA_FLAGS,
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
				fe_helpers_add_new_mailcap_data(fep->cd->ci.type,
												src_str,NULL,
												NET_COMMAND_UNKNOWN, 1);
				XP_FREE(src_str);
			  }
		  }
		if (text) XP_FREE(text);
	  }
	else if (XmToggleButtonGetState(fep->save_b))
	  {
		fe_helpers_update_mailcap_entry(fep->cd->ci.type, old_md,
										NET_COMMAND_SAVE_TO_DISK);
	  }
	else /* Everything else, we set it to be unknown */
	  {
		fe_helpers_update_mailcap_entry(fep->cd->ci.type, old_md,
										NET_COMMAND_UNKNOWN);
	  }
		
}
static Boolean
fe_helpers_get_mime_data_real_pos(NET_cdataStruct *old_cd, int *real_pos)
{
	Boolean found = False; 
	XP_List *infoList = cinfo_MasterListPointer();
	NET_cdataStruct *cd;

	if (!infoList) return found;
	*real_pos = 0;

	while ((cd=(NET_cdataStruct*)XP_ListNextObject(infoList)))
	  {
		if ( !fe_helpers_is_deleted_type(cd) )
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

static NET_cdataStruct *
fe_helpers_get_exist_data(NET_cdataStruct *old_cd, int *old_pos)
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
			if( !strcasecomp(old_cd->ci.type, cd->ci.type))
			  {
				/* found matching type regardless whether it is deleted one or not */
				/* caller needs to decide what they want to do with this old_cd */
				found_cd = cd;
				break;
			  }
			else (*old_pos)++;
		  }
	  }
	return found_cd;
}

static NET_cdataStruct *
fe_helpers_can_combine ( NET_cdataStruct *new_cd, int* old_pos)
{
	NET_cdataStruct *old_cd = NULL;

	old_cd = fe_helpers_get_exist_data(new_cd, old_pos);

	return old_cd;
}
 
static Boolean 
fe_helpers_start_new_line( char *old_string, char *de, int n )
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


static char *
fe_helpers_new_string(char *old_str, char* type,  char *value, char *del)
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


		if (fe_helpers_start_new_line(old_str, del, 2) )
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

static char *
fe_replace_new_mime_string(NET_cdataStruct *old_cd, NET_cdataStruct *new_cd)
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
			str = fe_helpers_updateKey(old_str, TYPE, new_cd->ci.type, 0 );

			if ( !str )
			  {
				str = fe_helpers_new_string(old_str, TYPE,
											new_cd->ci.type, SPACE);
			  }
			XP_FREE(old_str);
		  }
		else 
		  {
			old_str = str;
			str = fe_helpers_deleteKey(str, TYPE, 0 );
			XP_FREE(old_str);
		  }
	
	  }
	else
	  {
		old_str = str;
		str = fe_helpers_new_string(str, TYPE,
									new_cd->ci.type, SPACE);
		XP_FREE(old_str);
	  }



	if (old_cd->ci.desc && *old_cd->ci.desc )
	  {
		if (new_cd->ci.desc && *new_cd->ci.desc ) 
		  {
			old_str = str;
			str = fe_helpers_updateKey(str, DESC, new_cd->ci.desc, 0 );
			
			if ( !str )
			  {
				str = fe_helpers_new_string(old_str, DESC,
											new_cd->ci.desc, SPACE);
			  }
			XP_FREE(old_str);
		  }
		else 
		  {
			old_str = str;
			str = fe_helpers_deleteKey(str, DESC,0 );
			XP_FREE(old_str);
		  }
	  }
	else
	  {
		old_str = str;
		str = fe_helpers_new_string(str, DESC,
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
			str = fe_helpers_updateKey(str, EXTS, new_exts, 0 );
			if ( !str )
			  {
				str = fe_helpers_new_string(old_str, EXTS,
											new_exts, SPACE);
			  }
			XP_FREE(old_str);
		  }
		else
		  {
			old_str = str;
			str = fe_helpers_deleteKey(str, EXTS, 0);
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
			str = fe_helpers_new_string(str, EXTS,
										new_exts, SPACE);
			XP_FREE(old_str);
		  }
	  }

	return str;
}

static char *
fe_helpers_construct_new_mime_string(NET_cdataStruct *new_cd)
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
			str = fe_helpers_new_string(old_str, TYPE, new_cd->ci.type, SPACE);
			XP_FREE(old_str);
		  }
		if ( new_cd->ci.encoding && *new_cd->ci.encoding )
		  {
			old_str = str;
			str = fe_helpers_new_string(old_str, ENC, new_cd->ci.encoding, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.language && *new_cd->ci.language)
		  {
			old_str = str;
			str = fe_helpers_new_string(old_str, LANG, new_cd->ci.language, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.desc && *new_cd->ci.desc)
		  {
			old_str = str;
			str = fe_helpers_new_string(old_str, DESC, 
										new_cd->ci.desc, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.alt_text && *new_cd->ci.alt_text)
		  {
			old_str = str;
			str = fe_helpers_new_string(old_str, ALT, new_cd->ci.alt_text, SPACE);
			XP_FREE(old_str);
		  }

		if ( new_cd->ci.icon && *new_cd->ci.icon)
		  {
			old_str = str;
			str = fe_helpers_new_string(old_str, ICON, new_cd->ci.icon, SPACE);
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
			str = fe_helpers_new_string(old_str, EXTS, exts, SPACE);
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

static void
fe_helpers_done_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	struct fe_prefs_helpers_data *fep = (struct fe_prefs_helpers_data *) closure;
	char *text = 0;
	char *u = 0;
	int i;
	int old_pos;
	NET_cdataStruct *old_cd = NULL;
	NET_cdataStruct *new_cd = NULL;
	NET_mdataStruct *md = NULL;
	Boolean add_new_cd = False;
	char *src_string =0;
	int real_pos;

	fe_MarkHelpersModified(fep->context);

	/* Type  is critical. If this field is empty, then basically we don't
	   do a thing here */

	text = fe_GetTextField(fep->mime_types_text);
	if ( !text ) return;
	text = fe_StringTrim(text);
	if ( !strlen(text)) {
	  FE_Alert(fep->context, XP_GetString(XFE_HELPERS_EMPTY_MIMETYPE));
	  XP_FREE(text); 
	  return;
	}

	if(text) XP_FREE(text);

	/* Empty type Business has been taken care of */
	/* Do some serious stuff here now...*/

	new_cd = NET_cdataCreate();
	new_cd->is_local = True;
	if ( fep->cd )
	  {
		new_cd->is_external = fep->cd->is_external;
		new_cd->is_modified = 1;
	  }
	else{
	  new_cd->is_external = 1;
	  new_cd->is_modified = 1;

	  /* If new entry was deleted before commit, we don't want to save the info out*/
	  new_cd->is_new = 1; /* remember this for deletion*/

	  add_new_cd = True;
	}


	/* Description */
	text = fe_GetTextField(fep->mime_types_desc_text);
	if ( text && *text ) {
	  fe_StringTrim(text);
	  StrAllocCopy(new_cd->ci.desc, text);
	}
	if(text) XP_FREE(text);

	/* Type */
	text = fe_GetTextField(fep->mime_types_text);
	if ( text && *text ){
	  fe_StringTrim(text);
	  StrAllocCopy( new_cd->ci.type, text);
	}
	if(text) XP_FREE(text);

	/* Suffix */
	text = fe_GetTextField(fep->mime_types_suffix_text);
	u = XP_STRTOK(text, ",");

	while (u) 
	  {
		u = fe_StringTrim(u);
		fe_helpers_build_exts(u, new_cd);
		u = XP_STRTOK(NULL, ",");
	  }
	if(text) XP_FREE(text);


	if ( (old_cd=fe_helpers_can_combine (new_cd, &old_pos)) )
	  {
		if ( !fe_helpers_is_deleted_type(old_cd) )
		  { 
			/* Only cd's that aren't deleted are displayed in the list */
			/* Therefore, we need to make sure if the item is in the list */
			/* in order to delete */

			if ( fe_helpers_get_mime_data_real_pos(old_cd, &real_pos) )
				old_pos = real_pos;

			if ( old_pos > fep->pos ) 
				XmLGridDeleteRows(fep->helpers_list, XmCONTENT, old_pos, 1);
			else if (fep->pos > old_pos )
			  {
	  
				XmLGridDeleteRows(fep->helpers_list, XmCONTENT, old_pos, 1);
				fep->pos -= 1;
			  }
			else if ( fep->pos == 0 && old_pos == 0)
			  {
				XmLGridDeleteRows(fep->helpers_list, XmCONTENT, old_pos, 1);
			  }
		  }

		/* CanCombine=True will remove the fep->cd from the list */
		/* Therefore, make the new one the current */
		/* If action = add_new_cd, and can_combine = true, we should remove the old one */
		if ( add_new_cd ) 
		  {
			NET_cdataRemove(old_cd);
			old_cd = NULL;
		  }
	  }


	if ( fep->cd ) /* Then, this is not an action: NEW  */
	  {

		if ( !fep->cd->is_local && !fep->cd->is_external )
		  {
			/* read from Global mime.type in old mime format */
			/* convert it to the new format here */
			src_string = fe_helpers_construct_new_mime_string( new_cd );
		  }
		else if ( !fep->cd->is_local && !fep->cd->num_exts )
		  {
			/* This entry is added by an existing mailcap type */
			src_string = fe_helpers_construct_new_mime_string(new_cd);
		  }
		else	
			src_string = fe_replace_new_mime_string( fep->cd, new_cd );

		XP_FREE(fep->cd->src_string);
		fep->cd->src_string = src_string;

		XP_FREE(fep->cd->ci.desc );
		XP_FREE(fep->cd->ci.type );
 
		fep->cd->ci.desc = 0;
		fep->cd->ci.type = 0;
 
		for ( i = 0; i < fep->cd->num_exts; i++ )
			XP_FREE (fep->cd->exts[i]);
		XP_FREE(fep->cd->exts);

		fep->cd->exts = 0;
		fep->cd->num_exts = 0;
		fep->cd->is_external = new_cd->is_external;
		fep->cd->is_modified = new_cd->is_modified;
		StrAllocCopy(fep->cd->ci.desc, new_cd->ci.desc);
		StrAllocCopy(fep->cd->ci.type, new_cd->ci.type);
		for ( i = 0; i < new_cd->num_exts; i++ )
			fe_helpers_build_exts(new_cd->exts[i], fep->cd);

		NET_cdataFree(new_cd);
	  }
	else {
	  src_string = fe_helpers_construct_new_mime_string( new_cd );
	  if ( new_cd->src_string) XP_FREE(new_cd->src_string);
	  new_cd->src_string = src_string;
	  fep->cd = new_cd;
	}

	fe_helpers_build_handle(fep);
	if ( add_new_cd && (fep->pos == 0)) 
	  {
		fep->pos = -1;
		NET_cdataAdd(fep->cd);
	  }

	if ( !fe_helpers_is_deleted_type(fep->cd) )
	  {
		md = fe_helpers_get_mailcap_from_type(fep->cd->ci.type);
		fe_helpers_append_type_to_list(fep, fep->cd, md,  fep->pos);
	  }

	XtDestroyWidget(widget);
}
 
 
static void
fe_helpers_cancel_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	XtDestroyWidget(widget);
}
 
static void
fe_helpers_destroy_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
	struct fe_prefs_helpers_data *fep = (struct fe_prefs_helpers_data *) closure;
	int i;

	if (!fep->editor)
		/* The helper edit dialog is not up. Do nothing. */
		return;

	fep->editor = NULL;
	fep->pos = 0;
	fep->cd = NULL;

	/* Free the backend allocated plugin names */
	for (i=0; fep->plugins && fep->plugins[i]; i++)
		XP_FREE(fep->plugins[i]);
	if (fep->plugins) XP_FREE(fep->plugins);
	fep->plugins = 0;
}

static void
fe_helpers_verify_toggle_cb( Widget widget, XtPointer closure,
							 XtPointer call_data)
{
	struct fe_prefs_helpers_data *fep = (struct fe_prefs_helpers_data *) closure;
	XmToggleButtonCallbackStruct *cb =
	  (XmToggleButtonCallbackStruct *)call_data;
 
 
	if (!cb->set)
	  {
		XtVaSetValues (widget, XmNset, True, 0);
	  }
	else if (widget == fep->navigator_b)
	  {
		XtVaSetValues (fep->plugin_b, XmNset, False, 0);
		XtVaSetValues (fep->app_b, XmNset, False, 0);
		XtVaSetValues (fep->save_b, XmNset, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
		XtVaSetValues (fep->unknown_b, XmNset, False, 0);
	  }
	else if (widget == fep->plugin_b)
	  {
		Widget w;
		int n;
		char *pluginName;

		XtVaGetValues(fep->plugin_option, XmNmenuHistory, &w, 0);
		XtVaGetValues(w, XmNuserData, &n, 0);
		pluginName = fep->plugins[n];
		fe_helpers_plugin_selected(fep, pluginName);

		XtVaSetValues (fep->navigator_b, XmNset, False, 0);
		XtVaSetValues (fep->app_b, XmNset, False, 0);
		XtVaSetValues (fep->save_b, XmNset, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
		XtVaSetValues (fep->unknown_b, XmNset, False, 0);
	  }
	else if (widget == fep->app_b)
	  {
		XtVaSetValues (fep->navigator_b, XmNset, False, 0);
		XtVaSetValues (fep->plugin_b, XmNset, False, 0);
		XtVaSetValues (fep->save_b, XmNset, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, True, 0);
		XtVaSetValues (fep->unknown_b, XmNset, False, 0);
	  }
	else if (widget == fep->save_b)
	  {
		XtVaSetValues (fep->navigator_b, XmNset, False, 0);
		XtVaSetValues (fep->plugin_b, XmNset, False, 0);
		XtVaSetValues (fep->app_b, XmNset, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
		XtVaSetValues (fep->unknown_b, XmNset, False, 0);
	  }
	else if (widget == fep->unknown_b)
	  {
		XtVaSetValues (fep->navigator_b, XmNset, False, 0);
		XtVaSetValues (fep->plugin_b, XmNset, False, 0);
		XtVaSetValues (fep->app_b, XmNset, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
		XtVaSetValues (fep->save_b, XmNset, False, 0);
	  }
	else
		abort ();
}
 
static Widget
fe_helpers_make_edit_dialog(struct fe_prefs_helpers_data *fep)
{
	Widget mainw = CONTEXT_WIDGET (fep->context);
	Widget dialog;
	Widget frame1;
	Widget form, form1, form2;
	Widget mime_types_desc_label, mime_types_desc_text;
	Widget mime_types_label, mime_types_text;
	Widget mime_types_suffix_label, mime_types_suffix_text;
	Widget navigator_b, plugin_b, app_b, app_text, app_browse;
	Widget save_b, unknown_b;
	Widget label1;
	Widget kids[30];
 
	Widget plugin_option;
	Widget plugin_pulldown;
 
	Dimension size, space;
 
	Visual *v = 0;
	Colormap cmap = 0;
	Cardinal depth = 0;
	Arg av[20];
	int ac;
	int i;

	/* If the pref is up, we need to make this the child of the pref dialog */
	if (CONTEXT_DATA(fep->context)->currentPrefDialog)
		mainw = CONTEXT_DATA(fep->context)->currentPrefDialog;
	while(!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
		mainw = XtParent(mainw);

 
	XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
				   XtNdepth, &depth, 0);
	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
	XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
	XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); ac++;
	XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
	XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
	XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
	/*  XtSetArg (av[ac], XmNnoResize, True); ac++; */
	dialog = XmCreatePromptDialog (mainw, "helperEditor", av, ac);
 
	XtUnmanageChild(XmSelectionBoxGetChild (dialog, XmDIALOG_SEPARATOR));
	XtUnmanageChild(XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT));
	XtUnmanageChild (XmSelectionBoxGetChild (dialog,
											 XmDIALOG_SELECTION_LABEL));
	XtUnmanageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
	XtUnmanageChild(XmSelectionBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
#endif
 
	XtAddCallback (dialog, XmNokCallback, fe_helpers_done_cb, fep);
	XtAddCallback (dialog, XmNcancelCallback, fe_helpers_cancel_cb, fep);
	XtAddCallback (dialog, XmNdestroyCallback, fe_helpers_destroy_cb, fep);
 
	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (dialog, "helperEdit", av, ac);
	XtManageChild (form);
 
	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	form1 = XmCreateForm (form, "helperEditForm", av, ac);
 
	ac = 0;
	i = 0;
	kids[i++] = mime_types_desc_label= XmCreateLabelGadget (form1,
															"mimeTypesDescriptionLabel", av, ac);
	kids[i++] = mime_types_desc_text = fe_CreateTextField (form1,
														   "mimeTypesDescriptionText",av,ac);
	kids[i++] = mime_types_label = XmCreateLabelGadget (form1,
														"mimeTypesLabel", av, ac);
	kids[i++] = mime_types_text = fe_CreateTextField (form1,
													  "mimeTypesText",av,ac);
	kids[i++] = mime_types_suffix_label = XmCreateLabelGadget (form1,
															   "mimeTypesSuffixLabel", av, ac);
	kids[i++] = mime_types_suffix_text = fe_CreateTextField (form1,
															 "mimeTypesSuffixText",av,ac);
 

	if (fe_globalData.nonterminal_text_translations)
	  {
		XtOverrideTranslations (mime_types_desc_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (mime_types_text,
								fe_globalData.nonterminal_text_translations);
	  }

	XtAddCallback(mime_types_text,  XmNvalueChangedCallback,
				  fe_helpers_set_handle_by_cb, fep);
 
	/* Layout the helper editor widgets in the form1 */
	XtVaSetValues (mime_types_desc_label,
				   XmNalignment, XmALIGNMENT_END,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (mime_types_desc_text,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget,	mime_types_desc_label,
				   XmNrightAttachment, XmATTACH_FORM,
				   0);

	XtVaSetValues(mime_types_desc_label, XmNheight, 
				  XfeHeight(mime_types_desc_text),0);
 
	XtVaSetValues (mime_types_label,
				   XmNalignment, XmALIGNMENT_END,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, mime_types_desc_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNrightWidget, mime_types_desc_label,
				   0);
 
	XtVaSetValues (mime_types_text,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget,mime_types_desc_text ,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, mime_types_desc_text,
				   XmNrightAttachment, XmATTACH_FORM,
				   0);
 
	XtVaSetValues(mime_types_label, XmNheight, 
				  XfeHeight(mime_types_text), 0);

	XtVaSetValues (mime_types_suffix_label,
				   XmNalignment, XmALIGNMENT_END,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, mime_types_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNrightWidget, mime_types_label,
				   0);
 
	XtVaSetValues (mime_types_suffix_text,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, mime_types_text,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, mime_types_suffix_label,
				   XmNrightAttachment, XmATTACH_FORM,
				   0);

	XtVaSetValues(mime_types_suffix_label, XmNheight, 
				  XfeHeight(mime_types_suffix_text), 0);

 
	XtManageChildren(kids,i);
	XtManageChild(form1);

 
	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, form1); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	frame1 = XmCreateFrame (form, "helperEditFrame", av, ac);
	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "helperEditFrameLabel", av, ac);
 
	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame1, "helperEditHandleBox", av, ac);
 
	ac = 0;
	i = 0;
	kids [i++] = navigator_b = XmCreateToggleButtonGadget (form2,
														   "helperEditNavigator", av,ac);
	kids [i++] = plugin_b = XmCreateToggleButtonGadget (form2,
														"helperEditPlugin", av,ac);
 
 
	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	plugin_pulldown = XmCreatePulldownMenu (form2, "pluginPullDown", av, ac);
 
	ac = 0;
	XtSetArg(av[ac], XmNsubMenuId, plugin_pulldown); ac++;
	kids [i++] = plugin_option = fe_CreateOptionMenu (form2,"pluginMenu", av, ac);
 
	kids [i++] = app_b = XmCreateToggleButtonGadget (form2,
													 "helperEditApp", av,ac);
	kids [i++] = app_text = fe_CreateTextField (form2,
												"helperEditAppText", av,ac);
	kids [i++] = app_browse = XmCreatePushButton (form2,
												  "helperEditAppBrowse", av,ac);
	kids [i++] = save_b = XmCreateToggleButtonGadget (form2,
													  "helperEditSave", av,ac);
	kids [i++] = unknown_b = XmCreateToggleButtonGadget (form2,
														 "helperEditUnknown", av,ac);
 
	fep->mime_types_desc_text = mime_types_desc_text;
	fep->mime_types_text = mime_types_text;
	fep->mime_types_suffix_text = mime_types_suffix_text;
	fep->navigator_b = navigator_b;
	fep->plugin_b    = plugin_b;
	fep->app_b       = app_b;
	fep->app_text    = app_text;
	fep->app_browse  = app_browse;
	fep->save_b      = save_b;
	fep->unknown_b   = unknown_b;
	fep->plugin_option = plugin_option;
	fep->plugin_pulldown = plugin_pulldown;
  
	fe_helpers_build_plugin_list(fep, (fep->cd ? fep->cd->ci.type:NULL));
  
	XtVaSetValues (navigator_b,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNsensitive, True,
				   0);

	XtVaSetValues (plugin_b,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, plugin_option,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, plugin_option,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, navigator_b,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (plugin_option,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, navigator_b,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, XfeWidth(plugin_b) + 20,
				   XmNrightAttachment, XmATTACH_FORM,
				   0);
 
	XtVaSetValues (app_b,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, plugin_option,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, plugin_b,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaGetValues( app_b, XmNindicatorSize, &size, 
				   XmNspacing, &space, 0 );

	XtVaSetValues (app_text,
				   XmNsensitive, False,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, app_b,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, plugin_b,
				   XmNleftOffset, size+space,
				   XmNrightAttachment, XmATTACH_FORM,
				   XmNrightOffset, XfeWidth(app_browse) + 10,
				   0);
 
	XtVaSetValues (app_browse,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, app_text,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, app_text,
				   XmNleftAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_FORM,
				   0);
 
	XtVaSetValues (save_b,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, app_text,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, app_b,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
 
	XtVaSetValues (unknown_b,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, save_b,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, save_b,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtAddCallback(navigator_b,  XmNvalueChangedCallback,
				  fe_helpers_verify_toggle_cb, fep);
	XtAddCallback(plugin_b,  XmNvalueChangedCallback,
				  fe_helpers_verify_toggle_cb, fep);
	XtAddCallback(app_b,  XmNvalueChangedCallback,
				  fe_helpers_verify_toggle_cb, fep);
	XtAddCallback(save_b,  XmNvalueChangedCallback,
				  fe_helpers_verify_toggle_cb, fep);
	XtAddCallback(unknown_b,  XmNvalueChangedCallback,
				  fe_helpers_verify_toggle_cb, fep);
 
	XtAddCallback(app_browse,  XmNactivateCallback,
				  fe_helpers_application_browse_cb, fep);
 
 
	XtManageChildren (kids, i);
	XtManageChild(form2);
	XtManageChild(label1);
	XtManageChild(frame1);
	XtManageChild(form);
 
	return dialog;
}

static void
fe_helpers_application_browse_cb(Widget widget, XtPointer closure,
								 XtPointer call_data)
{
	struct fe_prefs_helpers_data *fep =  (struct fe_prefs_helpers_data *) closure;
	fe_browse_file_of_text (fep->context, fep->app_text, False);
}

static Bool
fe_helpers_handle_by_netscape(char *type)
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
		!strcmp(type, APPLICATION_PRE_ENCRYPTED) ||
		!strcmp(type, APPLICATION_NS_PROXY_AUTOCONFIG)
		)
 	
	  {
		rightType = True;
	  }
	return rightType;
}
static Bool
fe_helpers_handle_by_saveToDisk(char *type)
{
	Bool rightType = False;
	if ( !strcmp(type, APPLICATION_OCTET_STREAM) )
		rightType = True;

	return rightType;
}

static XP_Bool
fe_helpers_handle_by_plugin(struct fe_prefs_helpers_data *fep, char *type)
{
	Bool b;
	/* Create plugins options menu and sensitize the plugin option menu and
	   toggle button, depending on the mimetype passed in */

	fe_helpers_build_plugin_list(fep, type);
	XtVaGetValues(fep->plugin_b, XmNsensitive, &b, 0);
	return(b);
}

static void
fe_helpers_set_handle_by_flag ( struct fe_prefs_helpers_data *fep, 
								NET_cdataStruct *cd)
{
	NET_mdataStruct *md = NULL;
  
	XtVaSetValues(fep->navigator_b, XmNsensitive, False, 0);
	/* Netscape Type ?*/

	if (fe_helpers_handle_by_netscape(cd->ci.type) )
	  {
		XtVaSetValues(fep->navigator_b, XmNsensitive, True, 0);
	  }

	md = fe_helpers_get_mailcap_from_type(cd->ci.type);

	if ( md && md->command && *md->command )
	  {
		XtVaSetValues(fep->app_text, XmNsensitive, False, 0 );
		fe_SetTextField(fep->app_text, md->command);
	  }
	
	if (md && md->xmode && *md->xmode)
	  {
		md->xmode = fe_StringTrim(md->xmode);

		if (!strcmp(md->xmode, NET_COMMAND_UNKNOWN))
			XtVaSetValues(fep->unknown_b, XmNset, True, 0 );
		else if (fe_IsMailcapEntryPlugin(md))
			XtVaSetValues(fep->plugin_b, XmNset, True, 0 );
		else if (!strcmp(md->xmode, NET_COMMAND_SAVE_TO_DISK) ||
				 (!strcmp(md->xmode, NET_COMMAND_SAVE_BY_NETSCAPE)))
			XtVaSetValues(fep->save_b, XmNset, True, 0 );
		else if (!strcmp(md->xmode,NET_COMMAND_NETSCAPE))
			XtVaSetValues(fep->navigator_b, XmNset, True, 0 );
		else if (!strcmp(md->xmode,NET_COMMAND_DELETED))
			XtVaSetValues(fep->unknown_b, XmNset, True, 0 );
	  }
	else if ( md  && md->command && *md->command)
	  {
		XtVaSetValues(fep->app_b, XmNset, True, 0 );
		XtVaSetValues(fep->app_text,
					  XmNsensitive, True, 0 );
	  }
	else if (fe_helpers_handle_by_netscape(cd->ci.type) )
		XtVaSetValues(fep->navigator_b, XmNset, True, 0 );
	else if (fe_helpers_handle_by_saveToDisk(cd->ci.type) )
		XtVaSetValues(fep->save_b, XmNset, True, 0 );
	else
		XtVaSetValues(fep->unknown_b, XmNset, True, 0);
}

static void 
fe_helpers_set_handle_by_cb (Widget w, XtPointer closure, XtPointer call_data )
{
	struct fe_prefs_helpers_data *fep = (struct fe_prefs_helpers_data *) closure;
	char *text = NULL;
	Bool nav_p = False, plugin_p = False;

	text = fe_GetTextField(fep->mime_types_text);

	if (text && *text && fe_helpers_handle_by_netscape(text)) {
	  XtVaSetValues(fep->navigator_b, XmNsensitive, True, 0);
	  nav_p = True;
	}
	else
		XtVaSetValues(fep->navigator_b, XmNsensitive, False, 0);

	if (text && *text && fe_helpers_handle_by_plugin(fep, text))
		/* fe_helpers_handle_by_plugin() will sensitize the plugin button. */
		plugin_p = True;
  
	/* if Navigator used to handle this but cannot anymore, unknown takes over */
	if (XmToggleButtonGetState(fep->navigator_b) && !nav_p)
		XmToggleButtonSetState(fep->unknown_b, True, True);

	/* if Plugin used to handle this but cannot anymore, unknown takes over */
	if (XmToggleButtonGetState(fep->plugin_b) && !plugin_p)
		XmToggleButtonSetState(fep->unknown_b, True, True);

	if (text) XP_FREE(text);
} 

static void
fe_helpers_fill_edit_with_data(struct fe_prefs_helpers_data *fep,  
							   NET_cdataStruct *cd)
{
	int i;
	char *extensions = 0; /* has to initialize bf. passing to StrAllocCopy() */

	if ( !fep || !cd->ci.type || !*cd->ci.type ) return;
	/* Found Type */
	XtVaSetValues(fep->mime_types_text, XmNcursorPosition, 0, 0);
	fe_SetTextField(fep->mime_types_text, cd->ci.type);

	/* MIME Description */
	if ( cd->ci.desc && *cd->ci.desc ) {
		XtVaSetValues(fep->mime_types_desc_text, XmNcursorPosition, 0, 0 );
		fe_SetTextField(fep->mime_types_desc_text, cd->ci.desc);
	}

	/* MIME Suffix */
	for ( i = 0; i < cd->num_exts; i++ )
	  {
		if ( i == 0 )
		  {
			StrAllocCopy(extensions, cd->exts[i] );
		  }
		else 
		  {
			extensions = XP_AppendStr(extensions, ",");
			extensions = XP_AppendStr(extensions,  cd->exts[i]);
		  }
	  }

	if (cd->num_exts > 0 )
	  {
		XtVaSetValues(fep->mime_types_suffix_text, XmNcursorPosition, 0, 0 );
		XtVaSetValues(fep->mime_types_suffix_text, extensions);
	  }

	/* Handle By...this mail cap */
	fe_helpers_set_handle_by_flag(fep, cd);
}

static NET_cdataStruct *
fe_helpers_get_mime_data_at_pos(int pos) /* Starting from 0 */
{
	int count = 0;
	NET_cdataStruct *cd = NULL;
	XP_List *infoList = cinfo_MasterListPointer();

	while((cd = XP_ListNextObject(infoList)))
	  {
		if ( cd->ci.type  && *cd->ci.type 
			 && !fe_helpers_is_deleted_type(cd))
		  {
			if ( pos == count ) 
			  {
				break;
			  }
			else
				count++;
		  }
	  }

	return cd;
}

static void
fe_helpers_edit_cb( Widget w, XtPointer closure, XtPointer call_data)
{
	NET_cdataStruct *cd = NULL;
	struct fe_prefs_helpers_data *fep = (struct fe_prefs_helpers_data *) closure;
	int    selectedCount;
	int    pos[1]; /* we are browse select, so only 1 row is selected */

	selectedCount = XmLGridGetSelectedRowCount(fep->helpers_list);

	if ( selectedCount != 1 )
		return;

	XmLGridGetSelectedRows(fep->helpers_list, pos, selectedCount);

	
	/* pos in the list always start from 0 */
	cd = fe_helpers_get_mime_data_at_pos(pos[0]);
	if ( fep )
	  {
		fep->pos = pos[0];
		fep->cd = cd;
		fep->editor = fe_helpers_make_edit_dialog(fep);
		fe_helpers_fill_edit_with_data(fep, cd);
		XtManageChild(fep->editor);
	  }
} 

static void
fe_helpers_new_cb( Widget w, XtPointer closure, XtPointer call_data)
{
	struct fe_prefs_helpers_data *fep = (struct fe_prefs_helpers_data *) closure;
	if ( fep) 
	  {
		fep->pos = 0;
		fep->editor = fe_helpers_make_edit_dialog(fep);
		fep->cd = NULL;
		XtManageChild(fep->editor);
	  }
} 

static void
fe_helpers_delete_cb( Widget w, XtPointer closure, XtPointer call_data)
{
	char *old_str = 0;
	char *src_str = 0;
	NET_cdataStruct* cd = NULL;
	NET_mdataStruct* old_md= NULL;
	struct fe_prefs_helpers_data *fep = (struct fe_prefs_helpers_data *) closure;
	int    selectedCount;
	int    pos[1]; /* we are browse select, so only 1 row is selected */
 
	selectedCount = XmLGridGetSelectedRowCount(fep->helpers_list);
 
	if ( selectedCount != 1 )
		return;
 
	XmLGridGetSelectedRows(fep->helpers_list, pos, selectedCount);
 
 
	/* pos in the list always start from 0 */
	cd = fe_helpers_get_mime_data_at_pos(pos[0]);
	if ( fep )
	  {
		fep->pos = pos[0];
		fep->cd = cd;


		if (XFE_Confirm (fep->context, fe_globalData.helper_app_delete_message)) 
		  {
			fe_MarkHelpersModified(fep->context);

			old_md = fe_helpers_get_mailcap_from_type(fep->cd->ci.type); 

			if ( old_md )  
			  {
				/* If there is a mailcap entry, 
				   then update to reflect delete */
				old_str = old_md->src_string;
				old_md->src_string = 0;

			  }
			if ( !old_str || !strlen(old_str))
			  {
				src_str = fe_helpers_construct_new_mailcap_string(old_md, 
																  fep->cd->ci.type, NULL, 
																  NET_COMMAND_DELETED);
			  }
			else
			  {
	
				src_str = fe_helpers_deleteCommand(old_str);
				XP_FREE(old_str);

				old_str = src_str;
				src_str = fe_helpers_updateKey(src_str, NET_MOZILLA_FLAGS, 
											   NET_COMMAND_DELETED, 1 );
				XP_FREE(old_str);
			  }

			if ( old_md )
			  {
				if (old_md->src_string ) XP_FREE(old_md->src_string);
				old_md->src_string = 0;

				if ( src_str && *src_str )
					old_md->src_string = src_str;

				if (old_md->xmode) XP_FREE(old_md->xmode);
				old_md->xmode = strdup(NET_COMMAND_DELETED);
				old_md->is_modified = 1;
			  }
			else
			  {
				fe_helpers_add_new_mailcap_data(fep->cd->ci.type, 
												src_str, NULL,  
												NET_COMMAND_DELETED, 1);
				XP_FREE(src_str);
			  }

			XmLGridDeleteRows(fep->helpers_list, XmCONTENT, fep->pos, 1);

			if (fep->cd->is_new  ) 
			  {
				NET_cdataRemove(fep->cd);
				if ( old_md )
					NET_mdataRemove(old_md);
			  }
			fep->cd = 0;
			fep->pos = 0;
		  }
	  }
} 

static void
fe_helpers_add_list_row(struct fe_prefs_helpers_data *fep, char *data)
{
	int total ;
	XtVaGetValues( fep->helpers_list, XmNrows, &total, 0 );
	XmLGridAddRows(fep->helpers_list, XmCONTENT, -1, 1);
	if ( total > 0 )
		XmLGridMoveRows(fep->helpers_list, 1, 0, total);
	XmLGridSetStringsPos(fep->helpers_list, XmCONTENT, 0, 
						 XmCONTENT, 0, data);
} 

static void
fe_helpers_append_list_row(struct fe_prefs_helpers_data *fep, int row, char *data)
{
	int total ;
	XtVaGetValues( fep->helpers_list, XmNrows, &total, 0 );
	if ( row >= total )
		XmLGridAddRows(fep->helpers_list, XmCONTENT, -1, 1);

	XmLGridSetStringsPos(fep->helpers_list, XmCONTENT, row, 
						 XmCONTENT, 0, data);
} 


NET_mdataStruct *
fe_helpers_get_mailcap_from_type(char *type)
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


static void
fe_helpers_append_type_to_list(struct fe_prefs_helpers_data *fep, NET_cdataStruct *cd_item, 
							   NET_mdataStruct *md,
							   int pos)
{
	char *desc = 0, *action = 0, *line = 0;
 
	if ( cd_item->ci.type && strlen(cd_item->ci.type) )
	  {
		cd_item->ci.type = fe_StringTrim(cd_item->ci.type);
		StrAllocCopy( desc, cd_item->ci.type);
	  }

	if ( md && md->xmode && *md->xmode )
	  {
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
	else if (md && md->command && *md->command)
	  {
		md->command = fe_StringTrim(md->command);
		StrAllocCopy( action, md->command);
	  }
	else if (fe_helpers_handle_by_netscape(cd_item->ci.type) )
		StrAllocCopy(action, XP_GetString(XFE_HELPERS_NETSCAPE));
	else if (fe_helpers_handle_by_saveToDisk(cd_item->ci.type))
		StrAllocCopy(action, XP_GetString(XFE_HELPERS_SAVE));
	else
		StrAllocCopy(action, XP_GetString(XFE_HELPERS_UNKNOWN));

	if ( desc && action )
	  {
		line = PR_smprintf("%s|%s", desc, action); 

		if (pos <= 0)
			fe_helpers_add_list_row(fep, line);
		else 
			fe_helpers_append_list_row(fep, pos, line);

	  }
	if (desc) XP_FREE(desc);
	if (action) XP_FREE(action);
	if (line) XP_FREE(line);
}
 
void
fe_helpers_build_mime_list(struct fe_prefs_helpers_data *fep)
{
	int item_count = 0;
	NET_mdataStruct *md;
	NET_cdataStruct *cd_item;

	/* Get beginning of the list */
	XP_List *infolist = cinfo_MasterListPointer();
	XtVaSetValues( fep->helpers_list, XmNlayoutFrozen, True, NULL);
 
	/* Get the file format from the object in order to
	   build the Helpers  */
	while ((cd_item = (NET_cdataStruct *)XP_ListNextObject(infolist)))
	  {
		if ( cd_item->ci.type  && (int)strlen(cd_item->ci.type) >0 )
		  {
			/* Add this item to the list table */
			if ( !fe_helpers_is_deleted_type(cd_item) )
			  {
				md = fe_helpers_get_mailcap_from_type(cd_item->ci.type);
				fe_helpers_append_type_to_list(fep, cd_item,md, item_count);
				item_count++;
			  }
		  }
	  }
	XtVaSetValues( fep->helpers_list, XmNlayoutFrozen, False, NULL);
}


struct fe_prefs_helpers_data *
fe_helpers_make_helpers_page (MWContext *context, Widget parent)
{
	Arg av [50];
	int ac;
	XmFontList fontList, labelFont/*, defaultLabelFont*/;
	short avgwidth, avgheight;
	Widget form, commands, table, b;
	Widget label1, frame1, form1;
	Dimension width;	

	/* Initialization */
	struct fe_prefs_helpers_data *fep = NULL; /* Helper data kept here */

	fep = (struct fe_prefs_helpers_data *) malloc (
												   sizeof (struct fe_prefs_helpers_data));
	memset (fep, 0, sizeof (struct fe_prefs_helpers_data));

	fep->context = context;
	fep->helpers_selector = parent;

	/* Create the enclosing form */
	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNresizePolicy, XmNONE); ac++;
	form = XmCreateForm (fep->helpers_selector, "helpers", av, ac);
	XtManageChild (form);
	fep->helpers_page = form;
  

	/* Create the frame and form for the helper files */
	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	frame1 = XmCreateFrame (form, "helperFrame", av, ac);
	ac = 0;
	form1 = XmCreateForm (frame1, "helperBox", av, ac);

	/* Create labels */
	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "helperBoxLabel", av, ac);

	XtVaGetValues(label1, XmNfontList, &labelFont, NULL);

	/* Create the widgets for the helper form */

	ac = 0;
	XtSetArg(av[ac], XmNselectionPolicy, XmSELECT_BROWSE_ROW); ac++;
	XtSetArg(av[ac], XmNhorizontalSizePolicy, XmCONSTANT); ac++;
	XtSetArg(av[ac], XmNallowColumnResize, True); ac++;
	XtSetArg(av[ac], XmNcolumnSizePolicy, XmVARIABLE); ac++;
	table = fep->helpers_list = XmLCreateGrid(form1, "helperApp", av, ac);
	XtVaSetValues( fep->helpers_list, XmNlayoutFrozen, True, NULL);

	XmLGridAddColumns(fep->helpers_list, XmCONTENT, -1, 2 );

	XtVaSetValues(fep->helpers_list, 
				  XmNrowType, XmHEADING,
				  XmNcolumn, 0,
				  XmNcellDefaults, True,
				  /*		XmNcellFontList, labelFont, */
				  XmNcolumnWidth,  DEFAULT_COLUMN_WIDTH,
                		  XmNcellType, XmSTRING_CELL,
                		  XmNcellEditable, True,
				  XmNcellAlignment, XmALIGNMENT_LEFT, NULL);
 

	/* Second Column */
	XtVaSetValues(fep->helpers_list, 
				  XmNrowType, XmHEADING,
				  XmNcolumn, 1,
				  XmNcellDefaults, True,
				  /*		XmNcellFontList, labelFont, */
                		  XmNcellType, XmSTRING_CELL,
                  		  XmNcellEditable, True,
				  XmNcolumnWidth, MAX_COLUMN_WIDTH,
				  XmNcellAlignment, XmALIGNMENT_LEFT, NULL);
 
	XmLGridAddRows(fep->helpers_list, XmHEADING, -1, 1);
	XmLGridSetStrings(fep->helpers_list, XP_GetString(XFE_HELPERS_LIST_HEADER));
 

	/* Get the font from *XmLGrid*fontList at this point.  - XXX dp */
	/* defaultLabelFont = fe_GetFont (context, 2, LO_FONT_FIXED); */

	XtVaSetValues(fep->helpers_list, XmNcellDefaults, True,
				  XmNcellType, XmSTRING_CELL,
				  XmNcellEditable, False,
				  /*		XmNcellFontList, (defaultLabelFont? defaultLabelFont: NULL), */
				  XmNcellLeftBorderType, XmBORDER_NONE,
				  XmNcellRightBorderType, XmBORDER_NONE,
				  XmNcellTopBorderType, XmBORDER_NONE,
				  XmNcellBottomBorderType, XmBORDER_NONE,
				  XmNcellAlignment, XmALIGNMENT_LEFT, NULL);

	XtVaSetValues(fep->helpers_list, XmNlayoutFrozen, False, NULL);


	/* Create for Edit..., New, Delete Buttons */
 
	ac = 0;
	XtSetArg (av [ac], XmNskipAdjust, True); ac++;
	XtSetArg (av [ac], XmNorientation, XmVERTICAL); ac++;
	XtSetArg (av [ac], XmNpacking, XmPACK_TIGHT); ac++;
	commands = XmCreateRowColumn(form1, "commands",  av, ac);
 
	ac = 0;
	fep->edit_b = b = XmCreatePushButton(commands, "edit", av, ac);
	XtAddCallback(b, XmNactivateCallback, fe_helpers_edit_cb, fep);
	XtManageChild(b);

	ac = 0;
	fep->new_b = b = XmCreatePushButton(commands, "new", av, ac);
	XtAddCallback(b, XmNactivateCallback, fe_helpers_new_cb, fep);
	XtManageChild(b);

	ac = 0;
	fep->delete_b = b = XmCreatePushButton(commands, "delete", av, ac);
	XtAddCallback(b, XmNactivateCallback, fe_helpers_delete_cb, fep);
	XtManageChild(b);
 
	XtVaSetValues(commands, XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);
 
	XtVaSetValues(fep->helpers_list, XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, commands,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);
 
	XtManageChild(commands);

	/* Finally, manage the widgets in form1 */
	XtManageChild(fep->helpers_list);
	XtManageChild (form1);
	XtManageChild (label1);
	XtManageChild(frame1);
	XtManageChild(form);

	/* Set up Column Width */
	XtVaGetValues(fep->helpers_list, XmNfontList, &fontList, XmNwidth, &width, NULL);
	XmLFontListGetDimensions(fontList, &avgwidth, &avgheight, TRUE);

	return fep;
}

/*
 * fe_helpers_prepareForDestroy()
 *
 * When the prefs is going to destroy the helpers and free the fep, prefs
 * will call this. The purpose of this is to take the neccessary steps to
 *		- free any internal data
 *		- remove destroy callback that will access the fep
 */
void
fe_helpers_prepareForDestroy(struct fe_prefs_helpers_data *fep)
{
	/* If the edit dialog was up, then the destroy of pref will result in the
	 * destroy of the edit dialog, that would inturn access the fep.
	 * Prevent that.
	 */
	if (fep->editor) {
		/* Prevent destroy from accessing the fep as it will be freed */
		XtRemoveCallback (fep->editor, XmNdestroyCallback,
						  fe_helpers_destroy_cb, fep);

		/* Do whatever destroy of the helper edit dialog does like
		 * freeing memory ect
		 */
		fe_helpers_destroy_cb(0, (XtPointer) fep, 0);
	}
	return;
}
