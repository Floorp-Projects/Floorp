/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LOGGING_CONSTANTS_H_
#define MOZILLA_GFX_LOGGING_CONSTANTS_H_

namespace mozilla {
namespace gfx {

// Attempting to be consistent with prlog values, but that isn't critical
// (and note that 5 has a special meaning - see the description
// with LoggingPrefs::sGfxLogLevel)
const int LOG_CRITICAL = 1;
const int LOG_WARNING = 2;
const int LOG_DEBUG = 3;
const int LOG_DEBUG_PRLOG = 4;
const int LOG_EVERYTHING = 5; // This needs to be the highest value

#if defined(DEBUG)
const int LOG_DEFAULT = LOG_EVERYTHING;
#else
const int LOG_DEFAULT = LOG_CRITICAL;
#endif

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_LOGGING_CONSTANTS_H_ */
