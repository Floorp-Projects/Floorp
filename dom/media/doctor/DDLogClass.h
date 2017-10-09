/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLogClass_h_
#define DDLogClass_h_

#include "mozilla/Assertions.h"
#include "mozilla/DefineEnum.h"

namespace mozilla {

// Enum used to classify log messages.
// Those starting with '_' are for internal use only.
MOZ_DEFINE_ENUM_CLASS(DDLogClass,
                      (_Construction,
                       _DerivedConstruction,
                       _Destruction,
                       _Link,
                       _Unlink,
                       Property,
                       Event,
                       API,
                       Log));

// Corresponding short strings, used as JSON property names when logs are
// retrieved.
extern const char* const kDDLogClassShortStrings[kDDLogClassCount];

inline const char*
ToShortString(DDLogClass aClass)
{
  MOZ_ASSERT(static_cast<size_t>(aClass) < kDDLogClassCount);
  return kDDLogClassShortStrings[static_cast<size_t>(aClass)];
}

// Corresponding long strings, for use in descriptive UI.
extern const char* const kDDLogClassLongStrings[kDDLogClassCount];

inline const char*
ToLongString(DDLogClass aClass)
{
  MOZ_ASSERT(static_cast<size_t>(aClass) < kDDLogClassCount);
  return kDDLogClassLongStrings[static_cast<size_t>(aClass)];
}

} // namespace mozilla

#endif // DDLogClass_h_
