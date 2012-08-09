/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessNodeWrap.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */


////////////////////////////////////////////////////////////////////////////////
// Class nsAccessNodeWrap
////////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------
// construction 
//-----------------------------------------------------

nsAccessNodeWrap::
  nsAccessNodeWrap(nsIContent* aContent, DocAccessible* aDoc) :
  nsAccessNode(aContent, aDoc)
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessNodeWrap::~nsAccessNodeWrap()
{
}

void nsAccessNodeWrap::InitAccessibility()
{
}

void nsAccessNodeWrap::ShutdownAccessibility()
{
  nsAccessNode::ShutdownXPAccessibility();
}

