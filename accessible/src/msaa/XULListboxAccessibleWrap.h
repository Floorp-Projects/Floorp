/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULListboxAccessibleWrap_h__
#define mozilla_a11y_XULListboxAccessibleWrap_h__

#include "XULListboxAccessible.h"

#include "CAccessibleTable.h"
#include "CAccessibleTableCell.h"

namespace mozilla {
namespace a11y {

/**
 * IA2 wrapper class for XULListboxAccessible class implementing
 * IAccessibleTable and IAccessibleTable2 interfaces.
 */
class XULListboxAccessibleWrap : public XULListboxAccessible,
                                 public CAccessibleTable
{
public:
  XULListboxAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

/**
 * IA2 wrapper class for XULListCellAccessible class, implements
 * IAccessibleTableCell interface.
 */
class XULListCellAccessibleWrap : public XULListCellAccessible,
                                  public CAccessibleTableCell
{
public:
  XULListCellAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

} // namespace a11y
} // namespace mozilla

#endif
