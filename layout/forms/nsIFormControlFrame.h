/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIFormControlFrame_h___
#define nsIFormControlFrame_h___

#include "nsQueryFrame.h"
class nsAString;
class nsIContent;
class nsIAtom;
struct nsSize;

/** 
  * nsIFormControlFrame is the common interface for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsIFormControlFrame : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIFormControlFrame)

  /**
   * 
   * @param aOn
   * @param aRepaint
   */
  virtual void SetFocus(bool aOn = true, bool aRepaint = false) = 0;

  /**
   * Set a property on the form control frame.
   *
   * @param aName name of the property to set
   * @param aValue value of the property
   * @returns NS_OK if the property name is valid, otherwise an error code
   */
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) = 0;
};

#endif

