/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIScriptError implementation.  Defined here, lacking a JS-specific
 * place to put XPCOM things.
 */

#include "xpcprivate.h"
#include "jsprf.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"
#include "nsILoadContext.h"
#include "nsIDocShell.h"
#include "nsIScriptError.h"
#include "nsISensitiveInfoHiddenURI.h"

static_assert(nsIScriptError::errorFlag == JSREPORT_ERROR &&
              nsIScriptError::warningFlag == JSREPORT_WARNING &&
              nsIScriptError::exceptionFlag == JSREPORT_EXCEPTION &&
              nsIScriptError::strictFlag == JSREPORT_STRICT &&
              nsIScriptError::infoFlag == JSREPORT_USER_1,
              "flags should be consistent");

nsScriptErrorBase::nsScriptErrorBase()
    :  mMessage(),
       mMessageName(),
       mSourceName(),
       mLineNumber(0),
       mSourceLine(),
       mColumnNumber(0),
       mFlags(0),
       mCategory(),
       mOuterWindowID(0),
       mInnerWindowID(0),
       mTimeStamp(0),
       mInitializedOnMainThread(false),
       mIsFromPrivateWindow(false)
{
}

nsScriptErrorBase::~nsScriptErrorBase() {}

void
nsScriptErrorBase::InitializeOnMainThread()
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mInitializedOnMainThread);

    if (mInnerWindowID) {
        nsGlobalWindow* window =
          nsGlobalWindow::GetInnerWindowWithId(mInnerWindowID);
        if (window) {
            nsPIDOMWindowOuter* outer = window->GetOuterWindow();
            if (outer)
                mOuterWindowID = outer->WindowID();

            nsIDocShell* docShell = window->GetDocShell();
            nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);

            if (loadContext) {
                // Never mark exceptions from chrome windows as having come from
                // private windows, since we always want them to be reported.
                nsIPrincipal* winPrincipal = window->GetPrincipal();
                mIsFromPrivateWindow = loadContext->UsePrivateBrowsing() &&
                                       !nsContentUtils::IsSystemPrincipal(winPrincipal);
            }
        }
    }

    mInitializedOnMainThread = true;
}

// nsIConsoleMessage methods
NS_IMETHODIMP
nsScriptErrorBase::GetMessageMoz(char16_t** result) {
    nsresult rv;

    nsAutoCString message;
    rv = ToString(message);
    if (NS_FAILED(rv))
        return rv;

    *result = UTF8ToNewUnicode(message);
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


NS_IMETHODIMP
nsScriptErrorBase::GetLogLevel(uint32_t* aLogLevel)
{
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
nsScriptErrorBase::SetStack(JS::HandleValue aStack) {
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

NS_IMETHODIMP
nsScriptErrorBase::Init(const nsAString& message,
                        const nsAString& sourceName,
                        const nsAString& sourceLine,
                        uint32_t lineNumber,
                        uint32_t columnNumber,
                        uint32_t flags,
                        const char* category)
{
    return InitWithWindowID(message, sourceName, sourceLine, lineNumber,
                            columnNumber, flags,
                            category ? nsDependentCString(category)
                                     : EmptyCString(),
                            0);
}

NS_IMETHODIMP
nsScriptErrorBase::InitWithWindowID(const nsAString& message,
                                    const nsAString& sourceName,
                                    const nsAString& sourceLine,
                                    uint32_t lineNumber,
                                    uint32_t columnNumber,
                                    uint32_t flags,
                                    const nsACString& category,
                                    uint64_t aInnerWindowID)
{
    mMessage.Assign(message);

    if (!sourceName.IsEmpty()) {
        mSourceName.Assign(sourceName);

        nsCOMPtr<nsIURI> uri;
        nsAutoCString pass;
        if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), sourceName)) &&
            NS_SUCCEEDED(uri->GetPassword(pass)) &&
            !pass.IsEmpty()) {
            nsCOMPtr<nsISensitiveInfoHiddenURI> safeUri =
                do_QueryInterface(uri);

            nsAutoCString loc;
            if (safeUri &&
                NS_SUCCEEDED(safeUri->GetSensitiveInfoHiddenSpec(loc))) {
                mSourceName.Assign(NS_ConvertUTF8toUTF16(loc));
            }
        }
    }

    mLineNumber = lineNumber;
    mSourceLine.Assign(sourceLine);
    mColumnNumber = columnNumber;
    mFlags = flags;
    mCategory = category;
    mTimeStamp = JS_Now() / 1000;
    mInnerWindowID = aInnerWindowID;

    if (aInnerWindowID && NS_IsMainThread()) {
        InitializeOnMainThread();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::ToString(nsACString& /*UTF8*/ aResult)
{
    static const char format0[] =
        "[%s: \"%s\" {file: \"%s\" line: %d column: %d source: \"%s\"}]";
    static const char format1[] =
        "[%s: \"%s\" {file: \"%s\" line: %d}]";
    static const char format2[] =
        "[%s: \"%s\"]";

    static const char error[]   = "JavaScript Error";
    static const char warning[] = "JavaScript Warning";

    const char* severity = !(mFlags & JSREPORT_WARNING) ? error : warning;

    char* temp;
    char* tempMessage = nullptr;
    char* tempSourceName = nullptr;
    char* tempSourceLine = nullptr;

    if (!mMessage.IsEmpty())
        tempMessage = ToNewUTF8String(mMessage);
    if (!mSourceName.IsEmpty())
        // Use at most 512 characters from mSourceName.
        tempSourceName = ToNewUTF8String(StringHead(mSourceName, 512));
    if (!mSourceLine.IsEmpty())
        // Use at most 512 characters from mSourceLine.
        tempSourceLine = ToNewUTF8String(StringHead(mSourceLine, 512));

    if (nullptr != tempSourceName && nullptr != tempSourceLine)
        temp = JS_smprintf(format0,
                           severity,
                           tempMessage,
                           tempSourceName,
                           mLineNumber,
                           mColumnNumber,
                           tempSourceLine);
    else if (!mSourceName.IsEmpty())
        temp = JS_smprintf(format1,
                           severity,
                           tempMessage,
                           tempSourceName,
                           mLineNumber);
    else
        temp = JS_smprintf(format2,
                           severity,
                           tempMessage);

    if (nullptr != tempMessage)
        free(tempMessage);
    if (nullptr != tempSourceName)
        free(tempSourceName);
    if (nullptr != tempSourceLine)
        free(tempSourceLine);

    if (!temp)
        return NS_ERROR_OUT_OF_MEMORY;

    aResult.Assign(temp);
    JS_smprintf_free(temp);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetOuterWindowID(uint64_t* aOuterWindowID)
{
    NS_WARN_IF_FALSE(NS_IsMainThread() || mInitializedOnMainThread,
                     "This can't be safely determined off the main thread, "
                     "returning an inaccurate value!");

    if (!mInitializedOnMainThread && NS_IsMainThread()) {
        InitializeOnMainThread();
    }

    *aOuterWindowID = mOuterWindowID;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetInnerWindowID(uint64_t* aInnerWindowID)
{
    *aInnerWindowID = mInnerWindowID;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetTimeStamp(int64_t* aTimeStamp)
{
    *aTimeStamp = mTimeStamp;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorBase::GetIsFromPrivateWindow(bool* aIsFromPrivateWindow)
{
    NS_WARN_IF_FALSE(NS_IsMainThread() || mInitializedOnMainThread,
                     "This can't be safely determined off the main thread, "
                     "returning an inaccurate value!");

    if (!mInitializedOnMainThread && NS_IsMainThread()) {
        InitializeOnMainThread();
    }

    *aIsFromPrivateWindow = mIsFromPrivateWindow;
    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsScriptError, nsIConsoleMessage, nsIScriptError)
