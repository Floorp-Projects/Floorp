/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Implements nsIJSStackFrameLocation */

#include "xpcprivate.h"

class XPCJSStackFrame : public nsIJSStackFrameLocation
{
public:
    NS_DECL_ISUPPORTS

    /* readonly attribute boolean isJSFrame; */
    NS_IMETHOD GetIsJSFrame(PRBool *aIsJSFrame);

    /* readonly attribute string filename; */
    NS_IMETHOD GetFilename(char * *aFilename);

    /* readonly attribute string function; */
    NS_IMETHOD GetFunctionName(char * *aFunction);

    /* readonly attribute PRInt32 lineNumber; */
    NS_IMETHOD GetLineNumber(PRInt32 *aLineNumber);

    /* readonly attribute nsIJSStackFrameLocation caller; */
    NS_IMETHOD GetCaller(nsIJSStackFrameLocation * *aCaller);

    /* string toString (); */
    NS_IMETHOD ToString(char **_retval);

    static XPCJSStackFrame* CreateStack(JSContext* cx, JSStackFrame* fp);

    static XPCJSStackFrame* CreateStackFrameLocation(
                                        JSBool isJSFrame,
                                        const char* aFilename,
                                        const char* aFunctionName,
                                        PRInt32 aLineNumber,
                                        nsIJSStackFrameLocation* aCaller);

    XPCJSStackFrame();
    virtual ~XPCJSStackFrame();

private:
    nsIJSStackFrameLocation* mCaller;

    char* mFilename;
    char* mFunname;
    PRInt32 mLineno;
    JSBool mJSFrame;
};

/**********************************************/

// static
nsIJSStackFrameLocation*
XPCJSStack::CreateStack(JSContext* cx)
{
    if(!cx || !cx->fp)
        return nsnull;

    return XPCJSStackFrame::CreateStack(cx, cx->fp);
}

// static
nsIJSStackFrameLocation*
XPCJSStack::CreateStackFrameLocation(JSBool isJSFrame,
                                     const char* aFilename,
                                     const char* aFunctionName,
                                     PRInt32 aLineNumber,
                                     nsIJSStackFrameLocation* aCaller)
{
    return XPCJSStackFrame::CreateStackFrameLocation(
                                        isJSFrame,
                                        aFilename,
                                        aFunctionName,
                                        aLineNumber,
                                        aCaller);
}


/**********************************************/

XPCJSStackFrame::XPCJSStackFrame()
    :   mCaller(nsnull),
        mFilename(nsnull),
        mFunname(nsnull),
        mLineno(0),
        mJSFrame(JS_FALSE)
{
    NS_INIT_REFCNT();
}

XPCJSStackFrame::~XPCJSStackFrame()
{
    if(mFilename)
        nsMemory::Free(mFilename);
    if(mFunname)
        nsMemory::Free(mFunname);
    if(mCaller)
        NS_RELEASE(mCaller);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(XPCJSStackFrame, nsIJSStackFrameLocation)

XPCJSStackFrame*
XPCJSStackFrame::CreateStack(JSContext* cx, JSStackFrame* fp)
{
    XPCJSStackFrame* self = new XPCJSStackFrame();
    JSBool failed = JS_FALSE;
    if(self)
    {
        NS_ADDREF(self);

        if(fp->down)
        {
            if(!(self->mCaller = CreateStack(cx, fp->down)))
                failed = JS_TRUE;
        }

        if(!failed)
        {
            self->mJSFrame = !JS_IsNativeFrame(cx, fp);
            if(self->mJSFrame)
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
                    self->mJSFrame = JS_FALSE;
                }
            }
        }
        if(failed)
            NS_RELEASE(self);
    }

    return self;
}

// static
XPCJSStackFrame*
XPCJSStackFrame::CreateStackFrameLocation(JSBool isJSFrame,
                                          const char* aFilename,
                                          const char* aFunctionName,
                                          PRInt32 aLineNumber,
                                          nsIJSStackFrameLocation* aCaller)
{
    JSBool failed = JS_FALSE;
    XPCJSStackFrame* self = new XPCJSStackFrame();
    if(self)
        NS_ADDREF(self);
    else
        failed = JS_TRUE;

    if(!failed)
    {
        self->mJSFrame = isJSFrame;
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
    return self;
}

/* readonly attribute boolean isJSFrame; */
NS_IMETHODIMP XPCJSStackFrame::GetIsJSFrame(PRBool *aIsJSFrame)
{
    if(!aIsJSFrame)
        return NS_ERROR_NULL_POINTER;

    *aIsJSFrame = mJSFrame;
    return NS_OK;
}

/* readonly attribute string filename; */
NS_IMETHODIMP XPCJSStackFrame::GetFilename(char * *aFilename)
{
    XPC_STRING_GETTER_BODY(aFilename, mFilename);
}

/* readonly attribute string function; */
NS_IMETHODIMP XPCJSStackFrame::GetFunctionName(char * *aFunction)
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

/* readonly attribute nsIJSStackFrameLocation caller; */
NS_IMETHODIMP XPCJSStackFrame::GetCaller(nsIJSStackFrameLocation * *aCaller)
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

    const char* frametype = mJSFrame ? "JS" : "native";
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

