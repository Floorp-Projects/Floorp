/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* All the XPCTools private declarations - only include locally. */

#ifndef xpctoolsprivate_h___
#define xpctoolsprivate_h___

#include <string.h>
#include <stdlib.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIMemory.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "jsapi.h"
#include "jshash.h"
#include "jsprf.h"
#include "jsinterp.h"
#include "jscntxt.h"
#include "jsdbgapi.h"

#include "nsILocalFile.h"

#include "nsIJSRuntimeService.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsServiceManagerUtils.h"

#include "nsIXPCToolsCompiler.h"
#include "nsIXPCToolsProfiler.h"

class nsXPCToolsCompiler : public nsIXPCToolsCompiler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTOOLSCOMPILER

  nsXPCToolsCompiler();
  virtual ~nsXPCToolsCompiler();
  // XXX add additional members
};

/***************************************************************************/

class ProfilerFile;

class ProfilerFunction
{
public:
    ProfilerFunction(const char* name, 
                     uintN lineno, uintn extent, size_t totalsize,
                     ProfilerFile* file);
    ~ProfilerFunction();

    const char*   GetName() const {return mName;}
    ProfilerFile* GetFile() const {return mFile;}
    uintN         GetBaseLineNumber() const {return mBaseLineNumber;}
    uintN         GetLineExtent() const {return mLineExtent;}
    size_t        GetTotalSize() const { return mTotalSize; }
    void          IncrementCallCount() {++mCallCount;}
    PRUint32      GetCallCount() {return mCallCount;}
    PRUint32      GetSum() {return mSum;}
    void          IncrementCompileCount() {++mCompileCount;}
    PRUint32      GetCompileCount() {return mCompileCount;}
    PRUint32      NowInMilliSecs()
        {PRUint64 now64 = LL_INIT(0,1000);
         PRUint32 now32;
         LL_DIV(now64,PR_Now(),now64);
         LL_L2UI(now32, now64);
         return now32;
        }
    void          SetStartTime() {mStartTime = NowInMilliSecs();}
    PRUint32      SetEndTime()
        {PRUint32 delta = NowInMilliSecs() - mStartTime;
         if(delta < mQuickTime)
            mQuickTime = delta;
         if (delta > mLongTime)
            mLongTime = delta;
         mSum += delta;
         return delta;}
    PRUint32      GetQuickTime() {return mQuickTime;}
    PRUint32      GetLongTime() {return mLongTime;}
    
    void          AddNotSelfTime(PRUint32 delta) {mNotSelfSum += delta;}
    PRUint32      GetSelf() { return mSum - mNotSelfSum;}

    ProfilerFunction(); // not implemented

private:
    char*           mName;
    uintN           mBaseLineNumber;
    uintN           mLineExtent;
    size_t          mTotalSize;     // JSFunction, JSScript, bytecode, &c, size
    ProfilerFile*   mFile;
    PRUint32        mCallCount;
    PRUint32        mCompileCount;
    PRUint32        mQuickTime;     // quickest delta in msec
    PRUint32        mLongTime;      // longest delta in msec
    PRUint32        mStartTime;     // time on enter
    PRUint32        mSum;
    PRUint32        mNotSelfSum;
};

struct FunctionID
{
    uintN lineno;
    uintN extent;

    FunctionID(uintN aLineno, uintN aExtent) :
        lineno(aLineno), extent(aExtent) { }

    PRBool operator==(const FunctionID &aOther) const {
        return aOther.lineno == this->lineno &&
            aOther.extent == this->extent;
    }
};

class FunctionKey : public PLDHashEntryHdr
{
public:
    typedef const FunctionID &KeyType;
    typedef const FunctionID *KeyTypePointer;

    FunctionKey(KeyTypePointer aF) : mF(*aF) { }
    FunctionKey(const FunctionKey &toCopy) : mF(toCopy.mF) { }
    ~FunctionKey() { }

    KeyType GetKey() const { return mF; }
    PRBool KeyEquals(const KeyTypePointer aKey) const
    {
        return mF == *aKey;
    }

    static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
    static PLDHashNumber HashKey(const KeyTypePointer aKey) {
        return (aKey->lineno * 17) | (aKey->extent * 7);
    }

    enum { ALLOW_MEMMOVE = PR_TRUE };

private:
    const FunctionID mF;
};

class ProfilerFile
{
public:
    ProfilerFile(const char* filename);
    ~ProfilerFile();

    
    ProfilerFunction* FindOrAddFunction(const char* aName,
                                        uintN aBaseLineNumber,
                                        uintN aLineExtent,
                                        size_t aTotalSize);

    typedef nsClassHashtable<FunctionKey, ProfilerFunction> FunctionTableType;
    typedef FunctionTableType::EnumReadFunction EnumeratorType;

    void EnumerateFunctions(EnumeratorType aFunc,
                            void *aClosure) const;

    const char* GetName() const {return mName;}

private:
    char*             mName;
    FunctionTableType mFunctionTable;
};

class nsXPCToolsProfiler : public nsIXPCToolsProfiler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTOOLSPROFILER

  nsXPCToolsProfiler();
  virtual ~nsXPCToolsProfiler();

private:
    JSBool InitializeRuntime();
    JSBool VerifyRuntime();

    /* Taking the unusual step of making all data public to simplify
    *  the implementation of the "C" static debugger hooks.  
    */

public:
    PRLock*         mLock;
    JSRuntime*      mRuntime;
    nsClassHashtable<nsCharPtrHashKey, ProfilerFile> mFileTable;
    nsDataHashtable<nsVoidPtrHashKey, ProfilerFunction*> mScriptTable;
};

#endif /* xpctoolsprivate_h___ */
