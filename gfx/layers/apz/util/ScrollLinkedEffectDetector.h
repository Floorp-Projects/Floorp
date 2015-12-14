/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ScrollLinkedEffectDetector_h
#define mozilla_layers_ScrollLinkedEffectDetector_h

#include "mozilla/RefPtr.h"

class nsIDocument;

namespace mozilla {
namespace layers {

// ScrollLinkedEffectDetector is used to detect the existence of a scroll-linked
// effect on a webpage. Generally speaking, a scroll-linked effect is something
// on the page that animates or changes with respect to the scroll position.
// Content authors usually rely on running some JS in response to the scroll
// event in order to implement such effects, and therefore it tends to be laggy
// or work improperly with APZ enabled. This class helps us detect such an
// effect so that we can warn the author and/or take other preventative
// measures.
class MOZ_STACK_CLASS ScrollLinkedEffectDetector
{
private:
  static uint32_t sDepth;
  static bool sFoundScrollLinkedEffect;

public:
  static void PositioningPropertyMutated();

  ScrollLinkedEffectDetector(nsIDocument* aDoc);
  ~ScrollLinkedEffectDetector();

private:
  RefPtr<nsIDocument> mDocument;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_ScrollLinkedEffectDetector_h */
