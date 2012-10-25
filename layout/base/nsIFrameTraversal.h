/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSIFRAMETRAVERSAL_H
#define NSIFRAMETRAVERSAL_H

#include "nsISupports.h"
#include "nsIFrame.h"

#define NS_IFRAMEENUMERATOR_IID \
{ 0x7c633f5d, 0x91eb, 0x494e, \
  { 0xa1, 0x40, 0x17, 0x46, 0x17, 0x4c, 0x23, 0xd3 } }

class nsIFrameEnumerator : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFRAMEENUMERATOR_IID)

  virtual void First() = 0;
  virtual void Next() = 0;
  virtual nsIFrame* CurrentItem() = 0;
  virtual bool IsDone() = 0;

  virtual void Last() = 0;
  virtual void Prev() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFrameEnumerator, NS_IFRAMEENUMERATOR_IID)

enum nsIteratorType {
  eLeaf,
  ePreOrder,
  ePostOrder
};

// {9d469828-9bf2-4151-a385-05f30219221b}
#define NS_IFRAMETRAVERSAL_IID \
{ 0x9d469828, 0x9bf2, 0x4151, { 0xa3, 0x85, 0x05, 0xf3, 0x02, 0x19, 0x22, 0x1b } }

class nsIFrameTraversal : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFRAMETRAVERSAL_IID)

  /**
   * Create a frame iterator with the specified properties.
   * @param aEnumerator [out] the created iterator
   * @param aPresContext [in]
   * @param aStart [in] the frame to start iterating from
   * @param aType [in] the type of the iterator: leaf, pre-order, or post-order
   * @param aVisual [in] whether the iterator should traverse frames in visual
   *        bidi order
   * @param aLockInScrollView [in] whether to stop iterating when exiting a
   *        scroll view
   * @param aFollowOOFs [in] whether the iterator should follow out-of-flows.
   *        If true, when reaching a placeholder frame while going down will get
   *        the real frame. Going back up will go on past the placeholder,
   *        so the placeholders are logically part of the frame tree.
   */
  NS_IMETHOD NewFrameTraversal(nsIFrameEnumerator **aEnumerator,
                               nsPresContext* aPresContext,
                               nsIFrame *aStart,
                               int32_t aType,
                               bool aVisual,
                               bool aLockInScrollView,
                               bool aFollowOOFs) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFrameTraversal, NS_IFRAMETRAVERSAL_IID)

#endif //NSIFRAMETRAVERSAL_H
