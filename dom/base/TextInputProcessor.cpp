/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventForwards.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextInputProcessor.h"
#include "nsIDocShell.h"
#include "nsIWidget.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"

using namespace mozilla::widget;

namespace mozilla {

NS_IMPL_ISUPPORTS(TextInputProcessor,
                  nsITextInputProcessor,
                  TextEventDispatcherListener,
                  nsISupportsWeakReference)

TextInputProcessor::TextInputProcessor()
  : mDispatcher(nullptr)
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
  if (mDispatcher && dispatcher == mDispatcher && aForTests == mForTests) {
    aSucceeded = true;
    return NS_OK;
  }

  // If this instance is composing, don't allow to initialize again.
  if (mDispatcher && mDispatcher->IsComposing()) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // And also if another instance is composing with the new dispatcher, it'll
  // fail to steal its ownership.  Then, we should not throw an exception,
  // just return false.
  if (dispatcher->IsComposing()) {
    return NS_OK;
  }

  // This instance has finished preparing to link to the dispatcher.  Therefore,
  // let's forget the old dispatcher and purpose.
  UnlinkFromTextEventDispatcher();

  if (aForTests) {
    rv = dispatcher->InitForTests(this);
  } else {
    rv = dispatcher->Init(this);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mDispatcher = dispatcher;
  mForTests = aForTests;
  aSucceeded = true;
  return NS_OK;
}

void
TextInputProcessor::UnlinkFromTextEventDispatcher()
{
  mDispatcher = nullptr;
  mForTests = false;
}

nsresult
TextInputProcessor::IsValidStateForComposition()
{
  if (NS_WARN_IF(!mDispatcher)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = mDispatcher->GetState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
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
  const nsAString* commitString =
    aOptionalArgc >= 1 ? &aCommitString : nullptr;
  return CommitCompositionInternal(commitString, aSucceeded);
}

nsresult
TextInputProcessor::CommitCompositionInternal(const nsAString* aCommitString,
                                              bool* aSucceeded)
{
  if (aSucceeded) {
    *aSucceeded = false;
  }
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsEventStatus status = nsEventStatus_eIgnore;
  rv = mDispatcher->CommitComposition(status, aCommitString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (aSucceeded) {
    *aSucceeded = status != nsEventStatus_eConsumeNoDefault;
  }
  return rv;
}

NS_IMETHODIMP
TextInputProcessor::CancelComposition()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  return CancelCompositionInternal();
}

nsresult
TextInputProcessor::CancelCompositionInternal()
{
  nsRefPtr<TextEventDispatcher> kungfuDeathGrip(mDispatcher);
  nsresult rv = IsValidStateForComposition();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsEventStatus status = nsEventStatus_eIgnore;
  return mDispatcher->CommitComposition(status, &EmptyString());
}

NS_IMETHODIMP
TextInputProcessor::NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                              const IMENotification& aNotification)
{
  // If This is called while this is being initialized, ignore the call.
  if (!mDispatcher) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  MOZ_ASSERT(aTextEventDispatcher == mDispatcher,
             "Wrong TextEventDispatcher notifies this");
  switch (aNotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION: {
      NS_ASSERTION(aTextEventDispatcher->IsComposing(),
                   "Why is this requested without composition?");
      CommitCompositionInternal();
      return NS_OK;
    }
    case REQUEST_TO_CANCEL_COMPOSITION: {
      NS_ASSERTION(aTextEventDispatcher->IsComposing(),
                   "Why is this requested without composition?");
      CancelCompositionInternal();
      return NS_OK;
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP_(void)
TextInputProcessor::OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher)
{
  // If This is called while this is being initialized, ignore the call.
  if (!mDispatcher) {
    return;
  }
  MOZ_ASSERT(aTextEventDispatcher == mDispatcher,
             "Wrong TextEventDispatcher notifies this");
  UnlinkFromTextEventDispatcher();
}

} // namespace mozilla
