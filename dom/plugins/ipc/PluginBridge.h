/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginBridge_h
#define mozilla_plugins_PluginBridge_h

#include <functional>

#include "base/process.h"

namespace mozilla {

namespace dom {
class ContentParent;
} // namespace dom

namespace ipc {
template<class PFooSide>
class Endpoint;
} // namespace ipc

namespace plugins {

class PPluginModuleParent;

bool
SetupBridge(uint32_t aPluginId, dom::ContentParent* aContentParent,
            nsresult* aResult, uint32_t* aRunID,
            ipc::Endpoint<PPluginModuleParent>* aEndpoint);

nsresult
FindPluginsForContent(uint32_t aPluginEpoch,
                      nsTArray<PluginTag>* aPlugins,
                      nsTArray<FakePluginTag>* aFakePlugins,
                      uint32_t* aNewPluginEpoch);

void
TakeFullMinidump(uint32_t aPluginId,
                 base::ProcessId aContentProcessId,
                 const nsAString& aBrowserDumpId,
                 std::function<void(nsString)>&& aCallback,
                 bool aAsync);

void
TerminatePlugin(uint32_t aPluginId,
                base::ProcessId aContentProcessId,
                const nsCString& aMonitorDescription,
                const nsAString& aDumpId,
                std::function<void(bool)>&& aCallback);
} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginBridge_h
