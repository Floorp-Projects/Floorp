/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULListboxAccessibleWrap_h__
#define mozilla_a11y_XULListboxAccessibleWrap_h__

#include "XULListboxAccessible.h"

#include "ia2AccessibleTable.h"
#include "ia2AccessibleTableCell.h"

namespace mozilla {
namespace a11y {

/**
 * IA2 wrapper class for XULListboxAccessible class implementing
 * IAccessibleTable and IAccessibleTable2 interfaces.
 */
class XULListboxAccessibleWrap : public XULListboxAccessible,
                                 public ia2AccessibleTable
{
  ~XULListboxAccessibleWrap() {}

public:
  XULListboxAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    XULListboxAccessible(aContent, aDoc), ia2AccessibleTable(this) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Shutdown() override;
};

} // namespace a11y
} // namespace mozilla

#endif
