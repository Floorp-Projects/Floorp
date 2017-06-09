/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Algorithms that determine column and table isizes used for CSS2's
 * 'table-layout: fixed'.
 */

#ifndef FixedTableLayoutStrategy_h_
#define FixedTableLayoutStrategy_h_

#include "mozilla/Attributes.h"
#include "nsITableLayoutStrategy.h"

class nsTableFrame;

class FixedTableLayoutStrategy : public nsITableLayoutStrategy
{
public:
  explicit FixedTableLayoutStrategy(nsTableFrame *aTableFrame);
  virtual ~FixedTableLayoutStrategy();

  // nsITableLayoutStrategy implementation
  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext,
                               bool aComputingSize) override;
  virtual void MarkIntrinsicISizesDirty() override;
  virtual void ComputeColumnISizes(const ReflowInput& aReflowInput)
               override;

private:
  nsTableFrame *mTableFrame;
  nscoord mMinISize;
  nscoord mLastCalcISize;
};

#endif /* !defined(FixedTableLayoutStrategy_h_) */
