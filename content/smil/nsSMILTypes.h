/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTYPES_H_
#define NS_SMILTYPES_H_

#include <stdint.h>

// A timestamp in milliseconds
//
// A time may represent:
//
//   simple time -- offset within the simple duration
//   active time -- offset within the active duration
//   document time -- offset since the document begin
//   wallclock time -- "real" time -- offset since the epoch
//
// For an overview of how this class is related to other SMIL time classes see
// the documentstation in nsSMILTimeValue.h
//
typedef int64_t nsSMILTime;

#endif // NS_SMILTYPES_H_
