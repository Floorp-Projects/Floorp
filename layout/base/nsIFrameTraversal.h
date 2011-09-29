/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
                               PRInt32 aType,
                               bool aVisual,
                               bool aLockInScrollView,
                               bool aFollowOOFs) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFrameTraversal, NS_IFRAMETRAVERSAL_IID)

#endif //NSIFRAMETRAVERSAL_H
