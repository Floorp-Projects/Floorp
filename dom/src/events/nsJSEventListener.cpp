/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsJSEventListener.h"
#include "nsJSUtils.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptRuntime.h"
#include "nsIXPConnect.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsIMutableArray.h"
#include "nsVariant.h"
#include "nsIDOMBeforeUnloadEvent.h"
#include "nsGkAtoms.h"
#include "nsIDOMEventTarget.h"
#include "nsIJSContextStack.h"
#include "xpcpublic.h"
#include "nsJSEnvironment.h"
#include "nsDOMJSUtils.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsDOMEvent.h"

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

using namespace mozilla;
using namespace mozilla::dom;

/*
 * nsJSEventListener implementation
 */
nsJSEventListener::nsJSEventListener(nsIScriptContext *aContext,
                                     JSObject* aScopeObject,
                                     nsISupports *aTarget,
                                     nsIAtom* aType,
                                     const nsEventHandler& aHandler)
  : nsIJSEventListener(aContext, aScopeObject, aTarget, aType, aHandler)
{
  if (mScopeObject) {
    NS_HOLD_JS_OBJECTS(this, nsJSEventListener);
  }
}

nsJSEventListener::~nsJSEventListener() 
{
  if (mScopeObject) {
    mScopeObject = nullptr;
    NS_DROP_JS_OBJECTS(this, nsJSEventListener);
  }
}

/* virtual */
void
nsJSEventListener::UpdateScopeObject(JSObject* aScopeObject)
{
  if (mScopeObject && !aScopeObject) {
    mScopeObject = nullptr;
    NS_DROP_JS_OBJECTS(this, nsJSEventListener);
  } else if (aScopeObject && !mScopeObject) {
    NS_HOLD_JS_OBJECTS(this, nsJSEventListener);
  }
  mScopeObject = aScopeObject;
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSEventListener)
  if (tmp->mScopeObject) {
    tmp->mScopeObject = nullptr;
    NS_DROP_JS_OBJECTS(tmp, nsJSEventListener);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  }
  tmp->mHandler.ForgetHandler();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSEventListener)
  if (MOZ_UNLIKELY(cb.WantDebugInfo()) && tmp->mEventName) {
    nsAutoCString name;
    name.AppendLiteral("nsJSEventListener handlerName=");
    name.Append(
      NS_ConvertUTF16toUTF8(nsDependentAtomString(tmp->mEventName)).get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name.get());
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSEventListener, tmp->mRefCnt.get())
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mHandler.Ptr())
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSEventListener)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScopeObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsJSEventListener)
  if (tmp->IsBlackForCC()) {
    return true;
  }
  // If we have a target, it is the one which has tmp as onfoo handler.
  if (tmp->mTarget) {
    nsXPCOMCycleCollectionParticipant* cp = nullptr;
    CallQueryInterface(tmp->mTarget, &cp);
    nsISupports* canonical = nullptr;
    tmp->mTarget->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                                 reinterpret_cast<void**>(&canonical));
    // Usually CanSkip ends up unmarking the event listeners of mTarget,
    // so tmp may become black.
    if (cp && canonical && cp->CanSkip(canonical, true)) {
      return tmp->IsBlackForCC();
    }
  }
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
  // We can claim to be black if all the things we reference are
  // effectively black already.
  if ((!mScopeObject || !xpc_IsGrayGCThing(mScopeObject)) &&
      (!mHandler.HasEventHandler() ||
       !mHandler.Ptr()->HasGrayCallable())) {
    if (!mContext) {
      // Well, we certainly won't be marking it, so move on!
      return true;
    }
    nsIScriptGlobalObject* sgo =
      static_cast<nsJSContext*>(mContext.get())->GetCachedGlobalObject();
    return sgo && sgo->IsBlackForCC();
  }
  return false;
}

nsresult
nsJSEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<EventTarget> target = do_QueryInterface(mTarget);
  if (!target || !mHandler.HasEventHandler())
    return NS_ERROR_FAILURE;

  if (mHandler.Type() == nsEventHandler::eOnError) {
    MOZ_ASSERT(mEventName == nsGkAtoms::onerror);

    nsString errorMsg, file;
    EventOrString msgOrEvent;
    Optional<nsAString> fileName;
    Optional<uint32_t> lineNumber;
    Optional<uint32_t> columnNumber;

    NS_ENSURE_TRUE(aEvent, NS_ERROR_UNEXPECTED);
    nsEvent* event = aEvent->GetInternalNSEvent();
    if (event->message == NS_LOAD_ERROR &&
        event->eventStructType == NS_SCRIPT_ERROR_EVENT) {
      nsScriptErrorEvent *scriptEvent =
        static_cast<nsScriptErrorEvent*>(event);
      errorMsg = scriptEvent->errorMsg;
      msgOrEvent.SetAsString() = static_cast<nsAString*>(&errorMsg);

      file = scriptEvent->fileName;
      fileName = &file;

      lineNumber.Construct();
      lineNumber.Value() = scriptEvent->lineNr;
    } else {
      msgOrEvent.SetAsEvent() = aEvent->InternalDOMEvent();
    }

    nsRefPtr<OnErrorEventHandlerNonNull> handler =
      mHandler.OnErrorEventHandler();
    ErrorResult rv;
    bool handled = handler->Call(mTarget, msgOrEvent, fileName, lineNumber,
                                 columnNumber, rv);
    if (rv.Failed()) {
      return rv.ErrorCode();
    }

    if (handled) {
      aEvent->PreventDefault();
    }
    return NS_OK;
  }

  if (mHandler.Type() == nsEventHandler::eOnBeforeUnload) {
    MOZ_ASSERT(mEventName == nsGkAtoms::onbeforeunload);

    nsRefPtr<BeforeUnloadEventHandlerNonNull> handler =
      mHandler.BeforeUnloadEventHandler();
    ErrorResult rv;
    nsString retval;
    handler->Call(mTarget, *(aEvent->InternalDOMEvent()), retval, rv);
    if (rv.Failed()) {
      return rv.ErrorCode();
    }

    nsCOMPtr<nsIDOMBeforeUnloadEvent> beforeUnload = do_QueryInterface(aEvent);
    NS_ENSURE_STATE(beforeUnload);

    if (!DOMStringIsNull(retval)) {
      aEvent->PreventDefault();

      nsAutoString text;
      beforeUnload->GetReturnValue(text);

      // Set the text in the beforeUnload event as long as it wasn't
      // already set (through event.returnValue, which takes
      // precedence over a value returned from a JS function in IE)
      if (text.IsEmpty()) {
        beforeUnload->SetReturnValue(retval);
      }
    }

    return NS_OK;
  }

  MOZ_ASSERT(mHandler.Type() == nsEventHandler::eNormal);
  ErrorResult rv;
  nsRefPtr<EventHandlerNonNull> handler = mHandler.EventHandler();
  JS::Value retval =
    handler->Call(mTarget, *(aEvent->InternalDOMEvent()), rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  // If the handler returned false and its sense is not reversed,
  // or the handler returned true and its sense is reversed from
  // the usual (false means cancel), then prevent default.
  if (retval.isBoolean() &&
      retval.toBoolean() == (mEventName == nsGkAtoms::onerror ||
                             mEventName == nsGkAtoms::onmouseover)) {
    aEvent->PreventDefault();
  }

  return NS_OK;
}

/*
 * Factory functions
 */

nsresult
NS_NewJSEventListener(nsIScriptContext* aContext, JSObject* aScopeObject,
                      nsISupports*aTarget, nsIAtom* aEventType,
                      const nsEventHandler& aHandler,
                      nsIJSEventListener** aReturn)
{
  MOZ_ASSERT(aContext || aHandler.HasEventHandler(),
             "Must have a handler if we don't have an nsIScriptContext");
  NS_ENSURE_ARG(aEventType);
  nsJSEventListener* it =
    new nsJSEventListener(aContext, aScopeObject, aTarget, aEventType,
                          aHandler);
  NS_ADDREF(*aReturn = it);

  return NS_OK;
}
