/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISelectControlFrame_h___
#define nsISelectControlFrame_h___

#include "nsQueryFrame.h"

/**
  * nsISelectControlFrame is the interface for combo boxes and listboxes
  */
class nsISelectControlFrame : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsISelectControlFrame)

  /**
   * Adds an option to the list at index
   */

  NS_IMETHOD AddOption(int32_t index) = 0;

  /**
   * Removes the option at index.  The caller must have a live script
   * blocker while calling this method.
   */
  NS_IMETHOD RemoveOption(int32_t index) = 0;

  /**
   * Sets whether the parser is done adding children
   * @param aIsDone whether the parser is done adding children
   */
  NS_IMETHOD DoneAddingChildren(bool aIsDone) = 0;

  /**
   * Notify the frame when an option is selected
   */
  NS_IMETHOD OnOptionSelected(int32_t aIndex, bool aSelected) = 0;

  /**
   * Notify the frame when selectedIndex was changed.  This might
   * destroy the frame.
   */
  NS_IMETHOD OnSetSelectedIndex(int32_t aOldIndex, int32_t aNewIndex) = 0;

};

#endif
