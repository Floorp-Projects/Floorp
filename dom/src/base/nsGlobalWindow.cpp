/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsGlobalWindow.h"
#include "nscore.h"
#include "nslayout.h"
#include "prmem.h"
#include "prtime.h"
#include "plstr.h"
#include "prinrval.h"
#include "nsIFactory.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "nsINetService.h"
#include "nsINetContainerApplication.h"
#include "nsITimer.h"
#include "nsEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIScriptEventListener.h"
#include "nsIPrivateDOMEvent.h"

#include "jsapi.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIScriptEventListenerIID, NS_ISCRIPTEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDOMWindowIID, NS_IDOMWINDOW_IID);
static NS_DEFINE_IID(kIDOMNavigatorIID, NS_IDOMNAVIGATOR_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMFocusListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMFormListenerIID, NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMLoadListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMDragListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID, NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIDOMEventCapturerIID, NS_IDOMEVENTCAPTURER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);

GlobalWindowImpl::GlobalWindowImpl()
{
  NS_INIT_REFCNT();
  mContext = nsnull;
  mScriptObject = nsnull;
  mDocument = nsnull;
  mNavigator = nsnull;

  mTimeouts = nsnull;
  mTimeoutInsertionPoint = nsnull;
  mRunningTimeout = nsnull;
  mTimeoutPublicIdCounter = 1;
  mListenerManager = nsnull;
}

GlobalWindowImpl::~GlobalWindowImpl() 
{
  if (nsnull != mScriptObject) {
    JS_RemoveRoot((JSContext *)mContext->GetNativeContext(), &mScriptObject);
    mScriptObject = nsnull;
  }
  
  if (nsnull != mContext) {
    NS_RELEASE(mContext);
  }

  if (nsnull != mDocument) {
    NS_RELEASE(mDocument);
  }
  
  NS_IF_RELEASE(mNavigator);
  NS_IF_RELEASE(mListenerManager);
}

NS_IMPL_ADDREF(GlobalWindowImpl)
NS_IMPL_RELEASE(GlobalWindowImpl)

nsresult 
GlobalWindowImpl::QueryInterface(const nsIID& aIID,
                                 void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptGlobalObjectIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptGlobalObject*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMWindowIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMWindow*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptGlobalObject*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIJSScriptObjectIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIJSScriptObject*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventCapturerIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIDOMEventCapturer*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventReceiverIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIDOMEventReceiver*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult 
GlobalWindowImpl::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

nsresult 
GlobalWindowImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptWindow(aContext, (nsIDOMWindow*)this,
                             nsnull, &mScriptObject);
    JS_AddNamedRoot((JSContext *)aContext->GetNativeContext(),
                    &mScriptObject, "window_object");
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetContext(nsIScriptContext *aContext)
{
  if (mContext) {
    NS_RELEASE(mContext);
  }

  mContext = aContext;
  NS_ADDREF(mContext);
}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetNewDocument(nsIDOMDocument *aDocument)
{
  if (nsnull != mDocument) {
    ClearAllTimeouts();

    if (nsnull != mScriptObject) {
      JS_ClearScope((JSContext *)mContext->GetNativeContext(),
                    (JSObject *)mScriptObject);
    }
    
    NS_RELEASE(mDocument);
    JS_GC((JSContext *)mContext->GetNativeContext());
  }

  mDocument = aDocument;
  
  if (nsnull != mDocument) {
    NS_ADDREF(mDocument);

    if (nsnull != mContext) {
      mContext->InitContext(this);
    }
  }
}

NS_IMETHODIMP    
GlobalWindowImpl::GetWindow(nsIDOMWindow** aWindow)
{
  *aWindow = this;
  NS_ADDREF(this);

  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::GetSelf(nsIDOMWindow** aWindow)
{
  *aWindow = this;
  NS_ADDREF(this);

  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::GetDocument(nsIDOMDocument** aDocument)
{
  *aDocument = mDocument;
  NS_ADDREF(mDocument);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetNavigator(nsIDOMNavigator** aNavigator)
{
  if (nsnull == mNavigator) {
    mNavigator = new NavigatorImpl();
    NS_IF_ADDREF(mNavigator);
  }

  *aNavigator = mNavigator;
  NS_IF_ADDREF(mNavigator);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Dump(const nsString& aStr)
{
  char *cstr = aStr.ToNewCString();
  
  if (nsnull != cstr) {
    printf("%s", cstr);
    delete [] cstr;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Alert(const nsString& aStr)
{
  // XXX Temporary
  return Dump(aStr);
}

nsresult
GlobalWindowImpl::ClearTimeoutOrInterval(PRInt32 aTimerID)
{
  PRUint32 public_id;
  nsTimeoutImpl **top, *timeout;

  public_id = (PRUint32)aTimerID;
  if (!public_id)    /* id of zero is reserved for internal use */
    return NS_ERROR_FAILURE;
  for (top = &mTimeouts; ((timeout = *top) != NULL); top = &timeout->next) {
    if (timeout->public_id == public_id) {
      if (mRunningTimeout == timeout) {
        /* We're running from inside the timeout.  Mark this
           timeout for deferred deletion by the code in
           win_run_timeout() */
        timeout->interval = 0;
      } 
      else {
        /* Delete the timeout from the pending timeout list */
        *top = timeout->next;
        if (timeout->timer) {
          timeout->timer->Cancel();
          NS_RELEASE(timeout->timer);
          DropTimeout(timeout);
        }
        DropTimeout(timeout);
      }
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::ClearTimeout(PRInt32 aTimerID)
{
  return ClearTimeoutOrInterval(aTimerID);
}

NS_IMETHODIMP    
GlobalWindowImpl::ClearInterval(PRInt32 aTimerID)
{
  return ClearTimeoutOrInterval(aTimerID);
}

void
GlobalWindowImpl::ClearAllTimeouts()
{
  nsTimeoutImpl *timeout, *next;

  for (timeout = mTimeouts; timeout; timeout = next) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == timeout)
      mTimeoutInsertionPoint = nsnull;
        
    next = timeout->next;
    if (timeout->timer) {
      timeout->timer->Cancel();
      NS_RELEASE(timeout->timer);
      // Drop the count since the timer isn't going to hold on
      // anymore.
      DropTimeout(timeout);
    }
    // Drop the count since we're removing it from the list.
    DropTimeout(timeout);
  }
   
  mTimeouts = NULL;
}

void
GlobalWindowImpl::HoldTimeout(nsTimeoutImpl *aTimeout)
{
  aTimeout->ref_count++;
}

void
GlobalWindowImpl::DropTimeout(nsTimeoutImpl *aTimeout)
{
  JSContext *cx;
  
  if (--aTimeout->ref_count > 0) {
    return;
  }
  
  cx = (JSContext *)mContext->GetNativeContext();
  
  if (aTimeout->expr) {
    PR_FREEIF(aTimeout->expr);
  }
  else if (aTimeout->funobj) {
    JS_RemoveRoot(cx, &aTimeout->funobj);
    if (aTimeout->argv) {
      int i;
      for (i = 0; i < aTimeout->argc; i++)
        JS_RemoveRoot(cx, &aTimeout->argv[i]);
      PR_FREEIF(aTimeout->argv);
    }
  }
  NS_IF_RELEASE(aTimeout->timer);
  PR_FREEIF(aTimeout->filename);
  NS_IF_RELEASE(aTimeout->window);
  PR_DELETE(aTimeout);
}

void
GlobalWindowImpl::InsertTimeoutIntoList(nsTimeoutImpl **aList, 
                                        nsTimeoutImpl *aTimeout)
{
  nsTimeoutImpl *to;
  
  while ((to = *aList) != nsnull) {
    if (LL_CMP(to->when, >, aTimeout->when))
      break;
    aList = &to->next;
  }
  aTimeout->next = to;
  *aList = aTimeout;
  // Increment the ref_count since we're in the list
  HoldTimeout(aTimeout);
}

void
nsGlobalWindow_RunTimeout(nsITimer *aTimer, void *aClosure)
{
  nsTimeoutImpl *timeout = (nsTimeoutImpl *)aClosure;

  timeout->window->RunTimeout(timeout);
  // Drop the ref_count since the timer won't be holding on to the
  // timeout struct anymore
  timeout->window->DropTimeout(timeout);
}

void
GlobalWindowImpl::RunTimeout(nsTimeoutImpl *aTimeout)
{
    nsTimeoutImpl *next, *timeout;
    nsTimeoutImpl *last_expired_timeout;
    nsTimeoutImpl dummy_timeout;
    JSContext *cx;
    PRInt64 now;
    jsval result;
    nsITimer *timer;

    timer = aTimeout->timer;
    cx = (JSContext *)mContext->GetNativeContext();

    /*
     *   A native timer has gone off.  See which of our timeouts need
     *   servicing
     */
    LL_I2L(now, PR_IntervalNow());

    /* The timeout list is kept in deadline order.  Discover the
       latest timeout whose deadline has expired. On some platforms,
       native timeout events fire "early", so we need to test the
       timer as well as the deadline. */
    last_expired_timeout = nsnull;
    for (timeout = mTimeouts; timeout; timeout = timeout->next) {
        if ((timeout == aTimeout) || !LL_CMP(timeout->when, >, now))
            last_expired_timeout = timeout;
    }

    /* Maybe the timeout that the event was fired for has been deleted
       and there are no others timeouts with deadlines that make them
       eligible for execution yet.  Go away. */
    if (!last_expired_timeout)
        return;

    /* Insert a dummy timeout into the list of timeouts between the portion
       of the list that we are about to process now and those timeouts that
       will be processed in a future call to win_run_timeout().  This dummy
       timeout serves as the head of the list for any timeouts inserted as
       a result of running a timeout. */
    dummy_timeout.timer = NULL;
    dummy_timeout.public_id = 0;
    dummy_timeout.next = last_expired_timeout->next;
    last_expired_timeout->next = &dummy_timeout;

    /* Don't let ClearWindowTimeouts throw away our stack-allocated
       dummy timeout. */
    dummy_timeout.ref_count = 2;
   
    mTimeoutInsertionPoint = &dummy_timeout.next;
    
    for (timeout = mTimeouts; timeout != &dummy_timeout; timeout = next) {
      next = timeout->next;

      /* Hold the timeout in case expr or funobj releases its doc. */
      mRunningTimeout = timeout;

      if (timeout->expr) {
        /* Evaluate the timeout expression. */
        JS_EvaluateScript(cx, (JSObject *)mScriptObject,
                          timeout->expr, 
                          PL_strlen(timeout->expr),
                          timeout->filename, timeout->lineno, 
                          &result);
      } 
      else {
        PRInt64 lateness64;
        PRInt32 lateness;

        /* Add "secret" final argument that indicates timeout
           lateness in milliseconds */
        LL_SUB(lateness64, now, timeout->when);
        LL_L2I(lateness, lateness64);
        lateness = PR_IntervalToMilliseconds(lateness);
        timeout->argv[timeout->argc] = INT_TO_JSVAL((jsint)lateness);
        JS_CallFunctionValue(cx, (JSObject *)mScriptObject,
                             OBJECT_TO_JSVAL(timeout->funobj),
                             timeout->argc + 1, timeout->argv, &result);
      }

      mRunningTimeout = nsnull;

      /* If we have a regular interval timer, we re-fire the
       *  timeout, accounting for clock drift.
       */
      if (timeout->interval) {
        /* Compute time to next timeout for interval timer. */
        PRInt32 delay32;
        PRInt64 interval, delay;
        LL_I2L(interval, PR_MillisecondsToInterval(timeout->interval));
        LL_ADD(timeout->when, timeout->when, interval);
        LL_I2L(now, PR_IntervalNow());
        LL_SUB(delay, timeout->when, now);
        LL_L2I(delay32, delay);

        /* If the next interval timeout is already supposed to
         *  have happened then run the timeout immediately.
         */
        if (delay32 < 0) {
          delay32 = 0;
        }
        delay32 = PR_IntervalToMilliseconds(delay32);

        NS_IF_RELEASE(timeout->timer);

        /* Reschedule timeout.  Account for possible error return in
           code below that checks for zero toid. */
        nsresult err = NS_NewTimer(&timeout->timer);
        if (NS_OK != err) {
          return;
        } 
        
        err = timeout->timer->Init(nsGlobalWindow_RunTimeout, timeout, 
                                   delay32);
        if (NS_OK != err) {
          return;
        } 
        // Increment ref_count to indicate that this timer is holding
        // on to the timeout struct.
        HoldTimeout(timeout);
      }

      /* Running a timeout can cause another timeout to be deleted,
         so we need to reset the pointer to the following timeout. */
      next = timeout->next;
      mTimeouts = next;
      // Drop timeout struct since it's out of the list
      DropTimeout(timeout);

      /* Free the timeout if this is not a repeating interval
       *  timeout (or if it was an interval timeout, but we were
       *  unsuccessful at rescheduling it.)
       */
      if (timeout->interval && timeout->timer) {
        /* Reschedule an interval timeout */
        /* Insert interval timeout onto list sorted in deadline order. */
        InsertTimeoutIntoList(mTimeoutInsertionPoint, timeout);
      }
    }

    /* Take the dummy timeout off the head of the list */
    mTimeouts = dummy_timeout.next;
    mTimeoutInsertionPoint = nsnull;
}

static const char *kSetIntervalStr = "setInterval";
static const char *kSetTimeoutStr = "setTimeout";

nsresult
GlobalWindowImpl::SetTimeoutOrInterval(JSContext *cx,
                                       jsval *argv, 
                                       PRUint32 argc, 
                                       PRInt32* aReturn,
                                       PRBool aIsInterval)
{
  char *expr = nsnull;
  JSObject *funobj = nsnull;
  JSString *str;
  nsTimeoutImpl *timeout, **insertion_point;
  jsdouble interval;
  PRInt64 now, delta;

  if (argc >= 2) {
    if (!JS_ValueToNumber(cx, argv[1], &interval)) {
      JS_ReportError(cx, "Second argument to %s must be a millisecond interval",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    switch (JS_TypeOfValue(cx, argv[0])) {
      case JSTYPE_FUNCTION:
        funobj = JSVAL_TO_OBJECT(argv[0]);
        break;
      case JSTYPE_STRING:
      case JSTYPE_OBJECT:
        if (!(str = JS_ValueToString(cx, argv[0])))
            return NS_ERROR_FAILURE;
        expr = PL_strdup(JS_GetStringBytes(str));
        if (nsnull == expr)
            return NS_ERROR_OUT_OF_MEMORY;
        break;
      default:
        JS_ReportError(cx, "useless %s call (missing quotes around argument?)", aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
        return NS_ERROR_FAILURE;
    }

    timeout = PR_NEWZAP(nsTimeoutImpl);
    if (nsnull == timeout) {
      PR_FREEIF(expr);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Initial ref_count to indicate that this timeout struct will
    // be held as the closure of a timer.
    timeout->ref_count = 1;
    if (aIsInterval)
        timeout->interval = (PRInt32)interval;
    timeout->expr = expr;
    timeout->funobj = funobj;
    if (expr) {
      timeout->argv = 0;
      timeout->argc = 0;
    } 
    else {
      int i;
      /* Leave an extra slot for a secret final argument that
         indicates to the called function how "late" the timeout is. */
      timeout->argv = (jsval *)PR_MALLOC((argc - 1) * sizeof(jsval));
      if (nsnull == timeout->argv) {
        DropTimeout(timeout);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (!JS_AddNamedRoot(cx, &timeout->funobj, "timeout.funobj")) {
        DropTimeout(timeout);
        return NS_ERROR_FAILURE;
      }
      
      timeout->argc = 0;
      for (i = 2; (PRUint32)i < argc; i++) {
        timeout->argv[i - 2] = argv[i];
        if (!JS_AddNamedRoot(cx, &timeout->argv[i - 2], "timeout.argv[i]")) {
          DropTimeout(timeout);
          return NS_ERROR_FAILURE;
        }
        timeout->argc++;
      }
    }
    
    LL_I2L(now, PR_IntervalNow());
    LL_D2L(delta, PR_MillisecondsToInterval((PRUint32)interval));
    LL_ADD(timeout->when, now, delta);

    nsresult err = NS_NewTimer(&timeout->timer);
    if (NS_OK != err) {
      DropTimeout(timeout);
      return err;
    } 
    
    err = timeout->timer->Init(nsGlobalWindow_RunTimeout, timeout, 
                               (PRInt32)interval);
    if (NS_OK != err) {
      DropTimeout(timeout);
      return err;
    } 

    timeout->window = this;
    NS_ADDREF(this);

    if (mTimeoutInsertionPoint == NULL)
        insertion_point = &mTimeouts;
    else
        insertion_point = mTimeoutInsertionPoint;

    InsertTimeoutIntoList(insertion_point, timeout);
    timeout->public_id = ++mTimeoutPublicIdCounter;
    *aReturn = timeout->public_id;
  }
  else {
    JS_ReportError(cx, "Function %s requires at least 2 parameters", aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::SetTimeout(JSContext *cx,
                             jsval *argv, 
                             PRUint32 argc, 
                             PRInt32* aReturn)
{
  return SetTimeoutOrInterval(cx, argv, argc, aReturn, PR_FALSE);
}

NS_IMETHODIMP    
GlobalWindowImpl::SetInterval(JSContext *cx,
                              jsval *argv, 
                              PRUint32 argc, 
                              PRInt32* aReturn)
{
  return SetTimeoutOrInterval(cx, argv, argc, aReturn, PR_TRUE);
}

PRBool    GlobalWindowImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    GlobalWindowImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    GlobalWindowImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    GlobalWindowImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsAutoString mPropName, mPrefix;
    mPropName.SetString(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    mPrefix.SetString(mPropName, 2);
    if (mPrefix == "on") {
      nsIEventListenerManager *mManager = nsnull;

      if (mPropName == "onmousedown" || mPropName == "onmouseup" || mPropName ==  "onclick" ||
         mPropName == "onmouseover" || mPropName == "onmouseout") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onkeydown" || mPropName == "onkeyup" || mPropName == "onkeypress") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMKeyListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onmousemove") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseMotionListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onfocus" || mPropName == "onblur") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFocusListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onsubmit" || mPropName == "onreset") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFormListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onload" || mPropName == "onunload" || mPropName == "onabort" ||
               mPropName == "onerror") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMLoadListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      NS_IF_RELEASE(mManager);
    }
  }
  return PR_TRUE;
}

PRBool    GlobalWindowImpl::EnumerateProperty(JSContext *aContext)
{
  return PR_TRUE;
}

PRBool    GlobalWindowImpl::Resolve(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

PRBool    GlobalWindowImpl::Convert(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

void      GlobalWindowImpl::Finalize(JSContext *aContext)
{
}

nsresult GlobalWindowImpl::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (nsnull != mListenerManager) {
    return mListenerManager->QueryInterface(kIEventListenerManagerIID, (void**) aInstancePtrResult);;
  }
  //This is gonna get ugly.  Can't use NS_NewEventListenerManager because of a circular link problem.
  nsIDOMEventCapturer *mDoc;
  if (nsnull != mDocument && NS_OK == mDocument->QueryInterface(kIDOMEventCapturerIID, (void**)&mDoc)) {
    if (NS_OK == mDoc->GetNewListenerManager(aInstancePtrResult)) {
      mListenerManager = *aInstancePtrResult;
      NS_ADDREF(mListenerManager);
      NS_RELEASE(mDoc);
      return NS_OK;
    }
  }
  NS_IF_RELEASE(mDoc);
  return NS_ERROR_FAILURE;
}

//XXX I need another way around the circular link problem.
nsresult GlobalWindowImpl::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  return NS_ERROR_FAILURE;
}

nsresult GlobalWindowImpl::HandleDOMEvent(nsIPresContext& aPresContext, 
                                    nsEvent* aEvent, 
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus& aEventStatus)
{
  nsresult mRet = NS_OK;
  nsIDOMEvent* mDOMEvent = nsnull;

  if (DOM_EVENT_INIT == aFlags) {
    nsIEventStateManager *mManager;
    if (NS_OK == aPresContext.GetEventStateManager(&mManager)) {
      mManager->SetEventTarget((nsIDOMWindow*)this);
      NS_RELEASE(mManager);
    }
 
    aDOMEvent = &mDOMEvent;
  }
  
  //Capturing stage
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
  }

  //Bubbling stage
  /*Up to frames?*/

  if (DOM_EVENT_INIT == aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      if (0 != (*aDOMEvent)->Release()) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *mPrivateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(kIPrivateDOMEventIID, (void**)&mPrivateEvent)) {
          mPrivateEvent->DuplicatePrivateData();
          NS_RELEASE(mPrivateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return mRet;
}

nsresult GlobalWindowImpl::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    mManager->AddEventListener(aListener, aIID);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult GlobalWindowImpl::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListener(aListener, aIID);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult GlobalWindowImpl::CaptureEvent(nsIDOMEventListener *aListener)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    mManager->CaptureEvent(aListener);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult GlobalWindowImpl::ReleaseEvent(nsIDOMEventListener *aListener)
{
  if (nsnull != mListenerManager) {
    mListenerManager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

extern "C" NS_DOM nsresult
NS_NewScriptGlobalObject(nsIScriptGlobalObject **aResult)
{
  if (nsnull == aResult) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  GlobalWindowImpl *global = new GlobalWindowImpl();
  if (nsnull == global) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return global->QueryInterface(kIScriptGlobalObjectIID, (void **)aResult);
}



//
//  Navigator class implementation 
//
NavigatorImpl::NavigatorImpl()
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
}

NavigatorImpl::~NavigatorImpl()
{
}

NS_IMPL_ADDREF(NavigatorImpl)
NS_IMPL_RELEASE(NavigatorImpl)

nsresult 
NavigatorImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNavigatorIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMNavigator*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult 
NavigatorImpl::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

nsresult 
NavigatorImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptNavigator(aContext, this, global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetUserAgent(nsString& aUserAgent)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {
    nsINetContainerApplication *container;

    res = service->GetContainerApplication(&container);
    if ((NS_OK == res) && (nsnull != container)) {
      nsAutoString appVersion;
      container->GetAppCodeName(aUserAgent);
      container->GetAppVersion(appVersion);

      aUserAgent.Append('/');
      aUserAgent.Append(appVersion);
      NS_RELEASE(container);
    }
    
    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppCodeName(nsString& aAppCodeName)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {
    nsINetContainerApplication *container;

    res = service->GetContainerApplication(&container);
    if ((NS_OK == res) && (nsnull != container)) {
      res = container->GetAppCodeName(aAppCodeName);
      NS_RELEASE(container);
    }
    
    NS_RELEASE(service);
  }
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppVersion(nsString& aAppVersion)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {
    nsINetContainerApplication *container;

    res = service->GetContainerApplication(&container);
    if ((NS_OK == res) && (nsnull != container)) {
      res = container->GetAppVersion(aAppVersion);
      NS_RELEASE(container);
    }
    
    NS_RELEASE(service);
  }
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppName(nsString& aAppName)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {
    nsINetContainerApplication *container;

    res = service->GetContainerApplication(&container);
    if ((NS_OK == res) && (nsnull != container)) {
      res = container->GetAppName(aAppName);
      NS_RELEASE(container);
    }
    
    NS_RELEASE(service);
  }
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetLanguage(nsString& aLanguage)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {
    nsINetContainerApplication *container;

    res = service->GetContainerApplication(&container);
    if ((NS_OK == res) && (nsnull != container)) {
      res = container->GetLanguage(aLanguage);
      NS_RELEASE(container);
    }
    
    NS_RELEASE(service);
  }
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetPlatform(nsString& aPlatform)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {
    nsINetContainerApplication *container;

    res = service->GetContainerApplication(&container);
    if ((NS_OK == res) && (nsnull != container)) {
      res = container->GetPlatform(aPlatform);
      NS_RELEASE(container);
    }
    
    NS_RELEASE(service);
  }
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetSecurityPolicy(nsString& aSecurityPolicy)
{
  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::JavaEnabled(PRBool* aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}
