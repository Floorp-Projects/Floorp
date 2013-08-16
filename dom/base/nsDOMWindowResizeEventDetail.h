/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMDOMWindowResizeEventDetail.h"
#include "nsSize.h"

class nsDOMWindowResizeEventDetail : public nsIDOMDOMWindowResizeEventDetail
{
public:
  nsDOMWindowResizeEventDetail(const nsIntSize& aSize)
    : mSize(aSize)
  {}

  virtual ~nsDOMWindowResizeEventDetail() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMWINDOWRESIZEEVENTDETAIL

private:
  const nsIntSize mSize;
};
