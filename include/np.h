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
 *  Prototypes for functions exported by libplugin and called by the FEs or other XP libs.
 *  Prototypes for functions exported by the FEs and called by libplugin are in nppg.h.
 */

#ifndef _NP_H
#define _NP_H

#include "lo_ele.h"
#include "npapi.h"
#include "net.h"

#ifdef XP_UNIX
/* Aaagh. npapi.h include Xlib.h. Bool is being #defined by Xlib and
   we are typedeffing it in the navigator. */
#ifdef Bool
#undef Bool
#endif /* Bool */
#endif

typedef enum { NP_Untyped = 0, NP_OLE, NP_Plugin } NPAppType;
typedef enum { NP_FullPage = 1, NP_Embedded } NPPageType;

typedef void* NPReference;
#define NPRefFromStart ((NPReference)NULL)

/* it's lame that this is supposed to support more than plugins but
it has plugin specific junk (wdata) in it -jg */

struct _NPEmbeddedApp {
	struct _NPEmbeddedApp *next;
    NPAppType type;
    void *fe_data;
    void *np_data;
    NPWindow *wdata;
    NPPageType pagePluginType;
};

/* Uncomment this to enable ANTHRAX.  .c files affected: npglue.c, layembed.c, layobj.c */
/* amusil 1.8.98 */
/* #define ANTHRAX */

XP_BEGIN_PROTOS

extern void 			NPL_Init(void);
extern void 			NPL_Shutdown(void);

extern void 			NPL_RegisterDefaultConverters(void);
extern NPError			NPL_RegisterPluginFile(const char* pluginname, const char* filename,
								const char* description, void* pd);
extern NPError			NPL_RegisterPluginType(NPMIMEType type, const char *extentstring,
								const char* description, void* fileType, void* pd, XP_Bool enabled);
extern NPError			NPL_RefreshPluginList(XP_Bool reloadPages);

extern NPBool			NPL_IteratePluginFiles(NPReference* ref, char** name, char** filename, char** description);
extern NPBool			NPL_IteratePluginTypes(NPReference* ref, NPReference plugin, NPMIMEType* type,
								char*** extents, char** description, void** fileType);
PR_EXTERN(char**) 			NPL_FindPluginsForType(const char* typeToFind);
extern char*			NPL_FindPluginEnabledForType(const char* typeToFind);

extern NPError			NPL_EnablePlugin(NPMIMEType type,
										 const char* pluginname,
										 XP_Bool enabled);
extern NPError			NPL_EnablePluginType(NPMIMEType type, void* pdesc, XP_Bool enabled);
extern NPError			NPL_DisablePlugin(NPMIMEType type);

extern NPEmbeddedApp*	NPL_EmbedCreate(MWContext *context, LO_EmbedStruct *embed_struct);
extern NPError			NPL_EmbedStart(MWContext* cx, LO_EmbedStruct* embed_struct, NPEmbeddedApp* app);

extern void				NPL_EmbedSize(NPEmbeddedApp *app);

/* ~~av the following is used in CGenericDoc::FreeEmbedElement */
extern int32      NPL_GetEmbedReferenceCount(NPEmbeddedApp *app);

extern void             NPL_EmbedDelete(MWContext *context, LO_EmbedStruct *embed_struct);

extern XP_Bool			NPL_IsLiveConnected(LO_EmbedStruct *embed);

extern int				NPL_HandleEvent(NPEmbeddedApp *app, void *event, void* window); /* window may be NULL */
extern void				NPL_Print(NPEmbeddedApp *app, void *printData);
extern void				NPL_SamePage(MWContext *context);
extern void 			NPL_SameElement(LO_EmbedStruct *embed);
extern void				NPL_DeleteSessionData(MWContext* context, void* sessionData);
extern XP_Bool 			NPL_HandleURL(MWContext *pContext, FO_Present_Types iFormatOut, URL_Struct *pURL,
                                              Net_GetUrlExitFunc *pExitFunc);
#ifndef XP_MAC								
extern void				NPL_DisplayPluginsAsHTML(FO_Present_Types format_out, URL_Struct *urls, MWContext *cx);
#endif
extern void				NPL_PreparePrint(MWContext* context, SHIST_SavedData* savedData);

extern NET_StreamClass* NPL_NewEmbedStream(FO_Present_Types format_out, void *type, URL_Struct *urls, MWContext *cx);
extern NET_StreamClass* NPL_NewPresentStream(FO_Present_Types format_out, void *type, URL_Struct *urls, MWContext *cx);
extern unsigned int     NPL_WriteReady(NET_StreamClass *stream);
extern int              NPL_Write(NET_StreamClass *stream, const unsigned char *str, int32 len);
extern void             NPL_Complete(NET_StreamClass *stream);
extern void             NPL_Abort(NET_StreamClass *stream, int status);
extern XP_Bool          NPL_IsEmbedWindowed(NPEmbeddedApp *app);
extern void				NPL_URLExit(URL_Struct *urls, int status, MWContext *cx);

#ifdef XP_MAC
extern XP_Bool			NPL_IsForcingRedraw();
#endif

#ifdef ANTHRAX
extern char**			NPL_FindAppletsForType(const char* typeToFind);
extern char* 			NPL_FindAppletEnabledForMimetype(const char* mimetype);
extern NPError			NPL_RegisterAppletType(NPMIMEType type, char* filename);
extern void				NPL_InstallAppletHandler(char* appletName, char* mimetype, char* extension);
#endif /* ANTHRAX */

PR_EXTERN(void)         NPL_SetPluginWindow(void *data);
PR_EXTERN(struct nsIPlugin*) NPL_LoadPluginByType(const char* typeAttribute);

/*
 * This callback is installed by the FE to handle the nsIPluginManager2::ProcessNextEvent
 * operation. The result parameter should return PR_TRUE if called on the mozilla thread
 * (unlike the old nsn_TickleHookProcPtr which returned false (I think)).
 */
typedef PRBool (PR_CALLBACK* NPL_ProcessNextEventProc)(void* data);

PR_EXTERN(void) 
NPL_InstallProcessNextEventProc(NPL_ProcessNextEventProc proc, void* data);

XP_END_PROTOS

#endif /* _NP_H */


