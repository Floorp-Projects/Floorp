/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAsyncScrollEventDetail.h"

/**
 * When we send a mozbrowserasyncscroll event (an instance of CustomEvent), we
 * use an instance of this class as the event's detail.
 */
class nsAsyncScrollEventDetail : public nsIAsyncScrollEventDetail
{
public:
  nsAsyncScrollEventDetail(const float left, const float top,
                           const float width, const float height,
                           const float contentWidth, const float contentHeigh)
    : mTop(top)
    , mLeft(left)
    , mWidth(width)
    , mHeight(height)
    , mScrollWidth(contentWidth)
    , mScrollHeight(contentHeigh)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCSCROLLEVENTDETAIL

private:
  virtual ~nsAsyncScrollEventDetail() {}
  const float mTop;
  const float mLeft;
  const float mWidth;
  const float mHeight;
  const float mScrollWidth;
  const float mScrollHeight;
};
