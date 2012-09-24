/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_ISVGSVGFRAME_H__
#define __NS_ISVGSVGFRAME_H__

#include "nsQueryFrame.h"

class nsISVGSVGFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsISVGSVGFrame)

  /**
   * Called when non-attribute changes have caused the element's width/height
   * or its for-children transform to change, and to get the element to notify
   * its children appropriately. aFlags must be set to
   * nsISVGChildFrame::COORD_CONTEXT_CHANGED and/or
   * nsISVGChildFrame::TRANSFORM_CHANGED.
   */
  virtual void NotifyViewportOrTransformChanged(uint32_t aFlags)=0; 
};

#endif // __NS_ISVGSVGFRAME_H__
