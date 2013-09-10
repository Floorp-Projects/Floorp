// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_UTIL_H_
#define BASE_MAC_UTIL_H_

struct FSRef;

#include <string>

namespace mac_util {

std::string PathFromFSRef(const FSRef& ref);
bool FSRefFromPath(const std::string& path, FSRef* ref);

// Returns true if the application is running from a bundle
bool AmIBundled();

}  // namespace mac_util

#endif  // BASE_MAC_UTIL_H_
