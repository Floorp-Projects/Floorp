/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextRange.h"

#include "HyperTextAccessible.h"

using namespace mozilla::a11y;

TextRange::TextRange(HyperTextAccessible* aRoot,
                     Accessible* aStartContainer, int32_t aStartOffset,
                     Accessible* aEndContainer, int32_t aEndOffset) :
  mRoot(aRoot), mStartContainer(aStartContainer), mEndContainer(aEndContainer),
  mStartOffset(aStartOffset), mEndOffset(aEndOffset)
{
}

void
TextRange::Text(nsAString& aText) const
{

}
