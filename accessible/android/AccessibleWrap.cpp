/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "DocAccessibleWrap.h"
#include "IDSet.h"
#include "nsAccessibilityService.h"

using namespace mozilla::a11y;

// IDs should be a positive 32bit integer.
IDSet sIDSet(31UL);

//-----------------------------------------------------
// construction
//-----------------------------------------------------
AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
  : Accessible(aContent, aDoc)
{
  if (aDoc) {
    mID = AcquireID();
    DocAccessibleWrap* doc = static_cast<DocAccessibleWrap*>(aDoc);
    doc->AddID(mID, this);
  }
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
AccessibleWrap::~AccessibleWrap() {}

void
AccessibleWrap::Shutdown()
{
  if (mDoc) {
    if (mID > 0) {
      if (auto doc = static_cast<DocAccessibleWrap*>(mDoc.get())) {
        doc->RemoveID(mID);
      }
      ReleaseID(mID);
      mID = 0;
    }
  }

  Accessible::Shutdown();
}

int32_t
AccessibleWrap::AcquireID()
{
  return sIDSet.GetID();
}

void
AccessibleWrap::ReleaseID(int32_t aID)
{
  sIDSet.ReleaseID(aID);
}
