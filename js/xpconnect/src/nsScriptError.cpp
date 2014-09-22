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
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"
#include "nsILoadContext.h"
#include "nsIDocShell.h"

NS_IMPL_ISUPPORTS(nsScriptError, nsIConsoleMessage, nsIScriptError)

nsScriptError::nsScriptError()
    :  mMessage(),
       mSourceName(),
       mLineNumber(0),
       mSourceLine(),
       mColumnNumber(0),
       mFlags(0),
       mCategory(),
       mOuterWindowID(0),
       mInnerWindowID(0),
       mTimeStamp(0),
       mIsFromPrivateWindow(false)
{
}

nsScriptError::~nsScriptError() {}

// nsIConsoleMessage methods
NS_IMETHODIMP
nsScriptError::GetMessageMoz(char16_t **result) {
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
nsScriptError::GetLogLevel(uint32_t* aLogLevel)
{
  *aLogLevel = mFlags & (uint32_t)nsIScriptError::errorFlag ?
               nsIConsoleMessage::error : nsIConsoleMessage::warn;
  return NS_OK;
}

// nsIScriptError methods
NS_IMETHODIMP
nsScriptError::GetErrorMessage(nsAString& aResult) {
    aResult.Assign(mMessage);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetSourceName(nsAString& aResult) {
    aResult.Assign(mSourceName);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetSourceLine(nsAString& aResult) {
    aResult.Assign(mSourceLine);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetLineNumber(uint32_t *result) {
    *result = mLineNumber;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetColumnNumber(uint32_t *result) {
    *result = mColumnNumber;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetFlags(uint32_t *result) {
    *result = mFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetCategory(char **result) {
    *result = ToNewCString(mCategory);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::Init(const nsAString& message,
                    const nsAString& sourceName,
                    const nsAString& sourceLine,
                    uint32_t lineNumber,
                    uint32_t columnNumber,
                    uint32_t flags,
                    const char *category)
{
    return InitWithWindowID(message, sourceName, sourceLine, lineNumber,
                            columnNumber, flags,
                            category ? nsDependentCString(category)
                                     : EmptyCString(),
                            0);
}

NS_IMETHODIMP
nsScriptError::InitWithWindowID(const nsAString& message,
                                const nsAString& sourceName,
                                const nsAString& sourceLine,
                                uint32_t lineNumber,
                                uint32_t columnNumber,
                                uint32_t flags,
                                const nsACString& category,
                                uint64_t aInnerWindowID)
{
    mMessage.Assign(message);
    mSourceName.Assign(sourceName);
    mLineNumber = lineNumber;
    mSourceLine.Assign(sourceLine);
    mColumnNumber = columnNumber;
    mFlags = flags;
    mCategory = category;
    mTimeStamp = JS_Now() / 1000;
    mInnerWindowID = aInnerWindowID;

    if (aInnerWindowID) {
        nsGlobalWindow* window =
          nsGlobalWindow::GetInnerWindowWithId(aInnerWindowID);
        if (window) {
            nsPIDOMWindow* outer = window->GetOuterWindow();
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

    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::ToString(nsACString& /*UTF8*/ aResult)
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
        tempSourceName = ToNewUTF8String(mSourceName);
    if (!mSourceLine.IsEmpty())
        tempSourceLine = ToNewUTF8String(mSourceLine);

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
        nsMemory::Free(tempMessage);
    if (nullptr != tempSourceName)
        nsMemory::Free(tempSourceName);
    if (nullptr != tempSourceLine)
        nsMemory::Free(tempSourceLine);

    if (!temp)
        return NS_ERROR_OUT_OF_MEMORY;

    aResult.Assign(temp);
    JS_smprintf_free(temp);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetOuterWindowID(uint64_t *aOuterWindowID)
{
    *aOuterWindowID = mOuterWindowID;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetInnerWindowID(uint64_t *aInnerWindowID)
{
    *aInnerWindowID = mInnerWindowID;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetTimeStamp(int64_t *aTimeStamp)
{
    *aTimeStamp = mTimeStamp;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetIsFromPrivateWindow(bool *aIsFromPrivateWindow)
{
    *aIsFromPrivateWindow = mIsFromPrivateWindow;
    return NS_OK;
}
