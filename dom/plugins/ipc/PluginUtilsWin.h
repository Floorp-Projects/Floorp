/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginUtilsWin_h
#define dom_plugins_PluginUtilsWin_h 1

#include "npapi.h"
#include "nscore.h"

namespace mozilla {
namespace plugins {

class PluginModuleParent;

namespace PluginUtilsWin {

nsresult RegisterForAudioDeviceChanges(PluginModuleParent* aModuleParent,
                                       bool aShouldRegister);

} // namespace PluginUtilsWin
} // namespace plugins
} // namespace mozilla

#endif //dom_plugins_PluginUtilsWin_h
