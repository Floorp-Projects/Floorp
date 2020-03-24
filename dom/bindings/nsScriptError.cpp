/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIScriptError implementation.
 */

#include "nsScriptError.h"
#include "js/Printf.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsIMutableArray.h"
#include "nsIScriptError.h"
#include "mozilla/BasePrincipal.h"

nsScriptErrorBase::nsScriptErrorBase()
    : mMessage(),
      mMessageName(),
      mSourceName(),
      mCssSelectors(),
      mSourceId(0),
      mLineNumber(0),
      mSourceLine(),
      mColumnNumber(0),
      mFlags(0),
      mCategory(),
      mOuterWindowID(0),
      mInnerWindowID(0),
      mTimeStamp(0),
      mInitializedOnMainThread(false),
      mIsFromPrivateWindow(false),
      mIsFromChromeContext(false) {}

nsScriptErrorBase::~nsScriptErrorBase() = default;

void nsScriptErrorBase::AddNote(nsIScriptErrorNote* note) {
  mNotes.AppendObject(note);
}

void nsScriptErrorBase::InitializeOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInitializedOnMainThread);

  if (mInnerWindowID) {
    nsGlobalWindowInner* window =
        nsGlobalWindowInner::GetInnerWindowWithId(mInnerWindowID);
    if (window) {
      nsPIDOMWindowOuter* outer = window->GetOuterWindow();
      if (outer) mOuterWindowID = outer->WindowID();
      mIsFromChromeContext = ComputeIsFromChromeContext(window);
      mIsFromPrivateWindow = ComputeIsFromPrivateWindow(window);
    }
  }

  mInitializedOnMainThread = true;
}

NS_IMETHODIMP
nsScriptErrorBase::InitSourceId(uint32_t value) {
  mSourceId = value;
  return NS_OK;
}

// nsIConsoleMessage methods
NS_IMETHODIMP
nsScriptErrorBase::GetMessageMoz(nsAString& aMessage) {
  nsAutoCString message;
  nsresult rv = ToString(message);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CopyUTF8toUTF16(message, aMessage);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetLogLevel(uint32_t* aLogLevel) {
  if (mFlags & (uint32_t)nsIScriptError::infoFlag) {
    *aLogLevel = nsIConsoleMessage::info;
  } else if (mFlags & (uint32_t)nsIScriptError::warningFlag) {
    *aLogLevel = nsIConsoleMessage::warn;
  } else {
    *aLogLevel = nsIConsoleMessage::error;
  }
  return NS_OK;
}

// nsIScriptError methods
NS_IMETHODIMP
nsScriptErrorBase::GetErrorMessage(nsAString& aResult) {
  aResult.Assign(mMessage);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetSourceName(nsAString& aResult) {
  aResult.Assign(mSourceName);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetCssSelectors(nsAString& aResult) {
  aResult.Assign(mCssSelectors);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::SetCssSelectors(const nsAString& aCssSelectors) {
  mCssSelectors = aCssSelectors;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetSourceId(uint32_t* result) {
  *result = mSourceId;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetSourceLine(nsAString& aResult) {
  aResult.Assign(mSourceLine);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetLineNumber(uint32_t* result) {
  *result = mLineNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetColumnNumber(uint32_t* result) {
  *result = mColumnNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetFlags(uint32_t* result) {
  *result = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetCategory(char** result) {
  *result = ToNewCString(mCategory);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetStack(JS::MutableHandleValue aStack) {
  aStack.setUndefined();
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::SetStack(JS::HandleValue aStack) { return NS_OK; }

NS_IMETHODIMP
nsScriptErrorBase::GetStackGlobal(JS::MutableHandleValue aStackGlobal) {
  aStackGlobal.setUndefined();
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetErrorMessageName(nsAString& aErrorMessageName) {
  aErrorMessageName = mMessageName;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::SetErrorMessageName(const nsAString& aErrorMessageName) {
  mMessageName = aErrorMessageName;
  return NS_OK;
}

static void AssignSourceNameHelper(nsString& aSourceNameDest,
                                   const nsAString& aSourceNameSrc) {
  if (aSourceNameSrc.IsEmpty()) return;

  aSourceNameDest.Assign(aSourceNameSrc);

  nsCOMPtr<nsIURI> uri;
  nsAutoCString pass;
  if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), aSourceNameSrc)) &&
      NS_SUCCEEDED(uri->GetPassword(pass)) && !pass.IsEmpty()) {
    NS_GetSanitizedURIStringFromURI(uri, aSourceNameDest);
  }
}

static void AssignSourceNameHelper(nsIURI* aSourceURI,
                                   nsString& aSourceNameDest) {
  if (!aSourceURI) return;

  if (NS_FAILED(NS_GetSanitizedURIStringFromURI(aSourceURI, aSourceNameDest))) {
    aSourceNameDest.AssignLiteral("[nsIURI::GetSpec failed]");
  }
}

NS_IMETHODIMP
nsScriptErrorBase::Init(const nsAString& message, const nsAString& sourceName,
                        const nsAString& sourceLine, uint32_t lineNumber,
                        uint32_t columnNumber, uint32_t flags,
                        const char* category, bool fromPrivateWindow,
                        bool fromChromeContext) {
  InitializationHelper(message, sourceLine, lineNumber, columnNumber, flags,
                       category ? nsDependentCString(category) : EmptyCString(),
                       0 /* inner Window ID */, fromChromeContext);
  AssignSourceNameHelper(mSourceName, sourceName);

  mIsFromPrivateWindow = fromPrivateWindow;
  mIsFromChromeContext = fromChromeContext;
  return NS_OK;
}

void nsScriptErrorBase::InitializationHelper(
    const nsAString& message, const nsAString& sourceLine, uint32_t lineNumber,
    uint32_t columnNumber, uint32_t flags, const nsACString& category,
    uint64_t aInnerWindowID, bool aFromChromeContext) {
  mMessage.Assign(message);
  mLineNumber = lineNumber;
  mSourceLine.Assign(sourceLine);
  mColumnNumber = columnNumber;
  mFlags = flags;
  mCategory = category;
  mTimeStamp = JS_Now() / 1000;
  mInnerWindowID = aInnerWindowID;
  mIsFromChromeContext = aFromChromeContext;
}

NS_IMETHODIMP
nsScriptErrorBase::InitWithWindowID(const nsAString& message,
                                    const nsAString& sourceName,
                                    const nsAString& sourceLine,
                                    uint32_t lineNumber, uint32_t columnNumber,
                                    uint32_t flags, const nsACString& category,
                                    uint64_t aInnerWindowID,
                                    bool aFromChromeContext) {
  InitializationHelper(message, sourceLine, lineNumber, columnNumber, flags,
                       category, aInnerWindowID, aFromChromeContext);
  AssignSourceNameHelper(mSourceName, sourceName);

  if (aInnerWindowID && NS_IsMainThread()) InitializeOnMainThread();

  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::InitWithSanitizedSource(
    const nsAString& message, const nsAString& sourceName,
    const nsAString& sourceLine, uint32_t lineNumber, uint32_t columnNumber,
    uint32_t flags, const nsACString& category, uint64_t aInnerWindowID,
    bool aFromChromeContext) {
  InitializationHelper(message, sourceLine, lineNumber, columnNumber, flags,
                       category, aInnerWindowID, aFromChromeContext);
  mSourceName = sourceName;

  if (aInnerWindowID && NS_IsMainThread()) InitializeOnMainThread();

  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::InitWithSourceURI(const nsAString& message,
                                     nsIURI* sourceURI,
                                     const nsAString& sourceLine,
                                     uint32_t lineNumber, uint32_t columnNumber,
                                     uint32_t flags, const nsACString& category,
                                     uint64_t aInnerWindowID,
                                     bool aFromChromeContext) {
  InitializationHelper(message, sourceLine, lineNumber, columnNumber, flags,
                       category, aInnerWindowID, aFromChromeContext);
  AssignSourceNameHelper(sourceURI, mSourceName);

  if (aInnerWindowID && NS_IsMainThread()) InitializeOnMainThread();

  return NS_OK;
}

static nsresult ToStringHelper(const char* aSeverity, const nsString& aMessage,
                               const nsString& aSourceName,
                               const nsString* aSourceLine,
                               uint32_t aLineNumber, uint32_t aColumnNumber,
                               nsACString& /*UTF8*/ aResult) {
  static const char format0[] =
      "[%s: \"%s\" {file: \"%s\" line: %d column: %d source: \"%s\"}]";
  static const char format1[] = "[%s: \"%s\" {file: \"%s\" line: %d}]";
  static const char format2[] = "[%s: \"%s\"]";

  JS::UniqueChars temp;
  char* tempMessage = nullptr;
  char* tempSourceName = nullptr;
  char* tempSourceLine = nullptr;

  if (!aMessage.IsEmpty()) tempMessage = ToNewUTF8String(aMessage);
  if (!aSourceName.IsEmpty())
    // Use at most 512 characters from mSourceName.
    tempSourceName = ToNewUTF8String(StringHead(aSourceName, 512));
  if (aSourceLine && !aSourceLine->IsEmpty())
    // Use at most 512 characters from mSourceLine.
    tempSourceLine = ToNewUTF8String(StringHead(*aSourceLine, 512));

  if (nullptr != tempSourceName && nullptr != tempSourceLine) {
    temp = JS_smprintf(format0, aSeverity, tempMessage, tempSourceName,
                       aLineNumber, aColumnNumber, tempSourceLine);
  } else if (!aSourceName.IsEmpty()) {
    temp = JS_smprintf(format1, aSeverity, tempMessage, tempSourceName,
                       aLineNumber);
  } else {
    temp = JS_smprintf(format2, aSeverity, tempMessage);
  }

  if (nullptr != tempMessage) free(tempMessage);
  if (nullptr != tempSourceName) free(tempSourceName);
  if (nullptr != tempSourceLine) free(tempSourceLine);

  if (!temp) return NS_ERROR_OUT_OF_MEMORY;

  aResult.Assign(temp.get());
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::ToString(nsACString& /*UTF8*/ aResult) {
  static const char error[] = "JavaScript Error";
  static const char warning[] = "JavaScript Warning";

  const char* severity =
      !(mFlags & nsIScriptError::warningFlag) ? error : warning;

  return ToStringHelper(severity, mMessage, mSourceName, &mSourceLine,
                        mLineNumber, mColumnNumber, aResult);
}

NS_IMETHODIMP
nsScriptErrorBase::GetOuterWindowID(uint64_t* aOuterWindowID) {
  NS_WARNING_ASSERTION(NS_IsMainThread() || mInitializedOnMainThread,
                       "This can't be safely determined off the main thread, "
                       "returning an inaccurate value!");

  if (!mInitializedOnMainThread && NS_IsMainThread()) {
    InitializeOnMainThread();
  }

  *aOuterWindowID = mOuterWindowID;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetInnerWindowID(uint64_t* aInnerWindowID) {
  *aInnerWindowID = mInnerWindowID;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetTimeStamp(int64_t* aTimeStamp) {
  *aTimeStamp = mTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetIsFromPrivateWindow(bool* aIsFromPrivateWindow) {
  NS_WARNING_ASSERTION(NS_IsMainThread() || mInitializedOnMainThread,
                       "This can't be safely determined off the main thread, "
                       "returning an inaccurate value!");

  if (!mInitializedOnMainThread && NS_IsMainThread()) {
    InitializeOnMainThread();
  }

  *aIsFromPrivateWindow = mIsFromPrivateWindow;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetIsFromChromeContext(bool* aIsFromChromeContext) {
  NS_WARNING_ASSERTION(NS_IsMainThread() || mInitializedOnMainThread,
                       "This can't be safely determined off the main thread, "
                       "returning an inaccurate value!");
  if (!mInitializedOnMainThread && NS_IsMainThread()) {
    InitializeOnMainThread();
  }
  *aIsFromChromeContext = mIsFromChromeContext;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetNotes(nsIArray** aNotes) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t len = mNotes.Length();
  for (uint32_t i = 0; i < len; i++) array->AppendElement(mNotes[i]);
  array.forget(aNotes);

  return NS_OK;
}

/* static */
bool nsScriptErrorBase::ComputeIsFromPrivateWindow(
    nsGlobalWindowInner* aWindow) {
  // Never mark exceptions from chrome windows as having come from private
  // windows, since we always want them to be reported.
  nsIPrincipal* winPrincipal = aWindow->GetPrincipal();
  return aWindow->IsPrivateBrowsing() && !winPrincipal->IsSystemPrincipal();
}

/* static */
bool nsScriptErrorBase::ComputeIsFromChromeContext(
    nsGlobalWindowInner* aWindow) {
  nsIPrincipal* winPrincipal = aWindow->GetPrincipal();
  return winPrincipal->IsSystemPrincipal();
}

NS_IMPL_ISUPPORTS(nsScriptError, nsIConsoleMessage, nsIScriptError)

nsScriptErrorNote::nsScriptErrorNote()
    : mMessage(),
      mSourceName(),
      mSourceId(0),
      mLineNumber(0),
      mColumnNumber(0) {}

nsScriptErrorNote::~nsScriptErrorNote() = default;

void nsScriptErrorNote::Init(const nsAString& message,
                             const nsAString& sourceName, uint32_t sourceId,
                             uint32_t lineNumber, uint32_t columnNumber) {
  mMessage.Assign(message);
  AssignSourceNameHelper(mSourceName, sourceName);
  mSourceId = sourceId;
  mLineNumber = lineNumber;
  mColumnNumber = columnNumber;
}

// nsIScriptErrorNote methods
NS_IMETHODIMP
nsScriptErrorNote::GetErrorMessage(nsAString& aResult) {
  aResult.Assign(mMessage);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorNote::GetSourceName(nsAString& aResult) {
  aResult.Assign(mSourceName);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorNote::GetSourceId(uint32_t* result) {
  *result = mSourceId;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorNote::GetLineNumber(uint32_t* result) {
  *result = mLineNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorNote::GetColumnNumber(uint32_t* result) {
  *result = mColumnNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorNote::ToString(nsACString& /*UTF8*/ aResult) {
  return ToStringHelper("JavaScript Note", mMessage, mSourceName, nullptr,
                        mLineNumber, mColumnNumber, aResult);
}

NS_IMPL_ISUPPORTS(nsScriptErrorNote, nsIScriptErrorNote)
