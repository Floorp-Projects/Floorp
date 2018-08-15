/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

using namespace mozilla;
using namespace mozilla::a11y;

void
a11y::PlatformInit()
{
}

void
a11y::PlatformShutdown()
{
}

void
a11y::ProxyCreated(ProxyAccessible*, uint32_t)
{
}

void
a11y::ProxyDestroyed(ProxyAccessible*)
{
}

void
a11y::ProxyEvent(ProxyAccessible*, uint32_t)
{
}

void
a11y::ProxyStateChangeEvent(ProxyAccessible*, uint64_t, bool)
{
}

void
a11y::ProxyCaretMoveEvent(ProxyAccessible* aTarget, int32_t aOffset)
{
}

void
a11y::ProxyTextChangeEvent(ProxyAccessible*, const nsString&, int32_t, uint32_t,
                     bool, bool)
{
}

void
a11y::ProxyShowHideEvent(ProxyAccessible*, ProxyAccessible*, bool, bool)
{
}

void
a11y::ProxySelectionEvent(ProxyAccessible*, ProxyAccessible*, uint32_t)
{
}

#if defined(ANDROID)
void
a11y::ProxyVirtualCursorChangeEvent(ProxyAccessible*, ProxyAccessible*,
                                    int32_t, int32_t, ProxyAccessible*,
                                    int32_t, int32_t, int16_t, int16_t, bool)
{
}
#endif
