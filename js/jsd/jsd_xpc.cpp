/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is mozilla.org code
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>
 *
 */

#include "jsd_xpc.h"
#include "jscntxt.h"

#include "nsIXPConnect.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPref.h"
#include "nsICategoryManager.h"
#include "nsIJSRuntimeService.h"
#include "nsIEventQueueService.h"
#include "nsMemory.h"
#include "jsdebug.h"
#include "nsReadableUtils.h"

/* XXX this stuff is used by NestEventLoop, a temporary hack to be refactored
 * later */
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsIJSContextStack.h"

#define CAUTIOUS_SCRIPTHOOK

#ifdef DEBUG_verbose
#   define DEBUG_COUNT(name, count)                                             \
        { if ((count % 10) == 0) printf (name ": %i\n", count); }
#   define DEBUG_CREATE(name, count) {count++; DEBUG_COUNT ("+++++ "name,count)}
#   define DEBUG_DESTROY(name, count) {count--; DEBUG_COUNT ("----- "name,count)}
#else
#   define DEBUG_CREATE(name, count) 
#   define DEBUG_DESTROY(name, count)
#endif

#define ASSERT_VALID_CONTEXT  { if (!mCx) return NS_ERROR_NOT_AVAILABLE; }
#define ASSERT_VALID_FRAME    { if (!mValid) return NS_ERROR_NOT_AVAILABLE; }
#define ASSERT_VALID_PROPERTY { if (!mValid) return NS_ERROR_NOT_AVAILABLE; }
#define ASSERT_VALID_SCRIPT   { if (!mValid) return NS_ERROR_NOT_AVAILABLE; }
#define ASSERT_VALID_VALUE    { if (!mValid) return NS_ERROR_NOT_AVAILABLE; }

#define JSDSERVICE_CID                               \
{ /* f1299dc2-1dd1-11b2-a347-ee6b7660e048 */         \
     0xf1299dc2,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xa3, 0x47, 0xee, 0x6b, 0x76, 0x60, 0xe0, 0x48} \
}

#define JSDASO_CID                                   \
{ /* f2723a7e-1dd1-11b2-9f9e-ff701f717575 */         \
     0xf2723a7e,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0x9f, 0x9e, 0xff, 0x70, 0x1f, 0x71, 0x75, 0x75} \
}

#define NS_CATMAN_CTRID "@mozilla.org/categorymanager;1"
#define NS_PREF_CTRID "@mozilla.org/preferences;1"
#define NS_JSRT_CTRID "@mozilla.org/js/xpc/RuntimeService;1"

#define APPSTART_CATEGORY "app-startup"
#define PROFILE_CHANGE_EVENT "profile-after-change"

JS_STATIC_DLL_CALLBACK (JSBool)
jsds_GCCallbackProc (JSContext *cx, JSGCStatus status);

/*******************************************************************************
 * global vars
 *******************************************************************************/

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

const char jsdServiceCtrID[] = "@mozilla.org/js/jsd/debugger-service;1";
const char jsdASObserverCtrID[] = "@mozilla.org/js/jsd/app-start-observer;1";

#ifdef DEBUG_verbose
PRUint32 gScriptCount   = 0;
PRUint32 gValueCount    = 0;
PRUint32 gPropertyCount = 0;
#endif

static jsdService   *gJsds       = 0;
static JSGCCallback  gLastGCProc = jsds_GCCallbackProc;
static JSGCStatus    gGCStatus   = JSGC_END;

static struct DeadScript {
    PRCList     links;
    JSDContext *jsdc;
    jsdIScript *script;
} *gDeadScripts = nsnull;

static struct LiveEphemeral *gLiveValues = nsnull, *gLiveProperties = nsnull;

/*******************************************************************************
 * utility functions for ephemeral lists
 *******************************************************************************/

void
jsds_InvalidateAllEphemerals (LiveEphemeral **listHead)
{
    LiveEphemeral *lv_record = 
        NS_REINTERPRET_CAST (LiveEphemeral *,
                             PR_NEXT_LINK(&(*listHead)->links));
    while (*listHead)
    {
        LiveEphemeral *next =
            NS_REINTERPRET_CAST (LiveEphemeral *,
                                 PR_NEXT_LINK(&lv_record->links));
        lv_record->value->Invalidate();
        lv_record = next;
    }
}

void
jsds_InsertEphemeral (LiveEphemeral **listHead, LiveEphemeral *item)
{
    if (*listHead) {
        /* if the list exists, add to it */
        PR_APPEND_LINK(&item->links, &(*listHead)->links);
    } else {
        /* otherwise create the list */
        PR_INIT_CLIST(&item->links);
        *listHead = item;
    }
}

void
jsds_RemoveEphemeral (LiveEphemeral **listHead, LiveEphemeral *item)
{
    LiveEphemeral *next = 
        NS_REINTERPRET_CAST (LiveEphemeral *,
                             PR_NEXT_LINK(&item->links));

    if (next == item)
    {
        /* if the current item is also the next item, we're the only element,
         * null out the list head */
        NS_ASSERTION (*listHead == item,
                      "How could we not be the head of a one item list?");
        *listHead = nsnull;
    }
    else if (item == *listHead)
    {
        /* otherwise, if we're currently the list head, change it */
        *listHead = next;
    }
    
    PR_REMOVE_AND_INIT_LINK(&item->links);
}

/*******************************************************************************
 * c callbacks
 *******************************************************************************/

JS_STATIC_DLL_CALLBACK (void)
jsds_NotifyPendingDeadScripts (JSContext *cx)
{
    nsCOMPtr<jsdIScriptHook> hook = 0;   
    gJsds->GetScriptHook (getter_AddRefs(hook));
    if (hook)
    {
        DeadScript *ds;
#ifdef CAUTIOUS_SCRIPTHOOK
        JSRuntime *rt = JS_GetRuntime(cx);
#endif
        do {
            ds = gDeadScripts;
            
            /* tell the user this script has been destroyed */
#ifdef CAUTIOUS_SCRIPTHOOK
            JS_DISABLE_GC(rt);
#endif
            hook->OnScriptDestroyed (ds->script);
#ifdef CAUTIOUS_SCRIPTHOOK
            JS_ENABLE_GC(rt);
#endif
            /* get next deleted script */
            gDeadScripts = NS_REINTERPRET_CAST(DeadScript *,
                                               PR_NEXT_LINK(&ds->links));
            /* take ourselves out of the circular list */
            PR_REMOVE_LINK(&ds->links);
            /* addref came from the FromPtr call in jsds_ScriptHookProc */
            NS_RELEASE(ds->script);
            /* free the struct! */
            PR_Free(ds);
        } while (&gDeadScripts->links != &ds->links);
        /* keep going until we catch up with our tail */
    }
    gDeadScripts = 0;
}

JS_STATIC_DLL_CALLBACK (JSBool)
jsds_GCCallbackProc (JSContext *cx, JSGCStatus status)
{
    gGCStatus = status;
#ifdef DEBUG
    printf ("new gc status is %i\n", status);
#endif
    if (status == JSGC_END && gDeadScripts)
        jsds_NotifyPendingDeadScripts (cx);
    
    if (gLastGCProc)
        return gLastGCProc (cx, status);
    
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK (JSBool)
jsds_EmptyCallHookProc (JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                        uintN type, void* callerdata)
{
    /* empty hook returns true so that we get called when the function
     * returns (we may have a hook function by then.) */
    return JS_TRUE;
}


JS_STATIC_DLL_CALLBACK (JSBool)
jsds_CallHookProc (JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                   uintN type, void* callerdata)
{
    nsCOMPtr<jsdICallHook> hook(0);

    switch (type)
    {
        case JSD_HOOK_TOPLEVEL_START:
        case JSD_HOOK_TOPLEVEL_END:
            gJsds->GetTopLevelHook(getter_AddRefs(hook));
            break;
            
        case JSD_HOOK_FUNCTION_CALL:
        case JSD_HOOK_FUNCTION_RETURN:
            gJsds->GetFunctionHook(getter_AddRefs(hook));
            break;

        default:
            NS_ASSERTION (0, "Unknown hook type.");
    }
    
    if (!hook)
        return JS_TRUE;

    JSDStackFrameInfo *native_frame = JSD_GetStackFrame (jsdc, jsdthreadstate);
    nsCOMPtr<jsdIStackFrame> frame =
        getter_AddRefs(jsdStackFrame::FromPtr(jsdc, jsdthreadstate,
                                              native_frame));
    hook->OnCall(frame, type);    
    frame->Invalidate();

    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK (PRUint32)
jsds_ExecutionHookProc (JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                        uintN type, void* callerdata, jsval* rval)
{
    nsCOMPtr<jsdIExecutionHook> hook(0);
    PRUint32 hook_rv = JSD_HOOK_RETURN_CONTINUE;
    jsdIValue *js_rv = 0;    

    switch (type)
    {
        case JSD_HOOK_INTERRUPTED:
            gJsds->GetInterruptHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_DEBUG_REQUESTED:
            gJsds->GetErrorHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_DEBUGGER_KEYWORD:
            gJsds->GetDebuggerHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_BREAKPOINT:
            gJsds->GetBreakpointHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_THROW:
        {
            hook_rv = JSD_HOOK_RETURN_CONTINUE_THROW;
            gJsds->GetThrowHook(getter_AddRefs(hook));
            if (hook) {
                JSDValue *jsdv = JSD_GetException (jsdc, jsdthreadstate);
                js_rv = jsdValue::FromPtr (jsdc, jsdv);
            }
            break;
        }
        default:
            NS_ASSERTION (0, "Unknown hook type.");
    }

    if (!hook)
        return hook_rv;
    
    JSDStackFrameInfo *native_frame = JSD_GetStackFrame (jsdc, jsdthreadstate);
    nsCOMPtr<jsdIStackFrame> frame =
        getter_AddRefs(jsdStackFrame::FromPtr(jsdc, jsdthreadstate,
                                              native_frame));
    hook->OnExecute (frame, type, &js_rv, &hook_rv);
    frame->Invalidate();
        
    if (hook_rv == JSD_HOOK_RETURN_RET_WITH_VAL ||
        hook_rv == JSD_HOOK_RETURN_THROW_WITH_VAL)
    {
        JSDValue *jsdv;
        js_rv->GetJSDValue (&jsdv);
        *rval = JSD_GetValueWrappedJSVal(jsdc, jsdv);
    }
    
    NS_IF_RELEASE(js_rv);
    return hook_rv;
}

JS_STATIC_DLL_CALLBACK (void)
jsds_ScriptHookProc (JSDContext* jsdc, JSDScript* jsdscript, JSBool creating,
                     void* callerdata)
{
#ifdef CAUTIOUS_SCRIPTHOOK
    JSContext *cx = JSD_GetDefaultJSContext(jsdc);
    JSRuntime *rt = JS_GetRuntime(cx);
#endif

    if (creating) {
        jsdIScriptHook *hook = 0;
        
        gJsds->GetScriptHook (&hook);
        if (!hook)
            return;
            
        nsCOMPtr<jsdIScript> script = 
            getter_AddRefs(jsdScript::FromPtr(jsdc, jsdscript));
#ifdef CAUTIOUS_SCRIPTHOOK
        JS_DISABLE_GC(rt);
#endif
        hook->OnScriptCreated (script);
#ifdef CAUTIOUS_SCRIPTHOOK
        JS_ENABLE_GC(rt);
#endif
    } else {
        
        jsdIScript *jsdis = jsdScript::FromPtr(jsdc, jsdscript);
        /* the initial addref is owned by the DeadScript record */
        jsdis->Invalidate();

        if (gGCStatus == JSGC_END) {
            /* if GC *isn't* running, we can tell the user about the script
             * delete now. */
            nsCOMPtr<jsdIScriptHook> hook = 0;   
            gJsds->GetScriptHook (getter_AddRefs(hook));
            if (hook) {
#ifdef CAUTIOUS_SCRIPTHOOK
                JS_DISABLE_GC(rt);
#endif
                hook->OnScriptDestroyed (jsdis);
#ifdef CAUTIOUS_SCRIPTHOOK
                JS_ENABLE_GC(rt);
#endif
            }
        } else {
            /* if a GC *is* running, we've got to wait until it's done before
             * we can execute any JS, so we queue the notification in a PRCList
             * until GC tells us it's done. See jsds_GCCallbackProc(). */
            DeadScript *ds = PR_NEW(DeadScript);
            if (!ds) {
                NS_RELEASE(jsdis);
                return; /* NS_ERROR_OUT_OF_MEMORY */
            }
        
            ds->jsdc = jsdc;
            ds->script = jsdis;
        
            if (gDeadScripts)
                /* if the queue exists, add to it */
                PR_APPEND_LINK(&ds->links, &gDeadScripts->links);
            else {
                /* otherwise create the queue */
                PR_INIT_CLIST(&ds->links);
                gDeadScripts = ds;
            }
        }
    }
    
            
}

/*******************************************************************************
 * reflected jsd data structures
 *******************************************************************************/

/* Contexts */
/*
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdContext, jsdIContext); 

NS_IMETHODIMP
jsdContext::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}
*/

/* Objects */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdObject, jsdIObject); 

NS_IMETHODIMP
jsdObject::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetJSDObject(JSDObject **_rval)
{
    *_rval = mObject;
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetCreatorURL(char **_rval)
{
    *_rval = ToNewCString(nsDependentCString(JSD_GetObjectNewURL(mCx, mObject)));
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetCreatorLine(PRUint32 *_rval)
{
    *_rval = JSD_GetObjectNewLineNumber(mCx, mObject);
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetConstructorURL(char **_rval)
{
    *_rval = ToNewCString(nsDependentCString(JSD_GetObjectConstructorURL(mCx, mObject)));
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetConstructorLine(PRUint32 *_rval)
{
    *_rval = JSD_GetObjectConstructorLineNumber(mCx, mObject);
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetValue(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetValueForObject (mCx, mObject);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

/* PC */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdPC, jsdIPC); 

NS_IMETHODIMP
jsdPC::GetPc(jsuword *_rval)
{
    *_rval = mPC;
    return NS_OK;
}

/* Properties */
NS_IMPL_THREADSAFE_ISUPPORTS2(jsdProperty, jsdIProperty, jsdIEphemeral);

jsdProperty::jsdProperty (JSDContext *aCx, JSDProperty *aProperty) :
    mCx(aCx), mProperty(aProperty)
{
    DEBUG_CREATE ("jsdProperty", gPropertyCount);
    mValid = (aCx && aProperty);
    NS_INIT_ISUPPORTS();
    mLiveListEntry.value = this;
    jsds_InsertEphemeral (&gLiveProperties, &mLiveListEntry);
}

jsdProperty::~jsdProperty () 
{
    DEBUG_DESTROY ("jsdProperty", gPropertyCount);
    if (mValid)
        Invalidate();
}

NS_IMETHODIMP
jsdProperty::Invalidate()
{
    ASSERT_VALID_VALUE;
    mValid = PR_FALSE;
    jsds_RemoveEphemeral (&gLiveProperties, &mLiveListEntry);
    JSD_DropProperty (mCx, mProperty);
    return NS_OK;
}

void
jsdProperty::InvalidateAll()
{
    if (gLiveProperties)
        jsds_InvalidateAllEphemerals (&gLiveProperties);
}

NS_IMETHODIMP
jsdProperty::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetJSDProperty(JSDProperty **_rval)
{
    *_rval = mProperty;
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetIsValid(PRBool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetAlias(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetPropertyValue (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetFlags(PRUint32 *_rval)
{
    *_rval = JSD_GetPropertyFlags (mCx, mProperty);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetName(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetPropertyName (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetValue(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetPropertyValue (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetVarArgSlot(PRUint32 *_rval)
{
    *_rval = JSD_GetPropertyVarArgSlot (mCx, mProperty);
    return NS_OK;
}

/* Scripts */
NS_IMPL_THREADSAFE_ISUPPORTS2(jsdScript, jsdIScript, jsdIEphemeral); 

jsdScript::jsdScript (JSDContext *aCx, JSDScript *aScript) : mValid(PR_FALSE),
                                                             mCx(aCx),
                                                             mScript(aScript),
                                                             mFileName(0), 
                                                             mFunctionName(0),
                                                             mBaseLineNumber(0),
                                                             mLineExtent(0)
{
    DEBUG_CREATE ("jsdScript", gScriptCount);
    NS_INIT_ISUPPORTS();

    if (mScript)
    {
        /* copy the script's information now, so we have it later, when it
         * gets destroyed. */
        JSD_LockScriptSubsystem(mCx);
        mFileName = new nsCString(JSD_GetScriptFilename(mCx, mScript));
        mFunctionName =
            new nsCString(JSD_GetScriptFunctionName(mCx, mScript));
        mBaseLineNumber = JSD_GetScriptBaseLineNumber(mCx, mScript);
        mLineExtent = JSD_GetScriptLineExtent(mCx, mScript);
        JSD_UnlockScriptSubsystem(mCx);
        
        mValid = PR_TRUE;
    }
}

jsdScript::~jsdScript () 
{
    DEBUG_DESTROY ("jsdScript", gScriptCount);
    if (mFileName)
        delete mFileName;
    if (mFunctionName)
        delete mFunctionName;
    
    /* Invalidate() needs to be called to release an owning reference to
     * ourselves, so if we got here without being invalidated, something
     * has gone wrong with our ref count. */
    NS_ASSERTION (!mValid, "Script destroyed without being invalidated.");
    
}

NS_IMETHODIMP
jsdScript::GetJSDContext(JSDContext **_rval)
{
    ASSERT_VALID_SCRIPT;
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetJSDScript(JSDScript **_rval)
{
    ASSERT_VALID_SCRIPT;
    *_rval = mScript;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::Invalidate()
{
    ASSERT_VALID_SCRIPT;
    mValid = PR_FALSE;
    
    /* release the addref we do in FromPtr */
    jsdIScript *script = NS_STATIC_CAST(jsdIScript *,
                                        JSD_GetScriptPrivate(mScript));
    NS_ASSERTION (script == this, "That's not my script!");
    NS_RELEASE(script);

    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetIsValid(PRBool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}
    
NS_IMETHODIMP
jsdScript::GetFileName(char **_rval)
{
    *_rval = ToNewCString(*mFileName);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionName(char **_rval)
{
    *_rval = ToNewCString(*mFunctionName);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetBaseLineNumber(PRUint32 *_rval)
{
    *_rval = mBaseLineNumber;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetLineExtent(PRUint32 *_rval)
{
    *_rval = mLineExtent;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::PcToLine(jsdIPC *aPC, PRUint32 *_rval)
{
    ASSERT_VALID_SCRIPT;
    jsuword pc;
    aPC->GetPc(&pc);
    *_rval = JSD_GetClosestLine (mCx, mScript, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::LineToPc(PRUint32 aLine, jsdIPC **_rval)
{
    ASSERT_VALID_SCRIPT;
    jsuword pc = JSD_GetClosestPC (mCx, mScript, aLine);
    *_rval = jsdPC::FromPtr (pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::SetBreakpoint(jsdIPC *aPC)
{
    ASSERT_VALID_SCRIPT;
    jsuword pc;
    aPC->GetPc (&pc);
    
    JSD_SetExecutionHook (mCx, mScript, pc, jsds_ExecutionHookProc,
                          NS_REINTERPRET_CAST(void *, PRIVATE_TO_JSVAL(NULL)));
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::ClearBreakpoint(jsdIPC *aPC)
{
    ASSERT_VALID_SCRIPT;    
    if (!aPC)
        return NS_ERROR_INVALID_ARG;
    
    jsuword pc;
    aPC->GetPc (&pc);
    
    JSD_ClearExecutionHook (mCx, mScript, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::ClearAllBreakpoints()
{
    ASSERT_VALID_SCRIPT;
    JSD_LockScriptSubsystem(mCx);
    JSD_ClearAllExecutionHooksForScript (mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

/* Stack Frames */
NS_IMPL_THREADSAFE_ISUPPORTS2(jsdStackFrame, jsdIStackFrame, jsdIEphemeral);

NS_IMETHODIMP
jsdStackFrame::GetJSDContext(JSDContext **_rval)
{
    ASSERT_VALID_FRAME;
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDThreadState(JSDThreadState **_rval)
{
    ASSERT_VALID_FRAME;
    *_rval = mThreadState;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDStackFrameInfo(JSDStackFrameInfo **_rval)
{
    ASSERT_VALID_FRAME;
    *_rval = mStackFrameInfo;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetIsValid(PRBool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::Invalidate()
{
    ASSERT_VALID_FRAME;
    mValid = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallingFrame(jsdIStackFrame **_rval)
{
    ASSERT_VALID_FRAME;
    JSDStackFrameInfo *sfi = JSD_GetCallingStackFrame (mCx, mThreadState,
                                                       mStackFrameInfo);
    *_rval = jsdStackFrame::FromPtr (mCx, mThreadState, sfi);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScript(jsdIScript **_rval)
{
    ASSERT_VALID_FRAME;
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    *_rval = jsdScript::FromPtr (mCx, script);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetPc(jsdIPC **_rval)
{
    ASSERT_VALID_FRAME;
    jsuword pc;
    pc = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
    *_rval = jsdPC::FromPtr (pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetLine(PRUint32 *_rval)
{
    ASSERT_VALID_FRAME;
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    jsuword pc = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
    *_rval = JSD_GetClosestLine (mCx, script, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallee(jsdIValue **_rval)
{
    ASSERT_VALID_FRAME;
    JSDValue *jsdv = JSD_GetCallObjectForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScope(jsdIValue **_rval)
{
    ASSERT_VALID_FRAME;
    JSDValue *jsdv = JSD_GetScopeChainForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetThisValue(jsdIValue **_rval)
{
    ASSERT_VALID_FRAME;
    JSDValue *jsdv = JSD_GetThisForStackFrame (mCx, mThreadState,
                                               mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}


NS_IMETHODIMP
jsdStackFrame::Eval (const nsAReadableString &bytes, const char *fileName,
                     PRUint32 line, jsdIValue **_rval)
{
    ASSERT_VALID_FRAME;
    jsval jv;

    const nsSharedBufferHandle<PRUnichar> *h = bytes.GetSharedBufferHandle();
    const jschar *char_bytes = NS_REINTERPRET_CAST(const jschar *,
                                                   h->DataStart());

    if (!JSD_EvaluateUCScriptInStackFrame (mCx, mThreadState, mStackFrameInfo,
                                           char_bytes, bytes.Length(), fileName,
                                           line, &jv))
        return NS_ERROR_FAILURE;
    
    JSDValue *jsdv = JSD_NewValue (mCx, jv);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}        

/* Values */
NS_IMPL_THREADSAFE_ISUPPORTS2(jsdValue, jsdIValue, jsdIEphemeral);

jsdValue::jsdValue (JSDContext *aCx, JSDValue *aValue) : mValid(PR_TRUE),
                                                         mCx(aCx), 
                                                         mValue(aValue)
{
    DEBUG_CREATE ("jsdValue", gValueCount);
    NS_INIT_ISUPPORTS();
    mLiveListEntry.value = this;
    jsds_InsertEphemeral (&gLiveValues, &mLiveListEntry);
}

jsdValue::~jsdValue() 
{
    DEBUG_DESTROY ("jsdValue", gValueCount);
    if (mValid)
        /* call Invalidate() to take ourselves out of the live list */
        Invalidate();
}   

NS_IMETHODIMP
jsdValue::GetIsValid(PRBool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::Invalidate()
{
    ASSERT_VALID_VALUE;
    mValid = PR_FALSE;
    jsds_RemoveEphemeral (&gLiveValues, &mLiveListEntry);
    JSD_DropValue (mCx, mValue);
    return NS_OK;
}

void
jsdValue::InvalidateAll()
{
    if (gLiveValues)
        jsds_InvalidateAllEphemerals (&gLiveValues);
}

NS_IMETHODIMP
jsdValue::GetJSDContext(JSDContext **_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJSDValue (JSDValue **_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = mValue;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNative (PRBool *_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = JSD_IsValueNative (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNumber (PRBool *_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = JSD_IsValueNumber (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsPrimitive (PRBool *_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = JSD_IsValuePrimitive (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsType (PRUint32 *_rval)
{
    ASSERT_VALID_VALUE;
    jsval val;

    val = JSD_GetValueWrappedJSVal (mCx, mValue);
    
    if (JSVAL_IS_NULL(val))
        *_rval = TYPE_NULL;
    else if (JSVAL_IS_BOOLEAN(val))
        *_rval = TYPE_BOOLEAN;
    else if (JSVAL_IS_DOUBLE(val))
        *_rval = TYPE_DOUBLE;
    else if (JSVAL_IS_INT(val))
        *_rval = TYPE_INT;
    else if (JSVAL_IS_STRING(val))
        *_rval = TYPE_STRING;
    else if (JSVAL_IS_VOID(val))
        *_rval = TYPE_VOID;
    else if (JSD_IsValueFunction (mCx, mValue))
        *_rval = TYPE_FUNCTION;
    else if (JSVAL_IS_OBJECT(val))
        *_rval = TYPE_OBJECT;
    else
        NS_ASSERTION (0, "Value has no discernible type.");

    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsPrototype (jsdIValue **_rval)
{
    ASSERT_VALID_VALUE;
    JSDValue *jsdv = JSD_GetValuePrototype (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsParent (jsdIValue **_rval)
{
    ASSERT_VALID_VALUE;
    JSDValue *jsdv = JSD_GetValueParent (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsClassName(char **_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = ToNewCString(nsDependentCString(JSD_GetValueClassName(mCx, mValue)));
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsConstructor (jsdIValue **_rval)
{
    ASSERT_VALID_VALUE;
    JSDValue *jsdv = JSD_GetValueConstructor (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsFunctionName(char **_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = ToNewCString(nsDependentCString(JSD_GetValueFunctionName(mCx, mValue)));
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetBooleanValue(PRBool *_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = JSD_GetValueBoolean (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetDoubleValue(double *_rval)
{
    ASSERT_VALID_VALUE;
    double *dp = JSD_GetValueDouble (mCx, mValue);
    if (dp)
        *_rval = *dp;
    else
        *_rval = 0;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIntValue(PRInt32 *_rval)
{
    ASSERT_VALID_VALUE;
    *_rval = JSD_GetValueInt (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetObjectValue(jsdIObject **_rval)
{
    ASSERT_VALID_VALUE;
    JSDObject *obj;
    obj = JSD_GetObjectForValue (mCx, mValue);
    *_rval = jsdObject::FromPtr (mCx, obj);
    return NS_OK;
}
    
NS_IMETHODIMP
jsdValue::GetStringValue(char **_rval)
{
    ASSERT_VALID_VALUE;
    JSString *jstr_val = JSD_GetValueString(mCx, mValue);
    *_rval = ToNewCString(nsDependentCString(JS_GetStringBytes(jstr_val)));
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetPropertyCount (PRInt32 *_rval)
{
    ASSERT_VALID_VALUE;
    if (JSD_IsValueObject(mCx, mValue))
        *_rval = JSD_GetCountOfProperties (mCx, mValue);
    else
        *_rval = -1;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetProperties (jsdIProperty ***propArray, PRUint32 *length)
{
    ASSERT_VALID_VALUE;
    if (!JSD_IsValueObject(mCx, mValue)) {
        *length = 0;
        *propArray = 0;
        return NS_OK;
    }

    jsdIProperty **pa_temp;
    PRUint32 prop_count = JSD_GetCountOfProperties (mCx, mValue);
    
    pa_temp = NS_STATIC_CAST(jsdIProperty **,
                             nsMemory::Alloc(sizeof (jsdIProperty *) * 
                                             prop_count));

    PRUint32     i    = 0;
    JSDProperty *iter = NULL;
    JSDProperty *prop;
    while ((prop = JSD_IterateProperties (mCx, mValue, &iter))) {
        pa_temp[i] = jsdProperty::FromPtr (mCx, prop);
        ++i;
    }
    
    NS_ASSERTION (prop_count == i, "property count mismatch");    

    /* if caller doesn't care about length, don't bother telling them */
    *propArray = pa_temp;
    if (length)
        *length = prop_count;
    
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetProperty (const char *name, jsdIProperty **_rval)
{
    ASSERT_VALID_VALUE;
    JSContext *cx = JSD_GetDefaultJSContext (mCx);
    /* not rooting this */
    JSString *jstr_name = JS_NewStringCopyZ (cx, name);

    JSDProperty *prop = JSD_GetValueProperty (mCx, mValue, jstr_name);
    
    *_rval = jsdProperty::FromPtr (mCx, prop);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::Refresh()
{
    ASSERT_VALID_VALUE;
    JSD_RefreshValue (mCx, mValue);
    return NS_OK;
}

/******************************************************************************
 * debugger service implementation
 ******************************************************************************/
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdService, jsdIDebuggerService); 

NS_IMETHODIMP
jsdService::GetIsOn (PRBool *_rval)
{
    *_rval = mOn;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::On (void)
{
    nsresult  rv;

    /* get JS things from the CallContext */
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (!xpc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXPCNativeCallContext> cc;
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    JSContext *cx;
    rv = cc->GetJSContext (&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    
    return OnForRuntime(JS_GetRuntime (cx));
    
}

NS_IMETHODIMP
jsdService::OnForRuntime (JSRuntime *rt)
{
    if (mOn)
        return (rt == mRuntime) ? NS_OK : NS_ERROR_ALREADY_INITIALIZED;

    mRuntime = rt;

    if (gLastGCProc == jsds_GCCallbackProc)
        /* condition indicates that the callback proc has not been set yet */
        gLastGCProc = JS_SetGCCallbackRT (rt, jsds_GCCallbackProc);

    mCx = JSD_DebuggerOnForUser (rt, NULL, NULL);
    if (!mCx)
        return NS_ERROR_FAILURE;

    JSContext *cx   = JSD_GetDefaultJSContext (mCx);
    JSObject  *glob = JS_GetGlobalObject (cx);

    /* init xpconnect on the debugger's context in case xpconnect tries to
     * use it for stuff. */
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (!xpc)
        return NS_ERROR_FAILURE;
    
    xpc->InitClasses (cx, glob);
    
    /* If any of these mFooHook objects are installed, do the required JSD
     * hookup now.   See also, jsdService::SetFooHook().
     */
    if (mThrowHook)
        JSD_SetThrowHook (mCx, jsds_ExecutionHookProc, NULL);
    if (mScriptHook)
        JSD_SetScriptHook (mCx, jsds_ScriptHookProc, NULL);
    if (mInterruptHook)
        JSD_SetInterruptHook (mCx, jsds_ExecutionHookProc, NULL);
    if (mDebuggerHook)
        JSD_SetDebuggerHook (mCx, jsds_ExecutionHookProc, NULL);
    if (mErrorHook)
        JSD_SetDebugBreakHook (mCx, jsds_ExecutionHookProc, NULL);
    if (mTopLevelHook)
        JSD_SetTopLevelHook (mCx, jsds_CallHookProc, NULL);
    else
        JSD_SetTopLevelHook (mCx, jsds_EmptyCallHookProc, NULL);
    if (mFunctionHook)
        JSD_SetFunctionHook (mCx, jsds_CallHookProc, NULL);
    else
        JSD_SetFunctionHook (mCx, jsds_EmptyCallHookProc, NULL);
    mOn = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
jsdService::Off (void)
{
    if (!mCx || !mRuntime)
        return NS_ERROR_NOT_INITIALIZED;
    
    if (gDeadScripts)
        if (gGCStatus == JSGC_END)
        {
            JSContext *cx = JSD_GetDefaultJSContext(mCx);
            jsds_NotifyPendingDeadScripts(cx);
        }
        else
            return NS_ERROR_NOT_AVAILABLE;

    /*
    if (gLastGCProc != jsds_GCCallbackProc)
        JS_SetGCCallbackRT (mRuntime, gLastGCProc);
    */

    jsdValue::InvalidateAll();
    jsdProperty::InvalidateAll();
    ClearAllBreakpoints();

    JSD_SetThrowHook (mCx, NULL, NULL);
    JSD_SetScriptHook (mCx, NULL, NULL);
    JSD_SetInterruptHook (mCx, NULL, NULL);
    JSD_SetDebuggerHook (mCx, NULL, NULL);
    JSD_SetDebugBreakHook (mCx, NULL, NULL);
    JSD_SetTopLevelHook (mCx, NULL, NULL);
    JSD_SetFunctionHook (mCx, NULL, NULL);
    
    JSD_DebuggerOff (mCx);

    mCx = nsnull;
    mRuntime = nsnull;
    mOn = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnumerateScripts (jsdIScriptEnumerator *enumerator)
{
    ASSERT_VALID_CONTEXT;
    
    JSDScript *script;
    JSDScript *iter = NULL;
    PRBool cont_flag;
    nsresult rv = NS_OK;
    
    JSD_LockScriptSubsystem(mCx);
    while((script = JSD_IterateScripts(mCx, &iter)) != NULL) {
        rv = enumerator->EnumerateScript (jsdScript::FromPtr(mCx, script),
                                          &cont_flag);
        if (NS_FAILED(rv) || !cont_flag)
            break;
    }
    JSD_UnlockScriptSubsystem(mCx);

    return rv;
}


NS_IMETHODIMP
jsdService::GC (void)
{
    ASSERT_VALID_CONTEXT;
    JSContext *cx = JSD_GetDefaultJSContext (mCx);
    JS_GC(cx);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::ClearAllBreakpoints (void)
{
    ASSERT_VALID_CONTEXT;

    JSD_LockScriptSubsystem(mCx);
    JSD_ClearAllExecutionHooks (mCx);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnterNestedEventLoop (PRUint32 *_rval)
{
    nsCOMPtr<nsIAppShell> appShell(do_CreateInstance(kAppShellCID));
    NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);
    nsCOMPtr<nsIEventQueueService> 
        eventService(do_GetService(kEventQueueServiceCID));
    NS_ENSURE_TRUE(eventService, NS_ERROR_FAILURE);
    
    appShell->Create(0, nsnull);
    appShell->Spinup();

    nsCOMPtr<nsIJSContextStack> 
        stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
    nsresult rv = NS_OK;
    PRUint32 nestLevel = ++mNestedLoopLevel;
    
    nsCOMPtr<nsIEventQueue> eventQ;
    if (stack && NS_SUCCEEDED(stack->Push(nsnull)) &&
        NS_SUCCEEDED(eventService->PushThreadEventQueue(getter_AddRefs(eventQ))))
    {
        while(NS_SUCCEEDED(rv) && mNestedLoopLevel >= nestLevel)
        {
            void* data;
            PRBool isRealEvent;
            //PRBool processEvent;
            
            rv = appShell->GetNativeEvent(isRealEvent, data);
            if(NS_SUCCEEDED(rv))
            {
                appShell->DispatchNativeEvent(isRealEvent, data);
            }
        }
        JSContext* cx;
        stack->Pop(&cx);
        NS_ASSERTION(cx == nsnull, "JSContextStack mismatch");
    }
    else
        rv = NS_ERROR_FAILURE;
    
    eventService->PopThreadEventQueue(eventQ);
    appShell->Spindown();

    NS_ASSERTION (mNestedLoopLevel <= nestLevel,
                  "nested event didn't unwind properly");
    if (mNestedLoopLevel == nestLevel)
        --mNestedLoopLevel;

    *_rval = mNestedLoopLevel;
    return rv;
}

NS_IMETHODIMP
jsdService::ExitNestedEventLoop (PRUint32 *_rval)
{
    if (mNestedLoopLevel > 0)
        --mNestedLoopLevel;
    else
        return NS_ERROR_FAILURE;

    *_rval = mNestedLoopLevel;    
    return NS_OK;
}    

/* hook attribute get/set functions */

NS_IMETHODIMP
jsdService::SetBreakpointHook (jsdIExecutionHook *aHook)
{    
    mBreakpointHook = aHook;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetBreakpointHook (jsdIExecutionHook **aHook)
{   
    *aHook = mBreakpointHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetErrorHook (jsdIExecutionHook *aHook)
{    
    mErrorHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx)
        return NS_OK;

    if (aHook)
        JSD_SetDebugBreakHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearDebugBreakHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetErrorHook (jsdIExecutionHook **aHook)
{   
    *aHook = mErrorHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetDebuggerHook (jsdIExecutionHook *aHook)
{    
    mDebuggerHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx)
        return NS_OK;

    if (aHook)
        JSD_SetDebuggerHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearDebuggerHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetDebuggerHook (jsdIExecutionHook **aHook)
{   
    *aHook = mDebuggerHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetInterruptHook (jsdIExecutionHook *aHook)
{    
    mInterruptHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx)
        return NS_OK;

    if (aHook)
        JSD_SetInterruptHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearInterruptHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetInterruptHook (jsdIExecutionHook **aHook)
{   
    *aHook = mInterruptHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetScriptHook (jsdIScriptHook *aHook)
{    
    mScriptHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx)
        return NS_OK;
    
    if (aHook)
        JSD_SetScriptHook (mCx, jsds_ScriptHookProc, NULL);
    else
        JSD_SetScriptHook (mCx, NULL, NULL);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetScriptHook (jsdIScriptHook **aHook)
{   
    *aHook = mScriptHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetThrowHook (jsdIExecutionHook *aHook)
{    
    mThrowHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx)
        return NS_OK;

    if (aHook)
        JSD_SetThrowHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearThrowHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetThrowHook (jsdIExecutionHook **aHook)
{   
    *aHook = mThrowHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetTopLevelHook (jsdICallHook *aHook)
{    
    mTopLevelHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx)
        return NS_OK;

    if (aHook)
        JSD_SetTopLevelHook (mCx, jsds_CallHookProc, NULL);
    else
        JSD_SetTopLevelHook (mCx, jsds_EmptyCallHookProc, NULL);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetTopLevelHook (jsdICallHook **aHook)
{   
    *aHook = mTopLevelHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetFunctionHook (jsdICallHook *aHook)
{    
    mFunctionHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx)
        return NS_OK;

    if (aHook)
        JSD_SetFunctionHook (mCx, jsds_CallHookProc, NULL);
    else
        JSD_SetFunctionHook (mCx, jsds_EmptyCallHookProc, NULL);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetFunctionHook (jsdICallHook **aHook)
{   
    *aHook = mFunctionHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

jsdService *
jsdService::GetService ()
{
    if (!gJsds)
        gJsds = new jsdService();
        
    NS_IF_ADDREF(gJsds);
    return gJsds;
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(jsdService, jsdService::GetService);

/* app-start observer. turns on the debugger at app-start if 
 * js.debugger.autostart pref is true
 */
class jsdASObserver : public nsIObserver 
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    jsdASObserver ()
    {
        NS_INIT_ISUPPORTS();
    }    
};

NS_IMPL_THREADSAFE_ISUPPORTS1(jsdASObserver, nsIObserver); 

NS_IMETHODIMP
jsdASObserver::Observe (nsISupports *aSubject, const PRUnichar *aTopic,
                        const PRUnichar *aData)
{
    nsresult rv;

    if (!nsCRT::strcmp(aTopic,
                       NS_LITERAL_STRING(PROFILE_CHANGE_EVENT).get())) {
        /* profile change means that the prefs file is loaded, so we can finally
         * check to see if we're supposed to start the debugger.
         */
            
        nsCOMPtr<nsIPref> pref = do_GetService (NS_PREF_CTRID);
        if (!pref)
            return NS_ERROR_FAILURE;
    
        PRBool f = PR_FALSE;
    
        rv = pref->GetBoolPref("js.debugger.autostart", &f);
    
        if (NS_SUCCEEDED(rv) && f) {
            jsdService *jsds = jsdService::GetService();
            nsCOMPtr<nsIJSRuntimeService> rts = do_GetService(NS_JSRT_CTRID);
            JSRuntime *rt;
            rts->GetRuntime (&rt);
            jsds->OnForRuntime(rt);
        }
    } else if (!nsCRT::strcmp(aTopic,
                              NS_LITERAL_STRING(APPSTART_CATEGORY).get())) {
        /* on app-start, register an interest in hearing when the profile is
         * loaded.
         */
        nsCOMPtr<nsIObserverService> observerService =
            do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
        if (observerService) {
            rv = observerService->AddObserver(this, 
                                              NS_LITERAL_STRING(PROFILE_CHANGE_EVENT).get());
            if (NS_FAILED(rv)) return rv;
        }
    }
    
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(jsdASObserver);

static NS_METHOD
RegisterASObserver (nsIComponentManager *aCompMgr, nsIFile *aPath,
                    const char *registryLocation, const char *componentType,
                    const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager>
        categoryManager(do_GetService(NS_CATMAN_CTRID, &rv));
    if (NS_SUCCEEDED(rv)) {
        rv = categoryManager->AddCategoryEntry(APPSTART_CATEGORY,
                                               "JSDebugger app-start observer",
                                               jsdASObserverCtrID,
                                               PR_TRUE, PR_TRUE,nsnull);
    }
    return rv;
}

static NS_METHOD
UnRegisterASObserver(nsIComponentManager *aCompMgr, nsIFile *aPath, 
                     const char *registryLocation,
                     const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> 
        categoryManager(do_GetService(NS_CATMAN_CTRID, &rv));
    if (NS_SUCCEEDED(rv)) {
        rv = 
            categoryManager->DeleteCategoryEntry(APPSTART_CATEGORY,
                                                 "JSDebugger app-start observer",
                                                 PR_TRUE);
    }
    return rv;
}

static nsModuleComponentInfo components[] = {
    {"JSDService", JSDSERVICE_CID, jsdServiceCtrID, jsdServiceConstructor},
    {"JSDASObserver", JSDASO_CID, jsdASObserverCtrID,
     jsdASObserverConstructor, RegisterASObserver, UnRegisterASObserver }
};

NS_IMPL_NSGETMODULE(JavaScript_Debugger, components);

/********************************************************************************
 ********************************************************************************
 * graveyard
 */

#if 0
/* Thread States */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdThreadState, jsdIThreadState); 

NS_IMETHODIMP
jsdThreadState::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetJSDThreadState(JSDThreadState **_rval)
{
    *_rval = mThreadState;
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetFrameCount (PRUint32 *_rval)
{
    *_rval = JSD_GetCountOfStackFrames (mCx, mThreadState);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetTopFrame (jsdIStackFrame **_rval)
{
    JSDStackFrameInfo *sfi = JSD_GetStackFrame (mCx, mThreadState);
    
    *_rval = jsdStackFrame::FromPtr (mCx, mThreadState, sfi);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetPendingException(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetException (mCx, mThreadState);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::SetPendingException(jsdIValue *aException)
{
    JSDValue *jsdv;
    
    nsresult rv = aException->GetJSDValue (&jsdv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    
    if (!JSD_SetException (mCx, mThreadState, jsdv))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

#endif
