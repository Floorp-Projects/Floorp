/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef JSDSERVICE_H___
#define JSDSERVICE_H___

#include "jsdIDebuggerService.h"
#include "jsdebug.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nspr.h"

// #if defined(DEBUG_rginda_l)
// #   define DEBUG_verbose
// #endif

struct LiveEphemeral {
    /* link in a chain of live values list */
    PRCList                  links;
    jsdIEphemeral           *value;
    void                    *key;
};

struct PCMapEntry {
    PRUint32 pc, line;
};
    
/*******************************************************************************
 * reflected jsd data structures
 *******************************************************************************/

class jsdObject : public jsdIObject
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDIOBJECT

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdObject (JSDContext *aCx, JSDObject *aObject) :
        mCx(aCx), mObject(aObject)
    {
    }

    static jsdIObject *FromPtr (JSDContext *aCx,
                                JSDObject *aObject)
    {
        if (!aObject)
            return nsnull;
        
        jsdIObject *rv = new jsdObject (aCx, aObject);
        NS_IF_ADDREF(rv);
        return rv;
    }

  private:
    jsdObject(); /* no implementation */
    jsdObject(const jsdObject&); /* no implementation */

    JSDContext *mCx;
    JSDObject *mObject;
};


class jsdProperty : public jsdIProperty
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDIPROPERTY
    NS_DECL_JSDIEPHEMERAL
    
    jsdProperty (JSDContext *aCx, JSDProperty *aProperty);
    virtual ~jsdProperty ();
    
    static jsdIProperty *FromPtr (JSDContext *aCx,
                                  JSDProperty *aProperty)
    {
        if (!aProperty)
            return nsnull;
        
        jsdIProperty *rv = new jsdProperty (aCx, aProperty);
        NS_IF_ADDREF(rv);
        return rv;
    }

    static void InvalidateAll();

  private:
    jsdProperty(); /* no implementation */
    jsdProperty(const jsdProperty&); /* no implementation */

    PRBool         mValid;
    LiveEphemeral  mLiveListEntry;
    JSDContext    *mCx;
    JSDProperty   *mProperty;
};

class jsdScript : public jsdIScript
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDISCRIPT
    NS_DECL_JSDIEPHEMERAL

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdScript (JSDContext *aCx, JSDScript *aScript);
    virtual ~jsdScript();
    
    static jsdIScript *FromPtr (JSDContext *aCx, JSDScript *aScript)
    {
        if (!aScript)
            return nsnull;

        void *data = JSD_GetScriptPrivate (aScript);
        jsdIScript *rv;
        
        if (data) {
            rv = static_cast<jsdIScript *>(data);
        } else {
            rv = new jsdScript (aCx, aScript);
            NS_IF_ADDREF(rv);  /* addref for the SetScriptPrivate, released in
                                * Invalidate() */
            JSD_SetScriptPrivate (aScript, static_cast<void *>(rv));
        }
        
        NS_IF_ADDREF(rv); /* addref for return value */
        return rv;
    }

    static void InvalidateAll();

  private:
    static PRUint32 LastTag;
    
    jsdScript(); /* no implementation */
    jsdScript (const jsdScript&); /* no implementation */
    PCMapEntry* CreatePPLineMap();
    PRUint32    PPPcToLine(PRUint32 aPC);
    PRUint32    PPLineToPc(PRUint32 aLine);
    
    PRBool      mValid;
    PRUint32    mTag;
    JSDContext *mCx;
    JSDScript  *mScript;
    nsCString  *mFileName;
    nsCString  *mFunctionName;
    PRUint32    mBaseLineNumber, mLineExtent;
    PCMapEntry *mPPLineMap;
    PRUint32    mPCMapSize;
    jsuword     mFirstPC;
    jsuword     mEndPC;
};

PRUint32 jsdScript::LastTag = 0;

class jsdContext : public jsdIContext
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDICONTEXT
    NS_DECL_JSDIEPHEMERAL

    jsdContext (JSDContext *aJSDCx, JSContext *aJSCx, nsISupports *aISCx);
    virtual ~jsdContext();

    static void InvalidateAll();
    static jsdIContext *FromPtr (JSDContext *aJSDCx, JSContext *aJSCx);
  private:
    static PRUint32 LastTag;

    jsdContext (); /* no implementation */
    jsdContext (const jsdContext&); /* no implementation */

    PRBool                 mValid;
    LiveEphemeral          mLiveListEntry;
    PRUint32               mTag;
    JSDContext            *mJSDCx;
    JSContext             *mJSCx;
    nsCOMPtr<nsISupports>  mISCx;
};

PRUint32 jsdContext::LastTag = 0;

class jsdStackFrame : public jsdIStackFrame
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDISTACKFRAME
    NS_DECL_JSDIEPHEMERAL

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdStackFrame (JSDContext *aCx, JSDThreadState *aThreadState,
                   JSDStackFrameInfo *aStackFrameInfo);
    virtual ~jsdStackFrame();

    static void InvalidateAll();
    static jsdIStackFrame* FromPtr (JSDContext *aCx,
                                    JSDThreadState *aThreadState,
                                    JSDStackFrameInfo *aStackFrameInfo);

  private:
    jsdStackFrame(); /* no implementation */
    jsdStackFrame(const jsdStackFrame&); /* no implementation */

    PRBool             mValid;
    LiveEphemeral      mLiveListEntry;
    JSDContext        *mCx;
    JSDThreadState    *mThreadState;
    JSDStackFrameInfo *mStackFrameInfo;
};

class jsdValue : public jsdIValue
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDIVALUE
    NS_DECL_JSDIEPHEMERAL

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdValue (JSDContext *aCx, JSDValue *aValue);
    virtual ~jsdValue();

    static jsdIValue *FromPtr (JSDContext *aCx, JSDValue *aValue);    
    static void InvalidateAll();
    
  private:
    jsdValue(); /* no implementation */
    jsdValue (const jsdScript&); /* no implementation */
    
    PRBool         mValid;
    LiveEphemeral  mLiveListEntry;
    JSDContext    *mCx;
    JSDValue      *mValue;
};

/******************************************************************************
 * debugger service
 ******************************************************************************/

class jsdService : public jsdIDebuggerService
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDIDEBUGGERSERVICE

    jsdService() : mOn(PR_FALSE), mPauseLevel(0),
                   mNestedLoopLevel(0), mCx(0), mRuntime(0), mErrorHook(0),
                   mBreakpointHook(0), mDebugHook(0), mDebuggerHook(0),
                   mInterruptHook(0), mScriptHook(0), mThrowHook(0),
                   mTopLevelHook(0), mFunctionHook(0)
    {
    }

    virtual ~jsdService();
    
    static jsdService *GetService ();
    
  private:
    PRBool      mOn;
    PRUint32    mPauseLevel;
    PRUint32    mNestedLoopLevel;
    JSDContext *mCx;
    JSRuntime  *mRuntime;

    nsCOMPtr<jsdIErrorHook>     mErrorHook;
    nsCOMPtr<jsdIExecutionHook> mBreakpointHook;
    nsCOMPtr<jsdIExecutionHook> mDebugHook;
    nsCOMPtr<jsdIExecutionHook> mDebuggerHook;
    nsCOMPtr<jsdIExecutionHook> mInterruptHook;
    nsCOMPtr<jsdIScriptHook>    mScriptHook;
    nsCOMPtr<jsdIExecutionHook> mThrowHook;
    nsCOMPtr<jsdICallHook>      mTopLevelHook;
    nsCOMPtr<jsdICallHook>      mFunctionHook;

};

#endif /* JSDSERVICE_H___ */


/* graveyard */

#if 0

class jsdContext : public jsdIContext
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDICONTEXT

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdContext (JSDContext *aCx) : mCx(aCx)
    {
        printf ("++++++ jsdContext\n");
    }

    static jsdIContext *FromPtr (JSDContext *aCx)
    {
        if (!aCx)
            return nsnull;
        
        void *data = JSD_GetContextPrivate (aCx);
        jsdIContext *rv;
        
        if (data) {
            rv = static_cast<jsdIContext *>(data);
        } else {
            rv = new jsdContext (aCx);
            NS_IF_ADDREF(rv);  // addref for the SetContextPrivate
            JSD_SetContextPrivate (aCx, static_cast<void *>(rv));
        }
        
        NS_IF_ADDREF(rv); // addref for the return value
        return rv;
    }

    virtual ~jsdContext() { printf ("------ ~jsdContext\n"); }
  private:            
    jsdContext(); /* no implementation */
    jsdContext(const jsdContext&); /* no implementation */
    
    JSDContext *mCx;
};

class jsdThreadState : public jsdIThreadState
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDITHREADSTATE

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdThreadState (JSDContext *aCx, JSDThreadState *aThreadState) :
        mCx(aCx), mThreadState(aThreadState)
    {
    }

    /* XXX These things are only valid for a short period of time, they reflect
     * state in the js engine that will go away after stepping past wherever
     * we were stopped at when this was created.  We could keep a list of every
     * instance of this we've created, and "invalidate" them before we let the
     * engine continue.  The next time we need a threadstate, we can search the
     * list to find an invalidated one, and just reuse it.
     */
    static jsdIThreadState *FromPtr (JSDContext *aCx,
                                     JSDThreadState *aThreadState)
    {
        if (!aThreadState)
            return nsnull;
        
        jsdIThreadState *rv = new jsdThreadState (aCx, aThreadState);
        NS_IF_ADDREF(rv);
        return rv;
    }

  private:
    jsdThreadState(); /* no implementation */
    jsdThreadState(const jsdThreadState&); /* no implementation */

    JSDContext     *mCx;
    JSDThreadState *mThreadState;
};

#endif
