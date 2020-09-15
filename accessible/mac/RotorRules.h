/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"
#include "Pivot.h"

using namespace mozilla::a11y;

/**
 * These rules match accessibles on a given role, filtering out non-direct
 * descendants if necessary.
 */

class RotorHeadingRule final : public PivotRoleRule {
 public:
  explicit RotorHeadingRule();
  explicit RotorHeadingRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorArticleRule final : public PivotRoleRule {
 public:
  explicit RotorArticleRule();
  explicit RotorArticleRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorTableRule final : public PivotRoleRule {
 public:
  explicit RotorTableRule();
  explicit RotorTableRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorLandmarkRule final : public PivotRoleRule {
 public:
  explicit RotorLandmarkRule();
  explicit RotorLandmarkRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorButtonRule final : public PivotRoleRule {
 public:
  explicit RotorButtonRule();
  explicit RotorButtonRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorControlRule final : public PivotRule {
 public:
  explicit RotorControlRule(AccessibleOrProxy& aDirectDescendantsFrom);
  explicit RotorControlRule();

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;

 private:
  AccessibleOrProxy mDirectDescendantsFrom;
};

/**
 * This rule matches all accessibles, filtering out non-direct
 * descendants if necessary.
 */
class RotorAllRule final : public PivotRule {
 public:
  explicit RotorAllRule(AccessibleOrProxy& aDirectDescendantsFrom);
  explicit RotorAllRule();

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;

 private:
  AccessibleOrProxy mDirectDescendantsFrom;
};
