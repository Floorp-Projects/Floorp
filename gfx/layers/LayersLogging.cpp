/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersLogging.h"
#include <stdint.h>              // for uint8_t
#include "FrameMetrics.h"        // for FrameMetrics, etc
#include "ImageTypes.h"          // for ImageFormat
#include "mozilla/gfx/Matrix.h"  // for Matrix4x4, Matrix
#include "mozilla/gfx/Point.h"   // for IntSize
#include "nsDebug.h"             // for NS_ERROR
#include "nsPoint.h"             // for nsPoint
#include "nsRect.h"              // for nsRect
#include "nsRectAbsolute.h"      // for nsRectAbsolute
#include "base/basictypes.h"

void print_stderr(std::stringstream& aStr) {
#if defined(ANDROID)
  // On Android logcat output is truncated to 1024 chars per line, and
  // we usually use std::stringstream to build up giant multi-line gobs
  // of output. So to avoid the truncation we find the newlines and
  // print the lines individually.
  std::string line;
  while (std::getline(aStr, line)) {
    printf_stderr("%s\n", line.c_str());
  }
#else
  printf_stderr("%s", aStr.str().c_str());
#endif
}

void fprint_stderr(FILE* aFile, std::stringstream& aStr) {
  if (aFile == stderr) {
    print_stderr(aStr);
  } else {
    fprintf_stderr(aFile, "%s", aStr.str().c_str());
  }
}
