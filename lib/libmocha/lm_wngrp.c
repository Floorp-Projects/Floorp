/*
 * lm_wngrp.c: Structures and functions to deal with new LM window groups
 *              and multiple LM threads and what not.
 * 
 * All blame to Mike McCool (mlm@netscape.com) 2/17/98
 */

#include "lm.h"
#include "xp.h"
#include "prclist.h"
#include "prthread.h"
#include "prmon.h"

#ifdef XP_MAC
#include "pprthred.h"  /* for PR_CreateThreadGCAble */
#else
#include "private/pprthred.h"
#endif

/* 
 * Notes to self
 * 
 * - lm_window_count: do we care that there's only one? 
 * - do we need to add a limit to number of threads we spawn()?
 * - JRIEnv: do we need multiple?
 * - JSRuntime: do we need multiple?
 * - Threading changes: impact on debugger?
 * - JS_SetGlobalObject(JSContext) will be important
 * - If the global structure is on the mozilla thread, can other threads
 *    creating a new thread group access, or do I have to post an event?
 *
 * ? Rename "MochaDecoder" to "LMWindow"
 * ? Change so that there is only one JSContext
 */

PRMonitor *wingroups_mon;
LMWindowGroup *wingroups;
PRMonitor *request_mon;

struct ContextListStr  {
    ContextList *next;
    ContextList *prev;
    MWContext   *context;
};

LMWindowGroup *_lm_NewWindowGroup(int priority);

/* Initialize my globals, from the Mozilla thread 
 */
void lm_InitWindowGroups(void)
{
    int priority;

    /* run at slightly lower priority than the mozilla thread */
    priority = PR_GetThreadPriority(PR_CurrentThread());
    PR_ASSERT(priority >= PR_PRIORITY_FIRST && priority <= PR_PRIORITY_LAST);
 
    if (priority == PR_PRIORITY_NORMAL)
        priority = PR_PRIORITY_LOW;
    else if (priority == PR_PRIORITY_HIGH)
        priority = PR_PRIORITY_NORMAL;
    else if (priority == PR_PRIORITY_URGENT)
        priority = PR_PRIORITY_HIGH;
    else
        priority = PR_PRIORITY_LOW;

    wingroups_mon = PR_NewMonitor();
    request_mon = PR_NewMonitor();
    wingroups = NULL;
    wingroups = _lm_NewWindowGroup(priority);
    if(wingroups != NULL)  {
        PR_INIT_CLIST(wingroups);
	lm_StartWindowGroup(wingroups);
    }  /* else huh?! */
}

/* Create a new window group.  Create new context, create new monitor, create
 *  new thread, event queue, queue stack, empty context list. 
 */
LMWindowGroup *lm_NewWindowGroup(void)
{
    int priority;

    /* Run at same priority as current thread (which should be a JS 
     * thread. 
     */
    priority = PR_GetThreadPriority(PR_CurrentThread());
    PR_ASSERT(priority>=PR_PRIORITY_FIRST && priority<=PR_PRIORITY_LAST);
    return _lm_NewWindowGroup(priority);
}

LMWindowGroup *_lm_NewWindowGroup(int priority)
{
    LMWindowGroup *newgrp = XP_NEW_ZAP(LMWindowGroup);
    if(newgrp != NULL)  {
        newgrp->done = PR_FALSE;
        newgrp->hasLock = PR_FALSE;

        /* Create a new JS Context for this set of windows. 
         * Note: Need to get global runtime lm_runtime from somewhere, 
         *        and perhaps the LM_STACK_SIZE? 
         */
/**************************************************** MLM *
        newgrp->js_context = JS_NewContext(lm_runtime, LM_STACK_SIZE);
        if(newgrp->js_context == NULL)  {
            XP_DELETE(newgrp);
            return NULL;
        }
 **************************************************** MLM */
	newgrp->js_context = NULL;
	/* JS_SetGCCallback(newgrp->js_context, LM_ShouldRunGC); */

        newgrp->mw_contexts = XP_NEW_ZAP(ContextList);
        if(newgrp->mw_contexts == NULL)  {
            JS_DestroyContext(newgrp->js_context);
            XP_DELETE(newgrp);
            return NULL;
        }
        PR_INIT_CLIST(newgrp->mw_contexts);
        newgrp->mw_contexts->context = NULL;
        newgrp->current_context = NULL;

        newgrp->queue_stack = XP_NEW_ZAP(QueueStackElement);
        if(!newgrp->queue_stack)  {
            JS_DestroyContext(newgrp->js_context);
            XP_DELETE(newgrp->mw_contexts);
            XP_DELETE(newgrp);
            return NULL;
        }

        /* Do this here so we don't race ourselves in lm_wait_for_events */
        PR_EnterMonitor(wingroups_mon);
        if(wingroups != NULL)  {
            PR_APPEND_LINK(newgrp, wingroups);
        }
        PR_ExitMonitor(wingroups_mon);

        newgrp->waiting_list = NULL;

        newgrp->owner_monitor = PR_NewMonitor();
        newgrp->queue_monitor = PR_NewMonitor();

        PR_EnterMonitor(newgrp->owner_monitor);
        newgrp->thread = PR_CreateThreadGCAble(PR_USER_THREAD,
                                               lm_wait_for_events, newgrp,
                                               priority, PR_LOCAL_THREAD,
                                               PR_UNJOINABLE_THREAD, 0);
        newgrp->owner              = newgrp->thread;
        newgrp->current_count      = 0;
        newgrp->mozWantsLock       = PR_FALSE;
        newgrp->mozGotLock         = PR_FALSE;
        newgrp->interruptCurrentOp = PR_FALSE;
        newgrp->queue_depth        = 0;
        newgrp->queue_count        = 0;

        /* Note: Need a unique identifier for this queue?
         */
        newgrp->interpret_queue = PR_CreateEventQueue("new_event_queue",
                                                      newgrp->thread);
        newgrp->queue_stack->queue = newgrp->interpret_queue;

	newgrp->lock_context = NULL;
	newgrp->js_timeout_insertion_point = NULL;
	newgrp->js_timeout_running = NULL;
	newgrp->inputRecurring = 0;
    }
    return newgrp;
}

void lm_StartWindowGroup(LMWindowGroup *grp)
{
    PR_Notify(grp->owner_monitor);
    PR_ExitMonitor(grp->owner_monitor);
}

void lm_DestroyWindowGroup(LMWindowGroup *grp)
{
    PR_EnterMonitor(wingroups_mon);
    /* Note: Thread terminates when the thread's main function (in
     *        this case, lm_wait_for_events) exits.
     */
    /************************************ MLM *
     * JS_DestroyContext(grp->js_context);
     ************************************ MLM */
    PR_DestroyMonitor(grp->owner_monitor);
    PR_DestroyMonitor(grp->queue_monitor);
    
    /* Note: How to destroy an event queue or two?
     */

    /* Note: Context list should already be null
     */

    PR_REMOVE_LINK(grp);
    XP_DELETE(grp);
    PR_ExitMonitor(wingroups_mon);
}

LMWindowGroup *LM_GetDefaultWindowGroup(MWContext *mwc)
{
    LMWindowGroup *ans;

    /* Check to see if this is a frame context.  If so, check its parent. */
    if((mwc != NULL) && (mwc->is_grid_cell))  {
        MWContext *grid_parent, *grid_child;
	grid_child = mwc;
	grid_parent = mwc->grid_parent;
	/* Find the root parent.  I wonder if I need to add cycle checking. */
	while(grid_parent != NULL)  {
	    grid_child = grid_parent;
	    grid_parent = grid_child->grid_parent;
	}
	if( (ans = lm_MWContextToGroup(grid_child)) != NULL)  {
	    /* The parent's been found, return its group. */
	    return ans;
	}  else  {  /* Else add the parent to the default group, 
		     *  and return that. */
    	    PR_EnterMonitor(wingroups_mon);
	    LM_AddContextToGroup(wingroups, grid_child);
	    ans = wingroups;
    	    PR_ExitMonitor(wingroups_mon);
	    return ans;
	}
    }
    PR_EnterMonitor(wingroups_mon);
    ans = wingroups;
    PR_ExitMonitor(wingroups_mon);
    return ans;
}

LMWindowGroup *lm_MWContextToGroup(MWContext *mwc)
{
    LMWindowGroup *ptr = NULL;
    LMWindowGroup *ans = NULL;

    PR_EnterMonitor(wingroups_mon);

    ptr = wingroups;
 
    if(lm_GetEntryForContext(wingroups, mwc) != NULL)  {
        ans = wingroups;
        PR_ExitMonitor(wingroups_mon);
        return ans;
    }
 
    ptr = PR_NEXT_LINK(ptr);
    while(ptr != wingroups)  {
        if(lm_GetEntryForContext(ptr, mwc) != NULL)  {
            ans = ptr;
            PR_ExitMonitor(wingroups_mon);
            return ans;
        }
        ptr = PR_NEXT_LINK(ptr);
    }
    PR_ExitMonitor(wingroups_mon);
    return NULL;
}

LMWindowGroup *lm_QueueStackToGroup(QueueStackElement *qse)
{
    LMWindowGroup *ptr = NULL;
    LMWindowGroup *ans = NULL;

    PR_EnterMonitor(wingroups_mon);
    ptr = wingroups;
 
    if(wingroups->queue_stack == qse)  {
        ans = wingroups;
        PR_ExitMonitor(wingroups_mon);
        return ans;
    }
 
    ptr = PR_NEXT_LINK(ptr);
    while(ptr != wingroups)  {
        if(ptr->queue_stack == qse)  {
            ans = ptr;
            PR_ExitMonitor(wingroups_mon);
            return ans;
        }
        ptr = PR_NEXT_LINK(ptr);
    }
    PR_ExitMonitor(wingroups_mon);
    return NULL;
}

PREventQueue *LM_MWContextToQueue(MWContext *mwc)
{
    /* Note: This gets the interpret queue, need to get top queue as well
     */
    LMWindowGroup *grp = lm_MWContextToGroup(mwc);
    if(grp != NULL)  {
        return LM_WindowGroupToQueue(grp);
    }
    return NULL;
}

PREventQueue *LM_WindowGroupToQueue(LMWindowGroup *lmg)
{
    return lmg->interpret_queue;
}

ContextList *lm_GetEntryForContext(LMWindowGroup *grp, MWContext *cx)
{
    ContextList *cxl = grp->mw_contexts;
    ContextList *ans = NULL;

    if(PR_CLIST_IS_EMPTY(cxl))  {
        return NULL;
    }  else  {
        ContextList *ptr = PR_NEXT_LINK(cxl);
        while(ptr != cxl)  {
            if(ptr->context == cx)  {
                ans = ptr;
                break;
            }
            ptr = PR_NEXT_LINK(ptr);
        }
    }
    return ans;
}

void LM_AddContextToGroup(LMWindowGroup *grp, MWContext *cx)
{
    ContextList *cxl;

    if(lm_MWContextToGroup(cx) != NULL)  {
        /* Hey, why are we adding this stuff twice? */
        XP_ASSERT(0);
    }

    cxl = XP_NEW_ZAP(ContextList);
    /* Note: failure?!?!?!
     */
    cxl->context = cx;

    PR_APPEND_LINK(cxl, grp->mw_contexts);
}

void lm_RemoveContextFromGroup(LMWindowGroup *grp, MWContext *cx);

void LM_RemoveContextFromGroup(MWContext *cx)
{
    LMWindowGroup *grp = lm_MWContextToGroup(cx);
    if(grp)  {
        lm_RemoveContextFromGroup(grp, cx);
    }
}

void lm_RemoveContextFromGroup(LMWindowGroup *grp, MWContext *cx)
{
    if(!PR_CLIST_IS_EMPTY(grp->mw_contexts))  {
        ContextList *entry = lm_GetEntryForContext(grp, cx);
        if(entry != NULL)  {
            PR_REMOVE_LINK(entry);
            XP_DELETE(entry);
        }
    }
    if(PR_CLIST_IS_EMPTY(grp->mw_contexts) && (grp != wingroups))  {
        grp->done = PR_TRUE;
    }
}

PRBool LM_IsLocked(LMWindowGroup *grp)
{
    PRBool ans;
    PR_EnterMonitor(request_mon);
    ans = grp->hasLock;
    PR_ExitMonitor(request_mon);
    return ans;
}

void LM_BeginRequest(LMWindowGroup *grp, JSContext *jsc)
{
    PR_EnterMonitor(request_mon);
    grp->hasLock = PR_TRUE;
    grp->lock_context = jsc;
    if((JS_GetContextThread(jsc)) != ((intN) grp->thread))  {
        JS_SetContextThread(jsc);
    }
    PR_ExitMonitor(request_mon);
    JS_BeginRequest(jsc);
}

void LM_EndRequest(LMWindowGroup *grp, JSContext *jsc)
{
    JS_EndRequest(jsc);
    PR_EnterMonitor(request_mon);
    grp->lock_context = NULL;
    grp->hasLock = PR_FALSE;
    PR_ExitMonitor(request_mon);
}

JSBool LM_ShouldRunGC(JSContext *cx, JSGCStatus status)
{
    JSBool ans = JS_TRUE;
    JSContext *active;
    LMWindowGroup *ptr;

    if(status != JSGC_BEGIN)  {
        return JS_TRUE;
    }

    PR_EnterMonitor(request_mon);
    if(wingroups->hasLock)  {
        active = wingroups->lock_context;
        if(active != cx)  {
            ans=JS_FALSE;
	    goto bye;
        }
    }
    ptr = PR_NEXT_LINK(wingroups);
    while(ptr != wingroups)  {
        if(ptr->hasLock)  {
            active = ptr->lock_context;
            if(active != cx)  {
                ans=JS_FALSE;
		goto bye;
            }
	}
        ptr = PR_NEXT_LINK(ptr);
    }
bye:
    PR_ExitMonitor(request_mon);
    return ans;
}
