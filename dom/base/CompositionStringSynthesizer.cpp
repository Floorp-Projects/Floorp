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

NS_IMPL_ISUPPORTS(CompositionStringSynthesizer,
                  nsICompositionStringSynthesizer)

CompositionStringSynthesizer::CompositionStringSynthesizer(
                                TextEventDispatcher* aDispatcher)
  : mDispatcher(aDispatcher)
{
}

CompositionStringSynthesizer::~CompositionStringSynthesizer()
{
}

NS_IMETHODIMP
CompositionStringSynthesizer::SetString(const nsAString& aString)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  return mDispatcher->SetPendingCompositionString(aString);
}

NS_IMETHODIMP
CompositionStringSynthesizer::AppendClause(uint32_t aLength,
                                           uint32_t aAttribute)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  return mDispatcher->AppendClauseToPendingComposition(aLength, aAttribute);
}

NS_IMETHODIMP
CompositionStringSynthesizer::SetCaret(uint32_t aOffset, uint32_t aLength)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  return mDispatcher->SetCaretInPendingComposition(aOffset, aLength);
}

NS_IMETHODIMP
CompositionStringSynthesizer::DispatchEvent(bool* aDefaultPrevented)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  NS_ENSURE_ARG_POINTER(aDefaultPrevented);
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = mDispatcher->FlushPendingComposition(status);
  *aDefaultPrevented = (status == nsEventStatus_eConsumeNoDefault);
  return rv;
}

} // namespace dom
} // namespace mozilla
