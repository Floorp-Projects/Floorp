/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUILD_CONSTANTS_H_
#define BUILD_CONSTANTS_H_

/**
 * Why not just use ifdefs?
 * ifdefs tend to result in code that compiles on one platform but not another.
 * Given the number of build and platform configurations we have, it's best to
 * aim to compile the same on as many platforms as possible, and let the
 * compiler see the constexprs and handle dead-code elision itself.
 */

namespace mozilla {

constexpr bool kIsDebug =
#ifdef DEBUG
    true;
#else
    false;
#endif

constexpr bool kIsWindows =
#ifdef XP_WIN
    true;
#else
    false;
#endif

constexpr bool kIsMacOS =
#ifdef XP_MACOSX
    true;
#else
    false;
#endif

constexpr bool kIsLinux =
#ifdef MOZ_WIDGET_GTK
    true;
#else
    false;
#endif

constexpr bool kIsAndroid =
#ifdef MOZ_WIDGET_ANDROID
    true;
#else
    false;
#endif

}  // namespace mozilla

#endif  // BUILD_CONSTANTS_H_
