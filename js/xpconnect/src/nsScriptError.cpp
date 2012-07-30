/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIScriptError implementation.  Defined here, lacking a JS-specific
 * place to put XPCOM things.
 */

#include "xpcprivate.h"
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsScriptError, nsIConsoleMessage, nsIScriptError)

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
       mTimeStamp(0)
{
}

nsScriptError::~nsScriptError() {}

// nsIConsoleMessage methods
NS_IMETHODIMP
nsScriptError::GetMessageMoz(PRUnichar **result) {
    nsresult rv;

    nsCAutoString message;
    rv = ToString(message);
    if (NS_FAILED(rv))
        return rv;

    *result = UTF8ToNewUnicode(message);
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;

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
nsScriptError::GetLineNumber(PRUint32 *result) {
    *result = mLineNumber;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetColumnNumber(PRUint32 *result) {
    *result = mColumnNumber;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetFlags(PRUint32 *result) {
    *result = mFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetCategory(char **result) {
    *result = ToNewCString(mCategory);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::Init(const PRUnichar *message,
                    const PRUnichar *sourceName,
                    const PRUnichar *sourceLine,
                    PRUint32 lineNumber,
                    PRUint32 columnNumber,
                    PRUint32 flags,
                    const char *category)
{
    return InitWithWindowID(message, sourceName, sourceLine, lineNumber,
                            columnNumber, flags, category, 0);
}

NS_IMETHODIMP
nsScriptError::InitWithWindowID(const PRUnichar *message,
                                const PRUnichar *sourceName,
                                const PRUnichar *sourceLine,
                                PRUint32 lineNumber,
                                PRUint32 columnNumber,
                                PRUint32 flags,
                                const char *category,
                                PRUint64 aInnerWindowID)
{
    mMessage.Assign(message);
    mSourceName.Assign(sourceName);
    mLineNumber = lineNumber;
    mSourceLine.Assign(sourceLine);
    mColumnNumber = columnNumber;
    mFlags = flags;
    mCategory.Assign(category);
    mTimeStamp = JS_Now() / 1000;
    mInnerWindowID = aInnerWindowID;

    if (aInnerWindowID) {
        nsGlobalWindow* window =
          nsGlobalWindow::GetInnerWindowWithId(aInnerWindowID);
        if (window) {
            nsPIDOMWindow* outer = window->GetOuterWindow();
            if (outer)
                mOuterWindowID = outer->WindowID();
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
nsScriptError::GetOuterWindowID(PRUint64 *aOuterWindowID)
{
    *aOuterWindowID = mOuterWindowID;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetInnerWindowID(PRUint64 *aInnerWindowID)
{
    *aInnerWindowID = mInnerWindowID;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetTimeStamp(PRInt64 *aTimeStamp)
{
    *aTimeStamp = mTimeStamp;
    return NS_OK;
}
