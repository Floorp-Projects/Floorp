/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollbarOwner_h___
#define nsIScrollbarOwner_h___

#include "nsQueryFrame.h"

class nsIDOMEventTarget;
class nsIFrame;

/**
 * An interface that represents a frame which manages scrollbars.
 */
class nsIScrollbarOwner : public nsQueryFrame {
public:
  NS_DECL_QUERYFRAME_TARGET(nsIScrollbarOwner)

  /**
   * Obtain the frame for the horizontal or vertical scrollbar, or null
   * if there is no such box.
   */
  virtual nsIFrame* GetScrollbarBox(bool aVertical) = 0;

  /**
   * Show or hide scrollbars on 2 fingers touch.
   * Subclasses should call their ScrollbarActivity's corresponding methods.
   */
  virtual void ScrollbarActivityStarted() const = 0;
  virtual void ScrollbarActivityStopped() const = 0;
};

#endif
