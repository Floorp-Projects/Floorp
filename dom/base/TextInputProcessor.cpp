/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextInputProcessor.h"
#include "nsIDocShell.h"
#include "nsIWidget.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(TextInputProcessor,
                  nsITextInputProcessor)

TextInputProcessor::TextInputProcessor()
  : mDispatcher(nullptr)
  , mIsInitialized(false)
  , mForTests(false)
{
}

TextInputProcessor::~TextInputProcessor()
{
}

NS_IMETHODIMP
TextInputProcessor::Init(nsIDOMWindow* aWindow,
                         bool* aSucceeded)
{
  MOZ_RELEASE_ASSERT(aSucceeded, "aSucceeded must not be nullptr");
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  return InitInternal(aWindow, false, *aSucceeded);
}

NS_IMETHODIMP
TextInputProcessor::InitForTests(nsIDOMWindow* aWindow,
                                 bool* aSucceeded)
{
  MOZ_RELEASE_ASSERT(aSucceeded, "aSucceeded must not be nullptr");
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  return InitInternal(aWindow, true, *aSucceeded);
}

nsresult
TextInputProcessor::InitInternal(nsIDOMWindow* aWindow,
                                 bool aForTests,
                                 bool& aSucceeded)
{
  aSucceeded = false;
  bool wasInitialized = mIsInitialized;
  mIsInitialized = false;
  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsPIDOMWindow> pWindow(do_QueryInterface(aWindow));
  if (NS_WARN_IF(!pWindow)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIDocShell> docShell(pWindow->GetDocShell());
  if (NS_WARN_IF(!docShell)) {
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<nsPresContext> presContext;
  nsresult rv = docShell->GetPresContext(getter_AddRefs(presContext));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!presContext)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIWidget> widget = presContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<TextEventDispatcher> dispatcher = widget->GetTextEventDispatcher();
  MOZ_RELEASE_ASSERT(dispatcher, "TextEventDispatcher must not be null");

  // If the instance was initialized and is being initialized for same
  // dispatcher and same purpose, we don't need to initialize the dispatcher
  // again.
  if (wasInitialized && dispatcher == mDispatcher && aForTests == mForTests) {
    mIsInitialized = true;
    aSucceeded = true;
    return NS_OK;
  }

  if (aForTests) {
    rv = dispatcher->InitForTests();
  } else {
    rv = dispatcher->Init();
  }

  // Another IME framework is using it now.
  if (rv == NS_ERROR_ALREADY_INITIALIZED) {
    return NS_OK;  // Don't throw exception.
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mIsInitialized = true;
  mForTests = aForTests;
  mDispatcher = dispatcher;
  aSucceeded = true;
  return NS_OK;
}

nsresult
TextInputProcessor::IsValidStateForComposition() const
{
  if (NS_WARN_IF(!mIsInitialized)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mDispatcher) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = mDispatcher->GetState();
  if (rv != NS_ERROR_NOT_INITIALIZED) {
    return rv;
  }

  return mForTests ? mDispatcher->InitForTests() : mDispatcher->Init();
}

NS_IMETHODIMP
TextInputProcessor::StartComposition(bool* aSucceeded)
{
  MOZ_RELEASE_ASSERT(aSucceeded, "aSucceeded must not be nullptr");
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  *aSucceeded = false;
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsEventStatus status = nsEventStatus_eIgnore;
  rv = mDispatcher->StartComposition(status);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  *aSucceeded = status != nsEventStatus_eConsumeNoDefault &&
                  mDispatcher && mDispatcher->IsComposing();
  return NS_OK;
}

NS_IMETHODIMP
TextInputProcessor::SetPendingCompositionString(const nsAString& aString)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mDispatcher->SetPendingCompositionString(aString);
}

NS_IMETHODIMP
TextInputProcessor::AppendClauseToPendingComposition(uint32_t aLength,
                                                     uint32_t aAttribute)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  switch (aAttribute) {
    case ATTR_RAW_CLAUSE:
    case ATTR_SELECTED_RAW_CLAUSE:
    case ATTR_CONVERTED_CLAUSE:
    case ATTR_SELECTED_CLAUSE:
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mDispatcher->AppendClauseToPendingComposition(aLength, aAttribute);
}

NS_IMETHODIMP
TextInputProcessor::SetCaretInPendingComposition(uint32_t aOffset)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mDispatcher->SetCaretInPendingComposition(aOffset, 0);
}

NS_IMETHODIMP
TextInputProcessor::FlushPendingComposition(bool* aSucceeded)
{
  MOZ_RELEASE_ASSERT(aSucceeded, "aSucceeded must not be nullptr");
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  *aSucceeded = false;
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsEventStatus status = nsEventStatus_eIgnore;
  rv = mDispatcher->FlushPendingComposition(status);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  *aSucceeded = status != nsEventStatus_eConsumeNoDefault;
  return rv;
}

NS_IMETHODIMP
TextInputProcessor::CommitComposition(const nsAString& aCommitString,
                                      uint8_t aOptionalArgc,
                                      bool* aSucceeded)
{
  MOZ_RELEASE_ASSERT(aSucceeded, "aSucceeded must not be nullptr");
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  const nsAString* commitString =
    aOptionalArgc >= 1 ? &aCommitString : nullptr;
  nsEventStatus status = nsEventStatus_eIgnore;
  rv = mDispatcher->CommitComposition(status, commitString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  *aSucceeded = status != nsEventStatus_eConsumeNoDefault;
  return rv;
}

NS_IMETHODIMP
TextInputProcessor::CancelComposition()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsEventStatus status = nsEventStatus_eIgnore;
  return mDispatcher->CommitComposition(status, &EmptyString());
}

} // namespace mozilla
