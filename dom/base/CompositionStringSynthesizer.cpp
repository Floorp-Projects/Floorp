/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositionStringSynthesizer.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIWidget.h"
#include "nsPIDOMWindow.h"
#include "nsView.h"
#include "mozilla/TextEvents.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS1(CompositionStringSynthesizer,
                   nsICompositionStringSynthesizer)

CompositionStringSynthesizer::CompositionStringSynthesizer(
                                nsPIDOMWindow* aWindow)
{
  mWindow = do_GetWeakReference(aWindow);
  ClearInternal();
}

CompositionStringSynthesizer::~CompositionStringSynthesizer()
{
}

void
CompositionStringSynthesizer::ClearInternal()
{
  mString.Truncate();
  mClauses.Clear();
  mCaret.mRangeType = 0;
}

nsIWidget*
CompositionStringSynthesizer::GetWidget()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (!window) {
    return nullptr;
  }
  nsIDocShell *docShell = window->GetDocShell();
  if (!docShell) {
    return nullptr;
  }
  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  nsIFrame* frame = presShell->GetRootFrame();
  if (!frame) {
    return nullptr;
  }
  return frame->GetView()->GetNearestWidget(nullptr);
}

NS_IMETHODIMP
CompositionStringSynthesizer::SetString(const nsAString& aString)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  NS_ENSURE_TRUE(widget && !widget->Destroyed(), NS_ERROR_NOT_AVAILABLE);

  mString = aString;
  return NS_OK;
}

NS_IMETHODIMP
CompositionStringSynthesizer::AppendClause(uint32_t aLength,
                                           uint32_t aAttribute)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  NS_ENSURE_TRUE(widget && !widget->Destroyed(), NS_ERROR_NOT_AVAILABLE);

  switch (aAttribute) {
    case ATTR_RAWINPUT:
    case ATTR_SELECTEDRAWTEXT:
    case ATTR_CONVERTEDTEXT:
    case ATTR_SELECTEDCONVERTEDTEXT: {
      TextRange textRange;
      textRange.mStartOffset =
        mClauses.IsEmpty() ? 0 : mClauses[mClauses.Length() - 1].mEndOffset;
      textRange.mEndOffset = textRange.mStartOffset + aLength;
      textRange.mRangeType = aAttribute;
      mClauses.AppendElement(textRange);
      return NS_OK;
    }
    default:
      return NS_ERROR_INVALID_ARG;
  }
}

NS_IMETHODIMP
CompositionStringSynthesizer::SetCaret(uint32_t aOffset, uint32_t aLength)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  NS_ENSURE_TRUE(widget && !widget->Destroyed(), NS_ERROR_NOT_AVAILABLE);

  mCaret.mStartOffset = aOffset;
  mCaret.mEndOffset = mCaret.mStartOffset + aLength;
  mCaret.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  return NS_OK;
}

NS_IMETHODIMP
CompositionStringSynthesizer::DispatchEvent(bool* aDefaultPrevented)
{
  NS_ENSURE_ARG_POINTER(aDefaultPrevented);
  nsCOMPtr<nsIWidget> widget = GetWidget();
  NS_ENSURE_TRUE(widget && !widget->Destroyed(), NS_ERROR_NOT_AVAILABLE);

  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!mClauses.IsEmpty() &&
      mClauses[mClauses.Length()-1].mEndOffset != mString.Length()) {
    NS_WARNING("Sum of length of the all clauses must be same as the string "
               "length");
    ClearInternal();
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (mCaret.mRangeType == NS_TEXTRANGE_CARETPOSITION) {
    if (mCaret.mEndOffset > mString.Length()) {
      NS_WARNING("Caret position is out of the composition string");
      ClearInternal();
      return NS_ERROR_ILLEGAL_VALUE;
    }
    mClauses.AppendElement(mCaret);
  }

  WidgetTextEvent textEvent(true, NS_TEXT_TEXT, widget);
  textEvent.time = PR_IntervalNow();
  textEvent.theText = mString;
  textEvent.rangeCount = mClauses.Length();
  textEvent.rangeArray = mClauses.Elements();

  // XXX How should we set false for this on b2g?
  textEvent.mFlags.mIsSynthesizedForTests = true;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = widget->DispatchEvent(&textEvent, status);
  *aDefaultPrevented = (status == nsEventStatus_eConsumeNoDefault);

  ClearInternal();

  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
