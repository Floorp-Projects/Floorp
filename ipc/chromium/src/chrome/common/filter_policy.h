// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FILTER_POLICY_H__
#define CHROME_COMMON_FILTER_POLICY_H__

#include "base/basictypes.h"

// When an insecure resource (mixed content or bad HTTPS) is loaded, the browser
// can decide to filter it.  The filtering is done in the renderer.  This class
// enumerates the different policy that can be used for the filtering.  It is
// passed along with resource response messages.
class FilterPolicy {
 public:
  enum Type {
    // Pass all types of resources through unmodified.
    DONT_FILTER = 0,

    // Block all types of resources, except images.  For images, modify them to
    // indicate that they have been filtered.
    // TODO(abarth): This is a misleading name for this enum value.  We should
    //               change it to something more suggestive of what this
    //               actually does.
    FILTER_ALL_EXCEPT_IMAGES,

    // Block all types of resources.
    FILTER_ALL
  };

  static bool ValidType(int32_t type) {
    return type >= DONT_FILTER && type <= FILTER_ALL;
  }

  static Type FromInt(int32_t type) {
    return static_cast<Type>(type);
  }

 private:
  // Don't instantiate this class.
  FilterPolicy();
  ~FilterPolicy();

};

#endif  // CHROME_COMMON_FILTER_POLICY_H__
