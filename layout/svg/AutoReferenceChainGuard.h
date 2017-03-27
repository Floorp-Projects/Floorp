/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_AUTOREFERENCELIMITER_H
#define NS_AUTOREFERENCELIMITER_H

#include "Element.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrancyGuard.h"
#include "mozilla/Likely.h"
#include "nsDebug.h"
#include "nsIDocument.h"
#include "nsIFrame.h"

namespace mozilla {

/**
 * This helper class helps us to protect against two related issues that can
 * occur in SVG content: reference loops, and reference chains that we deem to
 * be too long.
 *
 * Some SVG effects can reference another effect of the same type to produce
 * a chain of effects to be applied (e.g. clipPath), while in other cases it is
 * possible that while processing an effect of a certain type another effect
 * of the same type may be encountered indirectly (e.g. pattern).  In order to
 * avoid stack overflow crashes and performance issues we need to impose an
 * arbitrary limit on the length of the reference chains that SVG content may
 * try to create.  (Some SVG authoring tools have been known to create absurdly
 * long reference chains.  For example, bug 1253590 details a case where Adobe
 * Illustrator was used to created an SVG with a chain of 5000 clip paths which
 * could cause us to run out of stack space and crash.)
 *
 * This class is intended to be used with the nsIFrame's of SVG effects that
 * may involve reference chains.  To use it add a boolean member, something
 * like this:
 *
 *   // Flag used to indicate whether a methods that may reenter due to
 *   // following a reference to another instance is currently executing.
 *   bool mIsBeingProcessed;
 *
 * Make sure to initialize the member to false in the class' constructons.
 *
 * Then add the following to the top of any methods that may be reentered due
 * to following a reference:
 *
 *   static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
 *
 *   AutoReferenceChainGuard refChainGuard(this, &mIsBeingProcessed,
 *                                         &sRefChainLengthCounter);
 *   if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
 *     return; // Break reference chain
 *   }
 *
 * Note that mIsBeingProcessed and sRefChainLengthCounter should never be used
 * by the frame except when it initialize them as indicated above.
 */
class MOZ_RAII AutoReferenceChainGuard
{
  static const int16_t sDefaultMaxChainLength = 10; // arbitrary length

public:
  static const int16_t noChain = -2;

  /**
   * @param aFrame The frame for an effect that may involve a reference chain.
   * @param aFrameInUse The member variable on aFrame that is used to indicate
   *   whether one of aFrame's methods that may involve following a reference
   *   to another effect of the same type is currently being executed.
   * @param aChainCounter A static variable in the method in which this class
   *   is instantiated that is used to keep track of how many times the method
   *   is reentered (and thus how long the a reference chain is).
   * @param aMaxChainLength The maximum number of links that are allowed in
   *   a reference chain.
   */
  AutoReferenceChainGuard(nsIFrame* aFrame,
                          bool* aFrameInUse,
                          int16_t* aChainCounter,
                          int16_t aMaxChainLength = sDefaultMaxChainLength
                          MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mFrame(aFrame)
    , mFrameInUse(aFrameInUse)
    , mMaxChainLength(aMaxChainLength)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(aFrame && aFrameInUse && aChainCounter);
    MOZ_ASSERT(!(*mFrameInUse), "Undetected reference loop!");
    MOZ_ASSERT(aMaxChainLength > 0);
    MOZ_ASSERT(*aChainCounter == noChain ||
               (*aChainCounter >= 0 && *aChainCounter < aMaxChainLength));

    if (*aChainCounter == noChain) {
      // Initialize - we start at aMaxChainLength and decrement towards zero.
      *aChainCounter = aMaxChainLength;
    }
    mChainCounter = aChainCounter;
  }

  ~AutoReferenceChainGuard() {
    *mFrameInUse = false;

    // If we fail this assert then there were more destructor calls than
    // Reference() calls (a consumer forgot to to call Reference()), or else
    // someone messed with the variable pointed to by mChainCounter.
    MOZ_ASSERT(*mChainCounter < mMaxChainLength);

    (*mChainCounter)++;

    if (*mChainCounter == mMaxChainLength) {
      *mChainCounter = noChain; // reset ready for use next time
    }
  }

  /**
   * Returns true on success (no reference loop/reference chain length is
   * within the specified limits), else returns false on failure (there is a
   * reference loop/the reference chain has exceeded the specified limits).
   * If it returns false then an error message will be reported to the DevTools
   * console (only once).
   */
  MOZ_MUST_USE bool Reference() {
    if (MOZ_UNLIKELY(*mFrameInUse)) {
      ReportErrorToConsole();
      return false;
    }

    // If we fail this assertion then either a consumer failed to break a
    // reference loop/chain, or else they called Reference() more than once
    MOZ_ASSERT(*mChainCounter >= 0);

    (*mChainCounter)--;

    if (MOZ_UNLIKELY(*mChainCounter < 0)) {
      ReportErrorToConsole();
      return false;
    }
    return true;
  }

private:
  void ReportErrorToConsole() {
    nsAutoString tag;
    mFrame->GetContent()->AsElement()->GetTagName(tag);
    const char16_t* params[] = { tag.get() };
    auto doc = mFrame->GetContent()->OwnerDoc();
    auto warning = *mFrameInUse ?
                     nsIDocument::eSVGReferenceLoop :
                     nsIDocument::eSVGReferenceChainLengthExceeded;
    doc->WarnOnceAbout(warning, /* asError */ true,
                       params, ArrayLength(params));
  }

  nsIFrame* mFrame;
  bool* mFrameInUse;
  int16_t* mChainCounter;
  const int16_t mMaxChainLength;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} // namespace mozilla

#endif // NS_AUTOREFERENCELIMITER_H
