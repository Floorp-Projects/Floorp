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
// INTERFACE TO NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).
//
// This superscedes the old plugin API (npapi.h, npupp.h), and 
// eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp. 
// Correspondences to the old API are shown throughout the file.
////////////////////////////////////////////////////////////////////////////////

// XXX THIS HEADER IS A BETA VERSION OF THE NEW PLUGIN INTERFACE.
// USE ONLY FOR EXPERIMENTAL PURPOSES!

#ifndef nsplugin_h___
#define nsplugin_h___

#ifdef __OS2__
#pragma pack(1)
#endif

// XXX Move this XP_ defining stuff to xpcom or nspr... 

#if defined (__OS2__ ) || defined (OS2)
#	ifndef XP_OS2
#		define XP_OS2 1
#	endif /* XP_OS2 */
#endif /* __OS2__ */

#ifdef _WINDOWS
#	ifndef XP_WIN
#		define XP_WIN 1
#	endif /* XP_WIN */
#endif /* _WINDOWS */

#ifdef __MWERKS__
#	define _declspec __declspec
#	ifdef macintosh
#		ifndef XP_MAC
#			define XP_MAC 1
#		endif /* XP_MAC */
#	endif /* macintosh */
#	ifdef __INTEL__
#		undef NULL
#		ifndef XP_WIN
#			define XP_WIN 1
#		endif /* __INTEL__ */
#	endif /* XP_PC */
#endif /* __MWERKS__ */

#ifdef XP_MAC
	#include <Quickdraw.h>
	#include <Events.h>
#endif

#ifdef XP_UNIX
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
#endif

#include "jri.h"                // XXX change to jni.h
#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////

/* The OS/2 version of Netscape uses RC_DATA to define the
   mime types, file extentions, etc that are required.
   Use a vertical bar to seperate types, end types with \0.
   FileVersion and ProductVersion are 32bit ints, all other
   entries are strings the MUST be terminated wwith a \0.

AN EXAMPLE:

RCDATA NP_INFO_ProductVersion { 1,0,0,1,}

RCDATA NP_INFO_MIMEType    { "video/x-video|",
                             "video/x-flick\0" }
RCDATA NP_INFO_FileExtents { "avi|",
                             "flc\0" }
RCDATA NP_INFO_FileOpenName{ "MMOS2 video player(*.avi)|",
                             "MMOS2 Flc/Fli player(*.flc)\0" }

RCDATA NP_INFO_FileVersion       { 1,0,0,1 }
RCDATA NP_INFO_CompanyName       { "Netscape Communications\0" }
RCDATA NP_INFO_FileDescription   { "NPAVI32 Extension DLL\0"
RCDATA NP_INFO_InternalName      { "NPAVI32\0" )
RCDATA NP_INFO_LegalCopyright    { "Copyright Netscape Communications \251 1996\0"
RCDATA NP_INFO_OriginalFilename  { "NVAPI32.DLL" }
RCDATA NP_INFO_ProductName       { "NPAVI32 Dynamic Link Library\0" }

*/


/* RC_DATA types for version info - required */
#define NP_INFO_ProductVersion      1
#define NP_INFO_MIMEType            2
#define NP_INFO_FileOpenName        3
#define NP_INFO_FileExtents         4

/* RC_DATA types for version info - used if found */
#define NP_INFO_FileDescription     5
#define NP_INFO_ProductName         6

/* RC_DATA types for version info - optional */
#define NP_INFO_CompanyName         7
#define NP_INFO_FileVersion         8
#define NP_INFO_InternalName        9
#define NP_INFO_LegalCopyright      10
#define NP_INFO_OriginalFilename    11

#ifndef RC_INVOKED

////////////////////////////////////////////////////////////////////////////////
// Structures and definitions

#ifdef XP_MAC
#pragma options align=mac68k
#endif

typedef const char*     nsMIMEType;

struct nsByteRange {
    PRInt32             offset; 	/* negative offset means from the end */
    PRUint32            length;
    struct nsByteRange* next;
};

struct nsRect {
    PRUint16            top;
    PRUint16            left;
    PRUint16            bottom;
    PRUint16            right;
};

////////////////////////////////////////////////////////////////////////////////
// Unix specific structures and definitions

#ifdef XP_UNIX

/*
 * Callback Structures.
 *
 * These are used to pass additional platform specific information.
 */
enum NPPluginCallbackType {
    NPPluginCallbackType_SetWindow = 1,
    NPPluginCallbackType_Print
};

struct NPPluginAnyCallbackStruct {
    PRInt32     type;
};

struct NPPluginSetWindowCallbackStruct {
    PRInt32     type;
    Display*    display;
    Visual*     visual;
    Colormap    colormap;
    PRUint32    depth;
};

struct NPPluginPrintCallbackStruct {
    PRInt32     type;
    FILE*       fp;
};

#endif /* XP_UNIX */

////////////////////////////////////////////////////////////////////////////////

// List of variable names for which NPP_GetValue shall be implemented
enum NPPluginVariable {
    NPPluginVariable_NameString = 1,
    NPPluginVariable_DescriptionString,
    NPPluginVariable_WindowBool,        // XXX go away
    NPPluginVariable_TransparentBool,   // XXX go away?
    NPPluginVariable_JavaClass,         // XXX go away
    NPPluginVariable_WindowSize
    // XXX add MIMEDescription (for unix) (but GetValue is on the instance, not the class)
};

// List of variable names for which NPN_GetValue is implemented by Mozilla
enum NPPluginManagerVariable {
    NPPluginManagerVariable_XDisplay = 1,
    NPPluginManagerVariable_XtAppContext,
    NPPluginManagerVariable_NetscapeWindow,
    NPPluginManagerVariable_JavascriptEnabledBool,      // XXX prefs accessor api
    NPPluginManagerVariable_ASDEnabledBool,             // XXX prefs accessor api
    NPPluginManagerVariable_IsOfflineBool               // XXX prefs accessor api
};

////////////////////////////////////////////////////////////////////////////////

enum NPPluginType {
    NPPluginType_Embedded = 1,
    NPPluginType_Full
};

// XXX this can go away now
enum NPStreamType {
    NPStreamType_Normal = 1,
    NPStreamType_Seek,
    NPStreamType_AsFile,
    NPStreamType_AsFileOnly
};

#define NP_STREAM_MAXREADY	(((unsigned)(~0)<<1)>>1)

/*
 * The type of a NPWindow - it specifies the type of the data structure
 * returned in the window field.
 */
enum NPPluginWindowType {
    NPPluginWindowType_Window = 1,
    NPPluginWindowType_Drawable
};

struct NPPluginWindow {
    void*       window;         /* Platform specific window handle */
                                /* OS/2: x - Position of bottom left corner  */
                                /* OS/2: y - relative to visible netscape window */
    PRUint32    x;              /* Position of top left corner relative */
    PRUint32    y;              /*	to a netscape page.					*/
    PRUint32    width;          /* Maximum window size */
    PRUint32    height;
    nsRect      clipRect;       /* Clipping rectangle in port coordinates */
                                /* Used by MAC only.			  */
#ifdef XP_UNIX
    void*       ws_info;        /* Platform-dependent additonal data */
#endif /* XP_UNIX */
    NPPluginWindowType type;    /* Is this a window or a drawable? */
};

struct NPPluginFullPrint {
    PRBool      pluginPrinted;	/* Set TRUE if plugin handled fullscreen */
                                /*	printing							 */
    PRBool      printOne;       /* TRUE if plugin should print one copy  */
                                /*	to default printer					 */
    void*       platformPrint;  /* Platform-specific printing info */
};

struct NPPluginEmbedPrint {
    NPPluginWindow    window;
    void*       platformPrint;	/* Platform-specific printing info */
};

struct NPPluginPrint {
    NPPluginType      mode;     /* NP_FULL or NPPluginType_Embedded */
    union
    {
        NPPluginFullPrint     fullPrint;	/* if mode is NP_FULL */
        NPPluginEmbedPrint    embedPrint;	/* if mode is NPPluginType_Embedded */
    } print;
};

struct NPPluginEvent {

#if defined(XP_MAC)
    EventRecord* event;
    void*       window;

#elif defined(XP_WIN)
    uint16      event;
    uint32      wParam;
    uint32      lParam;

#elif defined(XP_OS2)
    uint32      event;
    uint32      wParam;
    uint32      lParam;

#elif defined(XP_UNIX)
    XEvent      event;

#endif
};

#ifdef XP_MAC
typedef RgnHandle nsRegion;
#elif defined(XP_WIN)
typedef HRGN nsRegion;
#elif defined(XP_UNIX)
typedef Region nsRegion;
#else
typedef void *nsRegion;
#endif /* XP_MAC */

////////////////////////////////////////////////////////////////////////////////
// Mac-specific structures and definitions.

#ifdef XP_MAC

struct NPPort {
    CGrafPtr    port;   /* Grafport */
    PRInt32     portx;  /* position inside the topmost window */
    PRInt32     porty;
};

/*
 *  Non-standard event types that can be passed to HandleEvent
 */
#define getFocusEvent           (osEvt + 16)
#define loseFocusEvent          (osEvt + 17)
#define adjustCursorEvent       (osEvt + 18)

#endif /* XP_MAC */

////////////////////////////////////////////////////////////////////////////////
// Error and Reason Code definitions

enum NPPluginError {
    NPPluginError_Base = 0,
    NPPluginError_NoError = 0,
    NPPluginError_GenericError,
    NPPluginError_InvalidInstanceError,
    NPPluginError_InvalidFunctableError,
    NPPluginError_ModuleLoadFailedError,
    NPPluginError_OutOfMemoryError,
    NPPluginError_InvalidPluginError,
    NPPluginError_InvalidPluginDirError,
    NPPluginError_IncompatibleVersionError,
    NPPluginError_InvalidParam,
    NPPluginError_InvalidUrl,
    NPPluginError_FileNotFound,
    NPPluginError_NoData,
    NPPluginError_StreamNotSeekable
};

enum NPPluginReason {
    NPPluginReason_Base = 0,
    NPPluginReason_Done = 0,
    NPPluginReason_NetworkErr,
    NPPluginReason_UserBreak,
    NPPluginReason_NoReason
};

////////////////////////////////////////////////////////////////////////////////
// Classes
////////////////////////////////////////////////////////////////////////////////

class NPIStream;                        // base class for all streams

// Classes that must be implemented by the plugin DLL:
class NPIPlugin;                        // plugin class (MIME-type handler)
class NPILiveConnectPlugin;             // subclass of NPIPlugin
class NPIPluginInstance;                // plugin instance
class NPIPluginStream;                  // stream to receive data from the browser

// Classes that are implemented by the browser:
class NPIPluginManager;                 // minimum browser requirements
class NPIPluginManagerStream;           // stream to send data to the browser
class NPIPluginInstancePeer;            // parts of NPIPluginInstance implemented by the browser
class NPILiveConnectPluginInstancePeer; // subclass of NPIPluginInstancePeer
class NPIPluginStreamPeer;              // parts of NPIPluginStream implemented by the browser
class NPISeekablePluginStreamPeer;      // seekable subclass of NPIPluginStreamPeer

//       Plugin DLL Side                Browser Side
//
//         
//       +-----------+                 +-----------------------+
//       | Plugin /  |                 | Plugin Manager        |
//       | LC Plugin |                 |                       |
//       +-----------+                 +-----------------------+
//            ^                               ^
//            |                               |
//            |                        +-----------------------+
//            |                        | Plugin Manager Stream |
//            |                        +-----------------------+
//            |
//            |
//       +-----------------+   peer    +-----------------------+
//       | Plugin Instance |---------->| Plugin Instance Peer  |
//       |                 |           | / LC Plugin Inst Peer |
//       +-----------------+           +-----------------------+
//
//       +-----------------+   peer    +-----------------------+
//       | Plugin Stream   |---------->| Plugin Stream Peer /  |
//       |                 |           | Seekable P Stream Peer|
//       +-----------------+           +-----------------------+

////////////////////////////////////////////////////////////////////////////////
// This is the main entry point to the plugin's DLL. The plugin manager finds
// this symbol and calls it to create the plugin class. Once the plugin object
// is returned to the plugin manager, instances on the page are created by 
// calling NPIPlugin::NewInstance.

// (Corresponds to NPP_Initialize.)
extern "C" NS_EXPORT NPPluginError
NP_CreatePlugin(NPIPluginManager* mgr, NPIPlugin* *result);

////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Interface
// This base class is shared by both the plugin and the plugin manager.

class NPIStream : public nsISupports {
public:

    // The Release method on NPIPlugin corresponds to NPP_DestroyStream.
    
    // (Corresponds to NPP_WriteReady.)
    NS_IMETHOD_(PRInt32)
    WriteReady(void) = 0;

    // (Corresponds to NPP_Write and NPN_Write.)
    NS_IMETHOD_(PRInt32)
    Write(PRInt32 len, void* buffer) = 0;

};

#define NP_ISTREAM_IID                               \
{ /* 5d852ef0-a1bc-11d1-85b1-00805f0e4dfe */         \
    0x5d852ef0,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// THINGS THAT MUST BE IMPLEMENTED BY THE PLUGIN...
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Plugin Interface
// This is the minimum interface plugin developers need to support in order to
// implement a plugin. The plugin manager may QueryInterface for more specific 
// plugin types, e.g. NPILiveConnectPlugin.

class NPIPlugin : public nsISupports {
public:

    // The Release method on NPIPlugin corresponds to NPP_Shutdown.

    // The old NPP_New call has been factored into two plugin instance methods:
    //
    // NewInstance -- called once, after the plugin instance is created. This 
    // method is used to initialize the new plugin instance (although the actual
    // plugin instance object will be created by the plugin manager).
    //
    // NPIPluginInstance::Start -- called when the plugin instance is to be
    // started. This happens in two circumstances: (1) after the plugin instance
    // is first initialized, and (2) after a plugin instance is returned to
    // (e.g. by going back in the window history) after previously being stopped
    // by the Stop method. 

    NS_IMETHOD_(NPPluginError)
    NewInstance(NPIPluginInstancePeer* peer, NPIPluginInstance* *result) = 0;

#ifdef XP_UNIX  // XXX why can't this be XP?

    // (Corresponds to NPP_GetMIMEDescription.)
    NS_IMETHOD_(const char*)
    GetMIMEDescription(void) = 0;

#endif /* XP_UNIX */

};

#define NP_IPLUGIN_IID                               \
{ /* 8a623430-a1bc-11d1-85b1-00805f0e4dfe */         \
    0x8a623430,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// LiveConnect Plugin Interface
// This interface defines additional entry points that a plugin developer needs
// to implement in order for the plugin to support LiveConnect, i.e. be 
// scriptable by Java or JavaScript.

class NPILiveConnectPlugin : public NPIPlugin {
public:

    // (Corresponds to NPP_GetJavaClass.)
    NS_IMETHOD_(jclass)
    GetJavaClass(void) = 0;

};

#define NP_ILIVECONNECTPLUGIN_IID                    \
{ /* cf134df0-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xcf134df0,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Interface

// (Corresponds to NPP object.)
class NPIPluginInstance : public nsISupports {
public:

    // The Release method on NPIPluginInstance corresponds to NPP_Destroy.

    // See comment for NPIPlugin::NewInstance, above.
    NS_IMETHOD_(NPPluginError)
    Start(void) = 0;

    // The old NPP_Destroy call has been factored into two plugin instance 
    // methods:
    //
    // Stop -- called when the plugin instance is to be stopped (e.g. by 
    // displaying another plugin manager window, causing the page containing 
    // the plugin to become removed from the display).
    //
    // Release -- called once, before the plugin instance peer is to be 
    // destroyed. This method is used to destroy the plugin instance.

    NS_IMETHOD_(NPPluginError)
    Stop(void) = 0;

    // (Corresponds to NPP_SetWindow.)
    NS_IMETHOD_(NPPluginError)
    SetWindow(NPPluginWindow* window) = 0;

    // (Corresponds to NPP_NewStream.)
    NS_IMETHOD_(NPPluginError)
    NewStream(NPIPluginStreamPeer* peer, NPIPluginStream* *result) = 0;

    // (Corresponds to NPP_Print.)
    NS_IMETHOD_(void)
    Print(NPPluginPrint* platformPrint) = 0;

    // (Corresponds to NPP_HandleEvent.)
    // Note that for Unix and Mac the NPPluginEvent structure is different
    // from the old NPEvent structure -- it's no longer the native event
    // record, but is instead a struct. This was done for future extensibility,
    // and so that the Mac could receive the window argument too. For Windows
    // and OS2, it's always been a struct, so there's no change for them.
    NS_IMETHOD_(PRInt16)
    HandleEvent(NPPluginEvent* event) = 0;

    // (Corresponds to NPP_URLNotify.)
    NS_IMETHOD_(void)
    URLNotify(const char* url, const char* target,
              NPPluginReason reason, void* notifyData) = 0;

    // (Corresponds to NPP_GetValue.)
    NS_IMETHOD_(NPPluginError)
    GetValue(NPPluginVariable variable, void *value) = 0;

    // (Corresponds to NPP_SetValue.)
    NS_IMETHOD_(NPPluginError)
    SetValue(NPPluginManagerVariable variable, void *value) = 0;

};

#define NP_IPLUGININSTANCE_IID                       \
{ /* b62f3a10-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xb62f3a10,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Interface

class NPIPluginStream : public NPIStream {
public:

    // (Corresponds to NPP_NewStream's stype return parameter.)
    NS_IMETHOD_(NPStreamType)
    GetStreamType(void) = 0;

    // (Corresponds to NPP_StreamAsFile.)
    NS_IMETHOD_(void)
    AsFile(const char* fname) = 0;

};

#define NP_IPLUGINSTREAM_IID                         \
{ /* e7a97340-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xe7a97340,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// THINGS IMPLEMENTED BY THE BROWSER...
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Interface
// This interface defines the minimum set of functionality that a plugin
// manager will support if it implements plugins. Plugin implementations can
// QueryInterface to determine if a plugin manager implements more specific 
// APIs for the plugin to use.

class NPIPluginManager : public nsISupports {
public:

    // QueryInterface may be used to obtain a JRIEnv or JNIEnv
    // from an NPIPluginManager.
    // (Corresponds to NPN_GetJavaEnv.)

    // (Corresponds to NPN_ReloadPlugins.)
    NS_IMETHOD_(void)
    ReloadPlugins(PRBool reloadPages) = 0;

    // (Corresponds to NPN_MemAlloc.)
    NS_IMETHOD_(void*)
    MemAlloc(PRUint32 size) = 0;

    // (Corresponds to NPN_MemFree.)
    NS_IMETHOD_(void)
    MemFree(void* ptr) = 0;

    // (Corresponds to NPN_MemFlush.)
    NS_IMETHOD_(PRUint32)
    MemFlush(PRUint32 size) = 0;

};

#define NP_IPLUGINMANAGER_IID                        \
{ /* f10b9600-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xf10b9600,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

#define NP_IJRIENV_IID                               \
{ /* f9d4ea00-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xf9d4ea00,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

#define NP_IJNIENV_IID                               \
{ /* 04610650-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x04610650,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Peer Interface

class NPIPluginInstancePeer : public nsISupports {
public:

    NS_IMETHOD_(NPIPlugin*)
    GetClass(void) = 0;

    // (Corresponds to NPP_New's MIMEType argument.)
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void) = 0;

    // (Corresponds to NPP_New's mode argument.)
    NS_IMETHOD_(NPPluginType)
    GetMode(void) = 0;

    // (Corresponds to NPP_New's argc argument.)
    NS_IMETHOD_(PRUint16)
    GetArgCount(void) = 0;

    // (Corresponds to NPP_New's argn argument.)
    NS_IMETHOD_(const char**)
    GetArgNames(void) = 0;

    // (Corresponds to NPP_New's argv argument.)
    NS_IMETHOD_(const char**)
    GetArgValues(void) = 0;

    NS_IMETHOD_(NPIPluginManager*)
    GetPluginManager(void) = 0;

    // (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    NS_IMETHOD_(NPPluginError)
    GetURL(const char* url, const char* target, void* notifyData = NULL,
           const char* altHost = NULL, const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE) = 0;

    // (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    //   postHeaders: A string containing post headers.
    //   postHeadersLength: The length of the post headers string.
    NS_IMETHOD_(NPPluginError)
    PostURL(const char* url, const char* target, PRUint32 bufLen, 
            const char* buf, PRBool file, void* notifyData = NULL,
            const char* altHost = NULL, const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, const char* postHeaders = NULL) = 0;

    // (Corresponds to NPN_NewStream.)
    NS_IMETHOD_(NPPluginError)
    NewStream(nsMIMEType type, const char* target,
              NPIPluginManagerStream* *result) = 0;

    // (Corresponds to NPN_Status.)
    NS_IMETHOD_(void)
    ShowStatus(const char* message) = 0;

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD_(const char*)
    UserAgent(void) = 0;

    // (Corresponds to NPN_GetValue.)
    NS_IMETHOD_(NPPluginError)
    GetValue(NPPluginManagerVariable variable, void *value) = 0;

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD_(NPPluginError)
    SetValue(NPPluginVariable variable, void *value) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // XXX Only used by windowless plugin instances?...

    // (Corresponds to NPN_InvalidateRect.)
    NS_IMETHOD_(void)
    InvalidateRect(nsRect *invalidRect) = 0;

    // (Corresponds to NPN_InvalidateRegion.)
    NS_IMETHOD_(void)
    InvalidateRegion(nsRegion invalidRegion) = 0;

    // (Corresponds to NPN_ForceRedraw.)
    NS_IMETHOD_(void)
    ForceRedraw(void) = 0;
    
    // New top-level window handling calls for Mac:
    
    NS_IMETHOD_(void)
    RegisterWindow(void* window) = 0;
    
    NS_IMETHOD_(void)
    UnregisterWindow(void* window) = 0;

};

#define NP_IPLUGININSTANCEPEER_IID                   \
{ /* 15c75de0-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x15c75de0,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// LiveConnect Plugin Instance Peer Interface
// Browsers that support LiveConnect implement this subclass of plugin instance
// peer. 

// XXX Should this really be a separate subclass?

class NPILiveConnectPluginInstancePeer : public NPIPluginInstancePeer {
public:

    // (Corresponds to NPN_GetJavaPeer.)
    NS_IMETHOD_(jobject)
    GetJavaPeer(void) = 0;

};

#define NP_ILIVECONNECTPLUGININSTANCEPEER_IID        \
{ /* 1e3502a0-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x1e3502a0,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Stream Interface

class NPIPluginManagerStream : public NPIStream {
public:

    // (Corresponds to NPStream's url field.)
    NS_IMETHOD_(const char*)
    GetURL(void) = 0;

    // (Corresponds to NPStream's end field.)
    NS_IMETHOD_(PRUint32)
    GetEnd(void) = 0;

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD_(PRUint32)
    GetLastModified(void) = 0;

    // (Corresponds to NPStream's notifyData field.)
    NS_IMETHOD_(void*)
    GetNotifyData(void) = 0;

};

#define NP_IPLUGINMANAGERSTREAM_IID                  \
{ /* 30c24560-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x30c24560,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Peer Interface
// A plugin stream peer is passed to a plugin instance's NewStream call in 
// order to indicate that a new stream is to be created and be read by the
// plugin instance.

class NPIPluginStreamPeer : public nsISupports {
public:

    // (Corresponds to NPP_DestroyStream's reason argument.)
    NS_IMETHOD_(NPPluginReason)
    GetReason(void) = 0;

    // (Corresponds to NPP_NewStream's MIMEType argument.)
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void) = 0;

    NS_IMETHOD_(PRUint32)
    GetContentLength(void) = 0;
#if 0
    NS_IMETHOD_(const char*)
    GetContentEncoding(void) = 0;

    NS_IMETHOD_(const char*)
    GetCharSet(void) = 0;

    NS_IMETHOD_(const char*)
    GetBoundary(void) = 0;

    NS_IMETHOD_(const char*)
    GetContentName(void) = 0;

    NS_IMETHOD_(time_t)
    GetExpires(void) = 0;

    NS_IMETHOD_(time_t)
    GetLastModified(void) = 0;

    NS_IMETHOD_(time_t)
    GetServerDate(void) = 0;

    NS_IMETHOD_(NPServerStatus)
    GetServerStatus(void) = 0;
#endif
    NS_IMETHOD_(PRUint32)
    GetHeaderFieldCount(void) = 0;

    NS_IMETHOD_(const char*)
    GetHeaderFieldKey(PRUint32 index) = 0;

    NS_IMETHOD_(const char*)
    GetHeaderField(PRUint32 index) = 0;

};

#define NP_IPLUGINSTREAMPEER_IID                     \
{ /* 38278eb0-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x38278eb0,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Seekable Plugin Stream Peer Interface
// The browser implements this subclass of plugin stream peer if a stream
// is seekable. Plugins can query interface for this type, and call the 
// RequestRead method to seek to a particular position in the stream.

class NPISeekablePluginStreamPeer : public NPIPluginStreamPeer {
public:

    // QueryInterface for this class corresponds to NPP_NewStream's 
    // seekable argument.

    // (Corresponds to NPN_RequestRead.)
    NS_IMETHOD_(NPPluginError)
    RequestRead(nsByteRange* rangeList) = 0;

};

#define NP_ISEEKABLEPLUGINSTREAMPEER_IID             \
{ /* f55c8250-a73e-11d1-85b1-00805f0e4dfe */         \
    0xf55c8250,                                      \
    0xa73e,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}                                                    \

////////////////////////////////////////////////////////////////////////////////

#ifdef XP_MAC
#pragma options align=reset
#endif

#endif /* RC_INVOKED */
#ifdef __OS2__
#pragma pack()
#endif

#endif /* nsplugin_h___ */
