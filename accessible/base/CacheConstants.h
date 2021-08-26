/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CacheConstants_h_
#define _CacheConstants_h_

namespace mozilla {
namespace a11y {

class CacheDomain {
 public:
  static constexpr uint64_t Name = ((uint64_t)0x1) << 0;
  static constexpr uint64_t Value = ((uint64_t)0x1) << 1;
  static constexpr uint64_t Bounds = ((uint64_t)0x1) << 2;
  static constexpr uint64_t All = ~((uint64_t)0x0);
};

}  // namespace a11y
}  // namespace mozilla

#endif
