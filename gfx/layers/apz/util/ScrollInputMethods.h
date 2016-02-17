/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ScrollInputMethods_h
#define mozilla_layers_ScrollInputMethods_h

namespace mozilla {
namespace layers {

/**
 * An enumeration that lists various input methods used to trigger scrolling.
 * Used as the values for the SCROLL_INPUT_METHODS telemetry histogram.
 */
enum class ScrollInputMethod {

  // === Driven by APZ ===

  ApzTouch,          // touch events
  ApzWheelPixel,     // wheel events, pixel scrolling mode
  ApzWheelLine,      // wheel events, line scrolling mode
  ApzWheelPage,      // wheel events, page scrolling mode
  ApzPanGesture,     // pan gesture events (generally triggered by trackpad)
  ApzScrollbarDrag,  // dragging the scrollbar

  // === Driven by the main thread ===

  // Keyboard
  MainThreadScrollLine,       // line scrolling
                              // (generally triggered by up/down arrow keys)
  MainThreadScrollCharacter,  // character scrolling
                              // (generally triggered by left/right arrow keys)
  MainThreadScrollPage,       // page scrolling
                              // (generally triggered by PageUp/PageDown keys)
  MainThreadCompleteScroll,   // scrolling to the end of the scroll range
                              // (generally triggered by Home/End keys)
  MainThreadScrollCaretIntoView,  // scrolling to bring the caret into view
                                  // after moving the caret via the keyboard

  // Touch
  MainThreadTouch,            // touch events

  // Scrollbar
  MainThreadScrollbarDrag,         // dragging the scrollbar
  MainThreadScrollbarButtonClick,  // clicking the buttons at the ends of the
                                   // scrollback track
  MainThreadScrollbarTrackClick,   // clicking the scrollbar track above or
                                   // below the thumb

  // Autoscrolling
  MainThreadAutoscrolling,    // autoscrolling

  // New input methods can be added at the end, up to a maximum of 64.
  // They should only be added at the end, to preserve the numerical values
  // of the existing enumerators.
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_ScrollInputMethods_h */
