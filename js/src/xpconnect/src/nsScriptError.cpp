/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
 *
 */

/*
 * nsIScriptError implementation.  Defined here, lacking a JS-specific
 * place to put XPCOM things.
 */

#include "xpcprivate.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsScriptError, nsIConsoleMessage, nsIScriptError);

nsScriptError::nsScriptError()
    :  mMessage(nsnull),
       mSourceName(nsnull),
       mLineNumber(0),
       mSourceLine(nsnull),
       mColumnNumber(0),
       mFlags(0),
       mCategory(nsnull)
{
    NS_INIT_REFCNT();
}

nsScriptError::~nsScriptError() {};

// nsIConsoleMessage methods
NS_IMETHODIMP
nsScriptError::GetMessage(PRUnichar **result) {
    *result = mMessage.ToNewUnicode();
    return NS_OK;
}

// nsIScriptError methods
NS_IMETHODIMP
nsScriptError::GetSourceName(PRUnichar **result) {
    *result = mSourceName.ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::GetSourceLine(PRUnichar **result) {
    *result = mSourceLine.ToNewUnicode();
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
    *result = mCategory.ToNewCString();
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
    mMessage.Assign(message);
    mSourceName.Assign(sourceName);
    mLineNumber = lineNumber;
    mSourceLine.Assign(sourceLine);
    mColumnNumber = columnNumber;
    mFlags = flags;
    mCategory.Assign(category);

    return NS_OK;
}

NS_IMETHODIMP
nsScriptError::ToString(char **_retval)
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
        tempMessage = mMessage.ToNewCString();
    if(!mSourceName.IsEmpty())
        tempSourceName = mSourceName.ToNewCString();
    if(!mSourceLine.IsEmpty())
        tempSourceLine = mSourceLine.ToNewCString();

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

    char* final = nsnull;
    if(temp)
    {
        final = (char*) nsMemory::Clone(temp,
                                        sizeof(char)*(strlen(temp)+1));
        JS_smprintf_free(temp);
    }

    *_retval = final;
    return final ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
