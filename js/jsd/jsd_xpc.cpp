/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"
#include "jsd_xpc.h"
#include "xpcpublic.h"

#include "js/GCAPI.h"
#include "js/OldDebugAPI.h"

#include "nsIXPConnect.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsICategoryManager.h"
#include "nsIJSRuntimeService.h"
#include "nsIThreadInternal.h"
#include "nsIScriptError.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsMemory.h"
#include "jsdebug.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

/* XXX DOM dependency */
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsDOMJSUtils.h"
#include "SandboxPrivate.h"
#include "nsJSPrincipals.h"
#include "nsContentUtils.h"
#include "mozilla/dom/ScriptSettings.h"

using mozilla::AutoSafeJSContext;
using mozilla::AutoPushJSContext;
using mozilla::dom::AutoNoJSAPI;

/*
 * defining CAUTIOUS_SCRIPTHOOK makes jsds disable GC while calling out to the
 * script hook.  This was a hack to avoid some js engine problems that should
 * be fixed now (see Mozilla bug 77636).
 */
#undef CAUTIOUS_SCRIPTHOOK

#ifdef DEBUG_verbose
#   define DEBUG_COUNT(name, count)                                             \
        { if ((count % 10) == 0) printf (name ": %i\n", count); }
#   define DEBUG_CREATE(name, count) {count++; DEBUG_COUNT ("+++++ " name,count)}
#   define DEBUG_DESTROY(name, count) {count--; DEBUG_COUNT ("----- " name,count)}
#else
#   define DEBUG_CREATE(name, count) 
#   define DEBUG_DESTROY(name, count)
#endif

#define ASSERT_VALID_CONTEXT   { if (!mCx) return NS_ERROR_NOT_AVAILABLE; }
#define ASSERT_VALID_EPHEMERAL { if (!mValid) return NS_ERROR_NOT_AVAILABLE; }

#define JSDSERVICE_CID                               \
{ /* f1299dc2-1dd1-11b2-a347-ee6b7660e048 */         \
     0xf1299dc2,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xa3, 0x47, 0xee, 0x6b, 0x76, 0x60, 0xe0, 0x48} \
}

#define JSDASO_CID                                   \
{ /* 2fd6b7f6-eb8c-4f32-ad26-113f2c02d0fe */         \
     0x2fd6b7f6,                                     \
     0xeb8c,                                         \
     0x4f32,                                         \
    {0xad, 0x26, 0x11, 0x3f, 0x2c, 0x02, 0xd0, 0xfe} \
}

#define JSDS_MAJOR_VERSION 1
#define JSDS_MINOR_VERSION 2

#define NS_CATMAN_CTRID   "@mozilla.org/categorymanager;1"
#define NS_JSRT_CTRID     "@mozilla.org/js/xpc/RuntimeService;1"

#define AUTOREG_CATEGORY  "xpcom-autoregistration"
#define APPSTART_CATEGORY "app-startup"
#define JSD_AUTOREG_ENTRY "JSDebugger Startup Observer"
#define JSD_STARTUP_ENTRY "JSDebugger Startup Observer"

static void
jsds_GCSliceCallbackProc (JSRuntime *rt, JS::GCProgress progress, const JS::GCDescription &desc);

/*******************************************************************************
 * global vars
 ******************************************************************************/

const char implementationString[] = "Mozilla JavaScript Debugger Service";

const char jsdServiceCtrID[] = "@mozilla.org/js/jsd/debugger-service;1";
const char jsdARObserverCtrID[] = "@mozilla.org/js/jsd/app-start-observer;2";

#ifdef DEBUG_verbose
uint32_t gScriptCount   = 0;
uint32_t gValueCount    = 0;
uint32_t gPropertyCount = 0;
uint32_t gContextCount  = 0;
uint32_t gFrameCount  = 0;
#endif

static jsdService          *gJsds               = 0;
static JS::GCSliceCallback gPrevGCSliceCallback = jsds_GCSliceCallbackProc;
static bool                gGCRunning           = false;

static struct DeadScript {
    PRCList     links;
    JSDContext *jsdc;
    jsdIScript *script;
} *gDeadScripts = nullptr;

enum PatternType {
    ptIgnore     = 0U,
    ptStartsWith = 1U,
    ptEndsWith   = 2U,
    ptContains   = 3U,
    ptEquals     = 4U
};

static struct FilterRecord {
    PRCList      links;
    jsdIFilter  *filterObject;
    nsCString    urlPattern;
    PatternType  patternType;
    uint32_t     startLine;
    uint32_t     endLine;
} *gFilters = nullptr;

static struct LiveEphemeral *gLiveValues      = nullptr;
static struct LiveEphemeral *gLiveProperties  = nullptr;
static struct LiveEphemeral *gLiveContexts    = nullptr;
static struct LiveEphemeral *gLiveStackFrames = nullptr;

/*******************************************************************************
 * utility functions for ephemeral lists
 *******************************************************************************/
already_AddRefed<jsdIEphemeral>
jsds_FindEphemeral (LiveEphemeral **listHead, void *key)
{
    if (!*listHead)
        return nullptr;
    
    LiveEphemeral *lv_record = 
        reinterpret_cast<LiveEphemeral *>
                        (PR_NEXT_LINK(&(*listHead)->links));
    do
    {
        if (lv_record->key == key)
        {
            nsCOMPtr<jsdIEphemeral> ret = lv_record->value;
            return ret.forget();
        }
        lv_record = reinterpret_cast<LiveEphemeral *>
                                    (PR_NEXT_LINK(&lv_record->links));
    }
    while (lv_record != *listHead);

    return nullptr;
}

void
jsds_InvalidateAllEphemerals (LiveEphemeral **listHead)
{
    LiveEphemeral *lv_record = 
        reinterpret_cast<LiveEphemeral *>
                        (PR_NEXT_LINK(&(*listHead)->links));
    do
    {
        LiveEphemeral *next =
            reinterpret_cast<LiveEphemeral *>
                            (PR_NEXT_LINK(&lv_record->links));
        lv_record->value->Invalidate();
        lv_record = next;
    }
    while (*listHead);
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
    LiveEphemeral *next = reinterpret_cast<LiveEphemeral *>
                                          (PR_NEXT_LINK(&item->links));

    if (next == item)
    {
        /* if the current item is also the next item, we're the only element,
         * null out the list head */
        NS_ASSERTION (*listHead == item,
                      "How could we not be the head of a one item list?");
        *listHead = nullptr;
    }
    else if (item == *listHead)
    {
        /* otherwise, if we're currently the list head, change it */
        *listHead = next;
    }
    
    PR_REMOVE_AND_INIT_LINK(&item->links);
}

/*******************************************************************************
 * utility functions for filters
 *******************************************************************************/
void
jsds_FreeFilter (FilterRecord *rec)
{
    NS_IF_RELEASE (rec->filterObject);
    PR_Free (rec);
}

/* copies appropriate |filter| attributes into |rec|.
 * False return indicates failure, the contents of |rec| will not be changed.
 */
bool
jsds_SyncFilter (FilterRecord *rec, jsdIFilter *filter)
{
    NS_ASSERTION (rec, "jsds_SyncFilter without rec");
    NS_ASSERTION (filter, "jsds_SyncFilter without filter");

    uint32_t startLine;
    nsresult rv = filter->GetStartLine(&startLine);
    if (NS_FAILED(rv))
        return false;

    uint32_t endLine;
    rv = filter->GetStartLine(&endLine);
    if (NS_FAILED(rv))
        return false;    

    nsAutoCString urlPattern;
    rv = filter->GetUrlPattern (urlPattern);
    if (NS_FAILED(rv))
        return false;
    
    uint32_t len = urlPattern.Length();
    if (len) {
        if (urlPattern[0] == '*') {
            /* pattern starts with a *, shift all chars once to the left,
             * including the trailing null. */
            urlPattern = Substring(urlPattern, 1, len);

            if (urlPattern[len - 2] == '*') {
                /* pattern is in the format "*foo*", overwrite the final * with
                 * a null. */
                urlPattern.Truncate(len - 2);
                rec->patternType = ptContains;
            } else {
                /* pattern is in the format "*foo", just make a note of the
                 * new length. */
                rec->patternType = ptEndsWith;
            }
        } else if (urlPattern[len - 1] == '*') {
            /* pattern is in the format "foo*", overwrite the final * with a 
             * null. */
            urlPattern.Truncate(len - 1);
            rec->patternType = ptStartsWith;
        } else {
            /* pattern is in the format "foo". */
            rec->patternType = ptEquals;
        }
    } else {
        rec->patternType = ptIgnore;
    }

    /* we got everything we need without failing, now copy it into rec. */

    if (rec->filterObject != filter) {
        NS_IF_RELEASE(rec->filterObject);
        NS_ADDREF(filter);
        rec->filterObject = filter;
    }
    
    rec->startLine     = startLine;
    rec->endLine       = endLine;
    
    rec->urlPattern = urlPattern;

    return true;
            
}

FilterRecord *
jsds_FindFilter (jsdIFilter *filter)
{
    if (!gFilters)
        return nullptr;
    
    FilterRecord *current = gFilters;
    
    do {
        if (current->filterObject == filter)
            return current;
        current = reinterpret_cast<FilterRecord *>
                                  (PR_NEXT_LINK(&current->links));
    } while (current != gFilters);
    
    return nullptr;
}

/* returns true if the hook should be executed. */
bool
jsds_FilterHook (JSDContext *jsdc, JSDThreadState *state)
{
    JSDStackFrameInfo *frame = JSD_GetStackFrame (jsdc, state);

    if (!frame) {
        NS_WARNING("No frame in threadstate");
        return false;
    }

    JSDScript *script = JSD_GetScriptForStackFrame (jsdc, state, frame);
    if (!script)
        return true;

    uintptr_t pc = JSD_GetPCForStackFrame (jsdc, state, frame);

    nsCString url(JSD_GetScriptFilename (jsdc, script));
    if (url.IsEmpty()) {
        NS_WARNING ("Script with no filename");
        return false;
    }

    if (!gFilters)
        return true;    

    uint32_t currentLine = JSD_GetClosestLine (jsdc, script, pc);
    uint32_t len = 0;
    FilterRecord *currentFilter = gFilters;
    do {
        uint32_t flags = 0;

#ifdef DEBUG
        nsresult rv =
#endif
            currentFilter->filterObject->GetFlags(&flags);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Error getting flags for filter");

        if (flags & jsdIFilter::FLAG_ENABLED) {
            /* If there is no start line, or the start line is before 
             * or equal to the current */
            if ((!currentFilter->startLine ||
                 currentFilter->startLine <= currentLine) &&
                /* and there is no end line, or the end line is after
                 * or equal to the current */
                (!currentFilter->endLine ||
                 currentFilter->endLine >= currentLine)) {
                /* then we're going to have to compare the url. */
                if (currentFilter->patternType == ptIgnore)
                    return !!(flags & jsdIFilter::FLAG_PASS);

                if (!len)
                    len = url.Length();
                nsCString urlPattern = currentFilter->urlPattern;
                uint32_t patternLength = urlPattern.Length();
                if (len >= patternLength) {
                    switch (currentFilter->patternType) {
                        case ptEquals:
                            if (urlPattern.Equals(url))
                                return !!(flags & jsdIFilter::FLAG_PASS);
                            break;
                        case ptStartsWith:
                            if (urlPattern.Equals(Substring(url, 0, patternLength)))
                                return !!(flags & jsdIFilter::FLAG_PASS);
                            break;
                        case ptEndsWith:
                            if (urlPattern.Equals(Substring(url, len - patternLength)))
                                return !!(flags & jsdIFilter::FLAG_PASS);
                            break;
                        case ptContains:
                            {
                                nsACString::const_iterator start, end;
                                url.BeginReading(start);
                                url.EndReading(end);
                                if (FindInReadable(currentFilter->urlPattern, start, end))
                                    return !!(flags & jsdIFilter::FLAG_PASS);
                            }
                            break;
                        default:
                            NS_ERROR("Invalid pattern type");
                    }
                }                
            }
        }
        currentFilter = reinterpret_cast<FilterRecord *>
                                        (PR_NEXT_LINK(&currentFilter->links));
    } while (currentFilter != gFilters);

    return true;
    
}

/*******************************************************************************
 * c callbacks
 *******************************************************************************/

static void
jsds_NotifyPendingDeadScripts (JSRuntime *rt)
{
    jsdService *jsds = gJsds;

    nsCOMPtr<jsdIScriptHook> hook;
    if (jsds) {
        NS_ADDREF(jsds);
        jsds->GetScriptHook (getter_AddRefs(hook));
        jsds->DoPause(nullptr, true);
    }

    DeadScript *deadScripts = gDeadScripts;
    gDeadScripts = nullptr;
    while (deadScripts) {
        DeadScript *ds = deadScripts;
        /* get next deleted script */
        deadScripts = reinterpret_cast<DeadScript *>
                                       (PR_NEXT_LINK(&ds->links));
        if (deadScripts == ds)
            deadScripts = nullptr;

        if (hook)
        {
            /* tell the user this script has been destroyed */
#ifdef CAUTIOUS_SCRIPTHOOK
            JS_UNKEEP_ATOMS(rt);
#endif
            hook->OnScriptDestroyed (ds->script);
#ifdef CAUTIOUS_SCRIPTHOOK
            JS_KEEP_ATOMS(rt);
#endif
        }

        /* take it out of the circular list */
        PR_REMOVE_LINK(&ds->links);

        /* addref came from the FromPtr call in jsds_ScriptHookProc */
        NS_RELEASE(ds->script);
        /* free the struct! */
        PR_Free(ds);
    }

    if (jsds) {
        jsds->DoUnPause(nullptr, true);
        NS_RELEASE(jsds);
    }
}

static void
jsds_GCSliceCallbackProc (JSRuntime *rt, JS::GCProgress progress, const JS::GCDescription &desc)
{
    if (progress == JS::GC_CYCLE_END || progress == JS::GC_SLICE_END) {
        NS_ASSERTION(gGCRunning, "GC slice callback was missed");

        while (gDeadScripts)
            jsds_NotifyPendingDeadScripts (rt);

        gGCRunning = false;
    } else {
        NS_ASSERTION(!gGCRunning, "should not re-enter GC");
        gGCRunning = true;
    }

    if (gPrevGCSliceCallback)
        (*gPrevGCSliceCallback)(rt, progress, desc);
}

static unsigned
jsds_ErrorHookProc (JSDContext *jsdc, JSContext *cx, const char *message,
                    JSErrorReport *report, void *callerdata)
{
    static bool running = false;

    nsCOMPtr<jsdIErrorHook> hook;
    gJsds->GetErrorHook(getter_AddRefs(hook));
    if (!hook)
        return JSD_ERROR_REPORTER_PASS_ALONG;

    if (running)
        return JSD_ERROR_REPORTER_PASS_ALONG;

    running = true;

    nsCOMPtr<jsdIValue> val;
    if (JS_IsExceptionPending(cx)) {
        JS::RootedValue jv(cx);
        JS_GetPendingException(cx, &jv);
        JSDValue *jsdv = JSD_NewValue (jsdc, jv);
        val = dont_AddRef(jsdValue::FromPtr(jsdc, jsdv));
    }

    nsAutoCString fileName;
    uint32_t    line;
    uint32_t    pos;
    uint32_t    flags;
    uint32_t    errnum;
    bool        rval;
    if (report) {
        fileName.Assign(report->filename);
        line = report->lineno;
        pos = report->tokenptr - report->linebuf;
        flags = report->flags;
        errnum = report->errorNumber;
    }
    else
    {
        line     = 0;
        pos      = 0;
        flags    = 0;
        errnum   = 0;
    }
    
    gJsds->DoPause(nullptr, true);
    hook->OnError (nsDependentCString(message), fileName, line, pos, flags, errnum, val, &rval);
    gJsds->DoUnPause(nullptr, true);
    
    running = false;
    if (!rval)
        return JSD_ERROR_REPORTER_DEBUG;
    
    return JSD_ERROR_REPORTER_PASS_ALONG;
}

static bool
jsds_CallHookProc (JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                   unsigned type, void* callerdata)
{
    nsCOMPtr<jsdICallHook> hook;

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
        return true;

    if (!jsds_FilterHook (jsdc, jsdthreadstate))
        return false;

    JSDStackFrameInfo *native_frame = JSD_GetStackFrame (jsdc, jsdthreadstate);
    nsCOMPtr<jsdIStackFrame> frame =
        dont_AddRef(jsdStackFrame::FromPtr(jsdc, jsdthreadstate, native_frame));
    gJsds->DoPause(nullptr, true);
    hook->OnCall(frame, type);    
    gJsds->DoUnPause(nullptr, true);
    jsdStackFrame::InvalidateAll();

    return true;
}

static uint32_t
jsds_ExecutionHookProc (JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                        unsigned type, void* callerdata, jsval* rval)
{
    nsCOMPtr<jsdIExecutionHook> hook(0);
    uint32_t hook_rv = JSD_HOOK_RETURN_CONTINUE;
    nsCOMPtr<jsdIValue> js_rv;

    switch (type)
    {
        case JSD_HOOK_INTERRUPTED:
            gJsds->GetInterruptHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_DEBUG_REQUESTED:
            gJsds->GetDebugHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_DEBUGGER_KEYWORD:
            gJsds->GetDebuggerHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_BREAKPOINT:
            {
                /* we can't pause breakpoints the way we pause the other
                 * execution hooks (at least, not easily.)  Instead we bail
                 * here if the service is paused. */
                uint32_t level;
                gJsds->GetPauseDepth(&level);
                if (!level)
                    gJsds->GetBreakpointHook(getter_AddRefs(hook));
            }
            break;
        case JSD_HOOK_THROW:
        {
            hook_rv = JSD_HOOK_RETURN_CONTINUE_THROW;
            gJsds->GetThrowHook(getter_AddRefs(hook));
            if (hook) {
                JSDValue *jsdv = JSD_GetException (jsdc, jsdthreadstate);
                js_rv = dont_AddRef(jsdValue::FromPtr (jsdc, jsdv));
            }
            break;
        }
        default:
            NS_ASSERTION (0, "Unknown hook type.");
    }

    if (!hook)
        return hook_rv;
    
    if (!jsds_FilterHook (jsdc, jsdthreadstate))
        return JSD_HOOK_RETURN_CONTINUE;
    
    JSDStackFrameInfo *native_frame = JSD_GetStackFrame (jsdc, jsdthreadstate);
    nsCOMPtr<jsdIStackFrame> frame =
        dont_AddRef(jsdStackFrame::FromPtr(jsdc, jsdthreadstate, native_frame));
    gJsds->DoPause(nullptr, true);
    jsdIValue *inout_rv = js_rv;
    NS_IF_ADDREF(inout_rv);
    hook->OnExecute (frame, type, &inout_rv, &hook_rv);
    js_rv = inout_rv;
    NS_IF_RELEASE(inout_rv);
    gJsds->DoUnPause(nullptr, true);
    jsdStackFrame::InvalidateAll();
        
    if (hook_rv == JSD_HOOK_RETURN_RET_WITH_VAL ||
        hook_rv == JSD_HOOK_RETURN_THROW_WITH_VAL) {
        *rval = JSVAL_VOID;
        if (js_rv) {
            JSDValue *jsdv;
            if (NS_SUCCEEDED(js_rv->GetJSDValue (&jsdv)))
                *rval = JSD_GetValueWrappedJSVal(jsdc, jsdv);
        }
    }
    
    return hook_rv;
}

static void
jsds_ScriptHookProc (JSDContext* jsdc, JSDScript* jsdscript, bool creating,
                     void* callerdata)
{
#ifdef CAUTIOUS_SCRIPTHOOK
    JSRuntime *rt = JS_GetRuntime(nsContentUtils::GetSafeJSContext());
#endif

    if (creating) {
        nsCOMPtr<jsdIScriptHook> hook;
        gJsds->GetScriptHook(getter_AddRefs(hook));

        /* a script is being created */
        if (!hook) {
            /* nobody cares, just exit */
            return;
        }
            
        nsCOMPtr<jsdIScript> script = 
            dont_AddRef(jsdScript::FromPtr(jsdc, jsdscript));
#ifdef CAUTIOUS_SCRIPTHOOK
        JS_UNKEEP_ATOMS(rt);
#endif
        gJsds->DoPause(nullptr, true);
        hook->OnScriptCreated (script);
        gJsds->DoUnPause(nullptr, true);
#ifdef CAUTIOUS_SCRIPTHOOK
        JS_KEEP_ATOMS(rt);
#endif
    } else {
        /* a script is being destroyed.  even if there is no registered hook
         * we'll still need to invalidate the jsdIScript record, in order
         * to remove the reference held in the JSDScript private data. */
        nsCOMPtr<jsdIScript> jsdis = 
            static_cast<jsdIScript *>(JSD_GetScriptPrivate(jsdscript));
        if (!jsdis)
            return;

        jsdis->Invalidate();

        if (!gGCRunning) {
            nsCOMPtr<jsdIScriptHook> hook;
            gJsds->GetScriptHook(getter_AddRefs(hook));
            if (!hook)
                return;

            /* if GC *isn't* running, we can tell the user about the script
             * delete now. */
#ifdef CAUTIOUS_SCRIPTHOOK
            JS_UNKEEP_ATOMS(rt);
#endif
                
            gJsds->DoPause(nullptr, true);
            hook->OnScriptDestroyed (jsdis);
            gJsds->DoUnPause(nullptr, true);
#ifdef CAUTIOUS_SCRIPTHOOK
            JS_KEEP_ATOMS(rt);
#endif
        } else {
            /* if a GC *is* running, we've got to wait until it's done before
             * we can execute any JS, so we queue the notification in a PRCList
             * until GC tells us it's done. See jsds_GCCallbackProc(). */
            DeadScript *ds = PR_NEW(DeadScript);
            if (!ds)
                return; /* NS_ERROR_OUT_OF_MEMORY */
        
            ds->jsdc = jsdc;
            ds->script = jsdis;
            NS_ADDREF(ds->script);
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
NS_IMPL_ISUPPORTS(jsdContext, jsdIContext, jsdIEphemeral);

NS_IMETHODIMP
jsdContext::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}
*/

/* Objects */
NS_IMPL_ISUPPORTS(jsdObject, jsdIObject)

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
jsdObject::GetCreatorURL(nsACString &_rval)
{
    _rval.Assign(JSD_GetObjectNewURL(mCx, mObject));
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetCreatorLine(uint32_t *_rval)
{
    *_rval = JSD_GetObjectNewLineNumber(mCx, mObject);
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetConstructorURL(nsACString &_rval)
{
    _rval.Assign(JSD_GetObjectConstructorURL(mCx, mObject));
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetConstructorLine(uint32_t *_rval)
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

/* Properties */
NS_IMPL_ISUPPORTS(jsdProperty, jsdIProperty, jsdIEphemeral)

jsdProperty::jsdProperty (JSDContext *aCx, JSDProperty *aProperty) :
    mCx(aCx), mProperty(aProperty)
{
    DEBUG_CREATE ("jsdProperty", gPropertyCount);
    mValid = (aCx && aProperty);
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
    ASSERT_VALID_EPHEMERAL;
    mValid = false;
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
jsdProperty::GetIsValid(bool *_rval)
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
jsdProperty::GetFlags(uint32_t *_rval)
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

/* Scripts */
NS_IMPL_ISUPPORTS(jsdScript, jsdIScript, jsdIEphemeral)

static NS_IMETHODIMP
AssignToJSString(JSDContext *aCx, nsACString *x, JSString *str_)
{
    if (!str_) {
        x->SetLength(0);
        return NS_OK;
    }
    JS::RootedString str(JSD_GetJSRuntime(aCx), str_);
    AutoSafeJSContext cx;
    JSAutoCompartment ac(cx, JSD_GetDefaultGlobal(aCx)); // Just in case.
    size_t length = JS_GetStringEncodingLength(cx, str);
    if (length == size_t(-1))
        return NS_ERROR_FAILURE;
    x->SetLength(uint32_t(length));
    if (x->Length() != uint32_t(length))
        return NS_ERROR_OUT_OF_MEMORY;
    JS_EncodeStringToBuffer(cx, str, x->BeginWriting(), length);
    return NS_OK;
}

jsdScript::jsdScript (JSDContext *aCx, JSDScript *aScript) : mValid(false),
                                                             mTag(0),
                                                             mCx(aCx),
                                                             mScript(aScript),
                                                             mFileName(0), 
                                                             mFunctionName(0),
                                                             mBaseLineNumber(0),
                                                             mLineExtent(0),
                                                             mPPLineMap(0),
                                                             mFirstPC(0)
{
    DEBUG_CREATE ("jsdScript", gScriptCount);

    if (mScript) {
        /* copy the script's information now, so we have it later, when it
         * gets destroyed. */
        JSD_LockScriptSubsystem(mCx);
        mFileName = new nsCString(JSD_GetScriptFilename(mCx, mScript));
        mFunctionName = new nsCString();
        if (mFunctionName) {
            JSString *str = JSD_GetScriptFunctionId(mCx, mScript);
            if (str)
                AssignToJSString(mCx, mFunctionName, str);
        }
        mBaseLineNumber = JSD_GetScriptBaseLineNumber(mCx, mScript);
        mLineExtent = JSD_GetScriptLineExtent(mCx, mScript);
        mFirstPC = JSD_GetClosestPC(mCx, mScript, 0);
        JSD_UnlockScriptSubsystem(mCx);
        
        mValid = true;
    }
}

jsdScript::~jsdScript () 
{
    DEBUG_DESTROY ("jsdScript", gScriptCount);
    delete mFileName;
    delete mFunctionName;

    if (mPPLineMap)
        PR_Free(mPPLineMap);

    /* Invalidate() needs to be called to release an owning reference to
     * ourselves, so if we got here without being invalidated, something
     * has gone wrong with our ref count. */
    NS_ASSERTION (!mValid, "Script destroyed without being invalidated.");
}

/*
 * This method populates a line <-> pc map for a pretty printed version of this
 * script.  It does this by decompiling, and then recompiling the script.  The
 * resulting script is scanned for the line map, and then left as GC fodder.
 */
PCMapEntry *
jsdScript::CreatePPLineMap()
{
    AutoSafeJSContext cx;
    JSAutoCompartment ac(cx, JSD_GetDefaultGlobal (mCx)); // Just in case.
    JS::RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!obj)
        return nullptr;
    JS::RootedFunction fun(cx, JSD_GetJSFunction (mCx, mScript));
    JS::RootedScript script(cx); /* In JSD compartment */
    uint32_t    baseLine;
    JS::RootedString jsstr(cx);
    size_t      length;
    const jschar *chars;

    if (fun) {
        unsigned nargs;

        {
            JSAutoCompartment ac(cx, JS_GetFunctionObject(fun));
            nargs = JS_GetFunctionArgumentCount(cx, fun);
            if (nargs > 12)
                return nullptr;
            jsstr = JS_DecompileFunctionBody (cx, fun, 4);
            if (!jsstr)
                return nullptr;

            if (!(chars = JS_GetStringCharsAndLength(cx, jsstr, &length)))
                return nullptr;
        }

        JS::Anchor<JSString *> kungFuDeathGrip(jsstr);
        static const char *const argnames[] = {
            "arg1", "arg2", "arg3", "arg4",
            "arg5", "arg6", "arg7", "arg8",
            "arg9", "arg10", "arg11", "arg12"
        };
        JS::CompileOptions options(cx);
        options.setFileAndLine("x-jsd:ppbuffer?type=function", 3);
        fun = JS_CompileUCFunction (cx, obj, "ppfun", nargs, argnames, chars,
                                    length, options);
        if (!fun || !(script = JS_GetFunctionScript(cx, fun)))
            return nullptr;
        baseLine = 3;
    } else {
        script = JSD_GetJSScript(mCx, mScript);
        JSString *jsstr;

        {
            JSAutoCompartment ac(cx, script);

            jsstr = JS_DecompileScript (cx, script, "ppscript", 4);
            if (!jsstr)
                return nullptr;

            if (!(chars = JS_GetStringCharsAndLength(cx, jsstr, &length)))
                return nullptr;
        }

        JS::Anchor<JSString *> kungFuDeathGrip(jsstr);
        JS::CompileOptions options(cx);
        options.setFileAndLine("x-jsd:ppbuffer?type=script", 1);
        script = JS_CompileUCScript(cx, obj, chars, length, options);
        if (!script)
            return nullptr;
        baseLine = 1;
    }

    uint32_t scriptExtent = JS_GetScriptLineExtent (cx, script);
    jsbytecode* firstPC = JS_LineNumberToPC (cx, script, 0);
    /* allocate worst case size of map (number of lines in script + 1
     * for our 0 record), we'll shrink it with a realloc later. */
    PCMapEntry *lineMap =
        static_cast<PCMapEntry *>
                   (PR_Malloc((scriptExtent + 1) * sizeof (PCMapEntry)));
    uint32_t lineMapSize = 0;

    if (lineMap) {
        for (uint32_t line = baseLine; line < scriptExtent + baseLine; ++line) {
            jsbytecode* pc = JS_LineNumberToPC (cx, script, line);
            if (line == JS_PCToLineNumber (cx, script, pc)) {
                lineMap[lineMapSize].line = line;
                lineMap[lineMapSize].pc = pc - firstPC;
                ++lineMapSize;
            }
        }
        if (scriptExtent != lineMapSize) {
            lineMap =
                static_cast<PCMapEntry *>
                           (PR_Realloc(mPPLineMap = lineMap,
                                       lineMapSize * sizeof(PCMapEntry)));
            if (!lineMap) {
                PR_Free(mPPLineMap);
                lineMapSize = 0;
            }
        }
    }

    mPCMapSize = lineMapSize;
    return mPPLineMap = lineMap;
}

uint32_t
jsdScript::PPPcToLine (uint32_t aPC)
{
    if (!mPPLineMap && !CreatePPLineMap())
        return 0;
    uint32_t i;
    for (i = 1; i < mPCMapSize; ++i) {
        if (mPPLineMap[i].pc > aPC)
            return mPPLineMap[i - 1].line;            
    }

    return mPPLineMap[mPCMapSize - 1].line;
}

uint32_t
jsdScript::PPLineToPc (uint32_t aLine)
{
    if (!mPPLineMap && !CreatePPLineMap())
        return 0;
    uint32_t i;
    for (i = 1; i < mPCMapSize; ++i) {
        if (mPPLineMap[i].line > aLine)
            return mPPLineMap[i - 1].pc;
    }

    return mPPLineMap[mPCMapSize - 1].pc;
}

NS_IMETHODIMP
jsdScript::GetJSDContext(JSDContext **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetJSDScript(JSDScript **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = mScript;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetVersion (int32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    AutoSafeJSContext cx;
    JS::RootedScript script(cx, JSD_GetJSScript(mCx, mScript));
    JSAutoCompartment ac(cx, script);
    *_rval = static_cast<int32_t>(JS_GetScriptVersion(cx, script));
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetTag(uint32_t *_rval)
{
    if (!mTag)
        mTag = ++jsdScript::LastTag;
    
    *_rval = mTag;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::Invalidate()
{
    ASSERT_VALID_EPHEMERAL;
    mValid = false;
    
    /* release the addref we do in FromPtr */
    jsdIScript *script = static_cast<jsdIScript *>
                                    (JSD_GetScriptPrivate(mScript));
    NS_ASSERTION (script == this, "That's not my script!");
    NS_RELEASE(script);
    JSD_SetScriptPrivate(mScript, nullptr);
    return NS_OK;
}

void
jsdScript::InvalidateAll ()
{
    JSDContext *cx;
    if (NS_FAILED(gJsds->GetJSDContext (&cx)))
        return;

    JSDScript *script;
    JSDScript *iter = nullptr;
    
    JSD_LockScriptSubsystem(cx);
    while((script = JSD_IterateScripts(cx, &iter)) != nullptr) {
        nsCOMPtr<jsdIScript> jsdis = 
            static_cast<jsdIScript *>(JSD_GetScriptPrivate(script));
        if (jsdis)
            jsdis->Invalidate();
    }
    JSD_UnlockScriptSubsystem(cx);
}

NS_IMETHODIMP
jsdScript::GetIsValid(bool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::SetFlags(uint32_t flags)
{
    ASSERT_VALID_EPHEMERAL;
    JSD_SetScriptFlags(mCx, mScript, flags);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFlags(uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptFlags(mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFileName(nsACString &_rval)
{
    _rval.Assign(*mFileName);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionName(nsACString &_rval)
{
    _rval.Assign(*mFunctionName);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetParameterNames(uint32_t* count, char16_t*** paramNames)
{
    ASSERT_VALID_EPHEMERAL;
    AutoSafeJSContext cx;
    JS::RootedFunction fun(cx, JSD_GetJSFunction (mCx, mScript));
    if (!fun) {
        *count = 0;
        *paramNames = nullptr;
        return NS_OK;
    }

    JSAutoCompartment ac(cx, JS_GetFunctionObject(fun));

    unsigned nargs;
    if (!JS_FunctionHasLocalNames(cx, fun) ||
        (nargs = JS_GetFunctionArgumentCount(cx, fun)) == 0) {
        *count = 0;
        *paramNames = nullptr;
        return NS_OK;
    }

    char16_t **ret =
        static_cast<char16_t**>(NS_Alloc(nargs * sizeof(char16_t*)));
    if (!ret)
        return NS_ERROR_OUT_OF_MEMORY;

    void *mark;
    uintptr_t *names = JS_GetFunctionLocalNameArray(cx, fun, &mark);
    if (!names) {
        NS_Free(ret);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = NS_OK;
    for (unsigned i = 0; i < nargs; ++i) {
        JSAtom *atom = JS_LocalNameToAtom(names[i]);
        if (!atom) {
            ret[i] = 0;
        } else {
            JSString *str = JS_AtomKey(atom);
            ret[i] = NS_strndup(JS_GetInternedStringChars(str), JS_GetStringLength(str));
            if (!ret[i]) {
                NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, ret);
                rv = NS_ERROR_OUT_OF_MEMORY;
                break;
            }
        }
    }
    JS_ReleaseFunctionLocalNameArray(cx, mark);
    if (NS_FAILED(rv))
        return rv;
    *count = nargs;
    *paramNames = ret;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionObject(jsdIValue **_rval)
{
    JS::RootedFunction fun(JSD_GetJSRuntime(mCx), JSD_GetJSFunction(mCx, mScript));
    if (!fun)
        return NS_ERROR_NOT_AVAILABLE;

    AutoSafeJSContext jsContext;
    JS::RootedObject obj(jsContext, JS_GetFunctionObject(fun));
    if (!obj)
        return NS_ERROR_FAILURE;

    JSDContext *cx;
    if (NS_FAILED(gJsds->GetJSDContext (&cx)))
        return NS_ERROR_NOT_INITIALIZED;

    JSDValue *jsdv = JSD_NewValue(cx, OBJECT_TO_JSVAL(obj));
    if (!jsdv)
        return NS_ERROR_OUT_OF_MEMORY;

    *_rval = jsdValue::FromPtr(cx, jsdv);
    if (!*_rval) {
        JSD_DropValue(cx, jsdv);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionSource(nsAString & aFunctionSource)
{
    ASSERT_VALID_EPHEMERAL;
    AutoSafeJSContext cx_;
    JSContext *cx = cx_; // Appease the type system with Maybe<>s below.
    JS::RootedFunction fun(cx, JSD_GetJSFunction (mCx, mScript));

    JSString *jsstr;
    mozilla::Maybe<JSAutoCompartment> ac;
    if (fun) {
        ac.construct(cx, JS_GetFunctionObject(fun));
        jsstr = JS_DecompileFunction (cx, fun, 4);
    } else {
        JS::RootedScript script(cx, JSD_GetJSScript (mCx, mScript));
        ac.construct(cx, script);
        jsstr = JS_DecompileScript (cx, script, "ppscript", 4);
    }
    if (!jsstr)
        return NS_ERROR_FAILURE;

    size_t length;
    const jschar *chars = JS_GetStringCharsZAndLength(cx, jsstr, &length);
    if (!chars)
        return NS_ERROR_FAILURE;

    aFunctionSource = nsDependentString(chars, length);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetBaseLineNumber(uint32_t *_rval)
{
    *_rval = mBaseLineNumber;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetLineExtent(uint32_t *_rval)
{
    *_rval = mLineExtent;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetCallCount(uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptCallCount (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetMaxRecurseDepth(uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptMaxRecurseDepth (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetMinExecutionTime(double *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptMinExecutionTime (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetMaxExecutionTime(double *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptMaxExecutionTime (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetTotalExecutionTime(double *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptTotalExecutionTime (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetMinOwnExecutionTime(double *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptMinOwnExecutionTime (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetMaxOwnExecutionTime(double *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptMaxOwnExecutionTime (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetTotalOwnExecutionTime(double *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetScriptTotalOwnExecutionTime (mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::ClearProfileData()
{
    ASSERT_VALID_EPHEMERAL;
    JSD_ClearScriptProfileData(mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::PcToLine(uint32_t aPC, uint32_t aPcmap, uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    if (aPcmap == PCMAP_SOURCETEXT) {
        *_rval = JSD_GetClosestLine (mCx, mScript, mFirstPC + aPC);
    } else if (aPcmap == PCMAP_PRETTYPRINT) {
        *_rval = PPPcToLine(aPC);
    } else {
        return NS_ERROR_INVALID_ARG;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::LineToPc(uint32_t aLine, uint32_t aPcmap, uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    if (aPcmap == PCMAP_SOURCETEXT) {
        uintptr_t pc = JSD_GetClosestPC (mCx, mScript, aLine);
        *_rval = pc - mFirstPC;
    } else if (aPcmap == PCMAP_PRETTYPRINT) {
        *_rval = PPLineToPc(aLine);
    } else {
        return NS_ERROR_INVALID_ARG;
    }

    return NS_OK;
}

NS_IMETHODIMP
jsdScript::EnableSingleStepInterrupts(bool enable)
{
    ASSERT_VALID_EPHEMERAL;

    /* Must have set interrupt hook before enabling */
    if (enable && !jsdService::GetService()->CheckInterruptHook())
        return NS_ERROR_NOT_INITIALIZED;

    return (JSD_EnableSingleStepInterrupts(mCx, mScript, enable) ? NS_OK : NS_ERROR_FAILURE);
}

NS_IMETHODIMP
jsdScript::GetExecutableLines(uint32_t aPcmap, uint32_t aStartLine, uint32_t aMaxLines,
                              uint32_t* aCount, uint32_t** aExecutableLines)
{
    ASSERT_VALID_EPHEMERAL;
    if (aPcmap == PCMAP_SOURCETEXT) {
        uintptr_t start = JSD_GetClosestPC(mCx, mScript, 0);
        unsigned lastLine = JSD_GetScriptBaseLineNumber(mCx, mScript)
                       + JSD_GetScriptLineExtent(mCx, mScript) - 1;
        uintptr_t end = JSD_GetClosestPC(mCx, mScript, lastLine + 1);

        *aExecutableLines = static_cast<uint32_t*>(NS_Alloc((end - start + 1) * sizeof(uint32_t)));
        if (!JSD_GetLinePCs(mCx, mScript, aStartLine, aMaxLines, aCount, aExecutableLines,
                            nullptr))
            return NS_ERROR_OUT_OF_MEMORY;
        
        return NS_OK;
    }

    if (aPcmap == PCMAP_PRETTYPRINT) {
        if (!mPPLineMap) {
            if (!CreatePPLineMap())
                return NS_ERROR_OUT_OF_MEMORY;
        }

        nsTArray<uint32_t> lines;
        uint32_t i;

        for (i = 0; i < mPCMapSize; ++i) {
            if (mPPLineMap[i].line >= aStartLine)
                break;
        }

        for (; i < mPCMapSize && lines.Length() < aMaxLines; ++i) {
            lines.AppendElement(mPPLineMap[i].line);
        }

        if (aCount)
            *aCount = lines.Length();

        *aExecutableLines = static_cast<uint32_t*>(NS_Alloc(lines.Length() * sizeof(uint32_t)));
        if (!*aExecutableLines)
            return NS_ERROR_OUT_OF_MEMORY;

        for (i = 0; i < lines.Length(); ++i)
            (*aExecutableLines)[i] = lines[i];

        return NS_OK;
    }

    return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
jsdScript::IsLineExecutable(uint32_t aLine, uint32_t aPcmap, bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    if (aPcmap == PCMAP_SOURCETEXT) {    
        uintptr_t pc = JSD_GetClosestPC (mCx, mScript, aLine);
        *_rval = (aLine == JSD_GetClosestLine (mCx, mScript, pc));
    } else if (aPcmap == PCMAP_PRETTYPRINT) {
        if (!mPPLineMap && !CreatePPLineMap())
            return NS_ERROR_OUT_OF_MEMORY;
        *_rval = false;
        for (uint32_t i = 0; i < mPCMapSize; ++i) {
            if (mPPLineMap[i].line >= aLine) {
                *_rval = (mPPLineMap[i].line == aLine);
                break;
            }
        }
    } else {
        return NS_ERROR_INVALID_ARG;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::SetBreakpoint(uint32_t aPC)
{
    ASSERT_VALID_EPHEMERAL;
    uintptr_t pc = mFirstPC + aPC;
    JSD_SetExecutionHook (mCx, mScript, pc, jsds_ExecutionHookProc, nullptr);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::ClearBreakpoint(uint32_t aPC)
{
    ASSERT_VALID_EPHEMERAL;    
    uintptr_t pc = mFirstPC + aPC;
    JSD_ClearExecutionHook (mCx, mScript, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::ClearAllBreakpoints()
{
    ASSERT_VALID_EPHEMERAL;
    JSD_LockScriptSubsystem(mCx);
    JSD_ClearAllExecutionHooksForScript (mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

/* Contexts */
NS_IMPL_ISUPPORTS(jsdContext, jsdIContext, jsdIEphemeral)

jsdIContext *
jsdContext::FromPtr (JSDContext *aJSDCx, JSContext *aJSCx)
{
    if (!aJSDCx || !aJSCx)
        return nullptr;

    nsCOMPtr<jsdIContext> jsdicx;
    nsCOMPtr<jsdIEphemeral> eph = 
        jsds_FindEphemeral (&gLiveContexts, static_cast<void *>(aJSCx));
    if (eph)
    {
        jsdicx = do_QueryInterface(eph);
    }
    else
    {
        nsCOMPtr<nsISupports> iscx;
        if (JS::ContextOptionsRef(aJSCx).privateIsNSISupports())
            iscx = static_cast<nsISupports *>(JS_GetContextPrivate(aJSCx));
        jsdicx = new jsdContext (aJSDCx, aJSCx, iscx);
    }

    jsdIContext *ctx = nullptr;
    jsdicx.swap(ctx);
    return ctx;
}

jsdContext::jsdContext (JSDContext *aJSDCx, JSContext *aJSCx,
                        nsISupports *aISCx) : mValid(true),
                                              mScriptDisabledForWindowWithID(0),
                                              mTag(0),
                                              mJSDCx(aJSDCx),
                                              mJSCx(aJSCx), mISCx(aISCx)
{
    DEBUG_CREATE ("jsdContext", gContextCount);
    mLiveListEntry.value = this;
    mLiveListEntry.key   = static_cast<void *>(aJSCx);
    jsds_InsertEphemeral (&gLiveContexts, &mLiveListEntry);
}

jsdContext::~jsdContext() 
{
    DEBUG_DESTROY ("jsdContext", gContextCount);
    if (mValid)
    {
        /* call Invalidate() to take ourselves out of the live list */
        Invalidate();
    }
}

NS_IMETHODIMP
jsdContext::GetIsValid(bool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}

NS_IMETHODIMP
jsdContext::Invalidate()
{
    ASSERT_VALID_EPHEMERAL;
    mValid = false;
    jsds_RemoveEphemeral (&gLiveContexts, &mLiveListEntry);
    return NS_OK;
}

void
jsdContext::InvalidateAll()
{
    if (gLiveContexts)
        jsds_InvalidateAllEphemerals (&gLiveContexts);
}

NS_IMETHODIMP
jsdContext::GetJSContext(JSContext **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = mJSCx;
    return NS_OK;
}

/* Simulate the old options API in terms of the new one for backwards
 * compatibility */

#define JSOPTION_EXTRA_WARNINGS                 JS_BIT(0)
#define JSOPTION_WERROR                         JS_BIT(1)
#define JSOPTION_VAROBJFIX                      JS_BIT(2)
#define JSOPTION_PRIVATE_IS_NSISUPPORTS         JS_BIT(3)
#define JSOPTION_DONT_REPORT_UNCAUGHT           JS_BIT(8)
#define JSOPTION_NO_DEFAULT_COMPARTMENT_OBJECT  JS_BIT(11)
#define JSOPTION_NO_SCRIPT_RVAL                 JS_BIT(12)
#define JSOPTION_STRICT_MODE                    JS_BIT(17)
#define JSOPTION_MASK                           JS_BITMASK(20)

NS_IMETHODIMP
jsdContext::GetOptions(uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = (JS::ContextOptionsRef(mJSCx).extraWarnings() ? JSOPTION_EXTRA_WARNINGS : 0)
           | (JS::ContextOptionsRef(mJSCx).werror() ? JSOPTION_WERROR : 0)
           | (JS::ContextOptionsRef(mJSCx).varObjFix() ? JSOPTION_VAROBJFIX : 0)
           | (JS::ContextOptionsRef(mJSCx).privateIsNSISupports() ? JSOPTION_PRIVATE_IS_NSISUPPORTS : 0)
           | (JS::ContextOptionsRef(mJSCx).dontReportUncaught() ? JSOPTION_DONT_REPORT_UNCAUGHT : 0)
           | (JS::ContextOptionsRef(mJSCx).noDefaultCompartmentObject() ? JSOPTION_NO_DEFAULT_COMPARTMENT_OBJECT : 0)
           | (JS::ContextOptionsRef(mJSCx).noScriptRval() ? JSOPTION_NO_SCRIPT_RVAL : 0)
           | (JS::ContextOptionsRef(mJSCx).strictMode() ? JSOPTION_STRICT_MODE : 0);
    return NS_OK;
}

NS_IMETHODIMP
jsdContext::SetOptions(uint32_t options)
{
    ASSERT_VALID_EPHEMERAL;

    /* don't let users change this option, they'd just be shooting themselves
     * in the foot. */
    if (JS::ContextOptionsRef(mJSCx).privateIsNSISupports() !=
        !!(options & JSOPTION_PRIVATE_IS_NSISUPPORTS))
        return NS_ERROR_ILLEGAL_VALUE;

    JS::ContextOptionsRef(mJSCx).setExtraWarnings(options & JSOPTION_EXTRA_WARNINGS)
                                .setWerror(options & JSOPTION_WERROR)
                                .setVarObjFix(options & JSOPTION_VAROBJFIX)
                                .setDontReportUncaught(options & JSOPTION_DONT_REPORT_UNCAUGHT)
                                .setNoDefaultCompartmentObject(options & JSOPTION_NO_DEFAULT_COMPARTMENT_OBJECT)
                                .setNoScriptRval(options & JSOPTION_NO_SCRIPT_RVAL)
                                .setStrictMode(options & JSOPTION_STRICT_MODE);
    return NS_OK;
}

NS_IMETHODIMP
jsdContext::GetPrivateData(nsISupports **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    if (JS::ContextOptionsRef(mJSCx).privateIsNSISupports()) 
    {
        *_rval = static_cast<nsISupports*>(JS_GetContextPrivate(mJSCx));
        NS_IF_ADDREF(*_rval);
    }
    else
    {
        *_rval = nullptr;
    }

    return NS_OK;
}

NS_IMETHODIMP
jsdContext::GetWrappedContext(nsISupports **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    NS_IF_ADDREF(*_rval = mISCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdContext::GetTag(uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    if (!mTag)
        mTag = ++jsdContext::LastTag;
    
    *_rval = mTag;
    return NS_OK;
}

NS_IMETHODIMP
jsdContext::GetGlobalObject (jsdIValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSObject *glob = GetDefaultScopeFromJSContext(mJSCx);
    JSDValue *jsdv = JSD_NewValue (mJSDCx, OBJECT_TO_JSVAL(glob));
    if (!jsdv)
        return NS_ERROR_FAILURE;
    *_rval = jsdValue::FromPtr (mJSDCx, jsdv);
    if (!*_rval)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
jsdContext::GetScriptsEnabled (bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = IsScriptEnabled();
    return NS_OK;
}

NS_IMETHODIMP
jsdContext::SetScriptsEnabled (bool _rval)
{
    ASSERT_VALID_EPHEMERAL;
    if (_rval == IsScriptEnabled())
        return NS_OK;

    nsCOMPtr<nsIScriptContext> scx = do_QueryInterface(mISCx);
    NS_ENSURE_TRUE(scx && scx->GetWindowProxy(), NS_ERROR_NO_INTERFACE);
    nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(scx->GetGlobalObject());
    NS_ENSURE_TRUE(piWin, NS_ERROR_NO_INTERFACE);
    uint64_t currentWindowID = piWin->WindowID();

    if (_rval) {
        if (mScriptDisabledForWindowWithID != currentWindowID) {
            NS_WARNING("Please stop abusing JSD and fix your code!");
            return NS_ERROR_UNEXPECTED;
        }
        xpc::Scriptability::Get(scx->GetWindowProxy()).Unblock();
        piWin->ResumeTimeouts();
        mScriptDisabledForWindowWithID = 0;
    }
    else {
        piWin->SuspendTimeouts();
        xpc::Scriptability::Get(scx->GetWindowProxy()).Block();
        mScriptDisabledForWindowWithID = currentWindowID;
    }

    return NS_OK;
}

/* Stack Frames */
NS_IMPL_ISUPPORTS(jsdStackFrame, jsdIStackFrame, jsdIEphemeral)

jsdStackFrame::jsdStackFrame (JSDContext *aCx, JSDThreadState *aThreadState,
                              JSDStackFrameInfo *aStackFrameInfo) :
    mCx(aCx), mThreadState(aThreadState), mStackFrameInfo(aStackFrameInfo)
{
    DEBUG_CREATE ("jsdStackFrame", gFrameCount);
    mValid = (aCx && aThreadState && aStackFrameInfo);
    if (mValid) {
        mLiveListEntry.key = aStackFrameInfo;
        mLiveListEntry.value = this;
        jsds_InsertEphemeral (&gLiveStackFrames, &mLiveListEntry);
    }
}

jsdStackFrame::~jsdStackFrame() 
{
    DEBUG_DESTROY ("jsdStackFrame", gFrameCount);
    if (mValid)
    {
        /* call Invalidate() to take ourselves out of the live list */
        Invalidate();
    }
}

jsdIStackFrame *
jsdStackFrame::FromPtr (JSDContext *aCx, JSDThreadState *aThreadState,
                        JSDStackFrameInfo *aStackFrameInfo)
{
    if (!aStackFrameInfo)
        return nullptr;

    jsdIStackFrame *rv;
    nsCOMPtr<jsdIStackFrame> frame;

    nsCOMPtr<jsdIEphemeral> eph =
        jsds_FindEphemeral (&gLiveStackFrames,
                            reinterpret_cast<void *>(aStackFrameInfo));

    if (eph)
    {
        frame = do_QueryInterface(eph);
        rv = frame;
    }
    else
    {
        rv = new jsdStackFrame (aCx, aThreadState, aStackFrameInfo);
    }

    NS_IF_ADDREF(rv);
    return rv;
}

NS_IMETHODIMP
jsdStackFrame::Invalidate()
{
    ASSERT_VALID_EPHEMERAL;
    mValid = false;
    jsds_RemoveEphemeral (&gLiveStackFrames, &mLiveListEntry);
    return NS_OK;
}

void
jsdStackFrame::InvalidateAll()
{
    if (gLiveStackFrames)
        jsds_InvalidateAllEphemerals (&gLiveStackFrames);
}

NS_IMETHODIMP
jsdStackFrame::GetJSDContext(JSDContext **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDThreadState(JSDThreadState **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = mThreadState;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDStackFrameInfo(JSDStackFrameInfo **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = mStackFrameInfo;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetIsValid(bool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallingFrame(jsdIStackFrame **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDStackFrameInfo *sfi = JSD_GetCallingStackFrame (mCx, mThreadState,
                                                       mStackFrameInfo);
    *_rval = jsdStackFrame::FromPtr (mCx, mThreadState, sfi);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetExecutionContext(jsdIContext **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSContext *cx = JSD_GetJSContext (mCx, mThreadState);
    *_rval = jsdContext::FromPtr (mCx, cx);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetFunctionName(nsACString &_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSString *str = JSD_GetIdForStackFrame(mCx, mThreadState, mStackFrameInfo);
    if (str)
        return AssignToJSString(mCx, &_rval, str);
    
    _rval.AssignLiteral("anonymous");
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetIsDebugger(bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_IsStackFrameDebugger (mCx, mThreadState, mStackFrameInfo);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetIsConstructing(bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_IsStackFrameConstructing (mCx, mThreadState, mStackFrameInfo);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScript(jsdIScript **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    *_rval = jsdScript::FromPtr (mCx, script);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetPc(uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    if (!script)
        return NS_ERROR_FAILURE;
    uintptr_t pcbase = JSD_GetClosestPC(mCx, script, 0);
    
    uintptr_t pc = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
    if (pc)
        *_rval = pc - pcbase;
    else
        *_rval = pcbase;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetLine(uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    if (script) {
        uintptr_t pc = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
        *_rval = JSD_GetClosestLine (mCx, script, pc);
    } else {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallee(jsdIValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDValue *jsdv = JSD_GetCallObjectForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScope(jsdIValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDValue *jsdv = JSD_GetScopeChainForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetThisValue(jsdIValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDValue *jsdv = JSD_GetThisForStackFrame (mCx, mThreadState,
                                               mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}


NS_IMETHODIMP
jsdStackFrame::Eval (const nsAString &bytes, const nsACString &fileName,
                     uint32_t line, jsdIValue **result, bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;

    if (bytes.IsEmpty())
        return NS_ERROR_INVALID_ARG;

    // get pointer to buffer contained in |bytes|
    nsAString::const_iterator h;
    bytes.BeginReading(h);
    const jschar *char_bytes = reinterpret_cast<const jschar *>(h.get());

    JSExceptionState *estate = 0;

    AutoPushJSContext cx(JSD_GetJSContext (mCx, mThreadState));

    JS::RootedValue jv(cx);

    estate = JS_SaveExceptionState (cx);
    JS_ClearPendingException (cx);

    *_rval = JSD_AttemptUCScriptInStackFrame (mCx, mThreadState,
                                              mStackFrameInfo,
                                              char_bytes, bytes.Length(),
                                              PromiseFlatCString(fileName).get(),
                                              line, &jv);
    if (!*_rval) {
        if (JS_IsExceptionPending(cx))
            JS_GetPendingException (cx, &jv);
        else
            jv = JSVAL_NULL;
    }

    JS_RestoreExceptionState (cx, estate);

    JSDValue *jsdv = JSD_NewValue (mCx, jv);
    if (!jsdv)
        return NS_ERROR_FAILURE;
    *result = jsdValue::FromPtr (mCx, jsdv);
    if (!*result)
        return NS_ERROR_FAILURE;
    
    return NS_OK;
}        

/* Values */
NS_IMPL_ISUPPORTS(jsdValue, jsdIValue, jsdIEphemeral)
jsdIValue *
jsdValue::FromPtr (JSDContext *aCx, JSDValue *aValue)
{
    /* value will be dropped by te jsdValue destructor. */

    if (!aValue)
        return nullptr;
    
    jsdIValue *rv = new jsdValue (aCx, aValue);
    NS_IF_ADDREF(rv);
    return rv;
}

jsdValue::jsdValue (JSDContext *aCx, JSDValue *aValue) : mValid(true),
                                                         mCx(aCx), 
                                                         mValue(aValue)
{
    DEBUG_CREATE ("jsdValue", gValueCount);
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
jsdValue::GetIsValid(bool *_rval)
{
    *_rval = mValid;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::Invalidate()
{
    ASSERT_VALID_EPHEMERAL;
    mValid = false;
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
    ASSERT_VALID_EPHEMERAL;
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJSDValue (JSDValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = mValue;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNative (bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_IsValueNative (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNumber (bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_IsValueNumber (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsPrimitive (bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_IsValuePrimitive (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsType (uint32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JS::RootedValue val(JSD_GetJSRuntime(mCx), JSD_GetValueWrappedJSVal (mCx, mValue));

    if (val.isNull())
        *_rval = TYPE_NULL;
    else if (val.isBoolean())
        *_rval = TYPE_BOOLEAN;
    else if (val.isDouble())
        *_rval = TYPE_DOUBLE;
    else if (val.isInt32())
        *_rval = TYPE_INT;
    else if (val.isString())
        *_rval = TYPE_STRING;
    else if (val.isUndefined())
        *_rval = TYPE_VOID;
    else if (JSD_IsValueFunction (mCx, mValue))
        *_rval = TYPE_FUNCTION;
    else if (!val.isPrimitive())
        *_rval = TYPE_OBJECT;
    else
        NS_ASSERTION (0, "Value has no discernible type.");

    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsPrototype (jsdIValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDValue *jsdv = JSD_GetValuePrototype (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsParent (jsdIValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDValue *jsdv = JSD_GetValueParent (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsClassName(nsACString &_rval)
{
    ASSERT_VALID_EPHEMERAL;
    _rval.Assign(JSD_GetValueClassName(mCx, mValue));
    
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsConstructor (jsdIValue **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDValue *jsdv = JSD_GetValueConstructor (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsFunctionName(nsACString &_rval)
{
    ASSERT_VALID_EPHEMERAL;
    return AssignToJSString(mCx, &_rval, JSD_GetValueFunctionId(mCx, mValue));
}

NS_IMETHODIMP
jsdValue::GetBooleanValue(bool *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetValueBoolean (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetDoubleValue(double *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetValueDouble (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIntValue(int32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    *_rval = JSD_GetValueInt (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetObjectValue(jsdIObject **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDObject *obj;
    obj = JSD_GetObjectForValue (mCx, mValue);
    *_rval = jsdObject::FromPtr (mCx, obj);
    if (!*_rval)
        return NS_ERROR_FAILURE;
    return NS_OK;
}
    
NS_IMETHODIMP
jsdValue::GetStringValue(nsACString &_rval)
{
    ASSERT_VALID_EPHEMERAL;
    AutoSafeJSContext cx;
    JSString *jstr_val = JSD_GetValueString(mCx, mValue);
    if (jstr_val) {
        size_t length;
        const jschar *chars = JS_GetStringCharsZAndLength(cx, jstr_val, &length);
        if (!chars)
            return NS_ERROR_FAILURE;
        nsDependentString depStr(chars, length);
        CopyUTF16toUTF8(depStr, _rval);
    } else {
        _rval.Truncate();
    }
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetPropertyCount (int32_t *_rval)
{
    ASSERT_VALID_EPHEMERAL;
    if (JSD_IsValueObject(mCx, mValue))
        *_rval = JSD_GetCountOfProperties (mCx, mValue);
    else
        *_rval = -1;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetProperties (jsdIProperty ***propArray, uint32_t *length)
{
    ASSERT_VALID_EPHEMERAL;
    *propArray = nullptr;
    if (length)
        *length = 0;

    uint32_t prop_count = JSD_IsValueObject(mCx, mValue)
        ? JSD_GetCountOfProperties (mCx, mValue)
        : 0;
    NS_ENSURE_TRUE(prop_count, NS_OK);

    jsdIProperty **pa_temp =
        static_cast<jsdIProperty **>
                   (nsMemory::Alloc(sizeof (jsdIProperty *) * 
                                       prop_count));
    NS_ENSURE_TRUE(pa_temp, NS_ERROR_OUT_OF_MEMORY);

    uint32_t     i    = 0;
    JSDProperty *iter = nullptr;
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
jsdValue::GetProperty (const nsACString &name, jsdIProperty **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    AutoSafeJSContext cx;
    JSAutoCompartment ac(cx, JSD_GetDefaultGlobal (mCx)); // Just in case.

    /* not rooting this */
    JSString *jstr_name = JS_NewStringCopyZ(cx, PromiseFlatCString(name).get());
    if (!jstr_name)
        return NS_ERROR_OUT_OF_MEMORY;

    JSDProperty *prop = JSD_GetValueProperty (mCx, mValue, jstr_name);
    
    *_rval = jsdProperty::FromPtr (mCx, prop);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::Refresh()
{
    ASSERT_VALID_EPHEMERAL;
    JSD_RefreshValue (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetWrappedValue(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval)
{
    ASSERT_VALID_EPHEMERAL;

    aRetval.set(JSD_GetValueWrappedJSVal(mCx, mValue));
    if (!JS_WrapValue(aCx, aRetval))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetScript(jsdIScript **_rval)
{
    ASSERT_VALID_EPHEMERAL;
    JSDScript *script = JSD_GetScriptForValue(mCx, mValue);
    *_rval = jsdScript::FromPtr(mCx, script);
    return NS_OK;
}

/******************************************************************************
 * debugger service implementation
 ******************************************************************************/

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(jsdService)
  NS_INTERFACE_MAP_ENTRY(jsdIDebuggerService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, jsdIDebuggerService)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(jsdService,
                         mErrorHook, mBreakpointHook, mDebugHook,
                         mDebuggerHook, mInterruptHook, mScriptHook,
                         mThrowHook, mTopLevelHook, mFunctionHook,
                         mActivationCallback)
NS_IMPL_CYCLE_COLLECTING_ADDREF(jsdService)
NS_IMPL_CYCLE_COLLECTING_RELEASE(jsdService)

NS_IMETHODIMP
jsdService::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetFlags (uint32_t *_rval)
{
    ASSERT_VALID_CONTEXT;
    *_rval = JSD_GetContextFlags (mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetFlags (uint32_t flags)
{
    ASSERT_VALID_CONTEXT;
    JSD_SetContextFlags (mCx, flags);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetImplementationString(nsACString &aImplementationString)
{
    aImplementationString.AssignLiteral(implementationString);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetImplementationMajor(uint32_t *_rval)
{
    *_rval = JSDS_MAJOR_VERSION;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetImplementationMinor(uint32_t *_rval)
{
    *_rval = JSDS_MINOR_VERSION;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetIsOn (bool *_rval)
{
    *_rval = mOn;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::On (void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
jsdService::AsyncOn (jsdIActivationCallback *activationCallback)
{
    nsresult  rv;

    // Warn that JSD is deprecated, unless the caller has told us
    // that they know already.
    if (mDeprecationAcknowledged) {
        mDeprecationAcknowledged = false;
    } else if (!mWarnedAboutDeprecation) {
        // In any case, warn only once.
        mWarnedAboutDeprecation = true;

        // Ignore errors: simply being unable to print the message
        // shouldn't (effectively) disable JSD.
        nsContentUtils::ReportToConsoleNonLocalized(
            NS_LITERAL_STRING("\
The jsdIDebuggerService and its associated interfaces are deprecated. \
Please use Debugger, via IJSDebugger, instead."),
            nsIScriptError::warningFlag,
            NS_LITERAL_CSTRING("JSD"),
            nullptr);
    }

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_FAILED(rv)) return rv;

    mActivationCallback = activationCallback;

    return xpc->SetDebugModeWhenPossible(true, true);
}

NS_IMETHODIMP
jsdService::RecompileForDebugMode (JSContext *cx, JSCompartment *comp, bool mode) {
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  /* XPConnect now does this work itself, so this IDL entry point is no longer used. */
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
jsdService::DeactivateDebugger ()
{
    if (!mCx)
        return NS_OK;

    jsdContext::InvalidateAll();
    jsdScript::InvalidateAll();
    jsdValue::InvalidateAll();
    jsdProperty::InvalidateAll();
    jsdStackFrame::InvalidateAll();
    ClearAllBreakpoints();

    JSD_SetErrorReporter (mCx, nullptr, nullptr);
    JSD_SetScriptHook (mCx, nullptr, nullptr);
    JSD_ClearThrowHook (mCx);
    JSD_ClearInterruptHook (mCx);
    JSD_ClearDebuggerHook (mCx);
    JSD_ClearDebugBreakHook (mCx);
    JSD_ClearTopLevelHook (mCx);
    JSD_ClearFunctionHook (mCx);

    JSD_DebuggerOff (mCx);

    mCx = nullptr;
    mRuntime = nullptr;
    mOn = false;

    return NS_OK;
}


NS_IMETHODIMP
jsdService::ActivateDebugger (JSRuntime *rt)
{
    if (mOn)
        return (rt == mRuntime) ? NS_OK : NS_ERROR_ALREADY_INITIALIZED;

    mRuntime = rt;

    if (gPrevGCSliceCallback == jsds_GCSliceCallbackProc)
        /* condition indicates that the callback proc has not been set yet */
        gPrevGCSliceCallback = JS::SetGCSliceCallback (rt, jsds_GCSliceCallbackProc);

    mCx = JSD_DebuggerOnForUser (rt, nullptr, nullptr);
    if (!mCx)
        return NS_ERROR_FAILURE;

    AutoSafeJSContext cx;
    JS::RootedObject glob(cx, JSD_GetDefaultGlobal (mCx));
    JSAutoCompartment ac(cx, glob);

    /* init xpconnect on the debugger's context in case xpconnect tries to
     * use it for stuff. */
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_FAILED(rv))
        return rv;

    xpc->InitClasses (cx, glob);

    /* Start watching for script creation/destruction and manage jsdScript
     * objects accordingly
     */
    JSD_SetScriptHook (mCx, jsds_ScriptHookProc, nullptr);

    /* If any of these mFooHook objects are installed, do the required JSD
     * hookup now.   See also, jsdService::SetFooHook().
     */
    if (mErrorHook)
        JSD_SetErrorReporter (mCx, jsds_ErrorHookProc, nullptr);
    if (mThrowHook)
        JSD_SetThrowHook (mCx, jsds_ExecutionHookProc, nullptr);
    /* can't ignore script callbacks, as we need to |Release| the wrapper 
     * stored in private data when a script is deleted. */
    if (mInterruptHook)
        JSD_SetInterruptHook (mCx, jsds_ExecutionHookProc, nullptr);
    if (mDebuggerHook)
        JSD_SetDebuggerHook (mCx, jsds_ExecutionHookProc, nullptr);
    if (mDebugHook)
        JSD_SetDebugBreakHook (mCx, jsds_ExecutionHookProc, nullptr);
    if (mTopLevelHook)
        JSD_SetTopLevelHook (mCx, jsds_CallHookProc, nullptr);
    else
        JSD_ClearTopLevelHook (mCx);
    if (mFunctionHook)
        JSD_SetFunctionHook (mCx, jsds_CallHookProc, nullptr);
    else
        JSD_ClearFunctionHook (mCx);
    mOn = true;

#ifdef DEBUG
    printf ("+++ JavaScript debugging hooks installed.\n");
#endif

    nsCOMPtr<jsdIActivationCallback> activationCallback;
    mActivationCallback.swap(activationCallback);
    if (activationCallback)
        return activationCallback->OnDebuggerActivated();

    return NS_OK;
}

NS_IMETHODIMP
jsdService::Off (void)
{
    if (!mOn)
        return NS_OK;
    
    if (!mCx || !mRuntime)
        return NS_ERROR_NOT_INITIALIZED;
    
    if (gDeadScripts) {
        if (gGCRunning)
            return NS_ERROR_NOT_AVAILABLE;

        while (gDeadScripts)
            jsds_NotifyPendingDeadScripts (JS_GetRuntime(nsContentUtils::GetSafeJSContext()));
    }

    DeactivateDebugger();

#ifdef DEBUG
    printf ("+++ JavaScript debugging hooks removed.\n");
#endif

    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_FAILED(rv))
        return rv;

    xpc->SetDebugModeWhenPossible(false, true);

    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetPauseDepth(uint32_t *_rval)
{
    NS_ENSURE_ARG_POINTER(_rval);
    *_rval = mPauseLevel;
    return NS_OK;
}
    
NS_IMETHODIMP
jsdService::Pause(uint32_t *_rval)
{
    return DoPause(_rval, false);
}

nsresult
jsdService::DoPause(uint32_t *_rval, bool internalCall)
{
    if (!mCx)
        return NS_ERROR_NOT_INITIALIZED;

    if (++mPauseLevel == 1) {
        JSD_SetErrorReporter (mCx, nullptr, nullptr);
        JSD_ClearThrowHook (mCx);
        JSD_ClearInterruptHook (mCx);
        JSD_ClearDebuggerHook (mCx);
        JSD_ClearDebugBreakHook (mCx);
        JSD_ClearTopLevelHook (mCx);
        JSD_ClearFunctionHook (mCx);
        JSD_DebuggerPause (mCx);

        nsresult rv;
        nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
        if (NS_FAILED(rv)) return rv;

        if (!internalCall) {
            rv = xpc->SetDebugModeWhenPossible(false, false);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    if (_rval)
        *_rval = mPauseLevel;

    return NS_OK;
}

NS_IMETHODIMP
jsdService::UnPause(uint32_t *_rval)
{
    return DoUnPause(_rval, false);
}

nsresult
jsdService::DoUnPause(uint32_t *_rval, bool internalCall)
{
    if (!mCx)
        return NS_ERROR_NOT_INITIALIZED;

    if (mPauseLevel == 0)
        return NS_ERROR_NOT_AVAILABLE;

    /* check mOn before we muck with this stuff, it's possible the debugger
     * was turned off while we were paused.
     */
    if (--mPauseLevel == 0 && mOn) {
        JSD_DebuggerUnpause (mCx);
        if (mErrorHook)
            JSD_SetErrorReporter (mCx, jsds_ErrorHookProc, nullptr);
        if (mThrowHook)
            JSD_SetThrowHook (mCx, jsds_ExecutionHookProc, nullptr);
        if (mInterruptHook)
            JSD_SetInterruptHook (mCx, jsds_ExecutionHookProc, nullptr);
        if (mDebuggerHook)
            JSD_SetDebuggerHook (mCx, jsds_ExecutionHookProc, nullptr);
        if (mDebugHook)
            JSD_SetDebugBreakHook (mCx, jsds_ExecutionHookProc, nullptr);
        if (mTopLevelHook)
            JSD_SetTopLevelHook (mCx, jsds_CallHookProc, nullptr);
        else
            JSD_ClearTopLevelHook (mCx);
        if (mFunctionHook)
            JSD_SetFunctionHook (mCx, jsds_CallHookProc, nullptr);
        else
            JSD_ClearFunctionHook (mCx);

        nsresult rv;
        nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
        if (NS_FAILED(rv)) return rv;

        if (!internalCall) {
            rv = xpc->SetDebugModeWhenPossible(true, false);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }
    
    if (_rval)
        *_rval = mPauseLevel;

    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnumerateContexts (jsdIContextEnumerator *enumerator)
{
    ASSERT_VALID_CONTEXT;
    
    if (!enumerator)
        return NS_OK;
    
    JSContext *iter = nullptr;
    JSContext *cx;

    while ((cx = JS_ContextIterator (mRuntime, &iter)))
    {
        nsCOMPtr<jsdIContext> jsdicx = 
            dont_AddRef(jsdContext::FromPtr(mCx, cx));
        if (jsdicx)
        {
            if (NS_FAILED(enumerator->EnumerateContext(jsdicx)))
                break;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnumerateScripts (jsdIScriptEnumerator *enumerator)
{
    ASSERT_VALID_CONTEXT;
    
    JSDScript *script;
    JSDScript *iter = nullptr;
    nsresult rv = NS_OK;
    
    JSD_LockScriptSubsystem(mCx);
    while((script = JSD_IterateScripts(mCx, &iter))) {
        nsCOMPtr<jsdIScript> jsdis =
            dont_AddRef(jsdScript::FromPtr(mCx, script));
        rv = enumerator->EnumerateScript (jsdis);
        if (NS_FAILED(rv))
            break;
    }
    JSD_UnlockScriptSubsystem(mCx);

    return rv;
}

NS_IMETHODIMP
jsdService::GC (void)
{
    ASSERT_VALID_CONTEXT;
    JSRuntime *rt = JSD_GetJSRuntime (mCx);
    JS_GC(rt);
    return NS_OK;
}
    
NS_IMETHODIMP
jsdService::DumpHeap(const nsACString &fileName)
{
    ASSERT_VALID_CONTEXT;
#ifndef DEBUG
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsresult rv = NS_OK;
    FILE *file = !fileName.IsEmpty() ? fopen(PromiseFlatCString(fileName).get(), "w") : stdout;
    if (!file) {
        rv = NS_ERROR_FAILURE;
    } else {
        if (!JS_DumpHeap(JS_GetRuntime(nsContentUtils::GetSafeJSContext()),
                         file, nullptr, JSTRACE_OBJECT, nullptr, (size_t)-1, nullptr))
            rv = NS_ERROR_FAILURE;
        if (file != stdout)
            fclose(file);
    }
    return rv;
#endif
}

NS_IMETHODIMP
jsdService::ClearProfileData ()
{
    ASSERT_VALID_CONTEXT;
    JSD_ClearAllProfileData (mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::InsertFilter (jsdIFilter *filter, jsdIFilter *after)
{
    NS_ENSURE_ARG_POINTER (filter);
    if (jsds_FindFilter (filter))
        return NS_ERROR_INVALID_ARG;

    FilterRecord *rec = PR_NEWZAP (FilterRecord);
    if (!rec)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!jsds_SyncFilter (rec, filter)) {
        PR_Free (rec);
        return NS_ERROR_FAILURE;
    }
    
    if (gFilters) {
        if (!after) {
            /* insert at head of list */
            PR_INSERT_LINK(&rec->links, &gFilters->links);
            gFilters = rec;
        } else {
            /* insert somewhere in the list */
            FilterRecord *afterRecord = jsds_FindFilter (after);
            if (!afterRecord) {
                jsds_FreeFilter(rec);
                return NS_ERROR_INVALID_ARG;
            }
            PR_INSERT_AFTER(&rec->links, &afterRecord->links);
        }
    } else {
        if (after) {
            /* user asked to insert into the middle of an empty list, bail. */
            jsds_FreeFilter(rec);
            return NS_ERROR_NOT_INITIALIZED;
        }
        PR_INIT_CLIST(&rec->links);
        gFilters = rec;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::AppendFilter (jsdIFilter *filter)
{
    NS_ENSURE_ARG_POINTER (filter);
    if (jsds_FindFilter (filter))
        return NS_ERROR_INVALID_ARG;
    FilterRecord *rec = PR_NEWZAP (FilterRecord);

    if (!jsds_SyncFilter (rec, filter)) {
        PR_Free (rec);
        return NS_ERROR_FAILURE;
    }
    
    if (gFilters) {
        PR_INSERT_BEFORE(&rec->links, &gFilters->links);
    } else {
        PR_INIT_CLIST(&rec->links);
        gFilters = rec;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::RemoveFilter (jsdIFilter *filter)
{
    NS_ENSURE_ARG_POINTER(filter);
    FilterRecord *rec = jsds_FindFilter (filter);
    if (!rec)
        return NS_ERROR_INVALID_ARG;
    
    if (gFilters == rec) {
        gFilters = reinterpret_cast<FilterRecord *>
                                   (PR_NEXT_LINK(&rec->links));
        /* If we're the only filter left, null out the list head. */
        if (gFilters == rec)
            gFilters = nullptr;
    }

    
    PR_REMOVE_LINK(&rec->links);
    jsds_FreeFilter (rec);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SwapFilters (jsdIFilter *filter_a, jsdIFilter *filter_b)
{
    NS_ENSURE_ARG_POINTER(filter_a);
    NS_ENSURE_ARG_POINTER(filter_b);
    
    FilterRecord *rec_a = jsds_FindFilter (filter_a);
    if (!rec_a)
        return NS_ERROR_INVALID_ARG;
    
    if (filter_a == filter_b) {
        /* just a refresh */
        if (!jsds_SyncFilter (rec_a, filter_a))
            return NS_ERROR_FAILURE;
        return NS_OK;
    }
    
    FilterRecord *rec_b = jsds_FindFilter (filter_b);
    if (!rec_b) {
        /* filter_b is not in the list, replace filter_a with filter_b. */
        if (!jsds_SyncFilter (rec_a, filter_b))
            return NS_ERROR_FAILURE;
    } else {
        /* both filters are in the list, swap. */
        if (!jsds_SyncFilter (rec_a, filter_b))
            return NS_ERROR_FAILURE;
        if (!jsds_SyncFilter (rec_b, filter_a))
            return NS_ERROR_FAILURE;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnumerateFilters (jsdIFilterEnumerator *enumerator) 
{
    if (!gFilters)
        return NS_OK;
    
    FilterRecord *current = gFilters;
    do {
        jsds_SyncFilter (current, current->filterObject);
        /* SyncFilter failure would be bad, but what would we do about it? */
        if (enumerator) {
            nsresult rv = enumerator->EnumerateFilter (current->filterObject);
            if (NS_FAILED(rv))
                return rv;
        }
        current = reinterpret_cast<FilterRecord *>
                                  (PR_NEXT_LINK (&current->links));
    } while (current != gFilters);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::RefreshFilters ()
{
    return EnumerateFilters(nullptr);
}

NS_IMETHODIMP
jsdService::ClearFilters ()
{
    if (!gFilters)
        return NS_OK;

    FilterRecord *current = reinterpret_cast<FilterRecord *>
                                            (PR_NEXT_LINK (&gFilters->links));
    do {
        FilterRecord *next = reinterpret_cast<FilterRecord *>
                                             (PR_NEXT_LINK (&current->links));
        PR_REMOVE_AND_INIT_LINK(&current->links);
        jsds_FreeFilter(current);
        current = next;
    } while (current != gFilters);
    
    jsds_FreeFilter(current);
    gFilters = nullptr;
    
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
jsdService::WrapValue(JS::Handle<JS::Value> value, jsdIValue **_rval)
{
    ASSERT_VALID_CONTEXT;
    JSDValue *jsdv = JSD_NewValue(mCx, value);
    if (!jsdv)
        return NS_ERROR_FAILURE;
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}


NS_IMETHODIMP
jsdService::EnterNestedEventLoop (jsdINestCallback *callback, uint32_t *_rval)
{
    // Nesting event queues is a thing of the past.  Now, we just spin the
    // current event loop.
    nsresult rv = NS_OK;
    AutoNoJSAPI nojsapi;
    uint32_t nestLevel = ++mNestedLoopLevel;
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

    if (callback) {
        DoPause(nullptr, true);
        rv = callback->OnNest();
        DoUnPause(nullptr, true);
    }

    while (NS_SUCCEEDED(rv) && mNestedLoopLevel >= nestLevel) {
        if (!NS_ProcessNextEvent(thread))
            rv = NS_ERROR_UNEXPECTED;
    }

    NS_ASSERTION (mNestedLoopLevel <= nestLevel,
                  "nested event didn't unwind properly");
    if (mNestedLoopLevel == nestLevel)
        --mNestedLoopLevel;

    *_rval = mNestedLoopLevel;
    return rv;
}

NS_IMETHODIMP
jsdService::ExitNestedEventLoop (uint32_t *_rval)
{
    if (mNestedLoopLevel > 0)
        --mNestedLoopLevel;
    else
        return NS_ERROR_FAILURE;

    *_rval = mNestedLoopLevel;    
    return NS_OK;
}    

NS_IMETHODIMP
jsdService::AcknowledgeDeprecation()
{
    mDeprecationAcknowledged = true;
    return NS_OK;
}

/* hook attribute get/set functions */

NS_IMETHODIMP
jsdService::SetErrorHook (jsdIErrorHook *aHook)
{
    mErrorHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetErrorReporter (mCx, jsds_ErrorHookProc, nullptr);
    else
        JSD_SetErrorReporter (mCx, nullptr, nullptr);

    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetErrorHook (jsdIErrorHook **aHook)
{
    *aHook = mErrorHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

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
jsdService::SetDebugHook (jsdIExecutionHook *aHook)
{    
    mDebugHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetDebugBreakHook (mCx, jsds_ExecutionHookProc, nullptr);
    else
        JSD_ClearDebugBreakHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetDebugHook (jsdIExecutionHook **aHook)
{   
    *aHook = mDebugHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetDebuggerHook (jsdIExecutionHook *aHook)
{    
    mDebuggerHook = aHook;

    /* if the debugger isn't initialized, that's all we can do for now.  The
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetDebuggerHook (mCx, jsds_ExecutionHookProc, nullptr);
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
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetInterruptHook (mCx, jsds_ExecutionHookProc, nullptr);
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
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;
    
    if (aHook)
        JSD_SetScriptHook (mCx, jsds_ScriptHookProc, nullptr);
    /* we can't unset it if !aHook, because we still need to see script
     * deletes in order to Release the jsdIScripts held in JSDScript
     * private data. */
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
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetThrowHook (mCx, jsds_ExecutionHookProc, nullptr);
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
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetTopLevelHook (mCx, jsds_CallHookProc, nullptr);
    else
        JSD_ClearTopLevelHook (mCx);
    
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
     * ActivateDebugger() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetFunctionHook (mCx, jsds_CallHookProc, nullptr);
    else
        JSD_ClearFunctionHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetFunctionHook (jsdICallHook **aHook)
{   
    *aHook = mFunctionHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

/* virtual */
jsdService::~jsdService()
{
    ClearFilters();
    mErrorHook = nullptr;
    mBreakpointHook = nullptr;
    mDebugHook = nullptr;
    mDebuggerHook = nullptr;
    mInterruptHook = nullptr;
    mScriptHook = nullptr;
    mThrowHook = nullptr;
    mTopLevelHook = nullptr;
    mFunctionHook = nullptr;
    Off();
    gJsds = nullptr;
}

jsdService *
jsdService::GetService ()
{
    if (!gJsds)
        gJsds = new jsdService();
        
    NS_IF_ADDREF(gJsds);
    return gJsds;
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(jsdService, jsdService::GetService)

/* app-start observer.  turns on the debugger at app-start.  this is inserted
 * and/or removed from the app-start category by the jsdService::initAtStartup
 * property.
 */
class jsdASObserver MOZ_FINAL : public nsIObserver
{
    ~jsdASObserver () {}
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOBSERVER

    jsdASObserver () {}
};

NS_IMPL_ISUPPORTS(jsdASObserver, nsIObserver)

NS_IMETHODIMP
jsdASObserver::Observe (nsISupports *aSubject, const char *aTopic,
                        const char16_t *aData)
{
    nsresult rv;

    // Hmm.  Why is the app-startup observer called multiple times?
    //NS_ASSERTION(!gJsds, "app startup observer called twice");
    nsCOMPtr<jsdIDebuggerService> jsds = do_GetService(jsdServiceCtrID, &rv);
    if (NS_FAILED(rv))
        return rv;

    bool on;
    rv = jsds->GetIsOn(&on);
    if (NS_FAILED(rv) || on)
        return rv;
    
    nsCOMPtr<nsIJSRuntimeService> rts = do_GetService(NS_JSRT_CTRID, &rv);
    if (NS_FAILED(rv))
        return rv;    

    JSRuntime *rt;
    rts->GetRuntime (&rt);
    if (NS_FAILED(rv))
        return rv;

    rv = jsds->ActivateDebugger(rt);
    if (NS_FAILED(rv))
        return rv;
    
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(jsdASObserver)
NS_DEFINE_NAMED_CID(JSDSERVICE_CID);
NS_DEFINE_NAMED_CID(JSDASO_CID);

static const mozilla::Module::CIDEntry kJSDCIDs[] = {
    { &kJSDSERVICE_CID, false, nullptr, jsdServiceConstructor },
    { &kJSDASO_CID, false, nullptr, jsdASObserverConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kJSDContracts[] = {
    { jsdServiceCtrID, &kJSDSERVICE_CID },
    { jsdARObserverCtrID, &kJSDASO_CID },
    { nullptr }
};

static const mozilla::Module kJSDModule = {
    mozilla::Module::kVersion,
    kJSDCIDs,
    kJSDContracts
};

NSMODULE_DEFN(JavaScript_Debugger) = &kJSDModule;

void
global_finalize(JSFreeOp *aFop, JSObject *aObj)
{
    nsIScriptObjectPrincipal *sop =
        static_cast<nsIScriptObjectPrincipal *>(js::GetObjectPrivate(aObj));
    MOZ_ASSERT(sop);
    static_cast<SandboxPrivate *>(sop)->ForgetGlobalObject();
    NS_IF_RELEASE(sop);
}

JSObject *
CreateJSDGlobal(JSContext *aCx, const JSClass *aClasp)
{
    nsresult rv;
    nsCOMPtr<nsIPrincipal> nullPrin = do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
    NS_ENSURE_SUCCESS(rv, nullptr);

    JSPrincipals *jsPrin = nsJSPrincipals::get(nullPrin);
    JS::RootedObject global(aCx, JS_NewGlobalObject(aCx, aClasp, jsPrin, JS::DontFireOnNewGlobalHook));
    NS_ENSURE_TRUE(global, nullptr);

    // We have created a new global let's attach a private to it
    // that implements nsIGlobalObject.
    nsCOMPtr<nsIScriptObjectPrincipal> sbp =
        new SandboxPrivate(nullPrin, global);
    JS_SetPrivate(global, sbp.forget().take());

    JS_FireOnNewGlobalObject(aCx, global);

    return global;
}

/********************************************************************************
 ********************************************************************************
 * graveyard
 */

#if 0
/* Thread States */
NS_IMPL_ISUPPORTS(jsdThreadState, jsdIThreadState); 

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
jsdThreadState::GetFrameCount (uint32_t *_rval)
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
