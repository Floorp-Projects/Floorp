/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TableCellAccessible_h__
#define mozilla_a11y_TableCellAccessible_h__

#include "nsTArray.h"
#include <stdint.h>

namespace mozilla {
namespace a11y {

class Accessible;
class TableAccessible;

/**
 * Abstract interface implemented by table cell accessibles.
 */
class TableCellAccessible
{
public:

  /**
   * Return the table this cell is in.
   */
  virtual TableAccessible* Table() const = 0;

  /**
   * Return the column of the table this cell is in.
   */
  virtual uint32_t ColIdx() const = 0;

  /**
   * Return the row of the table this cell is in.
   */
  virtual uint32_t RowIdx() const = 0;

  /**
   * Return the column extent of this cell.
   */
  virtual uint32_t ColExtent() const { return 1; }

  /**
   * Return the row extent of this cell.
   */
  virtual uint32_t RowExtent() const { return 1; }

  /**
   * Return the column header cells for this cell.
   */
  virtual void ColHeaderCells(nsTArray<Accessible*>* aCells);

  /**
   * Return the row header cells for this cell.
   */
  virtual void RowHeaderCells(nsTArray<Accessible*>* aCells);

  /**
   * Returns true if this cell is selected.
   */
  virtual bool Selected() = 0;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_TableCellAccessible_h__
