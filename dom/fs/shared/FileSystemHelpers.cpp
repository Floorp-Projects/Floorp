/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemHelpers.h"

#include "nsString.h"

namespace mozilla::dom::fs {

bool IsValidName(const mozilla::dom::fs::Name& aName) {
  return !(aName.IsVoid() || aName.Length() == 0 ||
#ifdef XP_WIN
           aName.FindChar('\\') != kNotFound ||
#endif
           aName.FindChar('/') != kNotFound || aName.EqualsLiteral(".") ||
           aName.EqualsLiteral(".."));
}

}  // namespace mozilla::dom::fs
