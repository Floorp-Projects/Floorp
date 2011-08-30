/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
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

/*
 * nsIScriptError implementation.  Defined here, lacking a JS-specific
 * place to put XPCOM things.
 */

#include "xpcprivate.h"
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"

NS_IMPL_THREADSAFE_ISUPPORTS3(nsScriptError, nsIConsoleMessage, nsIScriptError,
                              nsIScriptError2)

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
    mTimeStamp = PR_Now() / 1000;
    mInnerWindowID = aInnerWindowID;

    if(aInnerWindowID) {
        nsGlobalWindow* window =
          nsGlobalWindow::GetInnerWindowWithId(aInnerWindowID);
        if(window) {
            nsPIDOMWindow* outer = window->GetOuterWindow();
            if(outer)
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
    char* tempMessage = nsnull;
    char* tempSourceName = nsnull;
    char* tempSourceLine = nsnull;

    if(!mMessage.IsEmpty())
        tempMessage = ToNewUTF8String(mMessage);
    if(!mSourceName.IsEmpty())
        tempSourceName = ToNewUTF8String(mSourceName);
    if(!mSourceLine.IsEmpty())
        tempSourceLine = ToNewUTF8String(mSourceLine);

    if(nsnull != tempSourceName && nsnull != tempSourceLine)
        temp = JS_smprintf(format0,
                           severity,
                           tempMessage,
                           tempSourceName,
                           mLineNumber,
                           mColumnNumber,
                           tempSourceLine);
    else if(!mSourceName.IsEmpty())
        temp = JS_smprintf(format1,
                           severity,
                           tempMessage,
                           tempSourceName,
                           mLineNumber);
    else
        temp = JS_smprintf(format2,
                           severity,
                           tempMessage);

    if(nsnull != tempMessage)
        nsMemory::Free(tempMessage);
    if(nsnull != tempSourceName)
        nsMemory::Free(tempSourceName);
    if(nsnull != tempSourceLine)
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
