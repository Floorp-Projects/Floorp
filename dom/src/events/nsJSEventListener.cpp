/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsJSEventListener.h"
#include "nsJSUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptRuntime.h"
#include "nsIXPConnect.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsDOMScriptObjectHolder.h"
#include "nsIMutableArray.h"
#include "nsVariant.h"
#include "nsIDOMBeforeUnloadEvent.h"
#include "nsGkAtoms.h"
#include "nsIDOMEventTarget.h"
#include "nsIJSContextStack.h"
#include "xpcpublic.h"
#include "nsJSEnvironment.h"
#include "nsDOMJSUtils.h"
#ifdef DEBUG

#include "nspr.h" // PR_fprintf

class EventListenerCounter
{
public:
  ~EventListenerCounter() {
  }
};

static EventListenerCounter sEventListenerCounter;
#endif

/*
 * nsJSEventListener implementation
 */
nsJSEventListener::nsJSEventListener(nsIScriptContext *aContext,
                                     JSObject* aScopeObject,
                                     nsISupports *aTarget,
                                     nsIAtom* aType,
                                     JSObject *aHandler)
  : nsIJSEventListener(aContext, aScopeObject, aTarget, aHandler),
    mEventName(aType)
{
  // aScopeObject is the inner window's JS object, which we need to lock
  // until we are done with it.
  NS_ASSERTION(aScopeObject && aContext,
               "EventListener with no context or scope?");
  NS_HOLD_JS_OBJECTS(this, nsJSEventListener);
}

nsJSEventListener::~nsJSEventListener() 
{
  if (mContext) {
    NS_DROP_JS_OBJECTS(this, nsJSEventListener);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSEventListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSEventListener)
  if (tmp->mContext) {
    NS_DROP_JS_OBJECTS(tmp, nsJSEventListener);
    tmp->mScopeObject = nsnull;
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContext)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsJSEventListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSEventListener)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScopeObject)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mHandler)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsJSEventListener)
  return tmp->IsBlackForCC();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsJSEventListener)
  return tmp->IsBlackForCC();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsJSEventListener)
  return tmp->IsBlackForCC();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIJSEventListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSEventListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSEventListener)

bool
nsJSEventListener::IsBlackForCC()
{
  if (mContext &&
      (!mScopeObject || !xpc_IsGrayGCThing(mScopeObject)) &&
      (!mHandler || !xpc_IsGrayGCThing(mHandler))) {
    nsIScriptGlobalObject* sgo =
      static_cast<nsJSContext*>(mContext.get())->GetCachedGlobalObject();
    return sgo && sgo->IsBlackForCC();
  }
  return false;
}

nsresult
nsJSEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mTarget);
  if (!target || !mContext || !mHandler)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIMutableArray> iargv;

  bool handledScriptError = false;
  if (mEventName == nsGkAtoms::onerror) {
    NS_ENSURE_TRUE(aEvent, NS_ERROR_UNEXPECTED);

    nsEvent* event = aEvent->GetInternalNSEvent();
    if (event->message == NS_LOAD_ERROR &&
        event->eventStructType == NS_SCRIPT_ERROR_EVENT) {
      nsScriptErrorEvent *scriptEvent =
        static_cast<nsScriptErrorEvent*>(event);
      // Create a temp argv for the error event.
      iargv = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;
      // Append the event args.
      nsCOMPtr<nsIWritableVariant>
          var(do_CreateInstance(NS_VARIANT_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = var->SetAsWString(scriptEvent->errorMsg);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = iargv->AppendElement(var, false);
      NS_ENSURE_SUCCESS(rv, rv);
      // filename
      var = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = var->SetAsWString(scriptEvent->fileName);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = iargv->AppendElement(var, false);
      NS_ENSURE_SUCCESS(rv, rv);
      // line number
      var = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = var->SetAsUint32(scriptEvent->lineNr);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = iargv->AppendElement(var, false);
      NS_ENSURE_SUCCESS(rv, rv);

      handledScriptError = true;
    }
  }

  if (!handledScriptError) {
    iargv = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    NS_ENSURE_TRUE(iargv != nsnull, NS_ERROR_OUT_OF_MEMORY);
    rv = iargv->AppendElement(aEvent, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // mContext is the same context which event listener manager pushes
  // to JS context stack.
#ifdef DEBUG
  JSContext* cx = nsnull;
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  NS_ASSERTION(stack && NS_SUCCEEDED(stack->Peek(&cx)) && cx &&
               GetScriptContextFromJSContext(cx) == mContext,
               "JSEventListener has wrong script context?");
#endif
  nsCOMPtr<nsIVariant> vrv;
  xpc_UnmarkGrayObject(mScopeObject);
  xpc_UnmarkGrayObject(mHandler);
  rv = mContext->CallEventHandler(mTarget, mScopeObject, mHandler, iargv,
                                  getter_AddRefs(vrv));

  if (NS_SUCCEEDED(rv)) {
    PRUint16 dataType = nsIDataType::VTYPE_VOID;
    if (vrv)
      vrv->GetDataType(&dataType);

    if (mEventName == nsGkAtoms::onbeforeunload) {
      nsCOMPtr<nsIDOMBeforeUnloadEvent> beforeUnload = do_QueryInterface(aEvent);
      NS_ENSURE_STATE(beforeUnload);

      if (dataType != nsIDataType::VTYPE_VOID) {
        aEvent->PreventDefault();
        nsAutoString text;
        beforeUnload->GetReturnValue(text);

        // Set the text in the beforeUnload event as long as it wasn't
        // already set (through event.returnValue, which takes
        // precedence over a value returned from a JS function in IE)
        if ((dataType == nsIDataType::VTYPE_DOMSTRING ||
             dataType == nsIDataType::VTYPE_CHAR_STR ||
             dataType == nsIDataType::VTYPE_WCHAR_STR ||
             dataType == nsIDataType::VTYPE_STRING_SIZE_IS ||
             dataType == nsIDataType::VTYPE_WSTRING_SIZE_IS ||
             dataType == nsIDataType::VTYPE_CSTRING ||
             dataType == nsIDataType::VTYPE_ASTRING)
            && text.IsEmpty()) {
          vrv->GetAsDOMString(text);
          beforeUnload->SetReturnValue(text);
        }
      }
    } else if (dataType == nsIDataType::VTYPE_BOOL) {
      // If the handler returned false and its sense is not reversed,
      // or the handler returned true and its sense is reversed from
      // the usual (false means cancel), then prevent default.
      bool brv;
      if (NS_SUCCEEDED(vrv->GetAsBool(&brv)) &&
          brv == (mEventName == nsGkAtoms::onerror ||
                  mEventName == nsGkAtoms::onmouseover)) {
        aEvent->PreventDefault();
      }
    }
  }

  return rv;
}

/* virtual */ void
nsJSEventListener::SetHandler(JSObject *aHandler)
{
  // Technically we should drop the old mHandler and hold the new
  // one... except for JS this is a no-op, and we're really not
  // pretending very hard to support anything else.  And since we
  // can't in fact only drop one script object (we'd have to drop
  // mScope too, and then re-hold it), let's just not worry about it
  // all.
  mHandler = aHandler;
}

/*
 * Factory functions
 */

nsresult
NS_NewJSEventListener(nsIScriptContext* aContext, JSObject* aScopeObject,
                      nsISupports*aTarget, nsIAtom* aEventType,
                      JSObject* aHandler, nsIJSEventListener** aReturn)
{
  NS_ENSURE_ARG(aEventType);
  nsJSEventListener* it =
    new nsJSEventListener(aContext, aScopeObject, aTarget, aEventType,
                          aHandler);
  NS_ADDREF(*aReturn = it);

  return NS_OK;
}
