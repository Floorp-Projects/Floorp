/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMCONSTANTS_H_
#define DOM_FS_FILESYSTEMCONSTANTS_H_

#include "nsLiteralString.h"

namespace mozilla::dom::fs {

constexpr nsLiteralString kRootName = u""_ns;

constexpr nsLiteralString kRootString = u"root"_ns;

constexpr uint32_t kStreamCopyBlockSize = 1024 * 1024;

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_FILESYSTEMCONSTANTS_H_
