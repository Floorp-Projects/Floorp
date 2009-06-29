// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

// We define BZ_NO_STDIO in third_party/bzip2 to remove its internal STDERR
// error reporting.  This requires us to export our own error handler.
extern "C"
void bz_internal_error(int errcode) {
  CHECK(false) << "bzip2 internal error: " << errcode;
}
