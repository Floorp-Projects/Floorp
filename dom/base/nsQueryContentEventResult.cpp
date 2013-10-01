/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsQueryContentEventResult.h"
#include "nsIWidget.h"
#include "nsPoint.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;

NS_INTERFACE_MAP_BEGIN(nsQueryContentEventResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIQueryContentEventResult)
  NS_INTERFACE_MAP_ENTRY(nsIQueryContentEventResult)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsQueryContentEventResult)
NS_IMPL_RELEASE(nsQueryContentEventResult)

nsQueryContentEventResult::nsQueryContentEventResult() :
  mEventID(0), mSucceeded(false)
{
}

nsQueryContentEventResult::~nsQueryContentEventResult()
{
}

NS_IMETHODIMP
nsQueryContentEventResult::GetOffset(uint32_t *aOffset)
{
  bool notFound;
  nsresult rv = GetNotFound(&notFound);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!notFound, NS_ERROR_NOT_AVAILABLE);
  *aOffset = mOffset;
  return NS_OK;
}

static bool IsRectEnabled(uint32_t aEventID)
{
  return aEventID == NS_QUERY_CARET_RECT ||
         aEventID == NS_QUERY_TEXT_RECT ||
         aEventID == NS_QUERY_EDITOR_RECT ||
         aEventID == NS_QUERY_CHARACTER_AT_POINT;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetReversed(bool *aReversed)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mEventID == NS_QUERY_SELECTED_TEXT,
                 NS_ERROR_NOT_AVAILABLE);
  *aReversed = mReversed;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetLeft(int32_t *aLeft)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectEnabled(mEventID),
                 NS_ERROR_NOT_AVAILABLE);
  *aLeft = mRect.x;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetWidth(int32_t *aWidth)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectEnabled(mEventID),
                 NS_ERROR_NOT_AVAILABLE);
  *aWidth = mRect.width;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetTop(int32_t *aTop)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectEnabled(mEventID),
                 NS_ERROR_NOT_AVAILABLE);
  *aTop = mRect.y;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetHeight(int32_t *aHeight)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectEnabled(mEventID),
                 NS_ERROR_NOT_AVAILABLE);
  *aHeight = mRect.height;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetText(nsAString &aText)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mEventID == NS_QUERY_SELECTED_TEXT ||
                 mEventID == NS_QUERY_TEXT_CONTENT,
                 NS_ERROR_NOT_AVAILABLE);
  aText = mString;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetSucceeded(bool *aSucceeded)
{
  NS_ENSURE_TRUE(mEventID != 0, NS_ERROR_NOT_INITIALIZED);
  *aSucceeded = mSucceeded;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetNotFound(bool *aNotFound)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mEventID == NS_QUERY_SELECTED_TEXT ||
                 mEventID == NS_QUERY_CHARACTER_AT_POINT,
                 NS_ERROR_NOT_AVAILABLE);
  *aNotFound = (mOffset == WidgetQueryContentEvent::NOT_FOUND);
  return NS_OK;
}

void
nsQueryContentEventResult::SetEventResult(nsIWidget* aWidget,
                                          const WidgetQueryContentEvent &aEvent)
{
  mEventID = aEvent.message;
  mSucceeded = aEvent.mSucceeded;
  mReversed = aEvent.mReply.mReversed;
  mRect = aEvent.mReply.mRect;
  mOffset = aEvent.mReply.mOffset;
  mString = aEvent.mReply.mString;

  if (!IsRectEnabled(mEventID) || !aWidget || !mSucceeded) {
    return;
  }

  nsIWidget* topWidget = aWidget->GetTopLevelWidget();
  if (!topWidget || topWidget == aWidget) {
    return;
  }

  // Convert the top widget related coordinates to the given widget's.
  nsIntPoint offset =
    aWidget->WidgetToScreenOffset() - topWidget->WidgetToScreenOffset();
  mRect.MoveBy(-offset);
}
