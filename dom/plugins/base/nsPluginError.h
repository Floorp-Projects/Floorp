/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Plugin Module Logging usage instructions and includes */
////////////////////////////////////////////////////////////////////////////////
#ifndef nsPluginError_h__
#define nsPluginError_h__

#include "nsError.h"

#define NS_ERROR_PLUGINS_PLUGINSNOTCHANGED    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_PLUGINS,1000)
#define NS_ERROR_PLUGIN_DISABLED    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_PLUGINS,1001)
#define NS_ERROR_PLUGIN_BLOCKLISTED    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_PLUGINS,1002)
#define NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_PLUGINS,1003)
#define NS_ERROR_PLUGIN_CLICKTOPLAY    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_PLUGINS,1004)

#endif   // nsPluginError_h__
