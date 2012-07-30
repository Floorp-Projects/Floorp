/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TableCellAccessible_h__
#define mozilla_a11y_TableCellAccessible_h__

#include "nsTArray.h"
#include "mozilla/StandardInteger.h"

class Accessible;

namespace mozilla {
namespace a11y {

  class TableAccessible;

/**
 * abstract interface implemented by table cell accessibles.
 */
class TableCellAccessible
{
public:

  /**
   * Return the table this cell is in.
   */
  virtual TableAccessible* Table() { return nullptr; }

  /**
   * Return the Column of the table this cell is in.
   */
  virtual uint32_t ColIdx() { return 0; }

  /**
   * Return the the row of the table this cell is in.
   */
  virtual uint32_t RowIdx() { return 0; }

  /**
   * Return the column extent of this cell.
   */
  virtual uint32_t ColExtent() { return 0; }

  /**
   * Return the row extent of this cell.
   */
  virtual uint32_t RowExtent() { return 0; }

  /**
   * Return the column header cells for this cell.
   */
  virtual void ColHeaderCells(nsTArray<Accessible*>* aCells) { }

  /**
   * Return the row header cells for this cell.
   */
  virtual void RowHeaderCells(nsTArray<Accessible*>* aCells) { }

  /**
   * Returns true if this cell is selected.
   */
  virtual bool Selected() { return false; }
};

}
}

#endif // mozilla_a11y_TableCellAccessible_h__
