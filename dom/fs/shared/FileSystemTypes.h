/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMTYPES_H_
#define DOM_FS_FILESYSTEMTYPES_H_

#include "nsStringFwd.h"

template <class T>
class nsTArray;

namespace mozilla::dom::fs {

using ContentType = nsString;
using EntryId = nsCString;
using Name = nsString;
using Origin = nsCString;
using PageNumber = uint32_t;
using Path = nsTArray<Name>;
using TimeStamp = int64_t;

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_FILESYSTEMTYPES_H_
