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
  static constexpr uint64_t NameAndDescription = ((uint64_t)0x1) << 0;
  static constexpr uint64_t Value = ((uint64_t)0x1) << 1;
  static constexpr uint64_t Bounds = ((uint64_t)0x1) << 2;
  static constexpr uint64_t Resolution = ((uint64_t)0x1) << 3;
  static constexpr uint64_t Text = ((uint64_t)0x1) << 4;
  static constexpr uint64_t DOMNodeID = ((uint64_t)0x1) << 5;
  static constexpr uint64_t State = ((uint64_t)0x1) << 6;
  static constexpr uint64_t GroupInfo = ((uint64_t)0x1) << 7;
  static constexpr uint64_t Actions = ((uint64_t)0x1) << 8;
  static constexpr uint64_t Style = ((uint64_t)0x1) << 9;
  static constexpr uint64_t TransformMatrix = ((uint64_t)0x1) << 10;
  static constexpr uint64_t ScrollPosition = ((uint64_t)0x1) << 11;
  static constexpr uint64_t Table = ((uint64_t)0x1) << 12;
  static constexpr uint64_t Spelling = ((uint64_t)0x1) << 13;
  static constexpr uint64_t All = ~((uint64_t)0x0);
};

enum class CacheUpdateType {
  /*
   * An initial cache push of a loaded document or inserted subtree.
   */
  Initial,

  /*
   * An incremental cache push of one or more fields that have changed.
   */
  Update,
};

}  // namespace a11y
}  // namespace mozilla

#endif
