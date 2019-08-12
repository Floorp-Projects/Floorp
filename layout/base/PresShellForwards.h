/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PresShellForwards_h
#define mozilla_PresShellForwards_h

#include "mozilla/TypedEnumBits.h"

struct CapturingContentInfo;

namespace mozilla {

class PresShell;

// Flags to pass to PresShell::SetCapturingContent().
enum class CaptureFlags {
  None = 0,
  // When assigning capture, ignore whether capture is allowed or not.
  IgnoreAllowedState = 1 << 0,
  // Set if events should be targeted at the capturing content or its children.
  RetargetToElement = 1 << 1,
  // Set if the current capture wants drags to be prevented.
  PreventDragStart = 1 << 2,
  // Set when the mouse is pointer locked, and events are sent to locked
  // element.
  PointerLock = 1 << 3,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CaptureFlags)

enum class ResizeReflowOptions : uint32_t {
  NoOption = 0,
  // the resulting BSize can be less than the given one, producing
  // shrink-to-fit sizing in the block dimension
  BSizeLimit = 1 << 0,
  // suppress resize events even if the content size is changed due to the
  // reflow.  This flag is used for mobile since on mobile we need to do an
  // additional reflow to zoom the content by the initial-scale or auto scaling
  // and we don't want any resize events during the initial paint.
  SuppressResizeEvent = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ResizeReflowOptions)

// This is actually pref-controlled, but we use this value if we fail to get
// the pref for any reason.
#define PAINTLOCK_EVENT_DELAY 5

enum class IntrinsicDirty {
  // XXXldb eResize should be renamed
  Resize,       // don't mark any intrinsic widths dirty
  TreeChange,   // mark intrinsic widths dirty on aFrame and its ancestors
  StyleChange,  // Do eTreeChange, plus all of aFrame's descendants
};

enum class ReflowRootHandling {
  PositionOrSizeChange,    // aFrame is changing position or size
  NoPositionOrSizeChange,  // ... NOT changing ...
  InferFromBitToAdd,       // is changing iff (aBitToAdd == NS_FRAME_IS_DIRTY)

  // Note:  With eStyleChange, these can also apply to out-of-flows
  // in addition to aFrame.
};

// WhereToScroll should be 0 ~ 100 or -1.  When it's in 0 ~ 100, it means
// percentage of scrollTop/scrollLeft in scrollHeight/scrollWidth.
// See the comment for constructor of ScrollAxis for the detail.
typedef int16_t WhereToScroll;
static const WhereToScroll kScrollToTop = 0;
static const WhereToScroll kScrollToLeft = 0;
static const WhereToScroll kScrollToCenter = 50;
static const WhereToScroll kScrollToBottom = 100;
static const WhereToScroll kScrollToRight = 100;
static const WhereToScroll kScrollMinimum = -1;

// See the comment for constructor of ScrollAxis for the detail.
enum class WhenToScroll : uint8_t {
  Always,
  IfNotVisible,
  IfNotFullyVisible,
};

struct ScrollAxis final {
  /**
   * aWhere:
   *   Either a percentage or a special value. PresShell defines:
   *   * (Default) kScrollMinimum = -1: The visible area is scrolled the
   *     minimum amount to show as much as possible of the frame. This won't
   *     hide any initially visible part of the frame.
   *   * kScrollToTop = 0: The frame's upper edge is aligned with the top edge
   *     of the visible area.
   *   * kScrollToBottom = 100: The frame's bottom edge is aligned with the
   *     bottom edge of the visible area.
   *   * kScrollToLeft = 0: The frame's left edge is aligned with the left edge
   *     of the visible area.
   *   * kScrollToRight = 100: The frame's right edge is aligned* with the right
   *     edge of the visible area.
   *   * kScrollToCenter = 50: The frame is centered along the axis the
   *     ScrollAxis is used for.
   *
   *   Other values are treated as a percentage, and the point*"percent"
   *   down the frame is placed at the point "percent" down the visible area.
   *
   * aWhen:
   *   * (Default) WhenToScroll::IfNotFullyVisible: Move the frame only if it is
   *     not fully visible (including if it's not visible at all). Note that
   *     in this case if the frame is too large to fit in view, it will only
   *     be scrolled if more of it can fit than is already in view.
   *   * WhenToScroll::IfNotVisible: Move the frame only if none of it is
   *     visible.
   *   * WhenToScroll::Always: Move the frame regardless of its current
   *     visibility.
   *
   * aOnlyIfPerceivedScrollableDirection:
   *   If the direction is not a perceived scrollable direction (i.e. no
   *   scrollbar showing and less than one device pixel of scrollable
   *   distance), don't scroll. Defaults to false.
   */
  explicit ScrollAxis(WhereToScroll aWhere = kScrollMinimum,
                      WhenToScroll aWhen = WhenToScroll::IfNotFullyVisible,
                      bool aOnlyIfPerceivedScrollableDirection = false)
      : mWhereToScroll(aWhere),
        mWhenToScroll(aWhen),
        mOnlyIfPerceivedScrollableDirection(
            aOnlyIfPerceivedScrollableDirection) {}

  WhereToScroll mWhereToScroll;
  WhenToScroll mWhenToScroll;
  bool mOnlyIfPerceivedScrollableDirection : 1;
};

enum class ScrollFlags {
  None = 0,
  ScrollFirstAncestorOnly = 1 << 0,
  ScrollOverflowHidden = 1 << 1,
  ScrollNoParentFrames = 1 << 2,
  ScrollSmooth = 1 << 3,
  ScrollSmoothAuto = 1 << 4,
  ScrollSnap = 1 << 5,
  IgnoreMarginAndPadding = 1 << 6,
  // ScrollOverflowHidden | ScrollNoParentFrames
  AnchorScrollFlags = (1 << 1) | (1 << 2),
  ALL_BITS = (1 << 7) - 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ScrollFlags)

enum class ScrollableDirection { Horizontal, Vertical, Either };

// See comment at declaration of RenderDocument() for the detail.
enum class RenderDocumentFlags {
  None = 0,
  IsUntrusted = 1 << 0,
  IgnoreViewportScrolling = 1 << 1,
  DrawCaret = 1 << 2,
  UseWidgetLayers = 1 << 3,
  AsyncDecodeImages = 1 << 4,
  DocumentRelative = 1 << 5,
  DrawWindowNotFlushing = 1 << 6,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RenderDocumentFlags)

// See comment at declaration of RenderSelection() for the detail.
enum class RenderImageFlags {
  None = 0,
  IsImage = 1 << 0,
  AutoScale = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RenderImageFlags)

enum class ResolutionChangeOrigin : uint8_t {
  Apz,
  MainThreadRestore,
  MainThreadAdjustment,
};

// See comment at declaration of AddCanvasBackgroundColorItem() for the detail.
enum class AddCanvasBackgroundColorFlags {
  None = 0,
  ForceDraw = 1 << 0,
  AddForSubDocument = 1 << 1,
  AppendUnscrolledOnly = 1 << 2,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(AddCanvasBackgroundColorFlags)

enum class PaintFlags {
  None = 0,
  /* Update the layer tree and paint PaintedLayers. If this is not specified,
   * we may still have to do it if the layer tree lost PaintedLayer contents
   * we need for compositing. */
  PaintLayers = 1 << 0,
  /* Composite layers to the window. */
  PaintComposite = 1 << 1,
  /* Sync-decode images. */
  PaintSyncDecodeImages = 1 << 2,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(PaintFlags)

// See comment at declaration of ScheduleViewManagerFlush() for the detail.
enum class PaintType { Default, DelayedCompress };

// This is a private enum class of PresShell, but currently,
// MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS isn't available in class definition.
// Therefore, we need to put this here.
enum class RenderingStateFlags : uint8_t {
  None = 0,
  IgnoringViewportScrolling = 1 << 0,
  DrawWindowNotFlushing = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RenderingStateFlags)

#ifdef DEBUG

enum class VerifyReflowFlags {
  None = 0,
  On = 1 << 0,
  Noisy = 1 << 1,
  All = 1 << 2,
  DumpCommands = 1 << 3,
  NoisyCommands = 1 << 4,
  ReallyNoisyCommands = 1 << 5,
  DuringResizeReflow = 1 << 6,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(VerifyReflowFlags)

#endif  // #ifdef DEBUG

}  // namespace mozilla

#endif  // #ifndef mozilla_PresShellForwards_h
