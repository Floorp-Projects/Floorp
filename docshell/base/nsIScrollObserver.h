/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollObserver_h___
#define nsIScrollObserver_h___

#include "nsISupports.h"
#include "Units.h"

#define NS_ISCROLLOBSERVER_IID \
  { 0x00bc10e3, 0xaa59, 0x4aa3, \
    { 0x88, 0xe9, 0x43, 0x0a, 0x01, 0xa3, 0x88, 0x04 } }

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
  virtual void AsyncPanZoomStarted(const mozilla::CSSIntPoint aScrollPos) {};

  /**
   * Called when an async panning/zooming transform is no longer applied
   * and passed the scroll offset
   */
  virtual void AsyncPanZoomStopped(const mozilla::CSSIntPoint aScrollPos) {};
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScrollObserver, NS_ISCROLLOBSERVER_IID)

#endif /* nsIScrollObserver_h___ */
