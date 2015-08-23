/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PermissionUtils_h_
#define mozilla_dom_PermissionUtils_h_

#include "mozilla/dom/PermissionsBinding.h"
#include "mozilla/dom/PermissionStatusBinding.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace dom {

const char* PermissionNameToType(PermissionName aName);
Maybe<PermissionName> TypeToPermissionName(const char* aType);

PermissionState ActionToPermissionState(uint32_t aAction);

} // namespace dom
} // namespace mozilla

#endif
