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
 * This rule matches all accessibles that satisfy the "boilerplate"
 * pivot conditions and have a corresponding native accessible.
 */
class RotorRule : public PivotRule {
 public:
  explicit RotorRule(Accessible* aDirectDescendantsFrom);
  explicit RotorRule();
  uint16_t Match(Accessible* aAcc) override;

 private:
  Accessible* mDirectDescendantsFrom;
};

/**
 * This rule matches all accessibles of a given role.
 */
class RotorRoleRule : public RotorRule {
 public:
  explicit RotorRoleRule(role aRole, Accessible* aDirectDescendantsFrom);
  explicit RotorRoleRule(role aRole);
  uint16_t Match(Accessible* aAcc) override;

 private:
  role mRole;
};

class RotorMacRoleRule : public RotorRule {
 public:
  explicit RotorMacRoleRule(NSString* aRole);
  explicit RotorMacRoleRule(NSString* aRole,
                            Accessible* aDirectDescendantsFrom);
  ~RotorMacRoleRule();
  virtual uint16_t Match(Accessible* aAcc) override;

 protected:
  NSString* mMacRole;
};

class RotorControlRule final : public RotorRule {
 public:
  explicit RotorControlRule(Accessible* aDirectDescendantsFrom);
  explicit RotorControlRule();

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorTextEntryRule final : public RotorRule {
 public:
  explicit RotorTextEntryRule(Accessible* aDirectDescendantsFrom);
  explicit RotorTextEntryRule();

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorLinkRule : public RotorRule {
 public:
  explicit RotorLinkRule();
  explicit RotorLinkRule(Accessible* aDirectDescendantsFrom);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorVisitedLinkRule final : public RotorLinkRule {
 public:
  explicit RotorVisitedLinkRule();
  explicit RotorVisitedLinkRule(Accessible* aDirectDescendantsFrom);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorUnvisitedLinkRule final : public RotorLinkRule {
 public:
  explicit RotorUnvisitedLinkRule();
  explicit RotorUnvisitedLinkRule(Accessible* aDirectDescendantsFrom);

  virtual uint16_t Match(Accessible* aAcc) override;
};

/**
 * This rule matches all accessibles that satisfy the "boilerplate"
 * pivot conditions and have a corresponding native accessible.
 */
class RotorNotMacRoleRule : public RotorMacRoleRule {
 public:
  explicit RotorNotMacRoleRule(NSString* aMacRole,
                               Accessible* aDirectDescendantsFrom);
  explicit RotorNotMacRoleRule(NSString* aMacRole);
  uint16_t Match(Accessible* aAcc) override;
};

class RotorStaticTextRule : public RotorRule {
 public:
  explicit RotorStaticTextRule();
  explicit RotorStaticTextRule(Accessible* aDirectDescendantsFrom);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorHeadingLevelRule : public RotorRoleRule {
 public:
  explicit RotorHeadingLevelRule(int32_t aLevel);
  explicit RotorHeadingLevelRule(int32_t aLevel,
                                 Accessible* aDirectDescendantsFrom);

  virtual uint16_t Match(Accessible* aAcc) override;

 private:
  int32_t mLevel;
};

class RotorLiveRegionRule : public RotorRule {
 public:
  explicit RotorLiveRegionRule(Accessible* aDirectDescendantsFrom)
      : RotorRule(aDirectDescendantsFrom) {}
  explicit RotorLiveRegionRule() : RotorRule() {}

  uint16_t Match(Accessible* aAcc) override;
};

/**
 * This rule matches all accessibles with roles::OUTLINEITEM. If
 * outlines are nested, it ignores the nested subtree and returns
 * only items which are descendants of the primary outline.
 */
class OutlineRule : public RotorRule {
 public:
  explicit OutlineRule();
  uint16_t Match(Accessible* aAcc) override;
};
