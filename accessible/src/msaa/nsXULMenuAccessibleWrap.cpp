/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULMenuAccessibleWrap.h"
#include "nsINameSpaceManager.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULMenuAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

nsXULMenuitemAccessibleWrap::
  nsXULMenuitemAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  nsXULMenuitemAccessible(aContent, aDoc)
{
}

ENameValueFlag
nsXULMenuitemAccessibleWrap::Name(nsString& aName)
{
  // XXX This should be done in get_accName() so that nsIAccessible::GetName()]
  // provides the same results on all platforms
  nsXULMenuitemAccessible::Name(aName);
  if (aName.IsEmpty())
    return eNameOK;
  
  nsAutoString accel;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, accel);
  if (!accel.IsEmpty())
    aName += NS_LITERAL_STRING("\t") + accel;

  return eNameOK;
}
