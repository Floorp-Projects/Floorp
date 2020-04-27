/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txToplevelItems.h"

#include <utility>

#include "txInstructions.h"
#include "txStylesheet.h"
#include "txXSLTPatterns.h"

using mozilla::UniquePtr;

TX_IMPL_GETTYPE(txAttributeSetItem, txToplevelItem::attributeSet)
TX_IMPL_GETTYPE(txImportItem, txToplevelItem::import)
TX_IMPL_GETTYPE(txOutputItem, txToplevelItem::output)
TX_IMPL_GETTYPE(txDummyItem, txToplevelItem::dummy)

TX_IMPL_GETTYPE(txStripSpaceItem, txToplevelItem::stripSpace)

txStripSpaceItem::~txStripSpaceItem() {
  int32_t i, count = mStripSpaceTests.Length();
  for (i = 0; i < count; ++i) {
    delete mStripSpaceTests[i];
  }
}

nsresult txStripSpaceItem::addStripSpaceTest(
    txStripSpaceTest* aStripSpaceTest) {
  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier, or change the return type to void.
  mStripSpaceTests.AppendElement(aStripSpaceTest);
  return NS_OK;
}

TX_IMPL_GETTYPE(txTemplateItem, txToplevelItem::templ)

txTemplateItem::txTemplateItem(UniquePtr<txPattern>&& aMatch,
                               const txExpandedName& aName,
                               const txExpandedName& aMode, double aPrio)
    : mMatch(std::move(aMatch)), mName(aName), mMode(aMode), mPrio(aPrio) {}

TX_IMPL_GETTYPE(txVariableItem, txToplevelItem::variable)

txVariableItem::txVariableItem(const txExpandedName& aName,
                               UniquePtr<Expr>&& aValue, bool aIsParam)
    : mName(aName), mValue(std::move(aValue)), mIsParam(aIsParam) {}
