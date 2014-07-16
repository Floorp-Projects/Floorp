/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLTableAccessibleWrap_h__
#define mozilla_a11y_HTMLTableAccessibleWrap_h__

#include "HTMLTableAccessible.h"

#include "ia2AccessibleTable.h"
#include "ia2AccessibleTableCell.h"

namespace mozilla {
namespace a11y {

/**
 * IA2 wrapper class for HTMLTableAccessible implementing IAccessibleTable
 * and IAccessibleTable2 interfaces.
 */
class HTMLTableAccessibleWrap : public HTMLTableAccessible,
                                public ia2AccessibleTable
{
  ~HTMLTableAccessibleWrap() {}

public:
  HTMLTableAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    HTMLTableAccessible(aContent, aDoc), ia2AccessibleTable(this)  {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Shutdown() MOZ_OVERRIDE;
};


/**
 * IA2 wrapper class for HTMLTableCellAccessible implementing
 * IAccessibleTableCell interface.
 */
class HTMLTableCellAccessibleWrap : public HTMLTableCellAccessible,
                                    public ia2AccessibleTableCell
{
  ~HTMLTableCellAccessibleWrap() {}

public:
  HTMLTableCellAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    HTMLTableCellAccessible(aContent, aDoc), ia2AccessibleTableCell(this) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Shutdown() MOZ_OVERRIDE;
};


/**
 * IA2 wrapper class for HTMLTableHeaderCellAccessible implementing
 * IAccessibleTableCell interface.
 */
class HTMLTableHeaderCellAccessibleWrap : public HTMLTableHeaderCellAccessible,
                                          public ia2AccessibleTableCell
{
  ~HTMLTableHeaderCellAccessibleWrap() {}

public:
  HTMLTableHeaderCellAccessibleWrap(nsIContent* aContent,
                                    DocAccessible* aDoc) :
    HTMLTableHeaderCellAccessible(aContent, aDoc), ia2AccessibleTableCell(this)
  {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Shutdown() MOZ_OVERRIDE;
};

} // namespace a11y
} // namespace mozilla

#endif

