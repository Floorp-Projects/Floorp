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
#include "nsIScriptGlobalObject.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
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
{ /* 2fd6b7f6-eb8c-4f32-ad26-113f2c02d0fe */         \
     0x2fd6b7f6,                                     \
     0xeb8c,                                         \
     0x4f32,                                         \
    {0xad, 0x26, 0x11, 0x3f, 0x2c, 0x02, 0xd0, 0xfe} \
}

#define NS_CATMAN_CTRID   "@mozilla.org/categorymanager;1"
#define NS_JSRT_CTRID     "@mozilla.org/js/xpc/RuntimeService;1"

#define AUTOREG_CATEGORY  "xpcom-autoregistration"
#define APPSTART_CATEGORY "app-startup"
#define JSD_STARTUP_ENTRY "JSDebugger Startup Observer"

JS_STATIC_DLL_CALLBACK (JSBool)
jsds_GCCallbackProc (JSContext *cx, JSGCStatus status);

/*******************************************************************************
 * global vars
 *******************************************************************************/

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

const char jsdServiceCtrID[] = "@mozilla.org/js/jsd/debugger-service;1";
const char jsdASObserverCtrID[] = "@mozilla.org/js/jsd/app-start-observer;2";

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
    void        *glob;
    char        *urlPattern;    
    PRUint32     patternLength;
    PatternType  patternType;
    PRUint32     startLine;
    PRUint32     endLine;
} *gFilters = nsnull;

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
 * utility functions for filters
 *******************************************************************************/
void
jsds_FreeFilter (FilterRecord *filter)
{
    NS_IF_RELEASE (filter->filterObject);
    if (filter->urlPattern)
        nsMemory::Free(filter->urlPattern);
    PR_Free (filter);
}

/* copies appropriate |filter| attributes into |rec|.
 * False return indicates failure, the contents of |rec| will not be changed.
 */
PRBool
jsds_SyncFilter (FilterRecord *rec, jsdIFilter *filter)
{
    NS_ASSERTION (rec, "jsds_SyncFilter without rec");
    NS_ASSERTION (filter, "jsds_SyncFilter without filter");
    
    JSObject *glob_proper = nsnull;
    nsCOMPtr<nsISupports> glob;
    nsresult rv = filter->GetGlob(getter_AddRefs(glob));
    if (NS_FAILED(rv))
        return PR_FALSE;
    if (glob) {
        nsCOMPtr<nsIScriptGlobalObject> nsiglob = do_QueryInterface(glob);
        if (nsiglob)
            glob_proper = nsiglob->GetGlobalJSObject();
    }
    
    PRUint32 startLine;
    rv = filter->GetStartLine(&startLine);
    if (NS_FAILED(rv))
        return PR_FALSE;

    PRUint32 endLine;
    rv = filter->GetStartLine(&endLine);
    if (NS_FAILED(rv))
        return PR_FALSE;    

    char *urlPattern;
    rv = filter->GetUrlPattern (&urlPattern);
    if (NS_FAILED(rv))
        return PR_FALSE;
    
    if (urlPattern) {
        PRUint32 len = PL_strlen(urlPattern);
        if (urlPattern[0] == '*') {
            /* pattern starts with a *, shift all chars once to the left,
             * including the trailing null. */
            memmove (&urlPattern[0], &urlPattern[1], len);

            if (urlPattern[len - 2] == '*') {
                /* pattern is in the format "*foo*", overwrite the final * with
                 * a null. */
                urlPattern[len - 2] = '\0';
                rec->patternType = ptContains;
                rec->patternLength = len - 2;
            } else {
                /* pattern is in the format "*foo", just make a note of the
                 * new length. */
                rec->patternType = ptEndsWith;
                rec->patternLength = len - 1;
            }
        } else if (urlPattern[len - 1] == '*') {
            /* pattern is in the format "foo*", overwrite the final * with a 
             * null. */
            urlPattern[len - 1] = '\0';
            rec->patternType = ptStartsWith;
            rec->patternLength = len - 1;
        } else {
            /* pattern is in the format "foo". */
            rec->patternType = ptEquals;
            rec->patternLength = len;
        }
    } else {
        rec->patternType = ptIgnore;
        rec->patternLength = 0;
    }

    /* we got everything we need without failing, now copy it into rec. */

    if (rec->filterObject != filter) {
        NS_IF_RELEASE(rec->filterObject);
        NS_ADDREF(filter);
        rec->filterObject = filter;
    }
    
    rec->glob = glob_proper;
    
    rec->startLine     = startLine;
    rec->endLine       = endLine;
    
    if (rec->urlPattern)
        nsMemory::Free (rec->urlPattern);
    rec->urlPattern = urlPattern;

    return PR_TRUE;
            
}

FilterRecord *
jsds_FindFilter (jsdIFilter *filter)
{
    if (!gFilters)
        return nsnull;
    
    FilterRecord *current = gFilters;
    
    do {
        if (current->filterObject == filter)
            return current;
        current = NS_REINTERPRET_CAST(FilterRecord *,
                                      PR_NEXT_LINK(&current->links));
    } while (current != gFilters);
    
    return nsnull;
}

/* returns true if the hook should be executed. */
PRBool
jsds_FilterHook (JSDContext *jsdc, JSDThreadState *state)
{
    if (!gFilters)
        return PR_TRUE;
    
    JSContext *cx = JSD_GetJSContext (jsdc, state);
    void *glob = NS_STATIC_CAST(void *, JS_GetGlobalObject (cx));
    if (NS_WARN_IF_FALSE(glob, "No global in threadstate"))
        return PR_TRUE;
    
    JSDStackFrameInfo *frame = JSD_GetStackFrame (jsdc, state);
    if (NS_WARN_IF_FALSE(frame, "No frame in threadstate"))
        return PR_TRUE;
    
    JSDScript *script = JSD_GetScriptForStackFrame (jsdc, state, frame);
    if (NS_WARN_IF_FALSE(script, "No script in threadstate"))
        return PR_TRUE;

    jsuint pc = JSD_GetPCForStackFrame (jsdc, state, frame);
    if (NS_WARN_IF_FALSE(pc, "No pc in threadstate"))
        return PR_TRUE;
    
    PRUint32 currentLine = JSD_GetClosestLine (jsdc, script, pc);
    if (NS_WARN_IF_FALSE(currentLine, "Can't convert pc to line"))
        return PR_TRUE;
    
    const char *url = nsnull;
    PRUint32 len = 0;
    FilterRecord *currentFilter = gFilters;
    do {
        PRUint32 flags = 0;
        nsresult rv = currentFilter->filterObject->GetFlags(&flags);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Error getting flags for filter");
        if (flags & jsdIFilter::FLAG_ENABLED) {
            /* if there is no glob, or the globs match */
            if ((!currentFilter->glob || currentFilter->glob == glob) &&
                /* and there is no start line, or the start line is before 
                 * or equal to the current */
                (!currentFilter->startLine || 
                 currentFilter->startLine <= currentLine) &&
                /* and there is no end line, or the end line is after
                 * or equal to the current */
                (!currentFilter->endLine ||
                 currentFilter->endLine >= currentLine)) {
                /* then we're going to have to compare the url. */
                if (currentFilter->patternType == ptIgnore)
                    return flags & jsdIFilter::FLAG_PASS;
            
                if (!url) {
                    url = JSD_GetScriptFilename (jsdc, script);
                    NS_ASSERTION (url, "Script with no filename");
                    len = PL_strlen(url);
                }
                
                if (len >= currentFilter->patternLength) {
                    switch (currentFilter->patternType) {
                        case ptEquals:
                            if (!PL_strcmp(currentFilter->urlPattern, url))
                                return flags & jsdIFilter::FLAG_PASS;
                            break;
                        case ptStartsWith:
                            if (!PL_strncmp(currentFilter->urlPattern, url, 
                                           currentFilter->patternLength))
                                return flags & jsdIFilter::FLAG_PASS;
                            break;
                        case ptEndsWith:
                            if (!PL_strcmp(currentFilter->urlPattern,
                                           &url[len - 
                                               currentFilter->patternLength]))
                                return flags & jsdIFilter::FLAG_PASS;
                            break;
                        case ptContains:
                            if (PL_strstr(url, currentFilter->urlPattern))
                                return flags & jsdIFilter::FLAG_PASS;
                            break;
                        default:
                            NS_ASSERTION(0, "Invalid pattern type");
                    }
                }                
            }
        }
        currentFilter = NS_REINTERPRET_CAST(FilterRecord *,
                                            PR_NEXT_LINK(&currentFilter->links));
    } while (currentFilter != gFilters);

    return PR_TRUE;
    
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
        gJsds->Pause(nsnull);
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
        gJsds->UnPause(nsnull);
    }
    gDeadScripts = 0;
}

JS_STATIC_DLL_CALLBACK (JSBool)
jsds_GCCallbackProc (JSContext *cx, JSGCStatus status)
{
    gGCStatus = status;
#ifdef DEBUG_verbose
    printf ("new gc status is %i\n", status);
#endif
    if (status == JSGC_END && gDeadScripts)
        jsds_NotifyPendingDeadScripts (cx);
    
    if (gLastGCProc)
        return gLastGCProc (cx, status);
    
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

    if (!jsds_FilterHook (jsdc, jsdthreadstate))
        return JS_FALSE;

    JSDStackFrameInfo *native_frame = JSD_GetStackFrame (jsdc, jsdthreadstate);
    nsCOMPtr<jsdIStackFrame> frame =
        getter_AddRefs(jsdStackFrame::FromPtr(jsdc, jsdthreadstate,
                                              native_frame));
    gJsds->Pause(nsnull);
    hook->OnCall(frame, type);    
    gJsds->UnPause(nsnull);
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
    
    if (!jsds_FilterHook (jsdc, jsdthreadstate))
        return JSD_HOOK_RETURN_CONTINUE;
    
    JSDStackFrameInfo *native_frame = JSD_GetStackFrame (jsdc, jsdthreadstate);
    nsCOMPtr<jsdIStackFrame> frame =
        getter_AddRefs(jsdStackFrame::FromPtr(jsdc, jsdthreadstate,
                                              native_frame));
    gJsds->Pause(nsnull);
    hook->OnExecute (frame, type, &js_rv, &hook_rv);
    gJsds->UnPause(nsnull);
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
        gJsds->Pause(nsnull);
        hook->OnScriptCreated (script);
        gJsds->UnPause(nsnull);
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
                
                gJsds->Pause(nsnull);
                hook->OnScriptDestroyed (jsdis);
                gJsds->UnPause(nsnull);
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

    if (mScript) {
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
jsdScript::IsLineExecutable(PRUint32 aLine, PRBool *_rval)
{
    ASSERT_VALID_SCRIPT;
    jsuword pc = JSD_GetClosestPC (mCx, mScript, aLine);
    *_rval = (aLine == JSD_GetClosestLine (mCx, mScript, pc));
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
    *_rval =
        ToNewCString(nsDependentCString(JSD_GetValueFunctionName(mCx, mValue)));
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
jsdService::GetInitAtStartup (PRBool *_rval)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager>
        categoryManager(do_GetService(NS_CATMAN_CTRID, &rv));
    if (NS_FAILED(rv))
        return rv;

    if (mInitAtStartup == triUnknown) {
        nsXPIDLCString notused;
        nsresult autoreg_rv, appstart_rv;
        
        autoreg_rv = categoryManager->GetCategoryEntry(AUTOREG_CATEGORY, 
                                                       JSD_STARTUP_ENTRY,
                                                       getter_Copies(notused));
        appstart_rv = categoryManager->GetCategoryEntry(APPSTART_CATEGORY,
                                                        JSD_STARTUP_ENTRY,
                                                        getter_Copies(notused));
        if (autoreg_rv != appstart_rv) {
            /* we have an inconsistent state in the registry, attempt to fix.
             * we need to make mInitAtStartup disagree with the state passed
             * to SetInitAtStartup to make it actually do something.
             */
            mInitAtStartup = triYes;
            rv = SetInitAtStartup (PR_FALSE);
            if (NS_FAILED(rv))
                return rv;
        } else if (autoreg_rv == NS_ERROR_NOT_AVAILABLE) {
            mInitAtStartup = triNo;
        } else if (NS_SUCCEEDED(autoreg_rv)) {
            mInitAtStartup = triYes;
        } else {
            return rv;
        }
    }
    
    if (_rval)
        *_rval = (mInitAtStartup == triYes);

    return NS_OK;
}

/*
 * The initAtStartup property controls whether or not we register the
 * app start observer (jsdASObserver.)  We register for both 
 * "xpcom-autoregistration" and "app-startup" notifications if |state| is true.
 * the autoreg message is sent just before registration occurs (before
 * "app-startup".)  We care about autoreg because it may load javascript
 * components.  autoreg does *not* fire if components haven't changed since the
 * last autoreg, so we watch "app-startup" as a fallback.
 */
NS_IMETHODIMP
jsdService::SetInitAtStartup (PRBool state)
{ 
    nsresult rv;

    if (mInitAtStartup == triUnknown) {
        /* side effect sets mInitAtStartup */
        rv = GetInitAtStartup(nsnull);
        if (NS_FAILED(rv))
            return rv;
    }

    if (state && mInitAtStartup == triYes ||
        !state && mInitAtStartup == triNo) {
        /* already in the requested state */
        return NS_OK;
    }
    
    nsCOMPtr<nsICategoryManager>
        categoryManager(do_GetService(NS_CATMAN_CTRID, &rv));

    if (state) {
        rv = categoryManager->AddCategoryEntry(AUTOREG_CATEGORY,
                                               JSD_STARTUP_ENTRY,
                                               jsdASObserverCtrID,
                                               PR_TRUE, PR_TRUE, nsnull);
        if (NS_FAILED(rv))
            return rv;
        rv = categoryManager->AddCategoryEntry(APPSTART_CATEGORY,
                                               JSD_STARTUP_ENTRY,
                                               jsdASObserverCtrID,
                                               PR_TRUE, PR_TRUE, nsnull);
        if (NS_FAILED(rv))
            return rv;
        mInitAtStartup = triYes;
    } else {
        rv = categoryManager->DeleteCategoryEntry(AUTOREG_CATEGORY,
                                                  JSD_STARTUP_ENTRY, PR_TRUE);
        if (NS_FAILED(rv))
            return rv;
        rv = categoryManager->DeleteCategoryEntry(APPSTART_CATEGORY,
                                                  JSD_STARTUP_ENTRY, PR_TRUE);
        if (NS_FAILED(rv))
            return rv;
        mInitAtStartup = triNo;
    }

    return NS_OK;
}


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
        JSD_ClearTopLevelHook (mCx);
    if (mFunctionHook)
        JSD_SetFunctionHook (mCx, jsds_CallHookProc, NULL);
    else
        JSD_ClearFunctionHook (mCx);
    mOn = PR_TRUE;

#ifdef DEBUG
    printf ("+++ JavaScript debuging hooks installed.\n");
#endif
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
        if (gGCStatus == JSGC_END)
        {
            JSContext *cx = JSD_GetDefaultJSContext(mCx);
            jsds_NotifyPendingDeadScripts(cx);
        }
        else
            return NS_ERROR_NOT_AVAILABLE;
    }

    /*
    if (gLastGCProc != jsds_GCCallbackProc)
        JS_SetGCCallbackRT (mRuntime, gLastGCProc);
    */

    jsdValue::InvalidateAll();
    jsdProperty::InvalidateAll();
    ClearAllBreakpoints();

    JSD_ClearThrowHook (mCx);
    JSD_SetScriptHook (mCx, NULL, NULL);
    JSD_ClearInterruptHook (mCx);
    JSD_ClearDebuggerHook (mCx);
    JSD_ClearDebugBreakHook (mCx);
    JSD_ClearTopLevelHook (mCx);
    JSD_ClearFunctionHook (mCx);
    
    JSD_DebuggerOff (mCx);

    mCx = nsnull;
    mRuntime = nsnull;
    mOn = PR_FALSE;

#ifdef DEBUG
    printf ("+++ JavaScript debuging hooks removed.\n");
#endif

    return NS_OK;
}

NS_IMETHODIMP
jsdService::Pause(PRUint32 *_rval)
{
    if (++mPauseLevel == 1) {
        JSD_ClearThrowHook (mCx);
        JSD_ClearInterruptHook (mCx);
        JSD_ClearDebuggerHook (mCx);
        JSD_ClearDebugBreakHook (mCx);
        JSD_ClearTopLevelHook (mCx);
        JSD_ClearFunctionHook (mCx);
    }

    if (_rval)
        *_rval = mPauseLevel;

    return NS_OK;
}

NS_IMETHODIMP
jsdService::UnPause(PRUint32 *_rval)
{
    if (mPauseLevel == 0)
        return NS_ERROR_NOT_AVAILABLE;

    if (--mPauseLevel == 0) {
        if (mThrowHook)
            JSD_SetThrowHook (mCx, jsds_ExecutionHookProc, NULL);
        if (mInterruptHook)
            JSD_SetInterruptHook (mCx, jsds_ExecutionHookProc, NULL);
        if (mDebuggerHook)
            JSD_SetDebuggerHook (mCx, jsds_ExecutionHookProc, NULL);
        if (mErrorHook)
            JSD_SetDebugBreakHook (mCx, jsds_ExecutionHookProc, NULL);
        if (mTopLevelHook)
            JSD_SetTopLevelHook (mCx, jsds_CallHookProc, NULL);
        else
            JSD_ClearTopLevelHook (mCx);
        if (mFunctionHook)
            JSD_SetFunctionHook (mCx, jsds_CallHookProc, NULL);
        else
            JSD_ClearFunctionHook (mCx);
    }
    
    if (_rval)
        *_rval = mPauseLevel;

    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnumerateScripts (jsdIScriptEnumerator *enumerator)
{
    ASSERT_VALID_CONTEXT;
    
    JSDScript *script;
    JSDScript *iter = NULL;
    nsresult rv = NS_OK;
    
    JSD_LockScriptSubsystem(mCx);
    while((script = JSD_IterateScripts(mCx, &iter)) != NULL) {
        rv = enumerator->EnumerateScript (jsdScript::FromPtr(mCx, script));
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
    JSContext *cx = JSD_GetDefaultJSContext (mCx);
    JS_GC(cx);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::InsertFilter (jsdIFilter *filter, jsdIFilter *after)
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
    
    if (gFilters == rec)
        gFilters = NS_REINTERPRET_CAST(FilterRecord *,
                                       PR_NEXT_LINK(&rec->links));
    
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
        current = NS_REINTERPRET_CAST(FilterRecord *,
                                      PR_NEXT_LINK (&current->links));
    } while (current != gFilters);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::RefreshFilters ()
{
    return EnumerateFilters(nsnull);
}

NS_IMETHODIMP
jsdService::ClearFilters ()
{
    if (!gFilters)
        return NS_OK;

    FilterRecord *current = NS_REINTERPRET_CAST(FilterRecord *,
                                                PR_NEXT_LINK (&gFilters->links));
    do {
        FilterRecord *next = NS_REINTERPRET_CAST(FilterRecord *,
                                                 PR_NEXT_LINK (&current->links));
        PR_REMOVE_AND_INIT_LINK(&current->links);
        jsds_FreeFilter(current);
        current = next;
    } while (current != gFilters);
    
    jsds_FreeFilter(current);
    gFilters = nsnull;
    
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
jsdService::EnterNestedEventLoop (jsdINestCallback *callback, PRUint32 *_rval)
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
        if (NS_SUCCEEDED(rv) && callback) {
            Pause(nsnull);
            rv = callback->OnNest();
            UnPause(nsnull);
        }
        
        while(NS_SUCCEEDED(rv) && mNestedLoopLevel >= nestLevel)
        {
            void* data;
            PRBool isRealEvent;
            //PRBool processEvent;
            
            rv = appShell->GetNativeEvent(isRealEvent, data);
            if(NS_SUCCEEDED(rv))
                appShell->DispatchNativeEvent(isRealEvent, data);
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
    if (!mCx || mPauseLevel)
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
    if (!mCx || mPauseLevel)
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
    if (!mCx || mPauseLevel)
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
    if (!mCx || mPauseLevel)
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
    if (!mCx || mPauseLevel)
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
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetTopLevelHook (mCx, jsds_CallHookProc, NULL);
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
     * OnForRuntime() method will do the rest when the coast is clear.
     */
    if (!mCx || mPauseLevel)
        return NS_OK;

    if (aHook)
        JSD_SetFunctionHook (mCx, jsds_CallHookProc, NULL);
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

jsdService *
jsdService::GetService ()
{
    if (!gJsds)
        gJsds = new jsdService();
        
    NS_IF_ADDREF(gJsds);
    return gJsds;
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(jsdService, jsdService::GetService);

/* app-start observer.  turns on the debugger at app-start.  this is inserted
 * and/or removed from the app-start category by the jsdService::initAtStartup
 * property.
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
jsdASObserver::Observe (nsISupports *aSubject, const char *aTopic,
                        const PRUnichar *aData)
{
    nsresult rv;

    jsdService *jsds = jsdService::GetService();
    PRBool on;
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

    rv = jsds->OnForRuntime(rt);
    
    return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(jsdASObserver);

static nsModuleComponentInfo components[] = {
    {"JSDService", JSDSERVICE_CID,     jsdServiceCtrID, jsdServiceConstructor},
    {"JSDASObserver",  JSDASO_CID,  jsdASObserverCtrID, jsdASObserverConstructor}
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
