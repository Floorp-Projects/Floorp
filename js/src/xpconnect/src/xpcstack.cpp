/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
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

/* Implements nsIStackFrame. */

#include "xpcprivate.h"

class XPCJSStackFrame : public nsIStackFrame
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTACKFRAME

    static nsresult CreateStack(JSContext* cx, JSStackFrame* fp,
                                XPCJSStackFrame** stack);

    static nsresult CreateStackFrameLocation(
                                        PRUint32 aLanguage,
                                        const char* aFilename,
                                        const char* aFunctionName,
                                        PRInt32 aLineNumber,
                                        nsIStackFrame* aCaller,
                                        XPCJSStackFrame** stack);

    XPCJSStackFrame();
    virtual ~XPCJSStackFrame();

    JSBool IsJSFrame() const
        {return mLanguage == nsIProgrammingLanguage::JAVASCRIPT;}

private:
    nsIStackFrame* mCaller;

    char* mFilename;
    char* mFunname;
    PRInt32 mLineno;
    PRUint32 mLanguage;
};

/**********************************************/

// static

nsresult
XPCJSStack::CreateStack(JSContext* cx, nsIStackFrame** stack)
{
    if(!cx || !cx->fp)
        return NS_ERROR_FAILURE;

    return XPCJSStackFrame::CreateStack(cx, cx->fp, (XPCJSStackFrame**) stack);
}

// static
nsresult
XPCJSStack::CreateStackFrameLocation(PRUint32 aLanguage,
                                     const char* aFilename,
                                     const char* aFunctionName,
                                     PRInt32 aLineNumber,
                                     nsIStackFrame* aCaller,
                                     nsIStackFrame** stack)
{
    return XPCJSStackFrame::CreateStackFrameLocation(
                                        aLanguage,
                                        aFilename,
                                        aFunctionName,
                                        aLineNumber,
                                        aCaller,
                                        (XPCJSStackFrame**) stack);
}


/**********************************************/

XPCJSStackFrame::XPCJSStackFrame()
    :   mCaller(nsnull),
        mFilename(nsnull),
        mFunname(nsnull),
        mLineno(0),
        mLanguage(nsIProgrammingLanguage::UNKNOWN)
{
    NS_INIT_REFCNT();
}

XPCJSStackFrame::~XPCJSStackFrame()
{
    if(mFilename)
        nsMemory::Free(mFilename);
    if(mFunname)
        nsMemory::Free(mFunname);
    NS_IF_RELEASE(mCaller);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(XPCJSStackFrame, nsIStackFrame)

nsresult
XPCJSStackFrame::CreateStack(JSContext* cx, JSStackFrame* fp,
                             XPCJSStackFrame** stack)
{
    XPCJSStackFrame* self = new XPCJSStackFrame();
    JSBool failed = JS_FALSE;
    if(self)
    {
        NS_ADDREF(self);

        if(fp->down)
        {
            if(NS_FAILED(CreateStack(cx, fp->down,
                         (XPCJSStackFrame**) &self->mCaller)))
                failed = JS_TRUE;
        }

        if(!failed)
        {
            if (JS_IsNativeFrame(cx, fp))
                self->mLanguage = nsIProgrammingLanguage::CPLUSPLUS;
            else
                self->mLanguage = nsIProgrammingLanguage::JAVASCRIPT;
            if(self->IsJSFrame())
            {
                JSScript* script = JS_GetFrameScript(cx, fp);
                jsbytecode* pc = JS_GetFramePC(cx, fp);
                if(script && pc)
                {
                    const char* filename = JS_GetScriptFilename(cx, script);
                    if(filename)
                    {
                        self->mFilename = (char*)
                                nsMemory::Clone(filename,
                                        sizeof(char)*(strlen(filename)+1));
                    }

                    self->mLineno = (PRInt32) JS_PCToLineNumber(cx, script, pc);


                    JSFunction* fun = JS_GetFrameFunction(cx, fp);
                    if(fun)
                    {
                        const char* funname = JS_GetFunctionName(fun);
                        if(funname)
                        {
                        self->mFunname = (char*)
                                nsMemory::Clone(funname,
                                        sizeof(char)*(strlen(funname)+1));
                        }
                    }
                }
                else
                {
                    self->mLanguage = nsIProgrammingLanguage::CPLUSPLUS;
                }
            }
        }
        if(failed)
            NS_RELEASE(self);
    }

    *stack = self;
    return self ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

// static
nsresult
XPCJSStackFrame::CreateStackFrameLocation(PRUint32 aLanguage,
                                          const char* aFilename,
                                          const char* aFunctionName,
                                          PRInt32 aLineNumber,
                                          nsIStackFrame* aCaller,
                                          XPCJSStackFrame** stack)
{
    JSBool failed = JS_FALSE;
    XPCJSStackFrame* self = new XPCJSStackFrame();
    if(self)
        NS_ADDREF(self);
    else
        failed = JS_TRUE;

    if(!failed)
    {
        self->mLanguage = aLanguage;
        self->mLineno = aLineNumber;
    }

    if(!failed && aFilename)
    {
        self->mFilename = (char*)
                nsMemory::Clone(aFilename,
                        sizeof(char)*(strlen(aFilename)+1));
        if(!self->mFilename)
            failed = JS_TRUE;
    }

    if(!failed && aFunctionName)
    {
        self->mFunname = (char*)
                nsMemory::Clone(aFunctionName,
                        sizeof(char)*(strlen(aFunctionName)+1));
        if(!self->mFunname)
            failed = JS_TRUE;
    }

    if(!failed && aCaller)
    {
        NS_ADDREF(aCaller);
        self->mCaller = aCaller;
    }

    if(failed && self)
    {
        NS_RELEASE(self);   // sets self to nsnull
    }

    *stack = self;
    return self ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute PRUint32 language; */
NS_IMETHODIMP XPCJSStackFrame::GetLanguage(PRUint32 *aLanguage)
{
    *aLanguage = mLanguage;
    return NS_OK;
}

/* readonly attribute string languageName; */
NS_IMETHODIMP XPCJSStackFrame::GetLanguageName(char * *aLanguageName)
{
    static const char js[] = "JavaScript";
    static const char cpp[] = "C++";
    char* temp;

    if(IsJSFrame())
        *aLanguageName = temp = (char*) nsMemory::Clone(js, sizeof(js));
    else
        *aLanguageName = temp = (char*) nsMemory::Clone(cpp, sizeof(cpp));

    return temp ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute string filename; */
NS_IMETHODIMP XPCJSStackFrame::GetFilename(char * *aFilename)
{
    XPC_STRING_GETTER_BODY(aFilename, mFilename);
}

/* readonly attribute string name; */
NS_IMETHODIMP XPCJSStackFrame::GetName(char * *aFunction)
{
    XPC_STRING_GETTER_BODY(aFunction, mFunname);
}

/* readonly attribute PRInt32 lineNumber; */
NS_IMETHODIMP XPCJSStackFrame::GetLineNumber(PRInt32 *aLineNumber)
{
    if(!aLineNumber)
        return NS_ERROR_NULL_POINTER;

    *aLineNumber = mLineno;
    return NS_OK;
}

/* readonly attribute string sourceLine; */
NS_IMETHODIMP XPCJSStackFrame::GetSourceLine(char * *aSourceLine)
{
    if(!aSourceLine)
        return NS_ERROR_NULL_POINTER;
    *aSourceLine = nsnull;
    return NS_OK;
}

/* readonly attribute nsIStackFrame caller; */
NS_IMETHODIMP XPCJSStackFrame::GetCaller(nsIStackFrame * *aCaller)
{
    if(!aCaller)
        return NS_ERROR_NULL_POINTER;

    if(mCaller)
        NS_ADDREF(mCaller);
    *aCaller = mCaller;
    return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP XPCJSStackFrame::ToString(char **_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    const char* frametype = IsJSFrame() ? "JS" : "native";
    const char* filename = mFilename ? mFilename : "<unknown filename>";
    const char* funname = mFunname ? mFunname : "<TOP_LEVEL>";
    static const char format[] = "%s frame :: %s :: %s :: line %d";
    int len = sizeof(char)*
                (strlen(frametype) + strlen(filename) + strlen(funname)) +
              sizeof(format) + 6 /* space for lineno */;

    char* buf = (char*) nsMemory::Alloc(len);
    if(!buf)
        return NS_ERROR_OUT_OF_MEMORY;

    JS_snprintf(buf, len, format, frametype, filename, funname, mLineno);
    *_retval = buf;
    return NS_OK;
}

