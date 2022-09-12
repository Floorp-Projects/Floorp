/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_loader_SkipCheckForBrokenURLOrZeroSized_h
#define mozilla_loader_SkipCheckForBrokenURLOrZeroSized_h

#include <stdint.h>  // uint8_t

namespace mozilla {
namespace loader {

// Represents the `aSkipCheckForBrokenURLOrZeroSized` parameter for
// `NS_NewChannel` function.
enum class SkipCheckForBrokenURLOrZeroSized : uint8_t { No, Yes };

}  // namespace loader
}  // namespace mozilla

#endif  // mozilla_loader_SkipCheckForBrokenURLOrZeroSized_h
