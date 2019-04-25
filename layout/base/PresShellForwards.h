/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PresShellForwards_h
#define mozilla_PresShellForwards_h

#include "mozilla/TypedEnumBits.h"

class nsIPresShell;

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

enum class RectVisibility {
  Visible,
  AboveViewport,
  BelowViewport,
  LeftOfViewport,
  RightOfViewport,
};

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
