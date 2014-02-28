/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollObserver_h___
#define nsIScrollObserver_h___

#include "nsISupports.h"

#define NS_ISCROLLOBSERVER_IID \
  { 0x7c1a8b63, 0xe322, 0x4827, \
    { 0xa4, 0xb1, 0x3b, 0x6e, 0x59, 0x03, 0x47, 0x7e } }

class nsIScrollObserver : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCROLLOBSERVER_IID)

  /**
   * Called when the scroll position of some element has changed.
   */
  virtual void ScrollPositionChanged() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScrollObserver, NS_ISCROLLOBSERVER_IID)

#endif /* nsIScrollObserver_h___ */
