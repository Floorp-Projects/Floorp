/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_AUTOREFERENCELIMITER_H
#define NS_AUTOREFERENCELIMITER_H

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrancyGuard.h"
#include "nsDebug.h"

namespace mozilla {

/**
 * This helper allows us to handle two related issues in SVG content: reference
 * loops, and reference chains that we deem to be too long.
 *
 * SVG content may contain reference loops where an SVG effect (a clipPath,
 * say) may reference itself either directly or, perhaps more likely,
 * indirectly via a reference chain to other elements that eventually leads
 * back to itself.  This helper class allows us to detect and immediately break
 * such reference loops when applying an effect so that we can prevent
 * reference loops causing us to recurse until we run out of stack space and
 * crash.
 *
 * SVG also allows for (non-loop) reference chains of arbitrary length, the
 * length depending entirely on the SVG content.  Some SVG authoring tools have
 * been known to create absurdly long reference chains.  (For example, bug
 * 1253590 details a case where Adobe Illustrator created an SVG with a chain
 * of 5000 clip paths which could cause us to run out of stack space and
 * crash.)  This helper class also allows us to limit the number of times we
 * recurse into a function, thereby allowing us to limit the length ofreference
 * chains.
 *
 * Consumers that need to handle the reference loop case should add a member
 * variable (mReferencing, say) to the class that represents and applies the
 * SVG effect in question (typically an nsIFrame sub-class), initialize that
 * member to AutoReferenceLimiter::notReferencing in the class' constructor
 * (and never touch that variable again), and then add something like the
 * following at the top of the method(s) that may recurse to follow references
 * when applying an effect:
 *
 *   AutoReferenceLimiter refLoopDetector(&mInUse, 1); // only one ref allowed
 *   if (!refLoopDetector.Reference()) {
 *     return; // reference loop
 *   }
 *
 * Consumers that need to limit reference chain lengths should add something
 * like the following code at the top of the method(s) that may recurse to
 * follow references when applying an effect:
 *
 *   static int16_t sChainLengthCounter = AutoReferenceLimiter::notReferencing;
 *
 *   AutoReferenceLimiter refChainLengthLimiter(&sChainLengthCounter, MAX_LEN);
 *   if (!refChainLengthLimiter.Reference()) {
 *     return; // reference chain too long
 *   }
 */
class MOZ_RAII AutoReferenceLimiter
{
public:
  static const int16_t notReferencing = -2;

  AutoReferenceLimiter(int16_t* aRefCounter, int16_t aMaxReferenceCount)
  {
    MOZ_ASSERT(aMaxReferenceCount > 0 &&
               aRefCounter &&
               (*aRefCounter == notReferencing ||
                (*aRefCounter >= 0 && *aRefCounter < aMaxReferenceCount)));

    if (*aRefCounter == notReferencing) {
      // initialize
      *aRefCounter = aMaxReferenceCount;
    }
    mRefCounter = aRefCounter;
    mMaxReferenceCount = aMaxReferenceCount;
  }

  ~AutoReferenceLimiter() {
    // If we fail this assert then there were more destructor calls than
    // Reference() calls (a consumer forgot to to call Reference()), or else
    // someone messed with the variable pointed to by mRefCounter.
    MOZ_ASSERT(*mRefCounter < mMaxReferenceCount);

    (*mRefCounter)++;

    if (*mRefCounter == mMaxReferenceCount) {
      *mRefCounter = notReferencing; // reset ready for use next time
    }
  }

  /**
   * Returns true on success (no reference loop/reference chain length is
   * within the specified limits), else returns false on failure (there is a
   * reference loop/the reference chain has exceeded the specified limits).
   */
  MOZ_MUST_USE bool Reference() {
    // If we fail this assertion then either a consumer failed to break a
    // reference loop/chain, or else they called Reference() more than once
    MOZ_ASSERT(*mRefCounter >= 0);

    (*mRefCounter)--;

    if (*mRefCounter < 0) {
      // TODO: This is an issue with the document, not with Mozilla code. We
      // should stop using NS_WARNING and send a message to the console
      // instead (but only once per document, not over and over as we repaint).
      if (mMaxReferenceCount == 1) {
        NS_WARNING("Reference loop detected!");
      } else {
        NS_WARNING("Reference chain length limit exceeded!");
      }
      return false;
    }
    return true;
  }

private:
  int16_t* mRefCounter;
  int16_t mMaxReferenceCount;
};

} // namespace mozilla

#endif // NS_AUTOREFERENCELIMITER_H
