/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAGridAccessible.h"

#include <stdint.h>
#include "LocalAccessible-inl.h"
#include "AccAttributes.h"
#include "mozilla/a11y/TableAccessible.h"
#include "mozilla/a11y/TableCellAccessible.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsGkAtoms.h"
#include "mozilla/a11y/Role.h"
#include "States.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ARIAGridCellAccessible
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Constructor

ARIAGridCellAccessible::ARIAGridCellAccessible(nsIContent* aContent,
                                               DocAccessible* aDoc)
    : HyperTextAccessible(aContent, aDoc) {
  mGenericTypes |= eTableCell;
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

void ARIAGridCellAccessible::ApplyARIAState(uint64_t* aState) const {
  HyperTextAccessible::ApplyARIAState(aState);

  // Return if the gridcell has aria-selected="true".
  if (*aState & states::SELECTED) return;

  // Check aria-selected="true" on the row.
  LocalAccessible* row = LocalParent();
  if (!row || row->Role() != roles::ROW) return;

  nsIContent* rowContent = row->GetContent();
  if (nsAccUtils::HasDefinedARIAToken(rowContent, nsGkAtoms::aria_selected) &&
      !nsAccUtils::ARIAAttrValueIs(rowContent->AsElement(),
                                   nsGkAtoms::aria_selected, nsGkAtoms::_false,
                                   eCaseMatters)) {
    *aState |= states::SELECTABLE | states::SELECTED;
  }
}

already_AddRefed<AccAttributes> ARIAGridCellAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes = HyperTextAccessible::NativeAttributes();

  // We only need to expose table-cell-index to clients. If we're in the content
  // process, we don't need this, so building a CachedTableAccessible is very
  // wasteful. This will be exposed by RemoteAccessible in the parent process
  // instead.
  if (!IPCAccessibilityActive()) {
    if (const TableCellAccessible* cell = AsTableCell()) {
      TableAccessible* table = cell->Table();
      const uint32_t row = cell->RowIdx();
      const uint32_t col = cell->ColIdx();
      const int32_t cellIdx = table->CellIndexAt(row, col);
      if (cellIdx != -1) {
        attributes->SetAttribute(nsGkAtoms::tableCellIndex, cellIdx);
      }
    }
  }

  return attributes.forget();
}
