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


#ifndef _NPPRIV_H_
#define _NPPRIV_H_

#include "xp_core.h"
#include "xp_mem.h"
#include "xp_trace.h"
#include "xp_mcom.h"
#include "lo_ele.h"
#include "npupp.h"
#include "npassoc.h"
#include "npapi.h"

#define ISFUNCPTR(x) (x != NULL)

typedef struct _np_handle np_handle;
typedef struct _np_mimetype np_mimetype;
typedef struct _np_instance np_instance;
typedef struct _np_stream np_stream;
typedef struct _np_data np_data;
typedef struct _np_urlsnode np_urlsnode;
typedef struct _np_reconnect np_reconnect;
struct NPIPlugin;

typedef enum {
	NPDataNormal = 0,		/* URL_Struct.fe_data -> NPEmbeddedApp.np_data -> np_data */
	NPDataCache = 1,		/* LO_EmbedStruct.session_data -> np_data */
	NPDataCached = 2,		/* LO_EmbedStruct.session_data -> np_data */
	NPDataSaved = 3			/* LO_EmbedStruct.session_data -> np_data */
} NPDataState;

struct _np_data {  
    NPDataState state;  
    np_handle *handle;
    NPEmbeddedApp *app;
    NPSavedData *sdata;
    /* Not valid in state NPDataSaved! */
    np_instance *instance;
    LO_EmbedStruct *lo_struct;
    int32 refs;
    XP_Bool streamStarted;
};

struct _np_handle {
    np_handle *next;
    NPPluginFuncs *f;
    void *pdesc;                /* pd glue description */
    int32 refs;
    np_instance *instances;
    np_mimetype *mimetypes;
    char *name;
    char *filename;
    char *description;
    XP_Bool usesNewAPI;
    struct NPIPlugin* userPlugin;
};

struct _np_mimetype {
	np_mimetype* next;
	NPMIMEType type;
    NPFileTypeAssoc *fassoc;
	np_handle* handle;
	XP_Bool enabled;
};

struct _np_instance {
    np_handle *handle;
    np_mimetype *mimetype;
    char *typeString;
    np_instance *next;
    NPEmbeddedApp *app;
    NPP npp;
    MWContext *cx;
    np_stream *streams;
    uint16 type;
    int reentrant;
    URL_Struct *delayedload;
    XP_List *url_list;
    JRIEnv* javaEnv;
    JRIGlobalRef javaInstance;
    XP_Bool windowed;           /* TRUE = has its own window, FALSE = windowless */
    XP_Bool transparent;        /* TRUE = transparent, FALSE = opaque */
	XP_Bool calling_netlib_all_the_time;
	JRIGlobalRef mochaWindow;
#ifdef LAYERS
    CL_Layer *layer;
#endif /* LAYERS */
};

struct _np_stream {
    np_instance *instance;
    np_handle *handle;
    np_stream *next;
    NPStream *pstream;
    char *url;                  /* convenience */
    URL_Struct *initial_urls;
    NET_StreamClass *nstream;
    int32 len;
    int init;
    int seek;                   /* 0 normal, -1 turn, 1 seek, 2 discard */
    int seekable;
    int dontclose;
    int asfile;
    int islocked;
    int32 offset;
    NPByteRange *deferred;
    NET_StreamClass *prev_stream;
};

struct _np_urlsnode {
    URL_Struct *urls;
    void* notifyData;
    XP_Bool cached;
    XP_Bool notify;
};

/* MWContext.pluginReconnect -> np_reconnect */
struct _np_reconnect {
	np_mimetype* mimetype;
	char* requestedtype;
	NPEmbeddedApp* app;
};

#endif /* _NPPRIV_H_ */
