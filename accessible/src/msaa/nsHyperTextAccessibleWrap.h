/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSHYPERTEXTACCESSIBLEWRAP_H
#define _NSHYPERTEXTACCESSIBLEWRAP_H

#include "nsHyperTextAccessible.h"
#include "CAccessibleText.h"
#include "CAccessibleEditableText.h"
#include "ia2AccessibleHyperText.h"

class nsHyperTextAccessibleWrap : public nsHyperTextAccessible,
                                  public ia2AccessibleHypertext,
                                  public CAccessibleEditableText
{
public:
  nsHyperTextAccessibleWrap(nsIContent* aContent, nsDocAccessible* aDoc) :
    nsHyperTextAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsAccessible
  virtual nsresult HandleAccEvent(AccEvent* aEvent);

protected:
  virtual nsresult GetModifiedText(bool aGetInsertedText, nsAString& aText,
                                   PRUint32 *aStartOffset,
                                   PRUint32 *aEndOffset);
};

#endif

