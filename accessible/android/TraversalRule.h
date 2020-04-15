/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _TraversalRule_H_
#define _TraversalRule_H_

#include "Pivot.h"

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * Class represents a simple traversal rule.
 */
class TraversalRule final : public PivotRule {
 public:
  TraversalRule();
  explicit TraversalRule(int32_t aGranularity);

  ~TraversalRule() = default;

  virtual uint16_t Match(Accessible* aAccessible) override;

 private:
  bool IsSingleLineage(Accessible* aAccessible);

  bool IsFlatSubtree(const Accessible* aAccessible);

  bool IsListItemBullet(const Accessible* aAccessible);

  bool HasName(const Accessible* aAccessible);

  uint16_t DefaultMatch(Accessible* aAccessible);

  uint16_t LinkMatch(Accessible* aAccessible);

  uint16_t HeadingMatch(Accessible* aAccessible);

  uint16_t ControlMatch(Accessible* aAccessible);

  uint16_t SectionMatch(Accessible* aAccessible);

  uint16_t LandmarkMatch(Accessible* aAccessible);

  int32_t mGranularity;
};

}  // namespace a11y
}  // namespace mozilla

#endif