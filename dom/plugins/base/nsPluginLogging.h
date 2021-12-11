/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Plugin Module Logging usage instructions and includes */
////////////////////////////////////////////////////////////////////////////////
#ifndef nsPluginLogging_h__
#define nsPluginLogging_h__

#include "mozilla/Logging.h"

////////////////////////////////////////////////////////////////////////////////
// Basic Plugin Logging Usage Instructions
//
// 1. Set this environment variable: MOZ_LOG=<name>:<level>

// Choose the <name> and <level> from this list (no quotes):

// Log Names            <name>
#define NPN_LOG_NAME "PluginNPN"
#define NPP_LOG_NAME "PluginNPP"
#define PLUGIN_LOG_NAME "Plugin"

// Levels                <level>
#define PLUGIN_LOG_ALWAYS mozilla::LogLevel::Error
#define PLUGIN_LOG_BASIC mozilla::LogLevel::Info
#define PLUGIN_LOG_NORMAL mozilla::LogLevel::Debug
#define PLUGIN_LOG_NOISY mozilla::LogLevel::Verbose

// 2. You can combine logs and levels by separating them with a comma:
//    My favorite Win32 Example: SET MOZ_LOG=Plugin:5,PluginNPP:5,PluginNPN:5

// 3. Instead of output going to the console, you can log to a file.
//    Additionally, set the MOZ_LOG_FILE environment variable to point to the
//    full path of a file.
//    My favorite Win32 Example: SET MOZ_LOG_FILE=c:\temp\pluginLog.txt

// 4. For complete information see the Gecko Developer guide:
// https://firefox-source-docs.mozilla.org/xpcom/logging.html

class nsPluginLogging {
 public:
  static mozilla::LazyLogModule gNPNLog;     // 4.x NP API, calls into navigator
  static mozilla::LazyLogModule gNPPLog;     // 4.x NP API, calls into plugin
  static mozilla::LazyLogModule gPluginLog;  // general plugin log
};

// Quick-use macros
#define NPN_PLUGIN_LOG(a, b) MOZ_LOG(nsPluginLogging::gNPNLog, a, b)
#define NPP_PLUGIN_LOG(a, b) MOZ_LOG(nsPluginLogging::gNPPLog, a, b)
#define PLUGIN_LOG(a, b) MOZ_LOG(nsPluginLogging::gPluginLog, a, b)

#endif  // nsPluginLogging_h__
