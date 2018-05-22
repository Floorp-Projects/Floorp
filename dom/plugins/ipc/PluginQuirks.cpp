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

    if (specialType == nsPluginHost::eSpecialType_Flash) {
        quirks |= QUIRK_FLASH_RETURN_EMPTY_DOCUMENT_ORIGIN;
#ifdef OS_WIN
        quirks |= QUIRK_WINLESS_TRACKPOPUP_HOOK;
        quirks |= QUIRK_FLASH_THROTTLE_WMUSER_EVENTS;
        quirks |= QUIRK_FLASH_HOOK_SETLONGPTR;
        quirks |= QUIRK_FLASH_HOOK_GETWINDOWINFO;
        quirks |= QUIRK_FLASH_FIXUP_MOUSE_CAPTURE;
        quirks |= QUIRK_WINLESS_HOOK_IME;
#if defined(_M_X64) || defined(__x86_64__)
        quirks |= QUIRK_FLASH_HOOK_GETKEYSTATE;
        quirks |= QUIRK_FLASH_HOOK_PRINTDLGW;
        quirks |= QUIRK_FLASH_HOOK_SSL;
        quirks |= QUIRK_FLASH_HOOK_CREATEMUTEXW;
#endif
#endif
    }

#ifdef XP_MACOSX
    // Whitelist Flash to support offline renderer.
    if (specialType == nsPluginHost::eSpecialType_Flash) {
        quirks |= QUIRK_ALLOW_OFFLINE_RENDERER;
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
