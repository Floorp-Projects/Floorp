/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
