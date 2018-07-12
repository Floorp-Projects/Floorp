/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIWidget.h"
#include "nsPoint.h"
#include "nsQueryContentEventResult.h"
#include "mozilla/Move.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;

/******************************************************************************
 * Is*PropertyAvailable() methods which check if the property is available
 * (valid) with the event message.
 ******************************************************************************/

static bool IsNotFoundPropertyAvailable(EventMessage aEventMessage)
{
  return aEventMessage == eQuerySelectedText ||
         aEventMessage == eQueryCharacterAtPoint;
}

static bool IsOffsetPropertyAvailable(EventMessage aEventMessage)
{
  return aEventMessage == eQueryTextContent ||
         aEventMessage == eQueryTextRect ||
         aEventMessage == eQueryCaretRect ||
         IsNotFoundPropertyAvailable(aEventMessage);
}

static bool IsRectRelatedPropertyAvailable(EventMessage aEventMessage)
{
  return aEventMessage == eQueryCaretRect ||
         aEventMessage == eQueryTextRect ||
         aEventMessage == eQueryEditorRect ||
         aEventMessage == eQueryCharacterAtPoint;
}

/******************************************************************************
 * nsQueryContentEventResult
 ******************************************************************************/

NS_INTERFACE_MAP_BEGIN(nsQueryContentEventResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIQueryContentEventResult)
  NS_INTERFACE_MAP_ENTRY(nsIQueryContentEventResult)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsQueryContentEventResult)
NS_IMPL_RELEASE(nsQueryContentEventResult)

nsQueryContentEventResult::nsQueryContentEventResult(mozilla::WidgetQueryContentEvent &aEvent)
  : mEventMessage(aEvent.mMessage)
  , mOffset(aEvent.mReply.mOffset)
  , mTentativeCaretOffset(aEvent.mReply.mTentativeCaretOffset)
  , mString(aEvent.mReply.mString)
  , mRect(aEvent.mReply.mRect)
  , mRectArray(std::move(aEvent.mReply.mRectArray))
  , mSucceeded(aEvent.mSucceeded)
  , mReversed(aEvent.mReply.mReversed)
{
  // Mark as result that is longer used.
  aEvent.mSucceeded = false;
}

nsQueryContentEventResult::~nsQueryContentEventResult()
{
}

NS_IMETHODIMP
nsQueryContentEventResult::GetOffset(uint32_t* aOffset)
{
  if (NS_WARN_IF(!mSucceeded)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_WARN_IF(!IsOffsetPropertyAvailable(mEventMessage))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // With some event message, both offset and notFound properties are available.
  // In that case, offset value may mean "not found".  If so, this method
  // shouldn't return mOffset as the result because it's a special value for
  // "not found".
  if (IsNotFoundPropertyAvailable(mEventMessage)) {
    bool notFound;
    nsresult rv = GetNotFound(&notFound);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv; // Just an unexpected case...
    }
    // As said above, if mOffset means "not found", offset property shouldn't
    // return its value without any errors.
    if (NS_WARN_IF(notFound)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  *aOffset = mOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetTentativeCaretOffset(uint32_t* aOffset)
{
  bool notFound;
  nsresult rv = GetTentativeCaretOffsetNotFound(&notFound);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(notFound)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aOffset = mTentativeCaretOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetReversed(bool *aReversed)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mEventMessage == eQuerySelectedText, NS_ERROR_NOT_AVAILABLE);
  *aReversed = mReversed;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetLeft(int32_t *aLeft)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectRelatedPropertyAvailable(mEventMessage),
                 NS_ERROR_NOT_AVAILABLE);
  *aLeft = mRect.x;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetWidth(int32_t *aWidth)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectRelatedPropertyAvailable(mEventMessage),
                 NS_ERROR_NOT_AVAILABLE);
  *aWidth = mRect.Width();
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetTop(int32_t *aTop)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectRelatedPropertyAvailable(mEventMessage),
                 NS_ERROR_NOT_AVAILABLE);
  *aTop = mRect.y;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetHeight(int32_t *aHeight)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectRelatedPropertyAvailable(mEventMessage),
                 NS_ERROR_NOT_AVAILABLE);
  *aHeight = mRect.Height();
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetText(nsAString &aText)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mEventMessage == eQuerySelectedText ||
                 mEventMessage == eQueryTextContent ||
                 mEventMessage == eQueryTextRect,
                 NS_ERROR_NOT_AVAILABLE);
  aText = mString;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetSucceeded(bool *aSucceeded)
{
  NS_ENSURE_TRUE(mEventMessage != eVoidEvent, NS_ERROR_NOT_INITIALIZED);
  *aSucceeded = mSucceeded;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetNotFound(bool* aNotFound)
{
  if (NS_WARN_IF(!mSucceeded) ||
      NS_WARN_IF(!IsNotFoundPropertyAvailable(mEventMessage))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aNotFound = (mOffset == WidgetQueryContentEvent::NOT_FOUND);
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetTentativeCaretOffsetNotFound(bool* aNotFound)
{
  if (NS_WARN_IF(!mSucceeded)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(mEventMessage != eQueryCharacterAtPoint)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aNotFound = (mTentativeCaretOffset == WidgetQueryContentEvent::NOT_FOUND);
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetCharacterRect(int32_t aOffset,
                                            int32_t* aLeft, int32_t* aTop,
                                            int32_t* aWidth, int32_t* aHeight)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mEventMessage == eQueryTextRectArray,
                 NS_ERROR_NOT_AVAILABLE);

  if (NS_WARN_IF(mRectArray.Length() <= static_cast<uint32_t>(aOffset))) {
    return NS_ERROR_FAILURE;
  }

  *aLeft = mRectArray[aOffset].x;
  *aTop = mRectArray[aOffset].y;
  *aWidth = mRectArray[aOffset].Width();
  *aHeight = mRectArray[aOffset].Height();

  return NS_OK;
}

void
nsQueryContentEventResult::SetEventResult(nsIWidget* aWidget)
{
  if (!IsRectRelatedPropertyAvailable(mEventMessage) ||
      !aWidget || !mSucceeded) {
    return;
  }

  nsIWidget* topWidget = aWidget->GetTopLevelWidget();
  if (!topWidget || topWidget == aWidget) {
    return;
  }

  // Convert the top widget related coordinates to the given widget's.
  LayoutDeviceIntPoint offset =
    aWidget->WidgetToScreenOffset() - topWidget->WidgetToScreenOffset();
  mRect.MoveBy(-offset);
  for (size_t i = 0; i < mRectArray.Length(); i++) {
    mRectArray[i].MoveBy(-offset);
  }
}
