/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

using namespace mozilla;
using namespace mozilla::a11y;

void a11y::PlatformInit() {}

void a11y::PlatformShutdown() {}

void a11y::ProxyCreated(RemoteAccessible*) {}

void a11y::ProxyDestroyed(RemoteAccessible*) {}

void a11y::ProxyEvent(RemoteAccessible*, uint32_t) {}

void a11y::ProxyStateChangeEvent(RemoteAccessible*, uint64_t, bool) {}

void a11y::ProxyCaretMoveEvent(RemoteAccessible* aTarget, int32_t aOffset,
                               bool aIsSelectionCollapsed,
                               int32_t aGranularity) {}

void a11y::ProxyTextChangeEvent(RemoteAccessible*, const nsAString&, int32_t,
                                uint32_t, bool, bool) {}

void a11y::ProxyShowHideEvent(RemoteAccessible*, RemoteAccessible*, bool,
                              bool) {}

void a11y::ProxySelectionEvent(RemoteAccessible*, RemoteAccessible*, uint32_t) {
}
