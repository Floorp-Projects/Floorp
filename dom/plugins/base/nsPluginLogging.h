/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Plugin Module Logging usage instructions and includes */
////////////////////////////////////////////////////////////////////////////////
#ifndef nsPluginLogging_h__
#define nsPluginLogging_h__

#define FORCE_PR_LOG /* Allow logging in the release build */
#define PR_LOGGING 1

#ifdef PR_LOGGING
#include "prlog.h"

#ifndef PLUGIN_LOGGING    // allow external override
#define PLUGIN_LOGGING 1  // master compile-time switch for pluging logging
#endif

////////////////////////////////////////////////////////////////////////////////
// Basic Plugin Logging Usage Instructions
//
// 1. Set this environment variable: NSPR_LOG_MODULES=<name>:<level>

// Choose the <name> and <level> from this list (no quotes):

// Log Names            <name>
#define NPN_LOG_NAME    "PluginNPN"
#define NPP_LOG_NAME    "PluginNPP"
#define PLUGIN_LOG_NAME "Plugin"

// Levels                <level>
#define PLUGIN_LOG_ALWAYS 1
#define PLUGIN_LOG_BASIC  3
#define PLUGIN_LOG_NORMAL 5
#define PLUGIN_LOG_NOISY  7
#define PLUGIN_LOG_MAX    9

// 2. You can combine logs and levels by separating them with a comma:
//    My favorite Win32 Example: SET NSPR_LOG_MODULES=Plugin:5,PluginNPP:5,PluginNPN:5

// 3. Instead of output going to the console, you can log to a file. Additionally, set the
//    NSPR_LOG_FILE environment variable to point to the full path of a file.
//    My favorite Win32 Example: SET NSPR_LOG_FILE=c:\temp\pluginLog.txt

// 4. For complete information see the NSPR Reference: 
//    http://www.mozilla.org/projects/nspr/reference/html/prlog.html


#ifdef PLUGIN_LOGGING

class nsPluginLogging
{
public:
  static PRLogModuleInfo* gNPNLog;  // 4.x NP API, calls into navigator
  static PRLogModuleInfo* gNPPLog;  // 4.x NP API, calls into plugin
  static PRLogModuleInfo* gPluginLog;  // general plugin log
};

#endif   // PLUGIN_LOGGING

#endif  // PR_LOGGING

// Quick-use macros
#ifdef PLUGIN_LOGGING
 #define NPN_PLUGIN_LOG(a, b)                              \
   PR_BEGIN_MACRO                                        \
   PR_LOG(nsPluginLogging::gNPNLog, a, b); \
   PR_LogFlush();                                                    \
   PR_END_MACRO
#else
 #define NPN_PLUGIN_LOG(a, b)
#endif

#ifdef PLUGIN_LOGGING
 #define NPP_PLUGIN_LOG(a, b)                              \
   PR_BEGIN_MACRO                                         \
   PR_LOG(nsPluginLogging::gNPPLog, a, b); \
   PR_LogFlush();                                                    \
   PR_END_MACRO
#else
 #define NPP_PLUGIN_LOG(a, b)
#endif

#ifdef PLUGIN_LOGGING
 #define PLUGIN_LOG(a, b)                              \
   PR_BEGIN_MACRO                                         \
   PR_LOG(nsPluginLogging::gPluginLog, a, b); \
   PR_LogFlush();                                                    \
   PR_END_MACRO
#else
 #define PLUGIN_LOG(a, b)
#endif

#endif   // nsPluginLogging_h__

