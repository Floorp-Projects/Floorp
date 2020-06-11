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

/**
 * nsFrame logging constants. We redefine the nspr
 * PRLogModuleInfo.level field to be a bitfield.  Each bit controls a
 * specific type of logging. Each logging operation has associated
 * inline methods defined below.
 *
 * Due to the redefinition of the level field we cannot use MOZ_LOG directly
 * as that will cause assertions due to invalid log levels.
 */
#define NS_FRAME_TRACE_CALLS 0x1
#define NS_FRAME_TRACE_PUSH_PULL 0x2
#define NS_FRAME_TRACE_CHILD_REFLOW 0x4
#define NS_FRAME_TRACE_NEW_FRAMES 0x8

#define NS_FRAME_LOG_TEST(_lm, _bit) \
  (int(((mozilla::LogModule*)_lm)->Level()) & (_bit))

#ifdef DEBUG
#  define NS_FRAME_LOG(_bit, _args)                          \
    PR_BEGIN_MACRO                                           \
    if (NS_FRAME_LOG_TEST(nsFrame::sFrameLogModule, _bit)) { \
      printf_stderr _args;                                   \
    }                                                        \
    PR_END_MACRO
#else
#  define NS_FRAME_LOG(_bit, _args)
#endif

// XXX Need to rework this so that logging is free when it's off
#ifdef DEBUG
#  define NS_FRAME_TRACE_IN(_method) Trace(_method, true)

#  define NS_FRAME_TRACE_OUT(_method) Trace(_method, false)

#  define NS_FRAME_TRACE(_bit, _args)                        \
    PR_BEGIN_MACRO                                           \
    if (NS_FRAME_LOG_TEST(nsFrame::sFrameLogModule, _bit)) { \
      TraceMsg _args;                                        \
    }                                                        \
    PR_END_MACRO

#  define NS_FRAME_TRACE_REFLOW_IN(_method) Trace(_method, true)

#  define NS_FRAME_TRACE_REFLOW_OUT(_method, _status) \
    Trace(_method, false, _status)

#else
#  define NS_FRAME_TRACE(_bits, _args)
#  define NS_FRAME_TRACE_IN(_method)
#  define NS_FRAME_TRACE_OUT(_method)
#  define NS_FRAME_TRACE_REFLOW_IN(_method)
#  define NS_FRAME_TRACE_REFLOW_OUT(_method, _status)
#endif

//----------------------------------------------------------------------

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

  // nsIFrame
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  void DidReflow(nsPresContext* aPresContext,
                 const ReflowInput* aReflowInput) override;

  //--------------------------------------------------
  // Additional methods

#ifdef DEBUG
  /**
   * Tracing method that writes a method enter/exit routine to the
   * nspr log using the nsIFrame log module. The tracing is only
   * done when the NS_FRAME_TRACE_CALLS bit is set in the log module's
   * level field.
   */
  void Trace(const char* aMethod, bool aEnter);
  void Trace(const char* aMethod, bool aEnter, const nsReflowStatus& aStatus);
  void TraceMsg(const char* fmt, ...) MOZ_FORMAT_PRINTF(2, 3);

  // Helper function that verifies that each frame in the list has the
  // NS_FRAME_IS_DIRTY bit set
  static void VerifyDirtyBitSet(const nsFrameList& aFrameList);

  // Display Reflow Debugging
  static void* DisplayReflowEnter(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  const ReflowInput& aReflowInput);
  static void* DisplayLayoutEnter(nsIFrame* aFrame);
  static void* DisplayIntrinsicISizeEnter(nsIFrame* aFrame, const char* aType);
  static void* DisplayIntrinsicSizeEnter(nsIFrame* aFrame, const char* aType);
  static void DisplayReflowExit(nsPresContext* aPresContext, nsIFrame* aFrame,
                                ReflowOutput& aMetrics,
                                const nsReflowStatus& aStatus,
                                void* aFrameTreeNode);
  static void DisplayLayoutExit(nsIFrame* aFrame, void* aFrameTreeNode);
  static void DisplayIntrinsicISizeExit(nsIFrame* aFrame, const char* aType,
                                        nscoord aResult, void* aFrameTreeNode);
  static void DisplayIntrinsicSizeExit(nsIFrame* aFrame, const char* aType,
                                       nsSize aResult, void* aFrameTreeNode);

  static void DisplayReflowStartup();
  static void DisplayReflowShutdown();
#endif

  /**
   * Adds display items for standard CSS background if necessary.
   * Does not check IsVisibleForPainting.
   * @param aForceBackground draw the background even if the frame
   * background style appears to have no background --- this is useful
   * for frames that might receive a propagated background via
   * nsCSSRendering::FindBackground
   * @return whether a themed background item was created.
   */
  bool DisplayBackgroundUnconditional(nsDisplayListBuilder* aBuilder,
                                      const nsDisplayListSet& aLists,
                                      bool aForceBackground);
  /**
   * Adds display items for standard CSS borders, background and outline for
   * for this frame, as necessary. Checks IsVisibleForPainting and won't
   * display anything if the frame is not visible.
   * @param aForceBackground draw the background even if the frame
   * background style appears to have no background --- this is useful
   * for frames that might receive a propagated background via
   * nsCSSRendering::FindBackground
   */
  void DisplayBorderBackgroundOutline(nsDisplayListBuilder* aBuilder,
                                      const nsDisplayListSet& aLists,
                                      bool aForceBackground = false);
  /**
   * Add a display item for the CSS outline. Does not check visibility.
   */
  void DisplayOutlineUnconditional(nsDisplayListBuilder* aBuilder,
                                   const nsDisplayListSet& aLists);
  /**
   * Add a display item for the CSS outline, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  void DisplayOutline(nsDisplayListBuilder* aBuilder,
                      const nsDisplayListSet& aLists);

  /**
   * Add a display item for CSS inset box shadows. Does not check visibility.
   */
  void DisplayInsetBoxShadowUnconditional(nsDisplayListBuilder* aBuilder,
                                          nsDisplayList* aList);

  /**
   * Add a display item for CSS inset box shadow, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  void DisplayInsetBoxShadow(nsDisplayListBuilder* aBuilder,
                             nsDisplayList* aList);

  /**
   * Add a display item for CSS outset box shadows. Does not check visibility.
   */
  void DisplayOutsetBoxShadowUnconditional(nsDisplayListBuilder* aBuilder,
                                           nsDisplayList* aList);

  /**
   * Add a display item for CSS outset box shadow, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  void DisplayOutsetBoxShadow(nsDisplayListBuilder* aBuilder,
                              nsDisplayList* aList);

 protected:
  // Protected constructor and destructor
  nsFrame(ComputedStyle* aStyle, nsPresContext* aPresContext, ClassID aID);
  explicit nsFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsFrame(aStyle, aPresContext, ClassID::nsFrame_id) {}
  virtual ~nsFrame();

 private:
  // Returns true if this frame has any kind of CSS animations.
  bool HasCSSAnimations();

  // Returns true if this frame has any kind of CSS transitions.
  bool HasCSSTransitions();
};

#endif /* nsFrame_h___ */
