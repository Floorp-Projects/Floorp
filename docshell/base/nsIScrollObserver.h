/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollObserver_h___
#define nsIScrollObserver_h___

#include "nsISupports.h"
#include "Units.h"

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_ISCROLLOBSERVER_IID \
  { 0xaa5026eb, 0x2f88, 0x4026, \
    { 0xa4, 0x6b, 0xf4, 0x59, 0x6b, 0x4e, 0xdf, 0x00 } }

class nsIScrollObserver : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCROLLOBSERVER_IID)

  /**
   * Called when the scroll position of some element has changed.
   */
  virtual void ScrollPositionChanged() = 0;

  /**
   * Called when an async panning/zooming transform has started being applied
   * and passed the scroll offset
   */
  virtual void AsyncPanZoomStarted() {};

  /**
   * Called when an async panning/zooming transform is no longer applied
   * and passed the scroll offset
   */
  virtual void AsyncPanZoomStopped() {};
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScrollObserver, NS_ISCROLLOBSERVER_IID)

#endif /* nsIScrollObserver_h___ */
