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

/*
 * plugin.c
 *
 * Xfe implementation of unix plugins.
 *
 * dp Suresh <dp@netscape.com>
 */

#include "mozilla.h"
#include "xfe.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "np.h"
#include "nppg.h"
#include "prlink.h"
#include "prthread.h"
#include "prlog.h"
#ifdef NSPR20
#include "prerror.h"
#endif /* NSPR20 */
#include "pref_helpers.h"
#include "edt.h"
#ifdef JAVA
#include "java.h"
#endif
#include "obsolete/probslet.h"
#include "npglue.h"

EXTERN void fe_GetProgramDirectory(char *path, int len);

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_PLUGIN_NO_PLUGIN;
extern int XFE_PLUGIN_CANT_LOAD;

#ifdef SCO_SV
#include "mcom_db.h"	/* for MAXPATHLEN */
#endif

/* extern from commands.c */
EXTERN NET_StreamClass *fe_MakeSaveAsStream (int format_out, void *data_obj,
					     URL_Struct *url_struct,
					     MWContext *context);

#define DEFAULT_LEGACY_PLUGIN_PATH "/usr/local/lib/netscape/plugins"

/* Things that are written to the "plugin-list" file */
#define PLUGIN_NAME_PARAM	"pluginName="
#define PLUGIN_DESC_PARAM	"pluginDescription="

#ifdef X_PLUGINS
typedef struct NPX_MimeInfo {
    char *name;
    char *extensions;
    char *description;
} NPX_MimeInfo;

typedef struct NPX_PlugIn {
    char *filename;
    char *pluginName;
    char *pluginDesc;
    time_t modifyTime;
    char found;
    char Delete;
    int numberOfOpens;
    PRLibrary *dlopen_obj;
    NPError(*load)(NPNetscapeFuncs *, NPPluginFuncs *);
    NPError(*shutdown)();
    NPError(*getvalue)(void *, NPPVariable variable, void *value);
    NPPluginFuncs fctns;
    int numMimeEntries;
    NPX_MimeInfo *mimeEntries;
    np_handle* handle;
} NPX_PlugIn;

typedef struct NPX_PlugInList {
    int num;
    int max;
    NPX_PlugIn *plugins;
} NPX_PlugInList;

static NPX_PlugInList *pluginList;


/* Forward declarations */
static NPX_PlugIn *findPluginInfoFromName(NPX_PlugInList *list,
					  char *pluginName);


/*---------------------- Mailcap handling functions -------------------- */
static void
fe_AddCinfo(char *mimeType)
{
  NET_cdataStruct *cd;
  
  cd = NET_cdataCreate();
  cd->ci.type = 0;
  cd->is_external = 1;
  cd->is_local = 0; /* This is internal; only for user to view */
  StrAllocCopy(cd->ci.type, mimeType);
  NET_cdataAdd(cd);
}

static NET_cdataStruct *
fe_GetCinfo(char *mimeType)
{
    NET_cdataStruct *cd_item = NULL;
    XP_List *infoList = cinfo_MasterListPointer();
    
    while ((cd_item = (NET_cdataStruct *) XP_ListNextObject(infoList))) {
	if (cd_item->ci.type &&
	    !strcmp(mimeType, cd_item->ci.type)) /* Found */
	    break;
    }

    return(cd_item);
}

static NET_mdataStruct *
fe_GetMailcapEntry(char *mimeType)
{
    return(fe_helpers_get_mailcap_from_type(mimeType));
}

static char *
fe_MakePluginXmodeEntry(char *pluginName)
{
  return (PR_smprintf("%s%s", NET_COMMAND_PLUGIN, pluginName));
}

static void
fe_AddPluginMailcapEntry(char *mimeType, char *pluginName)
{
    char *buf = NULL;
    buf = fe_MakePluginXmodeEntry(pluginName);
    if (!buf) return;
    fe_helpers_add_new_mailcap_data(mimeType, NULL, NULL, buf, 1);
    if (buf) XP_FREE(buf);
}
#endif /* X_PLUGINS */

XP_Bool
fe_IsMailcapEntryPlugin(NET_mdataStruct *md_item)
{
    if (!md_item || !md_item->xmode) return(False);
    return (!strncmp(md_item->xmode, NET_COMMAND_PLUGIN,
		     strlen(NET_COMMAND_PLUGIN)));
}

char *
fe_GetPluginNameFromMailcapEntry(NET_mdataStruct *md_item)
{
    if (!fe_IsMailcapEntryPlugin(md_item)) return(NULL);
    return (md_item->xmode+strlen(NET_COMMAND_PLUGIN));
}

/*
 * fe_GetMimetypeInfoFromPlugin:
 * Given a plugin name and a mime type that this plugin supports, return
 * a pointer to the description text and the extensions text.
 * WARNING: Caller should not modify or free the returned description
 *		or extension string.
 */
int
fe_GetMimetypeInfoFromPlugin(char *pluginName, char *mimetype,
			     char **r_desc, char **r_ext)
{
  int ret = -1;
#ifdef X_PLUGINS
  int i;
  NPX_PlugIn *p = NULL;
  NPX_MimeInfo *m = NULL;

  if (!mimetype || !*mimetype || !pluginName || !*pluginName) return(ret);

  p = findPluginInfoFromName(pluginList, pluginName);
  if (!p) return(ret);

  i = p->numMimeEntries;
  m = p->mimeEntries;
  while (i > 0) {
    if (!strcmp(mimetype, m->name))
      break;
    i--; m++;
  }
  if (i > 0) {
    /* Found the mimetype for plugin */
    if (r_desc) *r_desc = m->description;
    if (r_ext) *r_ext = m->extensions;
    ret = 0;
  }
#endif /* X_PLUGINS */
  return(ret);
}

#ifdef X_PLUGINS
/*---------------------------------------------------------------------- */

static char *skipSpaces(char *str)
{
    while (*str != '\0' && isspace(*str)) {
	str++;
    }
    return str;
}

static char *scanToken(char *str, char *buf, int len, char *stopChars, Bool noSpacesOnOutput)
{
    int pos = 0;
    
    str = skipSpaces(str);

    while (*str != '\0') {
	while (*str != '\0' &&
	       ! isspace(*str) &&
	       (stopChars == NULL ||
		strchr(stopChars, *str) == NULL)) {
	    buf[pos++] = *str++;
	}

	str = skipSpaces(str);
	if (*str == '\0' ||
	    stopChars == NULL ||
	    strchr(stopChars, *str) != NULL) {
	    break;
	}
	if (! noSpacesOnOutput) {
	    buf[pos++] = ' ';
	}
    }
    buf[pos] = '\0';
    return str;
}

static NPX_PlugIn *findPluginInfo(NPX_PlugInList *list, char *filename)
{
    int i;

    if (!list) return (NULL);

    for (i = 0; i < list->num; i++) {
	if (strcmp(list->plugins[i].filename, filename) == 0) {
	    return(&list->plugins[i]);
	}
    }
    return(NULL);
}

static NPX_PlugIn *
findPluginInfoFromName(NPX_PlugInList *list, char *pluginName)
{
    int i;

    if (!list) return (NULL);

    for (i = 0; i < list->num; i++) {
	if (strcmp(list->plugins[i].pluginName, pluginName) == 0) {
	    return(&list->plugins[i]);
	}
    }
    return(NULL);
}

static NPX_PlugIn *getNewPluginInfo(NPX_PlugInList *list, char *filename)
{
    NPX_PlugIn *info;

    if (list->num == list->max) {
	list->max *= 2;
	list->plugins = (NPX_PlugIn *) realloc(list->plugins, list->max * sizeof(NPX_PlugIn));
    }

    info = &list->plugins[list->num++];
    info->filename = strdup(filename);
    info->mimeEntries = NULL;
    info->found = 0;
    info->Delete = 0;
    info->numMimeEntries = 0;
    info->numberOfOpens = 0;
    info->pluginName = NULL;
    info->pluginDesc = NULL;
    info->dlopen_obj = NULL;
    info->handle = NULL;

    info->fctns.size = sizeof(NPPluginFuncs);


    return info;
}
 

static void clearPluginInfo(NPX_PlugIn *info)
{
    int i;
    NPX_MimeInfo *pMime;

    pMime = info->mimeEntries;
    for (i = 0; i < info->numMimeEntries; i++) {
	free(pMime->name);
	free(pMime->extensions);
	if (pMime->description)
  	    free(pMime->description);
	pMime++;
    }
    free(info->mimeEntries);
    if (info->pluginName)
	free(info->pluginName);
    if (info->pluginDesc)
	free(info->pluginDesc);
    info->found = 0;
    info->Delete = 0;
    info->numMimeEntries = 0;
    info->numberOfOpens = 0;
    info->dlopen_obj = NULL;
    info->mimeEntries = NULL;
    info->pluginName = 0;
    info->pluginDesc = 0;
}

static int parsePluginDescription(NPX_PlugIn *currentInfo, char *str)
{
    char buf[1000];
    int maxMimeEntries;
    NPX_MimeInfo *mimeEntry;

    maxMimeEntries = 2;
    currentInfo->mimeEntries = (NPX_MimeInfo *) malloc(2 * sizeof(NPX_MimeInfo));
    currentInfo->numMimeEntries = 0;
    
    while (1) {
	str = scanToken(str, buf, sizeof(buf), ":", FALSE);
	if (buf[0] == '\0' || *str != ':') {
	    break;
	}
	str++;
	if (currentInfo->numMimeEntries == maxMimeEntries) {
	    maxMimeEntries *= 2;
	    currentInfo->mimeEntries = (NPX_MimeInfo *) realloc(currentInfo->mimeEntries, maxMimeEntries * sizeof(NPX_MimeInfo));
	}
	
	mimeEntry = &currentInfo->mimeEntries[currentInfo->numMimeEntries++];
	mimeEntry->name = strdup(buf);
	mimeEntry->description = NULL;
	mimeEntry->extensions = NULL;
	
	str = scanToken(str, buf, sizeof(buf), ";:", TRUE);
	mimeEntry->extensions = strdup(buf);
	if (*str == ':') {
	    str++;
	    str = scanToken(str, buf, sizeof(buf), ";", FALSE);
	    mimeEntry->description = strdup(buf);
	}
#if 0
	else {
	    /* Use mimetype for mime description */
	    mimeEntry->description = strdup(mimeEntry->name);
	}
#endif /* 0 */

	if (*str != ';') {
	    break;
	}
	str++;
    }

    return (currentInfo->numMimeEntries == 0);
}


static void updateMimeInfo(NPX_PlugIn *plugin)
{
    int i;
    NPX_MimeInfo *pMime = plugin->mimeEntries;

    if (!plugin->pluginName)
	plugin->pluginName = fe_Basename(XP_STRDUP(plugin->filename));

    NPL_RegisterPluginFile(plugin->pluginName, plugin->filename,
				plugin->pluginDesc,
				(void *)plugin);
    
    for (i = 0; i < plugin->numMimeEntries; i++) {
      NPL_RegisterPluginType(pMime->name, pMime->extensions,
			     pMime->description?pMime->description:pMime->name,
			     NULL, (void *)plugin,
			     FALSE);
      pMime++;
    }
}

static NPX_PlugIn *readPluginInfo(NPX_PlugInList *list, char *filename)
{
    struct stat buf;
    NPX_PlugIn *currentInfo;
    char *(*fct)(void);
    NPError (*getvalue)(void *, int, void *);
    char *newInfo;
    PRLibrary *obj;

    if (stat(filename, &buf) != 0 || (buf.st_mode & S_IFREG) != S_IFREG) {
	return NULL;
    }

    currentInfo = findPluginInfo(list, filename);
    
    if (currentInfo != NULL) {
	/* currentInfo could have been filled in here because of
	 * 1. The plugin was in the plugin-list. (currentInfo->found == 0)
	 * 2. This is caused by a re-scan of the plugins directory.
	 *	(currentInfo->found == 1).
	 *	In this case, we can do a lot of things smart but since we
	 *	already have registered the plugin, we need to change the backed
	 *	too. That isn't supported yet like unregistering a plugin.
	 *	So we will let it go along as far it can and let fate decide...
	 */
	if (currentInfo->found)
	    return(currentInfo);

	currentInfo->Delete = 0;
	if (currentInfo->modifyTime == buf.st_mtime) {
	    currentInfo->found = 1;
  	    updateMimeInfo(currentInfo);
	    return currentInfo;
	}
	clearPluginInfo(currentInfo);
    }
    
    if ((obj = PR_LoadLibrary(filename)) != NULL) {

#ifndef NSPR20
	fct = (char *(*)())PR_FindSymbol("NP_GetMIMEDescription", obj);
#else
	fct = (char *(*)())PR_FindSymbol(obj, "NP_GetMIMEDescription");
#endif /* NSPR20 */
	if (fct == NULL ||
	    (newInfo = (*fct)()) == NULL) {
	    int err = PR_UnloadLibrary(obj);
	    PR_ASSERT(err == 0);
	    return NULL;
	}

	if (currentInfo == NULL) {
	    currentInfo = getNewPluginInfo(list, filename);
	}

	if (parsePluginDescription(currentInfo, newInfo)) {
	    int err = PR_UnloadLibrary(obj);
	    PR_ASSERT(err == 0);
	    currentInfo->Delete = 1;
	    return NULL;
	}

#ifndef NSPR20
	getvalue = (NPError (*)(void *, int, void *)) PR_FindSymbol("NP_GetValue", obj);
#else
	getvalue = (NPError (*)(void *, int, void *)) PR_FindSymbol(obj, "NP_GetValue");
#endif /* NSPR20 */
        if (getvalue != NULL) {
	    char *value;
	    NPError err;
	    err = (*getvalue)(NULL, NPPVpluginNameString, (void *) &value);
	    if (err == NPERR_NO_ERROR)
		currentInfo->pluginName = strdup(value);
	    else
	        currentInfo->pluginName =
		  fe_Basename(strdup(currentInfo->filename));
	    err = (*getvalue)(NULL, NPPVpluginDescriptionString,
			      (void *) &value);
	    if (err == NPERR_NO_ERROR)
		currentInfo->pluginDesc = strdup(value);
	}

	currentInfo->modifyTime = buf.st_mtime;
	currentInfo->found = 1;

	updateMimeInfo(currentInfo);
	{
	int err = PR_UnloadLibrary(obj);
	PR_ASSERT(err == 0);
	}
	return currentInfo;
    }
    else {
	fprintf (stderr, XP_GetString(XFE_PLUGIN_CANT_LOAD),
		 PR_GetErrorString(), filename);
    }

    return NULL;
}


static int readPluginList(NPX_PlugInList *list, char *filename)
{
    FILE *in;
    int versionNumber;
    char buf[10000];
    char fileNameBuf[MAXPATHLEN];
    char timeBuf[20];
    NPX_PlugIn *pInfo = NULL;
    char *str;

    if ((in = fopen(filename, "r")) != NULL) {
	if (fgets(buf, sizeof(buf), in) != NULL &&
	    strlen(buf) >= 20 &&
	    strncmp(buf, "PluginList Version ", 19) == 0) {
	    /* looking for header, which contains id, version, number of elements */

	    versionNumber = atoi(&buf[19]);

	    if (versionNumber != 1) {
		fclose(in);
		return 1;
	    }

	    while (fgets(buf, sizeof(buf), in) != NULL) {
		if (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		if (!strncmp(buf, PLUGIN_NAME_PARAM,
			     strlen(PLUGIN_NAME_PARAM))) {
		    if (pInfo)
			pInfo->pluginName =
			  strdup(&buf[strlen(PLUGIN_NAME_PARAM)]);
		    continue;
		}
		if (!strncmp(buf, PLUGIN_DESC_PARAM,
			     strlen(PLUGIN_DESC_PARAM))) {
		    if (pInfo)
			pInfo->pluginDesc =
			  strdup(&buf[strlen(PLUGIN_DESC_PARAM)]);
		    continue;
		}
		str = scanToken(buf, fileNameBuf, sizeof(fileNameBuf), NULL, FALSE);
		if (fileNameBuf == '\0') {
		    continue;
		}
		pInfo = findPluginInfo(list, fileNameBuf);
		if (pInfo == NULL) {
		    pInfo = getNewPluginInfo(list, fileNameBuf);
		}
		else {
		    clearPluginInfo(pInfo);
		}

		str = scanToken(str, timeBuf, sizeof(timeBuf), NULL, FALSE);

		if (timeBuf[0] == '\0') {
		    continue;
		}

		pInfo->modifyTime = atoi(timeBuf);

		parsePluginDescription(pInfo, str);
	    }
	}	    
	
	fclose(in);
    }
    return 1;
}

static int writePluginList(NPX_PlugInList *list, char *filename)
{
    FILE *out;
    int i;
    int j;
    NPX_PlugIn *pInfo;
    NPX_MimeInfo *pMime;

    if ((out = fopen(filename, "w")) != NULL) {
	fputs("PluginList Version 1\n", out);
	pInfo = list->plugins;
	for (i = 0; i < list->num; i++) {
	    if (pInfo->found && ! pInfo->Delete && pInfo->numMimeEntries > 0) {
		fprintf(out, "%s %d", pInfo->filename, pInfo->modifyTime);
		pMime = pInfo->mimeEntries;
		for (j = 0; j < pInfo->numMimeEntries; j++) {
		    fprintf(out, " %s: %s", pMime->name, pMime->extensions);
		    if (pMime->description)
		        fprintf(out, ":%s", pMime->description);
		    fputs(";", out);
		    pMime++;
		}
		fputs("\n", out);
		if (pInfo->pluginName && *pInfo->pluginName)
		    fprintf(out, "%s%s\n", PLUGIN_NAME_PARAM, pInfo->pluginName);
		if (pInfo->pluginDesc && *pInfo->pluginDesc)
		    fprintf(out, "%s%s\n", PLUGIN_DESC_PARAM, pInfo->pluginDesc);
	    }
	    pInfo++;
	}
	fclose(out);
	return 0;
    }
    return 1;
}

static void getPluginsFromDirectory(NPX_PlugInList *list, char *directoryName)
{
    DIR *dir;
    char filename[MAXPATHLEN];
    char *nameptr;
    struct dirent *dp;

#ifdef JAVA
    /* Add directory to the java backend so that it can find java plugins */
    LJ_AddToClassPath(directoryName);
#endif /* JAVA */

    if ((dir = opendir(directoryName)) != NULL) {
	strcpy(filename, directoryName);
	nameptr = &filename[strlen(filename)];
	*nameptr++ = '/';
	while ((dp = readdir(dir)) != NULL) {
	    if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
		continue;
	    strcpy(nameptr, dp->d_name);
	    if (fe_isDir(filename))
		getPluginsFromDirectory(list, filename);
	    if (!strcmp(dp->d_name, "libc.so.1"))
		continue;
		/*
		 *    Look for Composer (Editor) plugins:
		 *    the local filename has form: cp<something>.[zip|jar]
		 */
	    if (strncmp(dp->d_name, "cp", 2) == 0
			&&
			(fe_StrEndsWith(dp->d_name, ".zip")
			 ||
			 fe_StrEndsWith(dp->d_name, ".jar"))
			) {
#ifdef EDITOR
#ifdef DEBUG_mcafee
			printf("  Found editor plugin \"%s\" in %s.\n", filename, directoryName);
#endif
			EDT_RegisterPlugin(filename);
#endif
			continue;
		}

	    /* Ignore .class and .zip files */
	    if (fe_StrEndsWith(dp->d_name, ".class") ||
		fe_StrEndsWith(dp->d_name, ".zip"))
	      continue;
	    readPluginInfo(list, filename);
#ifdef DEBUG_mcafee
		printf("  Found plugin \"%s\" in %s.\n", filename, directoryName);
#endif
	}
	closedir(dir);
    }
}

static void getPluginsFromPath(NPX_PlugInList *list, char *path)
{
    char *str = path;
    char directoryName[MAXPATHLEN];

    while (*str != '\0') {
	str = scanToken(str, directoryName, sizeof(directoryName), ":", FALSE);

	if (directoryName[0] != '\0') {
#ifdef DEBUG_mcafee
			printf("Searching in %s.\n", directoryName);
#endif

	    getPluginsFromDirectory(list, directoryName);
	}

	if (*str == ':') {
	    str++;
	}
    }
}

static NPX_PlugInList *getPluginList(int numFiles)
{
    NPX_PlugInList *list;

    list = (NPX_PlugInList *) malloc(sizeof(NPX_PlugInList));
    list->num = 0;
    list->max = numFiles;
    list->plugins = (NPX_PlugIn *) malloc(numFiles * sizeof(NPX_PlugIn));

    return list;
}

#ifdef DEBUG_dp
static void dumpPluginInfo(NPX_PlugIn *info)
{
    int i;

    printf("filename: %s time: %d\n",
	    info->filename, info->modifyTime);

    for (i = 0; i < info->numMimeEntries; i++) {
	printf("    mimetype: %s extensions: %s\n", info->mimeEntries[i].name, info->mimeEntries[i].extensions);
    }
    printf("\n");
}
#endif
#endif /* X_PLUGINS */

void FE_RegisterPlugins()
{
#ifdef X_PLUGINS
    char filename[MAXPATHLEN];
    char newFilename[MAXPATHLEN];
    char oldFilename[MAXPATHLEN];
    char *npxPluginPath  = getenv("NPX_PLUGIN_PATH");
    char *mozHome        = getenv("MOZILLA_HOME");
	char *pluginPath     = NULL;
    char *home = getenv ("HOME");
    
    if (!home)
		home = "";
    else if (!strcmp (home, "/"))
        home = "";

	/* Algorithm for searching for plugins is:
	   if $NPX_PLUGIN_PATH
	       Use that
       else
	       Build up a colon-delimited search string like
             /usr/local/lib/netscape/plugins:$MOZILLA_HOME/plugins:
             $HOME/.netscape/plugins
           with last directory overriding other directories, since
           they all get searched. */
    if(npxPluginPath) {
		pluginPath = npxPluginPath;
	} else {
		/* Stuff in $MOZILLA_HOME if it's defined. */
		if(mozHome) {
			pluginPath = PR_smprintf("%s:%s/plugins:%.900s/.netscape/plugins",
									 DEFAULT_LEGACY_PLUGIN_PATH,
									 mozHome,
									 home);
		} else {
			/* Try to stuff argv[0] into the path */
			char buf[MAXPATHLEN];

			buf[0] = '\0';
			fe_GetProgramDirectory(buf, sizeof(buf)-1);
			strncat(buf, "plugins", sizeof(buf)-1 - strlen(buf));
			buf[sizeof(buf)-1] = '\0';

			pluginPath = PR_smprintf("%s:%s:%.900s/.netscape/plugins",
									 DEFAULT_LEGACY_PLUGIN_PATH,
									 buf,
									 home);			
		}
    }

    PR_snprintf(filename, sizeof(filename), "%.900s/.netscape/plugin-list", home);
    PR_snprintf(newFilename, sizeof(newFilename), "%.900s/.netscape/plugin-list.new", home);
    PR_snprintf(oldFilename, sizeof(oldFilename), "%.900s/.netscape/plugin-list.BAK", home);

    if (pluginList == NULL) {
        pluginList = getPluginList(32);
		readPluginList(pluginList, filename);
    }

    getPluginsFromPath(pluginList, pluginPath);

    if (writePluginList(pluginList, newFilename) == 0) {
        rename(filename, oldFilename);
        rename(newFilename, filename);
    }
    
    if (!npxPluginPath) 
		XP_FREE(pluginPath);  /* Free if we used PR_smprintf(). */

    fe_RegisterPluginConverters();

#endif /* X_PLUGINS */
}

/*
 * The purpose of this function is
 * 1. Add md and cinfo list entries for new mimetypes that plugins handle
 * 2. Enable plugins for the new mimetype that we handle
 * 3. Change the converter for which there is no plugin to save as.
 */
void
fe_RegisterPluginConverters(void)
{
  NET_cdataStruct *cd_item = NULL;
  NET_mdataStruct *md_item = NULL;
  XP_List *infoList = cinfo_MasterListPointer();
#ifdef X_PLUGINS
  NPX_PlugInList *list = pluginList;
  int i, n;

  if (!list) return;

  /* Register all default plugin converters */
  NPL_RegisterDefaultConverters();
 
  /* 1. Add md and cinfo list entries for new mimetypes that plugins handle
   * 2. Enable plugins for the new mimetype that we handle
   *
   */

  for (n=0; n<list->num; n++) {
    NPX_PlugIn *plugin = &list->plugins[n];
    NPX_MimeInfo *pMime = plugin->mimeEntries;
    for (i = 0; i < plugin->numMimeEntries; i++) {
      XP_Bool enable = TRUE;
      cd_item = fe_GetCinfo(pMime->name);
      md_item = NULL;

      if (cd_item) {
	md_item = fe_GetMailcapEntry(cd_item->ci.type);
	if (md_item) {
	  if (!fe_IsMailcapEntryPlugin(md_item) ||
	      (strcmp(fe_GetPluginNameFromMailcapEntry(md_item),
		      plugin->pluginName)))
	    enable = FALSE;

	  /* Eventhough the mailcap entry is not a plugin, if we had changed
	     it to save to Disk because a plugin suddenly was not found, we
	     will change it back to plugin once it was found. */
	  if (md_item->xmode &&
	      !XP_STRCASECMP(md_item->xmode, NET_COMMAND_SAVE_BY_NETSCAPE)) {
	    char *newxmode = fe_MakePluginXmodeEntry(plugin->pluginName);
	    /* Enable the plugin for this mimetype */
	    enable = TRUE;
	    
	    /* Change the mailcap entry back to plugin */
	    if (newxmode) {
	      fe_helpers_update_mailcap_entry(md_item->contenttype, md_item,
					      newxmode);
	      if (newxmode) XP_FREE(newxmode);
	    }
	  }
	}
      }

      if (enable) {
	if (!cd_item) {
	  fe_AddCinfo(pMime->name);
	}
	if (!md_item) {
	  fe_AddPluginMailcapEntry(pMime->name, plugin->pluginName);
	}

	if (NPL_EnablePlugin(pMime->name, plugin->pluginName, TRUE)
	    != NPERR_NO_ERROR) {
#ifdef DEBUG
	  fprintf (stderr, "Cannot enable plugin %s for type %s.\n",
		   plugin->pluginName, pMime->name);
#endif /* DEBUG */
	}
      } /* enabled */
      pMime++;
    }
  }
#endif /* X_PLUGINS */

  /* 3. Change the converter for which there is no plugin to save as. */
  while ((cd_item = (NET_cdataStruct *) XP_ListNextObject(infoList))) {
    md_item = fe_helpers_get_mailcap_from_type(cd_item->ci.type);

    if (fe_IsMailcapEntryPlugin(md_item)) {
      char *pname = fe_GetPluginNameFromMailcapEntry(md_item);
	     
      if (
#ifdef X_PLUGINS
	  !findPluginInfoFromName(pluginList, pname)
#else /* X_PLUGINS */
	  True /* Plugin info is always not found for NO PLUGIN */
#endif /* X_PLUGINS */
	  ) {
	/* We have a plugin entry but no plugin */
	fprintf (stderr, XP_GetString(XFE_PLUGIN_NO_PLUGIN),
		 pname, cd_item->ci.type);
	NET_RegisterContentTypeConverter(cd_item->ci.type,
					 FO_PRESENT,
					 NULL, fe_MakeSaveAsStream );
	fe_helpers_update_mailcap_entry(md_item->contenttype, md_item,
					      NET_COMMAND_SAVE_BY_NETSCAPE);
      }
    }
  }
}

    
NPPluginFuncs *
FE_LoadPlugin(void *pdesc, NPNetscapeFuncs *funcs, np_handle* handle)
{
#ifdef X_PLUGINS
    NPX_PlugIn *plugin = (NPX_PlugIn *) pdesc;
    NPError (*f)(NPNetscapeFuncs *, NPPluginFuncs *);

    NPError err;

    if (plugin->dlopen_obj == NULL) {
        plugin->dlopen_obj = PR_LoadLibrary(plugin->filename);
        if (plugin->dlopen_obj == NULL) {
	    fprintf(stderr, XP_GetString(XFE_PLUGIN_CANT_LOAD),
		    PR_GetErrorString(), plugin->filename);
            return NULL;
        }

	NP_CREATEPLUGIN npCreatePlugin =
#ifndef NSPR20
	    (NP_CREATEPLUGIN)PR_FindSymbol("NP_CreatePlugin", plugin->dlopen_obj);
#else
            (NP_CREATEPLUGIN)PR_FindSymbol(plugin->dlopen_obj, "NP_CreatePlugin");
#endif
	if (npCreatePlugin != NULL) {
	    if (thePluginManager == NULL) {
                static NS_DEFINE_IID(kIPluginManagerIID, NP_IPLUGINMANAGER_IID);
                if (nsPluginManager::Create(NULL, kIPluginManagerIID, (void**)&thePluginManager) != NS_OK)
                    return NULL;
	    }
	    NPIPlugin* userPlugin = NULL;
	    NPPluginError err = npCreatePlugin(thePluginManager, &userPlugin);
	    handle->userPlugin = userPlugin;
	    plugin->handle = handle;
	    if (err != NPPluginError_NoError || userPlugin == NULL) {
		int err = PR_UnloadLibrary(plugin->dlopen_obj);
		PR_ASSERT(err == 0);
		plugin->dlopen_obj = NULL;
		return NULL;
	    }
	}
	else {
#ifndef NSPR20
	    f = (NPError(*)(NPNetscapeFuncs *, NPPluginFuncs *)) PR_FindSymbol("NP_Initialize", plugin->dlopen_obj);
#else
	    f = (NPError(*)(NPNetscapeFuncs *, NPPluginFuncs *)) PR_FindSymbol(plugin->dlopen_obj, "NP_Initialize");
#endif /* NSPR20 */

	    if (f == NULL) {
		int err = PR_UnloadLibrary(plugin->dlopen_obj);
		PR_ASSERT(err == 0);
		plugin->dlopen_obj = NULL;
		return NULL;
	    }

	    plugin->load = f;
	    err = (*f)(funcs, &plugin->fctns);

	    if (err != NPERR_NO_ERROR) {
		int err = PR_UnloadLibrary(plugin->dlopen_obj);
		PR_ASSERT(err == 0);
		plugin->dlopen_obj = NULL;
		return NULL;
	    }

#ifndef NSPR20
	    plugin->shutdown = (NPError(*)(void)) PR_FindSymbol("NP_Shutdown", plugin->dlopen_obj);
#else
	    plugin->shutdown = (NPError(*)(void)) PR_FindSymbol(plugin->dlopen_obj, "NP_Shutdown");
#endif /* NSPR20 */

	}
    }

    plugin->numberOfOpens++;
    return &plugin->fctns;
#else /* X_PLUGINS */
    return (NULL);
#endif /* X_PLUGINS */
}


void
FE_UnloadPlugin(void *pdesc, struct _np_handle* handle)
{
#ifdef X_PLUGINS
    NPX_PlugIn *plugin =  (NPX_PlugIn *) pdesc;
    NPError (*f)(void);
    NPError err;

    if (plugin->handle) {
	if (plugin->handle->userPlugin) {
	    NPIPlugin* userPlugin = (NPIPlugin*)plugin->handle->userPlugin;
	    XP_VERIFY(userPlugin->Release() == 0);
	    plugin->handle = NULL;
	}
    } else {
	f = plugin->shutdown;
	if (f) {
	    err = (*f)();
	    if (err != NPERR_NO_ERROR) {
		/* XXX There was a error while unloading this plugin. What can we
		 *     do. Ignore it for now.
		 */  
	    }
	}
    }
    plugin->numberOfOpens--;

    if (plugin->numberOfOpens == 0) {
	int err = PR_UnloadLibrary(plugin->dlopen_obj);
	PR_ASSERT(err == 0);
	plugin->dlopen_obj = NULL;
    }
    else if (plugin->numberOfOpens < 0) {
        plugin->numberOfOpens = 0;
    }
#endif /* X_PLUGINS */
}

void
FE_EmbedURLExit(URL_Struct *urls, int status, MWContext *cx)
{
}

NPError
FE_PluginGetValue(void *pdesc, NPNVariable variable, void *r_value)
{
#ifdef X_PLUGINS
  NPError ret = NPERR_NO_ERROR;

  switch (variable) {
  case NPNVxDisplay:
    if (r_value)
      (*(Display **)r_value) = fe_display;
    break;
  case NPNVxtAppContext:
    if (r_value)
      (*(XtAppContext *)r_value) = fe_XtAppContext;
    break;
  default:
    ret = NPERR_INVALID_PARAM;
  }
  return ret;
#else /* X_PLUGINS */
  return(NPERR_INVALID_PARAM);
#endif /* X_PLUGINS */
}

