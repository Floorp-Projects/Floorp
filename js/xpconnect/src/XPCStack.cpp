/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implements nsIStackFrame. */

#include "xpcprivate.h"

class XPCJSStackFrame : public nsIStackFrame
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISTACKFRAME

    static nsresult CreateStack(JSContext* cx, XPCJSStackFrame** stack);

    static nsresult CreateStackFrameLocation(uint32_t aLanguage,
                                             const char* aFilename,
                                             const char* aFunctionName,
                                             int32_t aLineNumber,
                                             nsIStackFrame* aCaller,
                                             XPCJSStackFrame** stack);

    XPCJSStackFrame();
    virtual ~XPCJSStackFrame();

    bool IsJSFrame() const
        {return mLanguage == nsIProgrammingLanguage::JAVASCRIPT;}

private:
    nsCOMPtr<nsIStackFrame> mCaller;

    char* mFilename;
    char* mFunname;
    int32_t mLineno;
    uint32_t mLanguage;
};

/**********************************************/

// static

nsresult
XPCJSStack::CreateStack(JSContext* cx, nsIStackFrame** stack)
{
    if (!cx)
        return NS_ERROR_FAILURE;

    return XPCJSStackFrame::CreateStack(cx, (XPCJSStackFrame**) stack);
}

// static
nsresult
XPCJSStack::CreateStackFrameLocation(uint32_t aLanguage,
                                     const char* aFilename,
                                     const char* aFunctionName,
                                     int32_t aLineNumber,
                                     nsIStackFrame* aCaller,
                                     nsIStackFrame** stack)
{
    return XPCJSStackFrame::CreateStackFrameLocation(aLanguage,
                                                     aFilename,
                                                     aFunctionName,
                                                     aLineNumber,
                                                     aCaller,
                                                     (XPCJSStackFrame**) stack);
}


/**********************************************/

XPCJSStackFrame::XPCJSStackFrame()
    :   mFilename(nullptr),
        mFunname(nullptr),
        mLineno(0),
        mLanguage(nsIProgrammingLanguage::UNKNOWN)
{
}

XPCJSStackFrame::~XPCJSStackFrame()
{
    if (mFilename)
        nsMemory::Free(mFilename);
    if (mFunname)
        nsMemory::Free(mFunname);
}

NS_IMPL_ISUPPORTS1(XPCJSStackFrame, nsIStackFrame)

nsresult
XPCJSStackFrame::CreateStack(JSContext* cx, XPCJSStackFrame** stack)
{
    static const unsigned MAX_FRAMES = 100;

    nsRefPtr<XPCJSStackFrame> first = new XPCJSStackFrame();
    nsRefPtr<XPCJSStackFrame> self = first;

    JS::StackDescription* desc = JS::DescribeStack(cx, MAX_FRAMES);
    if (!desc)
        return NS_ERROR_FAILURE;

    for (size_t i = 0; i < desc->nframes && self; i++) {
        self->mLanguage = nsIProgrammingLanguage::JAVASCRIPT;

	JSAutoCompartment ac(cx, desc->frames[i].script);
        const char* filename = JS_GetScriptFilename(cx, desc->frames[i].script);
        if (filename) {
            self->mFilename = (char*)
                nsMemory::Clone(filename,
                                sizeof(char)*(strlen(filename)+1));
        }

        self->mLineno = desc->frames[i].lineno;

        JSFunction* fun = desc->frames[i].fun;
        if (fun) {
            JS::RootedString funid(cx, JS_GetFunctionDisplayId(fun));
            if (funid) {
                size_t length = JS_GetStringEncodingLength(cx, funid);
                if (length != size_t(-1)) {
                    self->mFunname = static_cast<char *>(nsMemory::Alloc(length + 1));
                    if (self->mFunname) {
                        JS_EncodeStringToBuffer(cx, funid, self->mFunname, length);
                        self->mFunname[length] = '\0';
                    }
                }
            }
        }

        XPCJSStackFrame* frame = new XPCJSStackFrame();
        self->mCaller = frame;
        self = frame;
    }

    JS::FreeStackDescription(cx, desc);

    *stack = first.forget().get();
    return NS_OK;
}

// static
nsresult
XPCJSStackFrame::CreateStackFrameLocation(uint32_t aLanguage,
                                          const char* aFilename,
                                          const char* aFunctionName,
                                          int32_t aLineNumber,
                                          nsIStackFrame* aCaller,
                                          XPCJSStackFrame** stack)
{
    bool failed = false;
    XPCJSStackFrame* self = new XPCJSStackFrame();
    if (self)
        NS_ADDREF(self);
    else
        failed = true;

    if (!failed) {
        self->mLanguage = aLanguage;
        self->mLineno = aLineNumber;
    }

    if (!failed && aFilename) {
        self->mFilename = (char*)
                nsMemory::Clone(aFilename,
                                sizeof(char)*(strlen(aFilename)+1));
        if (!self->mFilename)
            failed = true;
    }

    if (!failed && aFunctionName) {
        self->mFunname = (char*)
                nsMemory::Clone(aFunctionName,
                                sizeof(char)*(strlen(aFunctionName)+1));
        if (!self->mFunname)
            failed = true;
    }

    if (!failed && aCaller) {
        self->mCaller = aCaller;
    }

    if (failed && self) {
        NS_RELEASE(self);   // sets self to nullptr
    }

    *stack = self;
    return self ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute uint32_t language; */
NS_IMETHODIMP XPCJSStackFrame::GetLanguage(uint32_t *aLanguage)
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

    if (IsJSFrame())
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

/* readonly attribute int32_t lineNumber; */
NS_IMETHODIMP XPCJSStackFrame::GetLineNumber(int32_t *aLineNumber)
{
    *aLineNumber = mLineno;
    return NS_OK;
}

/* readonly attribute string sourceLine; */
NS_IMETHODIMP XPCJSStackFrame::GetSourceLine(char * *aSourceLine)
{
    *aSourceLine = nullptr;
    return NS_OK;
}

/* readonly attribute nsIStackFrame caller; */
NS_IMETHODIMP XPCJSStackFrame::GetCaller(nsIStackFrame * *aCaller)
{
    NS_IF_ADDREF(*aCaller = mCaller);
    return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP XPCJSStackFrame::ToString(char **_retval)
{
    const char* frametype = IsJSFrame() ? "JS" : "native";
    const char* filename = mFilename ? mFilename : "<unknown filename>";
    const char* funname = mFunname ? mFunname : "<TOP_LEVEL>";
    static const char format[] = "%s frame :: %s :: %s :: line %d";
    int len = sizeof(char)*
                (strlen(frametype) + strlen(filename) + strlen(funname)) +
              sizeof(format) + 6 /* space for lineno */;

    char* buf = (char*) nsMemory::Alloc(len);
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;

    JS_snprintf(buf, len, format, frametype, filename, funname, mLineno);
    *_retval = buf;
    return NS_OK;
}

