/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JSDSERVICE_H___
#define JSDSERVICE_H___

#include "jsdIDebuggerService.h"
#include "jsdebug.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nspr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

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
    uint32_t pc, line;
};
    
/*******************************************************************************
 * reflected jsd data structures
 *******************************************************************************/

class jsdObject MOZ_FINAL : public jsdIObject
{
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
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
            return nullptr;
        
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
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_JSDIPROPERTY
    NS_DECL_JSDIEPHEMERAL
    
    jsdProperty (JSDContext *aCx, JSDProperty *aProperty);
    virtual ~jsdProperty ();
    
    static jsdIProperty *FromPtr (JSDContext *aCx,
                                  JSDProperty *aProperty)
    {
        if (!aProperty)
            return nullptr;
        
        jsdIProperty *rv = new jsdProperty (aCx, aProperty);
        NS_IF_ADDREF(rv);
        return rv;
    }

    static void InvalidateAll();

  private:
    jsdProperty(); /* no implementation */
    jsdProperty(const jsdProperty&); /* no implementation */

    bool           mValid;
    LiveEphemeral  mLiveListEntry;
    JSDContext    *mCx;
    JSDProperty   *mProperty;
};

class jsdScript : public jsdIScript
{
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_JSDISCRIPT
    NS_DECL_JSDIEPHEMERAL

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdScript (JSDContext *aCx, JSDScript *aScript);
    virtual ~jsdScript();
    
    static jsdIScript *FromPtr (JSDContext *aCx, JSDScript *aScript)
    {
        if (!aScript)
            return nullptr;

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
    static uint32_t LastTag;
    
    jsdScript(); /* no implementation */
    jsdScript (const jsdScript&); /* no implementation */
    PCMapEntry* CreatePPLineMap();
    uint32_t    PPPcToLine(uint32_t aPC);
    uint32_t    PPLineToPc(uint32_t aLine);
    
    bool        mValid;
    uint32_t    mTag;
    JSDContext *mCx;
    JSDScript  *mScript;
    nsCString  *mFileName;
    nsCString  *mFunctionName;
    uint32_t    mBaseLineNumber, mLineExtent;
    PCMapEntry *mPPLineMap;
    uint32_t    mPCMapSize;
    uintptr_t   mFirstPC;
};

uint32_t jsdScript::LastTag = 0;

class jsdContext : public jsdIContext
{
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_JSDICONTEXT
    NS_DECL_JSDIEPHEMERAL

    jsdContext (JSDContext *aJSDCx, JSContext *aJSCx, nsISupports *aISCx);
    virtual ~jsdContext();

    static void InvalidateAll();
    static jsdIContext *FromPtr (JSDContext *aJSDCx, JSContext *aJSCx);
  private:
    static uint32_t LastTag;

    jsdContext (); /* no implementation */
    jsdContext (const jsdContext&); /* no implementation */

    bool                   mValid;
    // The API exposed by JSD here is problematic, because it allows for per-
    // JSContext script disabling, which no longer exists in the platform.
    // The only consumer here in practice is Firebug, which makes sure to re-
    // enable any disabled script before navigation. But if some other consumer
    // were to disable script, navigate, and try to re-enable it, we'd end up
    // with an unmatched UnblockScript call, which violates platform invariants.
    // So we make a half-hearted attempt to detect this by storing the Window ID
    // of the scope for which we disabled script.
    uint64_t               mScriptDisabledForWindowWithID;
    bool IsScriptEnabled() { return !mScriptDisabledForWindowWithID; }
    LiveEphemeral          mLiveListEntry;
    uint32_t               mTag;
    JSDContext            *mJSDCx;
    JSContext             *mJSCx;
    nsCOMPtr<nsISupports>  mISCx;
};

uint32_t jsdContext::LastTag = 0;

class jsdStackFrame : public jsdIStackFrame
{
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
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

    bool               mValid;
    LiveEphemeral      mLiveListEntry;
    JSDContext        *mCx;
    JSDThreadState    *mThreadState;
    JSDStackFrameInfo *mStackFrameInfo;
};

class jsdValue : public jsdIValue
{
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
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
    
    bool           mValid;
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
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_JSDIDEBUGGERSERVICE

    NS_DECL_CYCLE_COLLECTION_CLASS(jsdService)

    jsdService() : mOn(false), mPauseLevel(0),
                   mNestedLoopLevel(0), mCx(0), mRuntime(0), mErrorHook(0),
                   mBreakpointHook(0), mDebugHook(0), mDebuggerHook(0),
                   mInterruptHook(0), mScriptHook(0), mThrowHook(0),
                   mTopLevelHook(0), mFunctionHook(0),
                   mWarnedAboutDeprecation(false),
                   mDeprecationAcknowledged(false)
    {
    }

    virtual ~jsdService();
    
    static jsdService *GetService ();

    bool CheckInterruptHook() { return !!mInterruptHook; }
    
    nsresult DoPause(uint32_t *_rval, bool internalCall);
    nsresult DoUnPause(uint32_t *_rval, bool internalCall);

  private:
    bool        mOn;
    uint32_t    mPauseLevel;
    uint32_t    mNestedLoopLevel;
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
    nsCOMPtr<jsdIActivationCallback> mActivationCallback;

    // True if we have ever printed a warning about JSD being deprecated.
    // We only ever print the warning once.
    bool mWarnedAboutDeprecation;

    // True if the next call to asyncOn should not produce a warning,
    // because the consumer called jsdIDebuggerService::acknowledgeDeprecation.
    bool mDeprecationAcknowledged;
};

#endif /* JSDSERVICE_H___ */


/* graveyard */

#if 0

class jsdContext : public jsdIContext
{
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_JSDICONTEXT

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdContext (JSDContext *aCx) : mCx(aCx)
    {
        printf ("++++++ jsdContext\n");
    }

    static jsdIContext *FromPtr (JSDContext *aCx)
    {
        if (!aCx)
            return nullptr;
        
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
    NS_DECL_THREADSAFE_ISUPPORTS
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
            return nullptr;
        
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
