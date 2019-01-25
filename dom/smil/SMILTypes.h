/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SMILTypes_h
#define mozilla_SMILTypes_h

#include <stdint.h>

namespace mozilla {

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
// the documentation in SMILTimeValue.h
//
typedef int64_t SMILTime;

}  // namespace mozilla

#endif  // mozilla_SMILTypes_h
