/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIRunaround_h___
#define nsIRunaround_h___

#include <stdio.h>
#include "nsIFrame.h"

// IID for the nsIRunaround interface {3C6ABCF0-C028-11d1-853F-00A02468FAB6}
#define NS_IRUNAROUND_IID     \
{ 0x3c6abcf0, 0xc028, 0x11d1, \
  {0x85, 0x3f, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

/**
 * An interface for handling reflow that allows direct access to the space
 * manager. Note that this interface is not an nsISupports interface, and
 * therefore you cannot QueryInterface() back
 */
class nsIRunaround
{
public:
  /**
   * Resize reflow. The frame is given a maximum size and asked for its
   * desired rect. The space manager should be used to do runaround of anchored
   * items.
   *
   * @param aSpaceManager the space manager to use. The caller has translated
   *          the coordinate system so the frame has its own local coordinate
   *          space with an origin of (0, 0). If you translate the coordinate
   *          space you must restore it before returning.
   * @param aMaxSize the available space in which to lay out. Each dimension
   *          can either be constrained or unconstrained (a value of
   *          NS_UNCONSTRAINEDSIZE). If constrained you should choose a value
   *          that's less than or equal to the constrained size. If unconstrained
   *          you can choose as large a value as you like.
   *
   *          It's okay to return a desired size that exceeds the max size if
   *          that's the smallest you can be, i.e. it's your minimum size.
   *
   * @param aDesiredRect <i>out</i> parameter where you should return the desired
   *          origin and size. You should include any space you want for border
   *          and padding in the desired size.
   *
   *          The origin of the desired rect is relative to the upper-left of the
   *          local coordinate space.
   *
   * @param aMaxElementSize an optional parameter for returning your maximum
   *          element size. If may be null in which case you don't have to compute
   *          a maximum element size
   *
   * @see nsISpaceManager#Translate()
   */
  NS_IMETHOD  ResizeReflow(nsIPresContext*         aPresContext,
                           nsISpaceManager*        aSpaceManager,
                           const nsSize&           aMaxSize,
                           nsRect&                 aDesiredRect,
                           nsSize*                 aMaxElementSize,
                           nsIFrame::ReflowStatus& aStatus) = 0;

  /**
   * Incremental reflow. The reflow command contains information about the
   * type of change. The frame is given a maximum size and asked for its
   * desired rect. The space manager should be used to do runaround of anchored
   * items.
   *
   * @param aSpaceManager the space manager to use. The caller has translated
   *          the coordinate system so the frame has its own local coordinate
   *          space with an origin of (0, 0). If you translate the coordinate
   *          space you must restore it before returning.
   * @param aMaxSize the available space in which to lay out. Each dimension
   *          can either be constrained or unconstrained (a value of
   *          NS_UNCONSTRAINEDSIZE). If constrained you should choose a value
   *          that's less than or equal to the constrained size. If unconstrained
   *          you can choose as large a value as you like.
   *
   *          It's okay to return a desired size that exceeds the max size if
   *          that's the smallest you can be, i.e. it's your minimum size.
   *
   * @param aDesiredRect <i>out</i> parameter where you should return the desired
   *          origin and size. You should include any space you want for border
   *          and padding in the desired size.
   *
   *          The origin of the desired rect is relative to the upper-left of the
   *          local coordinate space.
   *
   * @param aReflowCommand the reflow command contains information about the
   *          type of change.
   */
  NS_IMETHOD IncrementalReflow(nsIPresContext*         aPresContext,
                               nsISpaceManager*        aSpaceManager,
                               const nsSize&           aMaxSize,
                               nsRect&                 aDesiredRect,
                               nsReflowCommand&        aReflowCommand,
                               nsIFrame::ReflowStatus& aStatus) = 0;
};

#endif /* nsIRunaround_h___ */
