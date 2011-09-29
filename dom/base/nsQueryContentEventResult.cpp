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
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsQueryContentEventResult.h"
#include "nsGUIEvent.h"
#include "nsIWidget.h"
#include "nsPoint.h"

NS_INTERFACE_MAP_BEGIN(nsQueryContentEventResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIQueryContentEventResult)
  NS_INTERFACE_MAP_ENTRY(nsIQueryContentEventResult)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsQueryContentEventResult)
NS_IMPL_RELEASE(nsQueryContentEventResult)

nsQueryContentEventResult::nsQueryContentEventResult() :
  mEventID(0), mSucceeded(PR_FALSE)
{
}

nsQueryContentEventResult::~nsQueryContentEventResult()
{
}

NS_IMETHODIMP
nsQueryContentEventResult::GetOffset(PRUint32 *aOffset)
{
  bool notFound;
  nsresult rv = GetNotFound(&notFound);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!notFound, NS_ERROR_NOT_AVAILABLE);
  *aOffset = mOffset;
  return NS_OK;
}

static bool IsRectEnabled(PRUint32 aEventID)
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
nsQueryContentEventResult::GetLeft(PRInt32 *aLeft)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectEnabled(mEventID),
                 NS_ERROR_NOT_AVAILABLE);
  *aLeft = mRect.x;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetWidth(PRInt32 *aWidth)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectEnabled(mEventID),
                 NS_ERROR_NOT_AVAILABLE);
  *aWidth = mRect.width;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetTop(PRInt32 *aTop)
{
  NS_ENSURE_TRUE(mSucceeded, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(IsRectEnabled(mEventID),
                 NS_ERROR_NOT_AVAILABLE);
  *aTop = mRect.y;
  return NS_OK;
}

NS_IMETHODIMP
nsQueryContentEventResult::GetHeight(PRInt32 *aHeight)
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
  *aNotFound = (mOffset == nsQueryContentEvent::NOT_FOUND);
  return NS_OK;
}

void
nsQueryContentEventResult::SetEventResult(nsIWidget* aWidget,
                                          const nsQueryContentEvent &aEvent)
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
