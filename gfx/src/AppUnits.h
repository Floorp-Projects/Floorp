/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppUnits_h
#define mozilla_AppUnits_h

#include <stdint.h>

namespace mozilla {
inline int32_t AppUnitsPerCSSPixel() { return 60; }
inline int32_t AppUnitsPerCSSInch() { return 96 * AppUnitsPerCSSPixel(); }
} // namespace mozilla
#endif /* _NS_APPUNITS_H_ */
