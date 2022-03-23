/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TableCellAccessible_h__
#define mozilla_a11y_TableCellAccessible_h__

#include "mozilla/a11y/TableCellAccessibleBase.h"
#include "TableAccessible.h"

namespace mozilla {
namespace a11y {

class LocalAccessible;

/**
 * Base class for LocalAccessible table cell implementations.
 */
class TableCellAccessible : public TableCellAccessibleBase {
 public:
  virtual TableAccessible* Table() const override = 0;
  virtual void ColHeaderCells(nsTArray<Accessible*>* aCells) override;
  virtual void RowHeaderCells(nsTArray<Accessible*>* aCells) override;

 private:
  LocalAccessible* PrevColHeader();
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_TableCellAccessible_h__
