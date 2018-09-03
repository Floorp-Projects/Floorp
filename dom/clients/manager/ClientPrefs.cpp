/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientPrefs.h"

#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {

namespace {

bool gDataURLUniqueOpaqueOrigin = false;

} // anonymous namespace

void
ClientPrefsInit()
{
  Preferences::AddBoolVarCache(&gDataURLUniqueOpaqueOrigin,
                               "security.data_uri.unique_opaque_origin",
                               false);
}

bool
ClientPrefsGetDataURLUniqueOpaqueOrigin()
{
  return gDataURLUniqueOpaqueOrigin;
}

} // namespace dom
} // namespace mozilla
