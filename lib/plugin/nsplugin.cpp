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

////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).
//
// This superscedes the old plugin API (npapi.h, npupp.h), and 
// eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp. 
// Correspondences to the old API are shown throughout the file.
////////////////////////////////////////////////////////////////////////////////

#include "npglue.h" 
#ifdef JAVA
#include "jvmmgr.h" 
#endif
#include "xp_mem.h"
#include "xpassert.h" 
#ifdef XP_MAC
#include "MacMemAllocator.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// THINGS IMPLEMENTED BY THE BROWSER...
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Interface
// This interface defines the minimum set of functionality that a plugin
// manager will support if it implements plugins. Plugin implementations can
// QueryInterface to determine if a plugin manager implements more specific 
// APIs for the plugin to use.

nsPluginManager* thePluginManager = NULL;

nsPluginManager::nsPluginManager(nsISupports* outer)
    : fJVMMgr(NULL)
{
    NS_INIT_AGGREGATED(outer);
}

nsPluginManager::~nsPluginManager(void)
{
    fJVMMgr->Release();
    fJVMMgr = NULL;
}

NS_IMPL_AGGREGATED(nsPluginManager);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_METHOD
nsPluginManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsPluginManager* mgr = new nsPluginManager(outer);
    nsresult result = mgr->QueryInterface(aIID, aInstancePtr);
    if (result != NS_OK) {
        delete mgr;
    }
    return result;
}

NS_METHOD_(void)
nsPluginManager::ReloadPlugins(PRBool reloadPages)
{
    npn_reloadplugins(reloadPages);
}

NS_METHOD_(void*)
nsPluginManager::MemAlloc(PRUint32 size)
{
    return XP_ALLOC(size);
}

NS_METHOD_(void)
nsPluginManager::MemFree(void* ptr)
{
    (void)XP_FREE(ptr);
}

NS_METHOD_(PRUint32)
nsPluginManager::MemFlush(PRUint32 size)
{
#ifdef XP_MAC
    /* Try to free some memory and return the amount we freed. */
    if (CallCacheFlushers(size))
        return size;
#endif
    return 0;
}

static NS_DEFINE_IID(kPluginManagerIID, NP_IPLUGINMANAGER_IID);
//static NS_DEFINE_IID(kNPIJVMPluginManagerIID, NP_IJVMPLUGINMANAGER_IID);

NS_METHOD
nsPluginManager::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    static NS_DEFINE_IID(kJRIEnvIID, NP_IJRIENV_IID); 
    if (aIID.Equals(kJRIEnvIID)) {
        // XXX Need to implement ISupports for JRIEnv
        *aInstancePtr = (void*) ((nsISupports*)npn_getJavaEnv()); 
//        AddRef();     // XXX should the plugin instance peer and the env be linked?
        return NS_OK; 
    } 
#if 0   // later
    static NS_DEFINE_IID(kJNIEnvIID, NP_IJNIENV_IID); 
    if (aIID.Equals(kJNIEnvIID)) {
        // XXX Need to implement ISupports for JNIEnv
        *aInstancePtr = (void*) ((nsISupports*)npn_getJavaEnv());       // XXX need JNI version
//        AddRef();     // XXX should the plugin instance peer and the env be linked?
        return NS_OK; 
    }
#endif 
    static NS_DEFINE_IID(kClassIID, kPluginManagerIID); 
    if (aIID.Equals(kClassIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this); 
        AddRef(); 
        return NS_OK; 
    } 
    // Aggregates...
    NPIJVMPluginManager* jvmMgr = GetJVMMgr(aIID);
    if (jvmMgr) {
        *aInstancePtr = (void*) ((nsISupports*)jvmMgr);
        return NS_OK; 
    }
    return NS_NOINTERFACE;
}

NPIJVMPluginManager*
nsPluginManager::GetJVMMgr(const nsIID& aIID)
{
    NPIJVMPluginManager* result = NULL;
#ifdef JAVA
    if (fJVMMgr == NULL) {
        // The plugin manager is the outer of the JVM manager
        if (JVMMgr::Create(this, kISupportsIID, (void**)&fJVMMgr) != NS_OK)
            return NULL;
    }
    if (fJVMMgr->QueryInterface(aIID, (void**)&result) != NS_OK)
        return NULL;
#endif
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Peer Interface

nsPluginInstancePeer::nsPluginInstancePeer(NPP npp)
    : npp(npp), userInst(NULL), fJVMInstancePeer(NULL)
{
    NS_INIT_AGGREGATED(NULL);
}

nsPluginInstancePeer::~nsPluginInstancePeer(void)
{
}

NS_IMPL_AGGREGATED(nsPluginInstancePeer);

static NS_DEFINE_IID(kLiveConnectPluginInstancePeerIID, NP_ILIVECONNECTPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kPluginInstancePeerIID, NP_IPLUGININSTANCEPEER_IID); 

NS_METHOD
nsPluginInstancePeer::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kLiveConnectPluginInstancePeerIID) ||
        aIID.Equals(kPluginInstancePeerIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this); 
        AddRef(); 
        return NS_OK; 
    } 
#ifdef JAVA
    // Aggregates...
    if (fJVMInstancePeer == NULL) {
        np_instance* instance = (np_instance*) npp->ndata;
        NPEmbeddedApp* app = instance->app;
        MWContext* cx = instance->cx;
        np_data* ndata = (np_data*) app->np_data;
        nsresult result =
            JVMInstancePeer::Create((nsISupports*)this, kISupportsIID, (void**)&fJVMInstancePeer,
                                    cx, (struct LO_JavaAppStruct_struct*)ndata->lo_struct); // XXX wrong kind of LO_Struct!
        if (result != NS_OK) return result;
    }
#endif
    return fJVMInstancePeer->QueryInterface(aIID, aInstancePtr);
}

NS_METHOD_(NPIPlugin*)
nsPluginInstancePeer::GetClass(void)
{
    return NULL;        // XXX
}

NS_METHOD_(nsMIMEType)
nsPluginInstancePeer::GetMIMEType(void)
{
    np_instance* instance = (np_instance*)npp->ndata;
    return instance->typeString;
}

NS_METHOD_(NPPluginType)
nsPluginInstancePeer::GetMode(void)
{
    np_instance* instance = (np_instance*)npp->ndata;
    return (NPPluginType)instance->type;
}

NS_METHOD_(PRUint16)
nsPluginInstancePeer::GetArgCount(void)
{
    np_instance* instance = (np_instance*)npp->ndata;
    if (instance->type == NP_EMBED) {
        np_data* ndata = (np_data*)instance->app->np_data;
        return (PRUint16)ndata->lo_struct->attribute_cnt;
    }
    else {
        return 1;
    }
}

NS_METHOD_(const char**)
nsPluginInstancePeer::GetArgNames(void)
{
    np_instance* instance = (np_instance*)npp->ndata;
    if (instance->type == NP_EMBED) {
        np_data* ndata = (np_data*)instance->app->np_data;
        return (const char**)ndata->lo_struct->attribute_list;
    }
    else {
        static char name[] = "PALETTE";
        static char* names[1];
        names[0] = name;
        return (const char**)names;
    }
}

NS_METHOD_(const char**)
nsPluginInstancePeer::GetArgValues(void)
{
    np_instance* instance = (np_instance*)npp->ndata;
    if (instance->type == NP_EMBED) {
        np_data* ndata = (np_data*)instance->app->np_data;
        return (const char**)ndata->lo_struct->value_list;
    }
    else {
        static char value[] = "foreground";
        static char* values[1];
        values[0] = value;
        return (const char**)values;
    }
}

NS_METHOD_(NPIPluginManager*)
nsPluginInstancePeer::GetPluginManager(void)
{
    return thePluginManager;
}

NS_METHOD_(NPPluginError)
nsPluginInstancePeer::GetURL(const char* url, const char* target, void* notifyData,
                             const char* altHost, const char* referrer,
                             PRBool forceJSEnabled)
{
    return (NPPluginError)np_geturlinternal(npp, url, target, altHost, referrer,
                                            forceJSEnabled, notifyData != NULL, notifyData);
}

NS_METHOD_(NPPluginError)
nsPluginInstancePeer::PostURL(const char* url, const char* target, PRUint32 bufLen, 
                              const char* buf, PRBool file, void* notifyData,
                              const char* altHost, const char* referrer,
                              PRBool forceJSEnabled,
                              PRUint32 postHeadersLength, const char* postHeaders)
{
    return (NPPluginError)np_posturlinternal(npp, url, target, altHost, referrer, forceJSEnabled,
                                             bufLen, buf, file, notifyData != NULL, notifyData);
}

NS_METHOD_(NPPluginError)
nsPluginInstancePeer::NewStream(nsMIMEType type, const char* target,
                                NPIPluginManagerStream* *result)
{
    NPStream* pstream;
    NPPluginError err = (NPPluginError)
        npn_newstream(npp, (char*)type, (char*)target, &pstream);
    if (err != NPPluginError_NoError)
        return err;
    *result = new nsPluginManagerStream(npp, pstream);
    return NPPluginError_NoError;
}

NS_METHOD_(void)
nsPluginInstancePeer::ShowStatus(const char* message)
{
    npn_status(npp, message);
}

NS_METHOD_(const char*)
nsPluginInstancePeer::UserAgent(void)
{
    return npn_useragent(npp);
}

NS_METHOD_(NPPluginError)
nsPluginInstancePeer::GetValue(NPPluginManagerVariable variable, void *value)
{
    return (NPPluginError)npn_getvalue(npp, (NPNVariable)variable, value);
}

NS_METHOD_(NPPluginError)
nsPluginInstancePeer::SetValue(NPPluginVariable variable, void *value)
{
    return (NPPluginError)npn_setvalue(npp, (NPPVariable)variable, value);
}

NS_METHOD_(void)
nsPluginInstancePeer::InvalidateRect(nsRect *invalidRect)
{
    npn_invalidaterect(npp, (NPRect*)invalidRect);
}

NS_METHOD_(void)
nsPluginInstancePeer::InvalidateRegion(nsRegion invalidRegion)
{
    npn_invalidateregion(npp, invalidRegion);
}

NS_METHOD_(void)
nsPluginInstancePeer::ForceRedraw(void)
{
    npn_forceredraw(npp);
}

NS_METHOD_(void)
nsPluginInstancePeer::RegisterWindow(void* window)
{
	npn_registerwindow(npp, window);
}

NS_METHOD_(void)
nsPluginInstancePeer::UnregisterWindow(void* window)
{
	npn_unregisterwindow(npp, window);
}

NS_METHOD_(jref)
nsPluginInstancePeer::GetJavaPeer(void)
{
    return npn_getJavaPeer(npp);
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Stream Interface

nsPluginManagerStream::nsPluginManagerStream(NPP npp, NPStream* pstr)
    : npp(npp), pstream(pstr)
{
    NS_INIT_REFCNT();
    // XXX get rid of the npp instance variable if this is true
    PR_ASSERT(npp == ((np_stream*)pstr->ndata)->instance->npp);
}

nsPluginManagerStream::~nsPluginManagerStream(void)
{
}

NS_METHOD_(PRInt32)
nsPluginManagerStream::WriteReady(void)
{
    // XXX This call didn't exist before, but I think that it should have.
    // Is this implementation right?
    return NP_STREAM_MAXREADY;
}

NS_METHOD_(PRInt32)
nsPluginManagerStream::Write(PRInt32 len, void* buffer)
{
    return npn_write(npp, pstream, len, buffer);
}

NS_METHOD_(const char*)
nsPluginManagerStream::GetURL(void)
{
    return pstream->url;
}

NS_METHOD_(PRUint32)
nsPluginManagerStream::GetEnd(void)
{
    return pstream->end;
}

NS_METHOD_(PRUint32)
nsPluginManagerStream::GetLastModified(void)
{
    return pstream->lastmodified;
}

NS_METHOD_(void*)
nsPluginManagerStream::GetNotifyData(void)
{
    return pstream->notifyData;
}

NS_DEFINE_IID(kPluginManagerStreamIID, NP_IPLUGINMANAGERSTREAM_IID);
NS_IMPL_QUERY_INTERFACE(nsPluginManagerStream, kPluginManagerStreamIID);
NS_IMPL_ADDREF(nsPluginManagerStream);
NS_IMPL_RELEASE(nsPluginManagerStream);

////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Peer Interface

nsPluginStreamPeer::nsPluginStreamPeer(URL_Struct *urls, np_stream *stream)
    : userStream(NULL), urls(urls), stream(stream), 
      reason(NPPluginReason_NoReason)
{
    NS_INIT_REFCNT();
}

nsPluginStreamPeer::~nsPluginStreamPeer(void)
{
}

NS_METHOD_(NPPluginReason)
nsPluginStreamPeer::GetReason(void)
{
    return reason;
}

NS_METHOD_(nsMIMEType)
nsPluginStreamPeer::GetMIMEType(void)
{
    return (nsMIMEType)urls->content_type;
}

NS_METHOD_(PRUint32)
nsPluginStreamPeer::GetContentLength(void)
{
    return urls->content_length;
}
#if 0
NS_METHOD_(const char*)
nsPluginStreamPeer::GetContentEncoding(void)
{
    return urls->content_encoding;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetCharSet(void)
{
    return urls->charset;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetBoundary(void)
{
    return urls->boundary;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetContentName(void)
{
    return urls->content_name;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetExpires(void)
{
    return urls->expires;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetLastModified(void)
{
    return urls->last_modified;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetServerDate(void)
{
    return urls->server_date;
}

NS_METHOD_(NPServerStatus)
nsPluginStreamPeer::GetServerStatus(void)
{
    return urls->server_status;
}
#endif
NS_METHOD_(PRUint32)
nsPluginStreamPeer::GetHeaderFieldCount(void)
{
    return urls->all_headers.empty_index;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetHeaderFieldKey(PRUint32 index)
{
    return urls->all_headers.key[index];
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetHeaderField(PRUint32 index)
{
    return urls->all_headers.value[index];
}

NS_METHOD_(NPPluginError)
nsPluginStreamPeer::RequestRead(nsByteRange* rangeList)
{
    return (NPPluginError)npn_requestread(stream->pstream,
                                          (NPByteRange*)rangeList);
}

NS_DEFINE_IID(kSeekablePluginStreamPeerIID, NP_ISEEKABLEPLUGINSTREAMPEER_IID);
NS_DEFINE_IID(kPluginStreamPeerIID, NP_IPLUGINSTREAMPEER_IID);
NS_IMPL_ADDREF(nsPluginStreamPeer);
NS_IMPL_RELEASE(nsPluginStreamPeer);

NS_METHOD
nsPluginStreamPeer::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if ((stream->seekable && aIID.Equals(kSeekablePluginStreamPeerIID)) ||
        aIID.Equals(kPluginStreamPeerIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this); 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
} 

////////////////////////////////////////////////////////////////////////////////
