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

class RotorListRule final : public PivotRoleRule {
 public:
  explicit RotorListRule();
  explicit RotorListRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorButtonRule final : public PivotRoleRule {
 public:
  explicit RotorButtonRule();
  explicit RotorButtonRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorFrameRule final : public PivotRoleRule {
 public:
  explicit RotorFrameRule();
  explicit RotorFrameRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorImageRule final : public PivotRoleRule {
 public:
  explicit RotorImageRule();
  explicit RotorImageRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorStaticTextRule : public PivotRule {
 public:
  explicit RotorStaticTextRule();
  explicit RotorStaticTextRule(AccessibleOrProxy& aDirectDescendantsFrom);

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;

 protected:
  AccessibleOrProxy mDirectDescendantsFrom;
};

class RotorCheckboxRule final : public PivotRoleRule {
 public:
  explicit RotorCheckboxRule();
  explicit RotorCheckboxRule(AccessibleOrProxy& aDirectDescendantsFrom);
};

class RotorControlRule final : public PivotRule {
 public:
  explicit RotorControlRule(AccessibleOrProxy& aDirectDescendantsFrom);
  explicit RotorControlRule();

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;

 private:
  AccessibleOrProxy mDirectDescendantsFrom;
};

class RotorLinkRule : public PivotRule {
 public:
  explicit RotorLinkRule();
  explicit RotorLinkRule(AccessibleOrProxy& aDirectDescendantsFrom);

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;

 protected:
  AccessibleOrProxy mDirectDescendantsFrom;
};

class RotorVisitedLinkRule final : public RotorLinkRule {
 public:
  explicit RotorVisitedLinkRule();
  explicit RotorVisitedLinkRule(AccessibleOrProxy& aDirectDescendantsFrom);

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;
};

class RotorUnvisitedLinkRule final : public RotorLinkRule {
 public:
  explicit RotorUnvisitedLinkRule();
  explicit RotorUnvisitedLinkRule(AccessibleOrProxy& aDirectDescendantsFrom);

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;
};

class RotorTextFieldRule : public PivotRule {
 public:
  explicit RotorTextFieldRule();
  explicit RotorTextFieldRule(AccessibleOrProxy& aDirectDescendantsFrom);

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;

 protected:
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
