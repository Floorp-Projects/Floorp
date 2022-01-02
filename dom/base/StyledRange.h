/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BASE_STYLED_RANGE_H_
#define DOM_BASE_STYLED_RANGE_H_

#include "mozilla/RefPtr.h"
#include "mozilla/TextRange.h"

class nsRange;

struct StyledRange {
  explicit StyledRange(nsRange* aRange);

  RefPtr<nsRange> mRange;
  mozilla::TextRangeStyle mTextRangeStyle;
};

#endif  // DOM_BASE_STYLED_RANGE_H_
