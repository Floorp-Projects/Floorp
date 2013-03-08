/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDISPLAYLISTINVALIDATION_H_
#define NSDISPLAYLISTINVALIDATION_H_

#include "nsRect.h"

class nsDisplayItem;
class nsDisplayListBuilder;
class nsDisplayBackgroundImage;

/**
 * This stores the geometry of an nsDisplayItem, and the area
 * that will be affected when painting the item.
 *
 * It is used to retain information about display items so they
 * can be compared against new display items in the next paint.
 */
class nsDisplayItemGeometry
{
public:
  nsDisplayItemGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);
  virtual ~nsDisplayItemGeometry();
  
  /**
   * Compute the area required to be invalidated if this
   * display item is removed.
   */
  const nsRect& ComputeInvalidationRegion() { return mBounds; }
  
  /**
   * Shifts all retained areas of the nsDisplayItemGeometry by the given offset.
   * 
   * This is used to compensate for scrolling, since the destination buffer
   * can scroll without requiring a full repaint.
   *
   * @param aOffset Offset to shift by.
   */
  virtual void MoveBy(const nsPoint& aOffset) = 0;

  /**
   * Bounds of the display item
   */
  nsRect mBounds;
};

/**
 * A default geometry implementation, used by nsDisplayItem. Retains
 * and compares the bounds, and border rect.
 *
 * This should be sufficient for the majority of display items.
 */
class nsDisplayItemGenericGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayItemGenericGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset);

  nsRect mBorderRect;
};

class nsDisplayItemBoundsGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayItemBoundsGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset);

  bool mHasRoundedCorners;
};

class nsDisplayBorderGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayBorderGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset);

  nsRect mContentRect;
};

class nsDisplayBackgroundGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayBackgroundGeometry(nsDisplayBackgroundImage* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset);

  nsRect mPositioningArea;
};

class nsDisplayBoxShadowInnerGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayBoxShadowInnerGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);
  
  virtual void MoveBy(const nsPoint& aOffset);

  nsRect mPaddingRect;
};

#endif /*NSDISPLAYLISTINVALIDATION_H_*/
