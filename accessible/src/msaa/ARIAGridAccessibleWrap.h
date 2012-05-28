/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_ARIAGRIDACCESSIBLEWRAP_H
#define MOZILLA_A11Y_ARIAGRIDACCESSIBLEWRAP_H

#include "ARIAGridAccessible.h"
#include "CAccessibleTable.h"
#include "CAccessibleTableCell.h"

namespace mozilla {
namespace a11y {

/**
 * IA2 wrapper class for ARIAGridAccessible implementing IAccessibleTable and
 * IAccessibleTable2 interfaces.
 */
class ARIAGridAccessibleWrap : public ARIAGridAccessible,
                               public CAccessibleTable
{
public:
  ARIAGridAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    ARIAGridAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

/**
 * IA2 wrapper class for ARIAGridCellAccessible implementing
 * IAccessibleTableCell interface.
 */
class ARIAGridCellAccessibleWrap : public ARIAGridCellAccessible,
                                   public CAccessibleTableCell
{
public:
  ARIAGridCellAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    ARIAGridCellAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

} // namespace a11y
} // namespace mozilla

#endif
