/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CONSTANTS_H_
#define DOM_QUOTA_CONSTANTS_H_

#include "nsLiteralString.h"

// The name of the file that we use to load/save the last access time of an
// origin.
// XXX We should get rid of old metadata files at some point, bug 1343576.
#define METADATA_FILE_NAME u".metadata"
#define METADATA_TMP_FILE_NAME u".metadata-tmp"
#define METADATA_V2_FILE_NAME u".metadata-v2"
#define METADATA_V2_TMP_FILE_NAME u".metadata-v2-tmp"

namespace mozilla::dom::quota {

const char kChromeOrigin[] = "chrome";

constexpr auto kSQLiteSuffix = u".sqlite"_ns;

constexpr nsLiteralCString kUUIDOriginScheme = "uuid"_ns;

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CONSTANTS_H_
