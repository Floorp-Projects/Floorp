/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxHal_h
#define mozilla_SandboxHal_h

namespace mozilla {
namespace hal_sandbox {

class PHalChild;
class PHalParent;

PHalChild* CreateHalChild();

PHalParent* CreateHalParent();

} // namespace hal_sandbox
} // namespace mozilla

#endif  // mozilla_SandboxHal_h
