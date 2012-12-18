/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextLeafAccessible.h"

#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Role.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// TextLeafAccessible
////////////////////////////////////////////////////////////////////////////////

TextLeafAccessible::
  TextLeafAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LinkableAccessible(aContent, aDoc)
{
  mType = eTextLeafType;
}

TextLeafAccessible::~TextLeafAccessible()
{
}

role
TextLeafAccessible::NativeRole()
{
  nsIFrame* frame = GetFrame();
  if (frame && frame->IsGeneratedContentFrame()) 
    return roles::STATICTEXT;

  return roles::TEXT_LEAF;
}

void
TextLeafAccessible::AppendTextTo(nsAString& aText, uint32_t aStartOffset,
                                 uint32_t aLength)
{
  aText.Append(Substring(mText, aStartOffset, aLength));
}

ENameValueFlag
TextLeafAccessible::Name(nsString& aName)
{
  // Text node, ARIA can't be used.
  aName = mText;
  return eNameOK;
}

void
TextLeafAccessible::CacheChildren()
{
  // No children for text accessible.
}
