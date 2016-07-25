/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginQuirks.h"

#include "nsPluginHost.h"

namespace mozilla {
namespace plugins {

int GetQuirksFromMimeTypeAndFilename(const nsCString& aMimeType,
                                     const nsCString& aPluginFilename)
{
    int quirks = 0;

    nsPluginHost::SpecialType specialType = nsPluginHost::GetSpecialType(aMimeType);

    if (specialType == nsPluginHost::eSpecialType_Silverlight) {
        quirks |= QUIRK_SILVERLIGHT_DEFAULT_TRANSPARENT;
#ifdef OS_WIN
        quirks |= QUIRK_WINLESS_TRACKPOPUP_HOOK;
        quirks |= QUIRK_SILVERLIGHT_FOCUS_CHECK_PARENT;
#endif
    }

    if (specialType == nsPluginHost::eSpecialType_Flash) {
        quirks |= QUIRK_FLASH_RETURN_EMPTY_DOCUMENT_ORIGIN;
#ifdef OS_WIN
        quirks |= QUIRK_WINLESS_TRACKPOPUP_HOOK;
        quirks |= QUIRK_FLASH_THROTTLE_WMUSER_EVENTS;
        quirks |= QUIRK_FLASH_HOOK_SETLONGPTR;
        quirks |= QUIRK_FLASH_HOOK_GETWINDOWINFO;
        quirks |= QUIRK_FLASH_FIXUP_MOUSE_CAPTURE;
        quirks |= QUIRK_WINLESS_HOOK_IME;
#endif
    }

#ifdef OS_WIN
    // QuickTime plugin usually loaded with audio/mpeg mimetype
    NS_NAMED_LITERAL_CSTRING(quicktime, "npqtplugin");
    if (FindInReadable(quicktime, aPluginFilename)) {
        quirks |= QUIRK_QUICKTIME_AVOID_SETWINDOW;
    }
#endif

#ifdef XP_MACOSX
    // Whitelist Flash and Quicktime to support offline renderer
    NS_NAMED_LITERAL_CSTRING(quicktime, "QuickTime Plugin.plugin");
    if (specialType == nsPluginHost::eSpecialType_Flash) {
        quirks |= QUIRK_ALLOW_OFFLINE_RENDERER;
    } else if (FindInReadable(quicktime, aPluginFilename)) {
        quirks |= QUIRK_ALLOW_OFFLINE_RENDERER;
    }
#endif

#ifdef OS_WIN
    if (specialType == nsPluginHost::eSpecialType_Unity) {
        quirks |= QUIRK_UNITY_FIXUP_MOUSE_CAPTURE;
    }
#endif

#ifdef OS_WIN
    if (specialType == nsPluginHost::eSpecialType_Test) {
        quirks |= QUIRK_WINLESS_HOOK_IME;
    }
#endif

    return quirks;
}

} /* namespace plugins */
} /* namespace mozilla */
