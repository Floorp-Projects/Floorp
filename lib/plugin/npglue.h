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

#ifndef npglue_h__
#define npglue_h__

#if defined(__cplusplus)
extern "C" {
#endif

#include "net.h"
#include "np.h"
#include "nppg.h"
#include "client.h"
#include "xpassert.h" 
#include "ntypes.h"
#include "fe_proto.h"
#include "cvactive.h"
#include "gui.h"			/* For XP_AppCodeName */
#include "merrors.h"
#include "xpgetstr.h"
#include "java.h"
#include "nppriv.h"
#include "shist.h"

#include "prefapi.h"
#include "proto.h"

#ifdef MOCHA
#include "libmocha.h"
#include "libevent.h"
#include "layout.h"         /* XXX From ../layout */
#endif

#ifdef LAYERS
#include "layers.h"
#endif /* LAYERS */

#include "nsAgg.h"      // nsPluginManager aggregates nsJVMManager
#include "nsjvm.h"

extern int XP_PLUGIN_LOADING_PLUGIN;
extern int MK_BAD_CONNECT;
extern int XP_PLUGIN_NOT_FOUND;
extern int XP_PLUGIN_CANT_LOAD_PLUGIN;
extern int XP_PROGRESS_STARTING_JAVA;

#define NP_LOCK    1
#define NP_UNLOCK  0

#define NPTRACE(n, msg)	TRACEMSG(msg)

#define RANGE_EQUALS  "bytes="

/* @@@@ steal the private call from netlib */
extern void NET_SetCallNetlibAllTheTime(MWContext *context, char *caller);
extern void NET_ClearCallNetlibAllTheTime(MWContext *context, char *caller);

#if defined(XP_WIN) || defined(XP_OS2)
/* Can't include FEEMBED.H because it's full of C++ */
extern NET_StreamClass *EmbedStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
extern void EmbedUrlExit(URL_Struct *pUrl, int iStatus, MWContext *pContext);
#endif

extern void NET_RegisterAllEncodingConverters(char* format_in, FO_Present_Types format_out);


/* Internal prototypes */

void
NPL_EmbedURLExit(URL_Struct *urls, int status, MWContext *cx);

void
NPL_URLExit(URL_Struct *urls, int status, MWContext *cx);

void
np_streamAsFile(np_stream* stream);

NPError
np_switchHandlers(np_instance* instance,
				  np_handle* newHandle,
				  np_mimetype* newMimeType,
				  char* requestedType);

NET_StreamClass*
np_newstream(URL_Struct *urls, np_handle *handle, np_instance *instance);

void
np_findPluginType(NPMIMEType type, void* pdesc, np_handle** outHandle, np_mimetype** outMimetype);

void 
np_enablePluginType(np_handle* handle, np_mimetype* mimetype, XP_Bool enabled);

void
np_bindContext(NPEmbeddedApp* app, MWContext* cx);

void
np_unbindContext(NPEmbeddedApp* app, MWContext* cx);

void
np_deleteapp(MWContext* cx, NPEmbeddedApp* app);

np_instance*
np_newinstance(np_handle *handle, MWContext *cx, NPEmbeddedApp *app,
			   np_mimetype *mimetype, char *requestedType);
			   				
void
np_delete_instance(np_instance *instance);

void
np_recover_mochaWindow(JRIEnv * env, np_instance * instance);

XP_Bool
np_FakeHTMLStream(URL_Struct* urls, MWContext* cx, char * fakehtml);

/* Navigator plug-in API function prototypes */

/*
 * Use this macro before each exported function
 * (between the return address and the function
 * itself), to ensure that the function has the
 * right calling conventions on Win16.
 */
#ifdef XP_WIN16
#define NP_EXPORT __export
#elif defined(XP_OS2)
#define NP_EXPORT _System
#else
#define NP_EXPORT
#endif

NPError NP_EXPORT
npn_requestread(NPStream *pstream, NPByteRange *rangeList);

NPError NP_EXPORT
npn_geturlnotify(NPP npp, const char* relativeURL, const char* target, void* notifyData);

NPError NP_EXPORT
npn_getvalue(NPP npp, NPNVariable variable, void *r_value);

NPError NP_EXPORT
npn_setvalue(NPP npp, NPPVariable variable, void *r_value);

NPError NP_EXPORT
npn_geturl(NPP npp, const char* relativeURL, const char* target);

NPError NP_EXPORT
npn_posturlnotify(NPP npp, const char* relativeURL, const char *target,
                  uint32 len, const char *buf, NPBool file, void* notifyData);

NPError NP_EXPORT
npn_posturl(NPP npp, const char* relativeURL, const char *target, uint32 len,
            const char *buf, NPBool file);

NPError
np_geturlinternal(NPP npp, const char* relativeURL, const char* target, 
                  const char* altHost, const char* referer, PRBool forceJSEnabled,
                  NPBool notify, void* notifyData);

NPError
np_posturlinternal(NPP npp, const char* relativeURL, const char *target, 
                   const char* altHost, const char* referer, PRBool forceJSEnabled,
                   uint32 len, const char *buf, NPBool file, NPBool notify, void* notifyData);

NPError NP_EXPORT
npn_newstream(NPP npp, NPMIMEType type, const char* window, NPStream** pstream);

int32 NP_EXPORT
npn_write(NPP npp, NPStream *pstream, int32 len, void *buffer);

NPError NP_EXPORT
npn_destroystream(NPP npp, NPStream *pstream, NPError reason);

void NP_EXPORT
npn_status(NPP npp, const char *message);

void NP_EXPORT
npn_registerwindow(NPP npp, void* window);

void NP_EXPORT
npn_unregisterwindow(NPP npp, void* window);

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
const char* NP_EXPORT
npn_useragent(NPP npp);
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
void* NP_EXPORT
npn_memalloc (uint32 size);
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif


void NP_EXPORT
npn_memfree (void *ptr);

uint32 NP_EXPORT
npn_memflush(uint32 size);

void NP_EXPORT
npn_reloadplugins(NPBool reloadPages);

void NP_EXPORT
npn_invalidaterect(NPP npp, NPRect *invalidRect);

void NP_EXPORT
npn_invalidateregion(NPP npp, NPRegion invalidRegion);

void NP_EXPORT
npn_forceredraw(NPP npp);

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
JRIEnv* NP_EXPORT
npn_getJavaEnv(void);
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif


#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
jref NP_EXPORT
npn_getJavaPeer(NPP npp);
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif


/* End of function prototypes */


/* this is a hack for now */
#define NP_MAXBUF (0xE000)

////////////////////////////////////////////////////////////////////////////////

typedef NPPluginError
(PR_CALLBACK* NP_CREATEPLUGIN)(NPIPluginManager* mgr, NPIPlugin* *result);

class nsPluginManager : public NPIPluginManager {
public:

    // QueryInterface may be used to obtain a JRIEnv or JNIEnv
    // from an NPIPluginManager.
    // (Corresponds to NPN_GetJavaEnv.)

    ////////////////////////////////////////////////////////////////////////////
    // from NPIPluginManager:

    // (Corresponds to NPN_ReloadPlugins.)
    NS_IMETHOD_(void)
    ReloadPlugins(PRBool reloadPages);

    // (Corresponds to NPN_MemAlloc.)
    NS_IMETHOD_(void*)
    MemAlloc(PRUint32 size);

    // (Corresponds to NPN_MemFree.)
    NS_IMETHOD_(void)
    MemFree(void* ptr);

    // (Corresponds to NPN_MemFlush.)
    NS_IMETHOD_(PRUint32)
    MemFlush(PRUint32 size);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginManager specific methods:

    NS_DECL_AGGREGATED

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

protected:
    nsPluginManager(nsISupports* outer);
    virtual ~nsPluginManager(void);

    // aggregated sub-intefaces:
    NPIJVMPluginManager* GetJVMMgr(const nsIID& aIID);
    nsISupports* fJVMMgr;

};

extern nsPluginManager* thePluginManager;

////////////////////////////////////////////////////////////////////////////////

class nsPluginInstancePeer : public NPILiveConnectPluginInstancePeer {
public:

    nsPluginInstancePeer(NPP npp);
    virtual ~nsPluginInstancePeer(void);

    NS_DECL_AGGREGATED

    NPIPluginInstance* GetUserInstance(void) {
        return userInst;
    }

    void SetUserInstance(NPIPluginInstance* inst) {
        userInst = inst;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Methods specific to NPIPluginInstancePeer:

    NS_IMETHOD_(NPIPlugin*)
    GetClass(void);

    // (Corresponds to NPP_New's MIMEType argument.)
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void);

    // (Corresponds to NPP_New's mode argument.)
    NS_IMETHOD_(NPPluginType)
    GetMode(void);

    // (Corresponds to NPP_New's argc argument.)
    NS_IMETHOD_(PRUint16)
    GetArgCount(void);

    // (Corresponds to NPP_New's argn argument.)
    NS_IMETHOD_(const char**)
    GetArgNames(void);

    // (Corresponds to NPP_New's argv argument.)
    NS_IMETHOD_(const char**)
    GetArgValues(void);

    NS_IMETHOD_(NPIPluginManager*)
    GetPluginManager(void);

    // (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
    NS_IMETHOD_(NPPluginError)
    GetURL(const char* url, const char* target, void* notifyData, 
           const char* altHost, const char* referer, PRBool forceJSEnabled);

    // (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
    NS_IMETHOD_(NPPluginError)
    PostURL(const char* url, const char* target,
            PRUint32 len, const char* buf, PRBool file, void* notifyData, 
            const char* altHost, const char* referer, PRBool forceJSEnabled,
            PRUint32 postHeadersLength, const char* postHeaders);

    // (Corresponds to NPN_NewStream.)
    NS_IMETHOD_(NPPluginError)
    NewStream(nsMIMEType type, const char* target,
              NPIPluginManagerStream* *result);

    // (Corresponds to NPN_Status.)
    NS_IMETHOD_(void)
    ShowStatus(const char* message);

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD_(const char*)
    UserAgent(void);

    // (Corresponds to NPN_GetValue.)
    NS_IMETHOD_(NPPluginError)
    GetValue(NPPluginManagerVariable variable, void *value);

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD_(NPPluginError)
    SetValue(NPPluginVariable variable, void *value);

    // (Corresponds to NPN_InvalidateRect.)
    NS_IMETHOD_(void)
    InvalidateRect(nsRect *invalidRect);

    // (Corresponds to NPN_InvalidateRegion.)
    NS_IMETHOD_(void)
    InvalidateRegion(nsRegion invalidRegion);

    // (Corresponds to NPN_ForceRedraw.)
    NS_IMETHOD_(void)
    ForceRedraw(void);

    NS_IMETHOD_(void)
    RegisterWindow(void* window);
	
    NS_IMETHOD_(void)
    UnregisterWindow(void* window);	

    ////////////////////////////////////////////////////////////////////////////
    // Methods specific to NPILiveConnectPluginInstancePeer:

    // (Corresponds to NPN_GetJavaPeer.)
    NS_IMETHOD_(jobject)
    GetJavaPeer(void);

protected:
    // NPP is the old plugin structure. If we were implementing this
    // from scratch we wouldn't use it, but for now we're calling the old
    // npglue.c routines wherever possible.
    NPP npp;

    NPIPluginInstance* userInst;

    // aggregated sub-intefaces:
    nsISupports* fJVMInstancePeer;

};

////////////////////////////////////////////////////////////////////////////////

class nsPluginManagerStream : public NPIPluginManagerStream {
public:

    nsPluginManagerStream(NPP npp, NPStream* pstr);
    virtual ~nsPluginManagerStream(void);

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from NPIStream:

    // (Corresponds to NPP_WriteReady.)
    NS_IMETHOD_(PRInt32)
    WriteReady(void);

    // (Corresponds to NPP_Write and NPN_Write.)
    NS_IMETHOD_(PRInt32)
    Write(PRInt32 len, void* buffer);

    ////////////////////////////////////////////////////////////////////////////
    // from NPIPluginManagerStream:

    // (Corresponds to NPStream's url field.)
    NS_IMETHOD_(const char*)
    GetURL(void);

    // (Corresponds to NPStream's end field.)
    NS_IMETHOD_(PRUint32)
    GetEnd(void);

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD_(PRUint32)
    GetLastModified(void);

    // (Corresponds to NPStream's notifyData field.)
    NS_IMETHOD_(void*)
    GetNotifyData(void);

protected:
    NPP npp;
    NPStream* pstream;

};

////////////////////////////////////////////////////////////////////////////////

class nsPluginStreamPeer : public NPISeekablePluginStreamPeer {
public:
    
    nsPluginStreamPeer(URL_Struct *urls, np_stream *stream);
    virtual ~nsPluginStreamPeer(void);

    NS_DECL_ISUPPORTS
    
    NPIPluginStream* GetUserStream(void) {
        return userStream;
    }

    void SetUserStream(NPIPluginStream* str) {
        userStream = str;
    }

    void SetReason(NPPluginReason r) {
        reason = r;
    }

    ////////////////////////////////////////////////////////////////////////////
    // from NPIPluginStreamPeer:

    // (Corresponds to NPP_DestroyStream's reason argument.)
    NS_IMETHOD_(NPPluginReason)
    GetReason(void);

    // (Corresponds to NPP_NewStream's MIMEType argument.)
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void);

    NS_IMETHOD_(PRUint32)
    GetContentLength(void);
#if 0
    NS_IMETHOD_(const char*)
    GetContentEncoding(void);

    NS_IMETHOD_(const char*)
    GetCharSet(void);

    NS_IMETHOD_(const char*)
    GetBoundary(void);

    NS_IMETHOD_(const char*)
    GetContentName(void);

    NS_IMETHOD_(time_t)
    GetExpires(void);

    NS_IMETHOD_(time_t)
    GetLastModified(void);

    NS_IMETHOD_(time_t)
    GetServerDate(void);

    NS_IMETHOD_(NPServerStatus)
    GetServerStatus(void);
#endif
    NS_IMETHOD_(PRUint32)
    GetHeaderFieldCount(void);

    NS_IMETHOD_(const char*)
    GetHeaderFieldKey(PRUint32 index);

    NS_IMETHOD_(const char*)
    GetHeaderField(PRUint32 index);

    ////////////////////////////////////////////////////////////////////////////
    // from NPISeekablePluginStreamPeer:

    // (Corresponds to NPN_RequestRead.)
    NS_IMETHOD_(NPPluginError)
    RequestRead(nsByteRange* rangeList);

protected:
    NPIPluginStream* userStream;
    URL_Struct *urls;
    np_stream *stream;
    NPPluginReason reason;

};

////////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* npglue_h__ */
