// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ICU_UTIL_H__
#define BASE_ICU_UTIL_H__

namespace icu_util {

// Call this function to load ICU's data tables for the current process.  This
// function should be called before ICU is used.
bool Initialize();

}  // namespace icu_util

#endif  // BASE_ICU_UTIL_H__
