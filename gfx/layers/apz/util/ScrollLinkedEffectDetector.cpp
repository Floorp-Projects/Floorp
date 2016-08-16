/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollLinkedEffectDetector.h"

#include "nsIDocument.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

uint32_t ScrollLinkedEffectDetector::sDepth = 0;
bool ScrollLinkedEffectDetector::sFoundScrollLinkedEffect = false;

/* static */ void
ScrollLinkedEffectDetector::PositioningPropertyMutated()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sDepth > 0) {
    // We are inside a scroll event dispatch
    sFoundScrollLinkedEffect = true;
  }
}

ScrollLinkedEffectDetector::ScrollLinkedEffectDetector(nsIDocument* aDoc)
  : mDocument(aDoc)
{
  MOZ_ASSERT(NS_IsMainThread());
  sDepth++;
}

ScrollLinkedEffectDetector::~ScrollLinkedEffectDetector()
{
  sDepth--;
  if (sDepth == 0) {
    // We have exited all (possibly-nested) scroll event dispatches,
    // record whether or not we found an effect, and reset state
    if (sFoundScrollLinkedEffect) {
      mDocument->ReportHasScrollLinkedEffect();
      sFoundScrollLinkedEffect = false;
    }
  }
}

} // namespace layers
} // namespace mozilla
