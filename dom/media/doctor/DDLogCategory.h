/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLogCategory_h_
#define DDLogCategory_h_

#include "mozilla/Assertions.h"
#include "mozilla/DefineEnum.h"

namespace mozilla {

// Enum used to categorize log messages.
// Those starting with '_' are for internal use only.
MOZ_DEFINE_ENUM_CLASS(DDLogCategory,
                      (_Construction,
                       _DerivedConstruction,
                       _Destruction,
                       _Link,
                       _Unlink,
                       Property,
                       Event,
                       API,
                       Log,
                       MozLogError,
                       MozLogWarning,
                       MozLogInfo,
                       MozLogDebug,
                       MozLogVerbose));

// Corresponding short strings, used as JSON property names when logs are
// retrieved.
extern const char* const kDDLogCategoryShortStrings[kDDLogCategoryCount];

inline const char*
ToShortString(DDLogCategory aCategory)
{
  MOZ_ASSERT(static_cast<size_t>(aCategory) < kDDLogCategoryCount);
  return kDDLogCategoryShortStrings[static_cast<size_t>(aCategory)];
}

// Corresponding long strings, for use in descriptive UI.
extern const char* const kDDLogCategoryLongStrings[kDDLogCategoryCount];

inline const char*
ToLongString(DDLogCategory aCategory)
{
  MOZ_ASSERT(static_cast<size_t>(aCategory) < kDDLogCategoryCount);
  return kDDLogCategoryLongStrings[static_cast<size_t>(aCategory)];
}

} // namespace mozilla

#endif // DDLogCategory_h_
