/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextAccessible.h"

#include "Role.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsTextAccessible
////////////////////////////////////////////////////////////////////////////////

nsTextAccessible::
  nsTextAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsLinkableAccessible(aContent, aDoc)
{
  mFlags |= eTextLeafAccessible;
}

role
nsTextAccessible::NativeRole()
{
  return roles::TEXT_LEAF;
}

void
nsTextAccessible::AppendTextTo(nsAString& aText, PRUint32 aStartOffset,
                               PRUint32 aLength)
{
  aText.Append(Substring(mText, aStartOffset, aLength));
}

void
nsTextAccessible::CacheChildren()
{
  // No children for text accessible.
}
