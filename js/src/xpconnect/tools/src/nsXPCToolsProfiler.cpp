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

/* Implements nsXPCToolsProfiler. */

#include "xpctools_private.h"

/***************************************************************************/

class FunctionKey : public nsHashKey {
protected:
    uintN         mLineno;
    uintN         mExtent;

public:
    FunctionKey(uintN         aLineno,
                uintN         aExtent)
        : mLineno(aLineno), mExtent(aExtent) {}
    ~FunctionKey(void) {}

    PRUint32 HashCode(void) const 
        {return (17*mLineno) + (7*mExtent);}
    PRBool Equals(const nsHashKey* aKey) const
        {const FunctionKey* o = (const FunctionKey*) aKey; 
         return (mLineno == o->mLineno) && (mExtent == o->mExtent);}
    nsHashKey* Clone() const
        {return new FunctionKey(mLineno, mExtent);}
};

/***************************************************************************/

ProfilerFile::ProfilerFile(const char* filename)
    :   mName(filename ? nsCRT::strdup(filename) : nsnull),
        mFunctionTable(new nsHashtable(16, PR_FALSE))
{
    // empty
}

ProfilerFile::~ProfilerFile()
{
    if(mName)
        nsCRT::free(mName);
    if(mFunctionTable)
        delete mFunctionTable;            
}

ProfilerFunction* 
ProfilerFile::FindOrAddFunction(const char* aName,
                                uintN aBaseLineNumber,
                                uintN aLineExtent,
                                size_t aTotalSize)
{
    if(!mFunctionTable)
        return nsnull;
    FunctionKey key(aBaseLineNumber, aLineExtent);
    ProfilerFunction* fun = (ProfilerFunction*) mFunctionTable->Get(&key);
    if(!fun)
    {
        fun = new ProfilerFunction(aName, aBaseLineNumber, aLineExtent,
                                   aTotalSize, this);
        if(fun)
            mFunctionTable->Put(&key, fun);
    }
    return fun; 
}

void ProfilerFile::EnumerateFunctions(nsHashtableEnumFunc aEnumFunc, void* closure)
{
    if(mFunctionTable)
        mFunctionTable->Enumerate(aEnumFunc, closure);
            
}

/***************************************************************************/

ProfilerFunction::ProfilerFunction(const char* name, 
                                   uintN lineno, uintn extent, size_t totalsize,
                                   ProfilerFile* file)
    :   mName(name ? nsCRT::strdup(name) : nsnull),
        mBaseLineNumber(lineno),
        mLineExtent(extent),
        mTotalSize(totalsize),
        mFile(file),
        mCallCount(0),
        mCompileCount(0),
        mQuickTime((PRUint32) -1),
        mLongTime(0),
        mStartTime(0),
        mSum(0)
{
    // empty        
}

ProfilerFunction::~ProfilerFunction()
{
    if(mName)
        nsCRT::free(mName);
}

/***************************************************************************/


NS_IMPL_ISUPPORTS1(nsXPCToolsProfiler, nsIXPCToolsProfiler)

nsXPCToolsProfiler::nsXPCToolsProfiler()
    :   mLock(PR_NewLock()),
        mRuntime(nsnull),
        mFileTable(new nsHashtable(128, PR_FALSE)),
        mScriptTable(new nsHashtable(256, PR_FALSE))
{
    NS_INIT_ISUPPORTS();
    InitializeRuntime();
}

JS_STATIC_DLL_CALLBACK(PRBool)
xpctools_ProfilerFunctionDeleter(nsHashKey *aKey, void *aData, void* closure)
{
    delete (ProfilerFunction*) aData;
    return PR_TRUE;        
}        

JS_STATIC_DLL_CALLBACK(PRBool)
xpctools_ProfilerFileDeleter(nsHashKey *aKey, void *aData, void* closure)
{
    ProfilerFile* file = (ProfilerFile*) aData;
    file->EnumerateFunctions(xpctools_ProfilerFunctionDeleter, closure);
    delete file;
    return PR_TRUE;        
}        

nsXPCToolsProfiler::~nsXPCToolsProfiler()
{
    Stop();
    if(mLock)
        PR_DestroyLock(mLock);
    if(mFileTable)
    {
        mFileTable->Reset(xpctools_ProfilerFileDeleter, this);
        delete mFileTable;
    }
    if(mScriptTable)
    {
        // elements not owned - don't purge them
        delete mScriptTable;
    }
}

/***************************************************************************/
// the hooks...

/* called just after script creation */
JS_STATIC_DLL_CALLBACK(void)
xpctools_JSNewScriptHook(JSContext  *cx,
                         const char *filename,  /* URL of script */
                         uintN      lineno,     /* line script starts */
                         JSScript   *script,
                         JSFunction *fun,
                         void       *callerdata)
{
    if(!script)
        return;
    if(!filename)
        filename = "<<<!!! has no name so may represent many different pages !!!>>>";
    nsXPCToolsProfiler* self = (nsXPCToolsProfiler*) callerdata;    
    nsAutoLock lock(self->mLock);

    if(self->mFileTable)
    {
        nsCStringKey key(filename);
        ProfilerFile* file = (ProfilerFile*) self->mFileTable->Get(&key);
        if(!file)
        {
            file = new ProfilerFile(filename);
            self->mFileTable->Put(&key, file);
        }
        if(file)
        {
            ProfilerFunction* function = 
                file->FindOrAddFunction(fun ? JS_GetFunctionName(fun) : nsnull, 
                                        JS_GetScriptBaseLineNumber(cx, script),
                                        JS_GetScriptLineExtent(cx, script),
                                        fun
                                        ? JS_GetFunctionTotalSize(cx, fun)
                                        : JS_GetScriptTotalSize(cx, script));
            if(function)
            {
                function->IncrementCompileCount();
                if(self->mScriptTable)
                {
                    nsVoidKey scriptkey(script);
                    self->mScriptTable->Put(&scriptkey, function);
                }
            }
        }
    }
}

/* called just before script destruction */
JS_STATIC_DLL_CALLBACK(void)
xpctools_JSDestroyScriptHook(JSContext  *cx,
                             JSScript   *script,
                             void       *callerdata)
{
    if(!script)
        return;
    nsXPCToolsProfiler* self = (nsXPCToolsProfiler*) callerdata;    
    nsAutoLock lock(self->mLock);
    if(self->mScriptTable)
    {
        nsVoidKey scriptkey(script);
        self->mScriptTable->Remove(&scriptkey);
    }
}


/* called on entry and return of functions and top level scripts */
JS_STATIC_DLL_CALLBACK(void*)
xpctools_InterpreterHook(JSContext *cx, JSStackFrame *fp, JSBool before,
                         JSBool *ok, void *closure)
{
    // ignore returns
    NS_ASSERTION(fp, "bad frame pointer!");

    JSScript* script = fp->script;
    if(script)
    {
        nsXPCToolsProfiler* self = (nsXPCToolsProfiler*) closure;    
        nsAutoLock lock(self->mLock);
        if(self->mScriptTable)
        {
            nsVoidKey scriptkey(script);
            ProfilerFunction* fun = 
                (ProfilerFunction*) self->mScriptTable->Get(&scriptkey);
            if(fun)
            {
                if(before == PR_TRUE)
                {
                    fun->IncrementCallCount();
                    fun->SetStartTime();
                }
                else
                {
                    fun->SetEndTime();
                }
            }
        }
    }   
    return closure;
}

/***************************************************************************/
// interface methods

/* void start (); */
NS_IMETHODIMP nsXPCToolsProfiler::Start()
{
    nsAutoLock lock(mLock);
    if(!VerifyRuntime())
        return NS_ERROR_UNEXPECTED; 
    
    JS_SetNewScriptHook(mRuntime, xpctools_JSNewScriptHook, this);
    JS_SetDestroyScriptHook(mRuntime, xpctools_JSDestroyScriptHook, this);
    JS_SetExecuteHook(mRuntime, xpctools_InterpreterHook, this);
    JS_SetCallHook(mRuntime, xpctools_InterpreterHook, this);

    return NS_OK;
}

/* void stop (); */
NS_IMETHODIMP nsXPCToolsProfiler::Stop()
{
    nsAutoLock lock(mLock);
    if(!VerifyRuntime())
        return NS_ERROR_UNEXPECTED; 
    
    JS_SetNewScriptHook(mRuntime, nsnull, nsnull);
    JS_SetDestroyScriptHook(mRuntime, nsnull, nsnull);
    JS_SetExecuteHook(mRuntime, nsnull, this);
    JS_SetCallHook(mRuntime, nsnull, this);
    
    return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP nsXPCToolsProfiler::Clear()
{
  // XXX implement me!
  return NS_ERROR_NOT_IMPLEMENTED;
}


JS_STATIC_DLL_CALLBACK(PRBool)
xpctools_FunctionNamePrinter(nsHashKey *aKey, void *aData, void* closure)
{
    ProfilerFunction* fun = (ProfilerFunction*) aData;
    FILE* out = (FILE*) closure;
    const char* name = fun->GetName();
    PRUint32 average;
    PRUint32 count;

    count = fun->GetCallCount();
    if (count != 0)
        average = fun->GetSum() / count;
    if(!name)
        name = "<top level>"; 
    fprintf(out,
        "    [%lu,%lu] %s() {%d-%d} %lu ",
        (unsigned long) fun->GetCompileCount(),
        (unsigned long) fun->GetCallCount(),
        name,
        (int) fun->GetBaseLineNumber(),
        (int)(fun->GetBaseLineNumber()+fun->GetLineExtent()-1),
        (unsigned long) fun->GetTotalSize());
    if(count != 0)
        fprintf(out,
            "{min %lu, max %lu avg %lu}\n",
            (unsigned long) fun->GetQuickTime(),
            (unsigned long) fun->GetLongTime(),
            (unsigned long) average);
    else
        fprintf(out, "\n" );
    return PR_TRUE;        
}        

JS_STATIC_DLL_CALLBACK(PRBool)
xpctools_FilenamePrinter(nsHashKey *aKey, void *aData, void* closure)
{
    ProfilerFile* file = (ProfilerFile*) aData;
    FILE* out = (FILE*) closure;
    fprintf(out, "%s\n", file->GetName());
    file->EnumerateFunctions(xpctools_FunctionNamePrinter, closure);
    return PR_TRUE;        
}        

/* void writeResults (in nsILocalFile aFile); */
NS_IMETHODIMP nsXPCToolsProfiler::WriteResults(nsILocalFile *aFile)
{
    nsAutoLock lock(mLock);
    if(!aFile)
        return NS_ERROR_FAILURE;
    FILE* out;
    if(NS_FAILED(aFile->OpenANSIFileDesc("w", &out)) || ! out)
        return NS_ERROR_FAILURE;

    if(mFileTable)
        mFileTable->Enumerate(xpctools_FilenamePrinter, out);
    return NS_OK;
}

/***************************************************************************/
// additional utility methods

JSBool
nsXPCToolsProfiler::VerifyRuntime()
{
    JSRuntime* rt;
    nsCOMPtr<nsIJSRuntimeService> rts = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
    return rts && NS_SUCCEEDED(rts->GetRuntime(&rt)) && rt && rt == mRuntime;
}

JSBool 
nsXPCToolsProfiler::InitializeRuntime()
{
    NS_ASSERTION(!mRuntime, "can't init runtime twice");
    JSRuntime* rt;
    nsCOMPtr<nsIJSRuntimeService> rts = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
    if(rts && NS_SUCCEEDED(rts->GetRuntime(&rt)) && rt)
        mRuntime = rt;
    return mRuntime != nsnull;
}
