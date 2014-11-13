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
a11y::ProxyCreated(ProxyAccessible*)
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
