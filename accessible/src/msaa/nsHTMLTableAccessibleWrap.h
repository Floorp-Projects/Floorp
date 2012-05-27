/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSHTMLTABLEACCESSIBLEWRAP_H
#define _NSHTMLTABLEACCESSIBLEWRAP_H

#include "nsHTMLTableAccessible.h"

#include "CAccessibleTable.h"
#include "CAccessibleTableCell.h"

/**
 * IA2 wrapper class for nsHTMLTableAccessible implementing IAccessibleTable
 * and IAccessibleTable2 interfaces.
 */
class nsHTMLTableAccessibleWrap : public nsHTMLTableAccessible,
                                  public CAccessibleTable
{
public:
  nsHTMLTableAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    nsHTMLTableAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};


/**
 * IA2 wrapper class for nsHTMLTableCellAccessible implementing
 * IAccessibleTableCell interface.
 */
class nsHTMLTableCellAccessibleWrap : public nsHTMLTableCellAccessible,
                                      public CAccessibleTableCell
{
public:
  nsHTMLTableCellAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    nsHTMLTableCellAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};


/**
 * IA2 wrapper class for nsHTMLTableHeaderCellAccessible implementing
 * IAccessibleTableCell interface.
 */
class nsHTMLTableHeaderCellAccessibleWrap : public nsHTMLTableHeaderCellAccessible,
                                            public CAccessibleTableCell
{
public:
  nsHTMLTableHeaderCellAccessibleWrap(nsIContent* aContent,
                                      DocAccessible* aDoc) :
    nsHTMLTableHeaderCellAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

#endif

