/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * an object that stores the result of determining whether a style struct that
 * was computed can be cached in the rule tree, and if so, what the cache
 * key is
 */

#include "RuleNodeCacheConditions.h"

#include "nsStyleContext.h"
#include "WritingModes.h"

using namespace mozilla;

bool
RuleNodeCacheConditions::Matches(nsStyleContext* aStyleContext) const
{
  MOZ_ASSERT(Cacheable());
  if ((mBits & eHaveFontSize) &&
      mFontSize != aStyleContext->StyleFont()->mFont.size) {
    return false;
  }
  if ((mBits & eHaveWritingMode) &&
      (mWritingMode != WritingMode(aStyleContext).GetBits())) {
    return false;
  }
  return true;
}

#ifdef DEBUG
void
RuleNodeCacheConditions::List() const
{
  printf("{ ");
  bool first = true;
  if (mBits & eHaveFontSize) {
    printf("FontSize(%d)", mFontSize);
    first = false;
  }
  if (mBits & eHaveWritingMode) {
    if (!first) {
      printf(", ");
    }
    printf("WritingMode(0x%x)", mWritingMode);
  }
  printf(" }");
}
#endif
