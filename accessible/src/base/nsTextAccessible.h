/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsTextAccessible_H_
#define _nsTextAccessible_H_

#include "nsBaseWidgetAccessible.h"

/**
 * Generic class used for text nodes.
 */
class nsTextAccessible : public nsLinkableAccessible
{
public:
  nsTextAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual void AppendTextTo(nsAString& aText, PRUint32 aStartOffset = 0,
                            PRUint32 aLength = PR_UINT32_MAX);

  // nsTextAccessible
  void SetText(const nsAString& aText) { mText = aText; }
  const nsString& Text() const { return mText; }

protected:
  // nsAccessible
  virtual void CacheChildren();

protected:
  nsString mText;
};


////////////////////////////////////////////////////////////////////////////////
// nsAccessible downcast method

inline nsTextAccessible*
nsAccessible::AsTextLeaf()
{
  return mFlags & eTextLeafAccessible ?
    static_cast<nsTextAccessible*>(this) : nsnull;
}

#endif

