/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULMenuAccessibleWrap.h"
#include "nsNameSpaceManager.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULMenuAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

XULMenuitemAccessibleWrap::
  XULMenuitemAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  XULMenuitemAccessible(aContent, aDoc)
{
}

ENameValueFlag
XULMenuitemAccessibleWrap::Name(nsString& aName)
{
  // XXX This should be done in MSAA's get_accName() so that Accessible::Name()]
  // provides the same results on all platforms
  XULMenuitemAccessible::Name(aName);
  if (aName.IsEmpty())
    return eNameOK;

  nsAutoString accel;
  if (mContent->IsElement()) {
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, accel);
  }
  if (!accel.IsEmpty())
    aName += NS_LITERAL_STRING("\t") + accel;

  return eNameOK;
}
