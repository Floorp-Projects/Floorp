/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginBridge_h
#define mozilla_plugins_PluginBridge_h

namespace mozilla {

namespace dom {
class ContentParent;
}

namespace plugins {

bool
SetupBridge(uint32_t aPluginId, dom::ContentParent* aContentParent,
            bool aForceBridgeNow, nsresult* aResult, uint32_t* aRunID);

bool
FindPluginsForContent(uint32_t aPluginEpoch,
                      nsTArray<PluginTag>* aPlugins,
                      uint32_t* aNewPluginEpoch);

void
TerminatePlugin(uint32_t aPluginId,
                const nsCString& aMonitorDescription,
                const nsAString& aBrowserDumpId);

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginBridge_h
