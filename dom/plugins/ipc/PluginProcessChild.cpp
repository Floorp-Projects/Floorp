/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/plugins/PluginProcessChild.h"

#include "prlink.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

#ifdef XP_WIN
#include <objbase.h>
#endif

using mozilla::ipc::IOThreadChild;

#ifdef OS_WIN
#include "nsSetDllDirectory.h"
#include <algorithm>

namespace {

std::size_t caseInsensitiveFind(std::string aHaystack, std::string aNeedle) {
    std::transform(aHaystack.begin(), aHaystack.end(), aHaystack.begin(), ::tolower);
    std::transform(aNeedle.begin(), aNeedle.end(), aNeedle.begin(), ::tolower);
    return aHaystack.find(aNeedle);
}

}
#endif

namespace mozilla {
namespace plugins {

bool
PluginProcessChild::Init()
{
#if defined(XP_MACOSX)
    // Remove the trigger for "dyld interposing" that we added in
    // GeckoChildProcessHost::PerformAsyncLaunchInternal(), in the host
    // process just before we were launched.  Dyld interposing will still
    // happen in our process (the plugin child process).  But we don't want
    // it to happen in any processes that the plugin might launch from our
    // process.
    nsCString interpose(PR_GetEnv("DYLD_INSERT_LIBRARIES"));
    if (!interpose.IsEmpty()) {
        // If we added the path to libplugin_child_interpose.dylib to an
        // existing DYLD_INSERT_LIBRARIES, we appended it to the end, after a
        // ":" path seperator.
        PRInt32 lastSeparatorPos = interpose.RFind(":");
        PRInt32 lastTriggerPos = interpose.RFind("libplugin_child_interpose.dylib");
        bool needsReset = false;
        if (lastTriggerPos != -1) {
            if (lastSeparatorPos == -1) {
                interpose.Truncate();
                needsReset = true;
            } else if (lastTriggerPos > lastSeparatorPos) {
                interpose.SetLength(lastSeparatorPos);
                needsReset = true;
            }
        }
        if (needsReset) {
            nsCString setInterpose("DYLD_INSERT_LIBRARIES=");
            if (!interpose.IsEmpty()) {
                setInterpose.Append(interpose);
            }
            // Values passed to PR_SetEnv() must be seperately allocated.
            char* setInterposePtr = strdup(PromiseFlatCString(setInterpose).get());
            PR_SetEnv(setInterposePtr);
        }
    }
#endif

#ifdef XP_WIN
    // Drag-and-drop needs OleInitialize to be called, and Silverlight depends
    // on the host calling CoInitialize (which is called by OleInitialize).
    ::OleInitialize(NULL);
#endif

    // Certain plugins, such as flash, steal the unhandled exception filter
    // thus we never get crash reports when they fault. This call fixes it.
    message_loop()->set_exception_restoration(true);

    std::string pluginFilename;

#if defined(OS_POSIX)
    // NB: need to be very careful in ensuring that the first arg
    // (after the binary name) here is indeed the plugin module path.
    // Keep in sync with dom/plugins/PluginModuleParent.
    std::vector<std::string> values = CommandLine::ForCurrentProcess()->argv();
    NS_ABORT_IF_FALSE(values.size() >= 2, "not enough args");

    pluginFilename = UnmungePluginDsoPath(values[1]);

#elif defined(OS_WIN)
    std::vector<std::wstring> values =
        CommandLine::ForCurrentProcess()->GetLooseValues();
    NS_ABORT_IF_FALSE(values.size() >= 1, "not enough loose args");

    pluginFilename = WideToUTF8(values[0]);

    bool protectCurrentDirectory = true;
    // Don't use SetDllDirectory for Shockwave Director
    const std::string shockwaveDirectorPluginFilename("\\np32dsw.dll");
    std::size_t index = caseInsensitiveFind(pluginFilename, shockwaveDirectorPluginFilename);
    if (index != std::string::npos &&
        index + shockwaveDirectorPluginFilename.length() == pluginFilename.length()) {
        protectCurrentDirectory = false;
    }
    if (protectCurrentDirectory) {
        SanitizeEnvironmentVariables();
        SetDllDirectory(L"");
    }

#else
#  error Sorry
#endif

    if (NS_FAILED(nsRegion::InitStatic())) {
      NS_ERROR("Could not initialize nsRegion");
      return false;
    }

    mPlugin.Init(pluginFilename, ParentHandle(),
                 IOThreadChild::message_loop(),
                 IOThreadChild::channel());

    return true;
}

void
PluginProcessChild::CleanUp()
{
#ifdef XP_WIN
    ::OleUninitialize();
#endif
    nsRegion::ShutdownStatic();
}

} // namespace plugins
} // namespace mozilla
