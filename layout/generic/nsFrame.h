/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class of all rendering objects */

#ifndef nsFrame_h___
#define nsFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Likely.h"
#include "mozilla/Logging.h"

#include "mozilla/ReflowInput.h"
#include "nsHTMLParts.h"
#include "nsISelectionDisplay.h"

namespace mozilla {
enum class TableSelectionMode : uint32_t;
class PresShell;
}  // namespace mozilla

struct nsBoxLayoutMetrics;
struct nsRect;

/**
 * Implementation of a simple frame that's not splittable and has no
 * child frames.
 *
 * Sets the NS_FRAME_SYNCHRONIZE_FRAME_AND_VIEW bit, so the default
 * behavior is to keep the frame and view position and size in sync.
 */
class nsFrame : public nsIFrame {
 public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  friend nsIFrame* NS_NewEmptyFrame(mozilla::PresShell* aShell,
                                    ComputedStyle* aStyle);

 private:
  // Left undefined; nsFrame objects are never allocated from the heap.
  void* operator new(size_t sz) noexcept(true);

 protected:
  // Overridden to prevent the global delete from being called, since
  // the memory came out of an arena instead of the heap.
  //
  // Ideally this would be private and undefined, like the normal
  // operator new.  Unfortunately, the C++ standard requires an
  // overridden operator delete to be accessible to any subclass that
  // defines a virtual destructor, so we can only make it protected;
  // worse, some C++ compilers will synthesize calls to this function
  // from the "deleting destructors" that they emit in case of
  // delete-expressions, so it can't even be undefined.
  void operator delete(void* aPtr, size_t sz);

 public:
  // nsQueryFrame
  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(nsFrame)
  nsQueryFrame::FrameIID GetFrameId() const override MOZ_MUST_OVERRIDE {
    return kFrameIID;
  }
  void* operator new(size_t, mozilla::PresShell*) MOZ_MUST_OVERRIDE;

 protected:
  // Protected constructor and destructor
  nsFrame(ComputedStyle* aStyle, nsPresContext* aPresContext, ClassID aID);
  explicit nsFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsFrame(aStyle, aPresContext, ClassID::nsFrame_id) {}
};

#endif /* nsFrame_h___ */
