/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
// LIVECONNECT SUBCLASSES
//
// This superscedes the old plugin API (npapi.h, npupp.h), and 
// eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp. 
// Correspondences to the old API are shown throughout the file.
////////////////////////////////////////////////////////////////////////////////

#ifndef nsILCPlg_h___
#define nsILCPlg_h___

#include "nsIPlug.h"    // base Plugin interfaces

//##############################################################################
// JNI-Based LiveConnect Classes
//##############################################################################

#include "jni.h"        // standard JVM API

////////////////////////////////////////////////////////////////////////////////
// LiveConnect Plugin Interface
// This interface defines additional entry points that a plugin developer needs
// to implement in order for the plugin to support JNI-based LiveConnect,
// i.e. be scriptable by Java or JavaScript.

class NPILiveConnectPlugin : public NPIPlugin {
public:

    // (New JNI-based entry point, roughly corresponds to NPP_GetJavaClass.)
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
// LiveConnect Plugin Instance Peer Interface
// Browsers that support JNI-based LiveConnect implement this subclass of
// plugin instance peer. 

class NPILiveConnectPluginInstancePeer : public NPIPluginInstancePeer {
public:

    // (New JNI-based entry point, roughly corresponds to NPN_GetJavaPeer.)
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

// QueryInterface for this IID on NPIPluginManager to get a JNIEnv
// XXX change this
#define NP_IJNIENV_IID                               \
{ /* 04610650-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x04610650,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

//##############################################################################
// JRI-Based LiveConnect Classes
//##############################################################################
//
// This stuff is here so that the browser can support older JRI-based
// LiveConnected plugins (when using old plugin to new C++-style plugin
// adapter code). 
//
// Warning: Don't use this anymore, unless you're sure that you have to!
////////////////////////////////////////////////////////////////////////////////

#include "jri.h"        // ancient

////////////////////////////////////////////////////////////////////////////////
// JRILiveConnect Plugin Interface
// This interface defines additional entry points that a plugin developer needs
// to implement in order for the plugin to support JRI-based LiveConnect,
// i.e. be scriptable by Java or JavaScript.

class NPIJRILiveConnectPlugin : public NPIPlugin {
public:

    // (Corresponds to NPP_GetJavaClass.)
    NS_IMETHOD_(jref)
    GetJavaClass(void) = 0;

};

#define NP_IJRILIVECONNECTPLUGIN_IID                 \
{ /* c94058e0-f772-11d1-815b-006008119d7a */         \
    0xc94058e0,                                      \
    0xf772,                                          \
    0x11d1,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////
// JRILiveConnect Plugin Instance Peer Interface
// Browsers that support JRI-based LiveConnect implement this subclass of
// plugin instance peer. 

class NPIJRILiveConnectPluginInstancePeer : public NPIPluginInstancePeer {
public:

    // (Corresponds to NPN_GetJavaPeer.)
    NS_IMETHOD_(jref)
    GetJavaPeer(void) = 0;

};

#define NP_IJRILIVECONNECTPLUGININSTANCEPEER_IID     \
{ /* 25b63f40-f773-11d1-815b-006008119d7a */         \
    0x25b63f40,                                      \
    0xf773,                                          \
    0x11d1,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

// QueryInterface for this IID on NPIPluginManager to get a JRIEnv
// XXX change this
#define NP_IJRIENV_IID                               \
{ /* f9d4ea00-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xf9d4ea00,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsILCPlg_h___ */
