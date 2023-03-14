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
  explicit RotorRule(Accessible* aDirectDescendantsFrom,
                     const nsString& aSearchText);
  explicit RotorRule(const nsString& aSearchText);
  uint16_t Match(Accessible* aAcc) override;

 private:
  Accessible* mDirectDescendantsFrom;
  const nsString& mSearchText;
};

/**
 * This rule matches all accessibles of a given role.
 */
class RotorRoleRule : public RotorRule {
 public:
  explicit RotorRoleRule(role aRole, Accessible* aDirectDescendantsFrom,
                         const nsString& aSearchText);
  explicit RotorRoleRule(role aRole, const nsString& aSearchText);
  uint16_t Match(Accessible* aAcc) override;

 private:
  role mRole;
};

class RotorMacRoleRule : public RotorRule {
 public:
  explicit RotorMacRoleRule(NSString* aRole, const nsString& aSearchText);
  explicit RotorMacRoleRule(NSString* aRole, Accessible* aDirectDescendantsFrom,
                            const nsString& aSearchText);
  ~RotorMacRoleRule();
  virtual uint16_t Match(Accessible* aAcc) override;

 protected:
  NSString* mMacRole;
};

class RotorControlRule final : public RotorRule {
 public:
  explicit RotorControlRule(Accessible* aDirectDescendantsFrom,
                            const nsString& aSearchText);
  explicit RotorControlRule(const nsString& aSearchText);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorTextEntryRule final : public RotorRule {
 public:
  explicit RotorTextEntryRule(Accessible* aDirectDescendantsFrom,
                              const nsString& aSearchText);
  explicit RotorTextEntryRule(const nsString& aSearchText);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorLinkRule : public RotorRule {
 public:
  explicit RotorLinkRule(const nsString& aSearchText);
  explicit RotorLinkRule(Accessible* aDirectDescendantsFrom,
                         const nsString& aSearchText);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorVisitedLinkRule final : public RotorLinkRule {
 public:
  explicit RotorVisitedLinkRule(const nsString& aSearchText);
  explicit RotorVisitedLinkRule(Accessible* aDirectDescendantsFrom,
                                const nsString& aSearchText);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorUnvisitedLinkRule final : public RotorLinkRule {
 public:
  explicit RotorUnvisitedLinkRule(const nsString& aSearchText);
  explicit RotorUnvisitedLinkRule(Accessible* aDirectDescendantsFrom,
                                  const nsString& aSearchText);

  virtual uint16_t Match(Accessible* aAcc) override;
};

/**
 * This rule matches all accessibles that satisfy the "boilerplate"
 * pivot conditions and have a corresponding native accessible.
 */
class RotorNotMacRoleRule : public RotorMacRoleRule {
 public:
  explicit RotorNotMacRoleRule(NSString* aMacRole,
                               Accessible* aDirectDescendantsFrom,
                               const nsString& aSearchText);
  explicit RotorNotMacRoleRule(NSString* aMacRole, const nsString& aSearchText);
  uint16_t Match(Accessible* aAcc) override;
};

class RotorStaticTextRule : public RotorRule {
 public:
  explicit RotorStaticTextRule(const nsString& aSearchText);
  explicit RotorStaticTextRule(Accessible* aDirectDescendantsFrom,
                               const nsString& aSearchText);

  virtual uint16_t Match(Accessible* aAcc) override;
};

class RotorHeadingLevelRule : public RotorRoleRule {
 public:
  explicit RotorHeadingLevelRule(int32_t aLevel, const nsString& aSearchText);
  explicit RotorHeadingLevelRule(int32_t aLevel,
                                 Accessible* aDirectDescendantsFrom,
                                 const nsString& aSearchText);

  virtual uint16_t Match(Accessible* aAcc) override;

 private:
  int32_t mLevel;
};

class RotorLiveRegionRule : public RotorRule {
 public:
  explicit RotorLiveRegionRule(Accessible* aDirectDescendantsFrom,
                               const nsString& aSearchText)
      : RotorRule(aDirectDescendantsFrom, aSearchText) {}
  explicit RotorLiveRegionRule(const nsString& aSearchText)
      : RotorRule(aSearchText) {}

  uint16_t Match(Accessible* aAcc) override;
};
