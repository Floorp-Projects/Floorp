/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollObserver_h___
#define nsIScrollObserver_h___

#include "nsISupports.h"

#define NS_ISCROLLOBSERVER_IID \
  { 0x03465b77, 0x9ce2, 0x4d19, \
    { 0xb2, 0xf6, 0x82, 0xae, 0xee, 0x85, 0xc3, 0xbf } }

class nsIScrollObserver : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCROLLOBSERVER_IID)

  /**
   * Called when the scroll position of some element has changed.
   */
  virtual void ScrollPositionChanged() = 0;

  /**
   * Called when an async panning/zooming transform has started being applied.
   */
  virtual void AsyncPanZoomStarted(){};

  /**
   * Called when an async panning/zooming transform is no longer applied.
   */
  virtual void AsyncPanZoomStopped(){};
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScrollObserver, NS_ISCROLLOBSERVER_IID)

#endif /* nsIScrollObserver_h___ */
