/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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

#include "nsISupports.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsEventListenerManager.h"
#include "nsCaret.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEventListener.h"
#include "nsITextControlFrame.h"
#include "nsGkAtoms.h"
#include "nsPIDOMWindow.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIJSEventListener.h"
#include "prmem.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptRuntime.h"
#include "nsLayoutUtils.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIJSContextStack.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsMutationEvent.h"
#include "nsIXPConnect.h"
#include "nsDOMCID.h"
#include "nsIScriptObjectOwner.h" // for nsIScriptEventHandlerOwner
#include "nsFocusManager.h"
#include "nsIDOMElement.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsContentCID.h"
#include "nsEventDispatcher.h"
#include "nsDOMJSUtils.h"
#include "nsDOMScriptObjectHolder.h"
#include "nsDataHashtable.h"
#include "nsCOMArray.h"
#include "nsEventListenerService.h"
#include "nsDOMEvent.h"
#include "nsIContentSecurityPolicy.h"

using namespace mozilla::dom;

#define EVENT_TYPE_EQUALS( ls, type, userType ) \
  (ls->mEventType == type && \
  (ls->mEventType != NS_USER_DEFINED_EVENT || ls->mTypeAtom == userType))

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

static const PRUint32 kAllMutationBits =
  NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
  NS_EVENT_BITS_MUTATION_NODEINSERTED |
  NS_EVENT_BITS_MUTATION_NODEREMOVED |
  NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
  NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT |
  NS_EVENT_BITS_MUTATION_ATTRMODIFIED |
  NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;

static PRUint32
MutationBitForEventType(PRUint32 aEventType)
{
  switch (aEventType) {
    case NS_MUTATION_SUBTREEMODIFIED:
      return NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
    case NS_MUTATION_NODEINSERTED:
      return NS_EVENT_BITS_MUTATION_NODEINSERTED;
    case NS_MUTATION_NODEREMOVED:
      return NS_EVENT_BITS_MUTATION_NODEREMOVED;
    case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
    case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
    case NS_MUTATION_ATTRMODIFIED:
      return NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
    case NS_MUTATION_CHARACTERDATAMODIFIED:
      return NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
    default:
      break;
  }
  return 0;
}

PRUint32 nsEventListenerManager::sCreatedCount = 0;

nsEventListenerManager::nsEventListenerManager(nsISupports* aTarget) :
  mMayHavePaintEventListener(PR_FALSE),
  mMayHaveMutationListeners(PR_FALSE),
  mMayHaveCapturingListeners(PR_FALSE),
  mMayHaveSystemGroupListeners(PR_FALSE),
  mMayHaveAudioAvailableEventListener(PR_FALSE),
  mMayHaveTouchEventListener(PR_FALSE),
  mNoListenerForEvent(0),
  mTarget(aTarget)
{
  NS_ASSERTION(aTarget, "unexpected null pointer");

  ++sCreatedCount;
}

nsEventListenerManager::~nsEventListenerManager() 
{
  // If your code fails this assertion, a possible reason is that
  // a class did not call our Disconnect() manually. Note that
  // this class can have Disconnect called in one of two ways:
  // if it is part of a cycle, then in Unlink() (such a cycle
  // would be with one of the listeners, not mTarget which is weak).
  // If not part of a cycle, then Disconnect must be called manually,
  // typically from the destructor of the owner class (mTarget).
  // XXX azakai: Is there any reason to not just call Disconnect
  //             from right here, if not previously called?
  NS_ASSERTION(!mTarget, "didn't call Disconnect");
  RemoveAllListeners();

}

void
nsEventListenerManager::RemoveAllListeners()
{
  mListeners.Clear();
}

void
nsEventListenerManager::Shutdown()
{
  sAddListenerID = JSID_VOID;
  nsDOMEvent::Shutdown();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEventListenerManager)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsEventListenerManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsEventListenerManager, Release)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsEventListenerManager)
  PRUint32 count = tmp->mListeners.Length();
  for (PRUint32 i = 0; i < count; i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mListeners[i] mListener");
    cb.NoteXPCOMChild(tmp->mListeners.ElementAt(i).mListener.get());
  }  
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(nsEventListenerManager)
  tmp->Disconnect();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


nsPIDOMWindow*
nsEventListenerManager::GetInnerWindowForTarget()
{
  nsCOMPtr<nsINode> node = do_QueryInterface(mTarget);
  if (node) {
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    nsIDocument* document = node->GetOwnerDoc();
    if (document)
      return document->GetInnerWindow();
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mTarget);
  if (window) {
    NS_ASSERTION(window->IsInnerWindow(), "Target should not be an outer window");
    return window;
  }

  return nsnull;
}

nsresult
nsEventListenerManager::AddEventListener(nsIDOMEventListener *aListener,
                                         PRUint32 aType,
                                         nsIAtom* aTypeAtom,
                                         PRInt32 aFlags)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aType, NS_ERROR_FAILURE);

  nsRefPtr<nsIDOMEventListener> kungFuDeathGrip = aListener;

  nsListenerStruct* ls;
  PRUint32 count = mListeners.Length();
  for (PRUint32 i = 0; i < count; i++) {
    ls = &mListeners.ElementAt(i);
    if (ls->mListener == aListener && ls->mFlags == aFlags &&
        EVENT_TYPE_EQUALS(ls, aType, aTypeAtom)) {
      return NS_OK;
    }
  }

  mNoListenerForEvent = NS_EVENT_TYPE_NULL;
  mNoListenerForEventAtom = nsnull;

  ls = mListeners.AppendElement();
  ls->mListener = aListener;
  ls->mEventType = aType;
  ls->mTypeAtom = aTypeAtom;
  ls->mFlags = aFlags;
  ls->mHandlerIsString = PR_FALSE;

  if (aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) {
    mMayHaveSystemGroupListeners = PR_TRUE;
  }
  if (aFlags & NS_EVENT_FLAG_CAPTURE) {
    mMayHaveCapturingListeners = PR_TRUE;
  }

  if (aType == NS_AFTERPAINT) {
    mMayHavePaintEventListener = PR_TRUE;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasPaintEventListeners();
    }
#ifdef MOZ_MEDIA
  } else if (aType == NS_MOZAUDIOAVAILABLE) {
    mMayHaveAudioAvailableEventListener = PR_TRUE;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasAudioAvailableEventListeners();
    }
#endif // MOZ_MEDIA
  } else if (aType >= NS_MUTATION_START && aType <= NS_MUTATION_END) {
    // For mutation listeners, we need to update the global bit on the DOM window.
    // Otherwise we won't actually fire the mutation event.
    mMayHaveMutationListeners = PR_TRUE;
    // Go from our target to the nearest enclosing DOM window.
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      // If aType is NS_MUTATION_SUBTREEMODIFIED, we need to listen all
      // mutations. nsContentUtils::HasMutationListeners relies on this.
      window->SetMutationListeners((aType == NS_MUTATION_SUBTREEMODIFIED) ?
                                   kAllMutationBits :
                                   MutationBitForEventType(aType));
    }
  } else if (aTypeAtom == nsGkAtoms::ondeviceorientation ||
             aTypeAtom == nsGkAtoms::ondevicemotion) {
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window)
      window->SetHasOrientationEventListener();
  } else if ((aType >= NS_MOZTOUCH_DOWN && aType <= NS_MOZTOUCH_UP) ||
             (aTypeAtom == nsGkAtoms::ontouchstart ||
              aTypeAtom == nsGkAtoms::ontouchend ||
              aTypeAtom == nsGkAtoms::ontouchmove ||
              aTypeAtom == nsGkAtoms::ontouchenter ||
              aTypeAtom == nsGkAtoms::ontouchleave ||
              aTypeAtom == nsGkAtoms::ontouchcancel)) {
    mMayHaveTouchEventListener = PR_TRUE;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window)
      window->SetHasTouchEventListeners();
  }

  return NS_OK;
}

void
nsEventListenerManager::RemoveEventListener(nsIDOMEventListener *aListener, 
                                            PRUint32 aType,
                                            nsIAtom* aUserType,
                                            PRInt32 aFlags)
{
  if (!aListener || !aType) {
    return;
  }

  nsListenerStruct* ls;
  aFlags &= ~NS_PRIV_EVENT_UNTRUSTED_PERMITTED;

  PRUint32 count = mListeners.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    ls = &mListeners.ElementAt(i);
    if (ls->mListener == aListener &&
        ((ls->mFlags & ~NS_PRIV_EVENT_UNTRUSTED_PERMITTED) == aFlags) &&
        EVENT_TYPE_EQUALS(ls, aType, aUserType)) {
      nsRefPtr<nsEventListenerManager> kungFuDeathGrip = this;
      mListeners.RemoveElementAt(i);
      mNoListenerForEvent = NS_EVENT_TYPE_NULL;
      mNoListenerForEventAtom = nsnull;
      break;
    }
  }
}

static inline PRBool
ListenerCanHandle(nsListenerStruct* aLs, nsEvent* aEvent)
{
  // This is slightly different from EVENT_TYPE_EQUALS in that it returns
  // true even when aEvent->message == NS_USER_DEFINED_EVENT and
  // aLs=>mEventType != NS_USER_DEFINED_EVENT as long as the atoms are the same
  return aEvent->message == NS_USER_DEFINED_EVENT ?
    (aLs->mTypeAtom == aEvent->userType) :
    (aLs->mEventType == aEvent->message);
}

nsresult
nsEventListenerManager::AddEventListenerByType(nsIDOMEventListener *aListener, 
                                               const nsAString& aType,
                                               PRInt32 aFlags)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aType);
  PRUint32 type = nsContentUtils::GetEventId(atom);
  return AddEventListener(aListener, type, atom, aFlags);
}

void
nsEventListenerManager::RemoveEventListenerByType(nsIDOMEventListener *aListener, 
                                                  const nsAString& aType,
                                                  PRInt32 aFlags)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aType);
  PRUint32 type = nsContentUtils::GetEventId(atom);
  RemoveEventListener(aListener, type, atom, aFlags);
}

nsListenerStruct*
nsEventListenerManager::FindJSEventListener(PRUint32 aEventType,
                                            nsIAtom* aTypeAtom)
{
  // Run through the listeners for this type and see if a script
  // listener is registered
  nsListenerStruct *ls;
  PRUint32 count = mListeners.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    ls = &mListeners.ElementAt(i);
    if (EVENT_TYPE_EQUALS(ls, aEventType, aTypeAtom) &&
        ls->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
      return ls;
    }
  }
  return nsnull;
}

nsresult
nsEventListenerManager::SetJSEventListener(nsIScriptContext *aContext,
                                           void *aScopeObject,
                                           nsIAtom* aName,
                                           PRBool aIsString,
                                           PRBool aPermitUntrustedEvents)
{
  nsresult rv = NS_OK;
  PRUint32 eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindJSEventListener(eventType, aName);

  if (!ls) {
    // If we didn't find a script listener or no listeners existed
    // create and add a new one.
    nsCOMPtr<nsIDOMEventListener> scriptListener;
    rv = NS_NewJSEventListener(aContext, aScopeObject, mTarget, aName,
                               getter_AddRefs(scriptListener));
    if (NS_SUCCEEDED(rv)) {
      AddEventListener(scriptListener, eventType, aName,
                       NS_EVENT_FLAG_BUBBLE | NS_PRIV_EVENT_FLAG_SCRIPT);

      ls = FindJSEventListener(eventType, aName);
    }
  }

  if (NS_SUCCEEDED(rv) && ls) {
    // Set flag to indicate possible need for compilation later
    ls->mHandlerIsString = aIsString;

    if (aPermitUntrustedEvents) {
      ls->mFlags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
    }
  }

  return rv;
}

nsresult
nsEventListenerManager::AddScriptEventListener(nsIAtom *aName,
                                               const nsAString& aBody,
                                               PRUint32 aLanguage,
                                               PRBool aDeferCompilation,
                                               PRBool aPermitUntrustedEvents)
{
  NS_PRECONDITION(aLanguage != nsIProgrammingLanguage::UNKNOWN,
                  "Must know the language for the script event listener");
  nsIScriptContext *context = nsnull;

  // |aPermitUntrustedEvents| is set to False for chrome - events
  // *generated* from an unknown source are not allowed.
  // However, for script languages with no 'sandbox', we want to reject
  // such scripts based on the source of their code, not just the source
  // of the event.
  if (aPermitUntrustedEvents && 
      aLanguage != nsIProgrammingLanguage::JAVASCRIPT) {
    NS_WARNING("Discarding non-JS event listener from untrusted source");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> node(do_QueryInterface(mTarget));

  nsCOMPtr<nsIDocument> doc;

  nsCOMPtr<nsIScriptGlobalObject> global;

  if (node) {
    // Try to get context from doc
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    doc = node->GetOwnerDoc();
    if (doc)
      global = doc->GetScriptGlobalObject();
  } else {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mTarget));
    if (win) {
      NS_ASSERTION(win->IsInnerWindow(),
                   "Event listener added to outer window!");

      nsCOMPtr<nsIDOMDocument> domdoc;
      win->GetDocument(getter_AddRefs(domdoc));
      doc = do_QueryInterface(domdoc);
      global = do_QueryInterface(win);
    } else {
      global = do_QueryInterface(mTarget);
    }
  }

  if (!global) {
    // This can happen; for example this document might have been
    // loaded as data.
    return NS_OK;
  }

  nsresult rv = NS_OK;
  // return early preventing the event listener from being added
  // 'doc' is fetched above
  if (doc) {
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);

    if (csp) {
      PRBool inlineOK;
      rv = csp->GetAllowsInlineScript(&inlineOK);
      NS_ENSURE_SUCCESS(rv, rv);

      if ( !inlineOK ) {
        // gather information to log with violation report
        nsIURI* uri = doc->GetDocumentURI();
        nsCAutoString asciiSpec;
        if (uri)
          uri->GetAsciiSpec(asciiSpec);
        nsAutoString scriptSample, attr, tagName(NS_LITERAL_STRING("UNKNOWN"));
        aName->ToString(attr);
        nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mTarget));
        if (domNode)
          domNode->GetNodeName(tagName);
        // build a "script sample" based on what we know about this element
        scriptSample.Assign(attr);
        scriptSample.AppendLiteral(" attribute on ");
        scriptSample.Append(tagName);
        scriptSample.AppendLiteral(" element");
        csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_INLINE_SCRIPT,
                                 NS_ConvertUTF8toUTF16(asciiSpec),
                                 scriptSample,
                                 nsnull);
        return NS_OK;
      }
    }
  }

  // This might be the first reference to this language in the global
  // We must init the language before we attempt to fetch its context.
  if (NS_FAILED(global->EnsureScriptEnvironment(aLanguage))) {
    NS_WARNING("Failed to setup script environment for this language");
    // but fall through and let the inevitable failure below handle it.
  }

  context = global->GetScriptContext(aLanguage);
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

  void *scope = global->GetScriptGlobal(aLanguage);

  if (!aDeferCompilation) {
    nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner =
      do_QueryInterface(mTarget);

    nsScriptObjectHolder handler(context);
    PRBool done = PR_FALSE;

    if (handlerOwner) {
      rv = handlerOwner->GetCompiledEventHandler(aName, handler);
      if (NS_SUCCEEDED(rv) && handler) {
        rv = context->BindCompiledEventHandler(mTarget, scope, aName, handler);
        if (NS_FAILED(rv))
          return rv;
        done = PR_TRUE;
      }
    }

    if (!done) {
      PRUint32 lineNo = 0;
      nsCAutoString url (NS_LITERAL_CSTRING("-moz-evil:lying-event-listener"));
      if (doc) {
        nsIURI *uri = doc->GetDocumentURI();
        if (uri) {
          uri->GetSpec(url);
          lineNo = 1;
        }
      }

      if (handlerOwner) {
        // Always let the handler owner compile the event handler, as
        // it may want to use a special context or scope object.
        rv = handlerOwner->CompileEventHandler(context, mTarget, aName,
                                               aBody, url.get(), lineNo, handler);
      }
      else {
        PRInt32 nameSpace = kNameSpaceID_Unknown;
        if (node && node->IsNodeOfType(nsINode::eCONTENT)) {
          nsIContent* content = static_cast<nsIContent*>(node.get());
          nameSpace = content->GetNameSpaceID();
        }
        else if (doc) {
          Element* root = doc->GetRootElement();
          if (root)
            nameSpace = root->GetNameSpaceID();
        }
        PRUint32 argCount;
        const char **argNames;
        nsContentUtils::GetEventArgNames(nameSpace, aName, &argCount,
                                         &argNames);

        nsCxPusher pusher;
        if (!pusher.Push((JSContext*)context->GetNativeContext())) {
          return NS_ERROR_FAILURE;
        }

        rv = context->CompileEventHandler(aName, argCount, argNames,
                                          aBody,
                                          url.get(), lineNo,
                                          SCRIPTVERSION_DEFAULT, // for now?
                                          handler);
        if (rv == NS_ERROR_ILLEGAL_VALUE) {
          NS_WARNING("Probably a syntax error in the event handler!");
          return NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA;
        }
        NS_ENSURE_SUCCESS(rv, rv);
        // And bind it.
        rv = context->BindCompiledEventHandler(mTarget, scope,
                                               aName, handler);
      }
      if (NS_FAILED(rv)) return rv;
    }
  }

  return SetJSEventListener(context, scope, aName, aDeferCompilation,
                            aPermitUntrustedEvents);
}

void
nsEventListenerManager::RemoveScriptEventListener(nsIAtom* aName)
{
  PRUint32 eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindJSEventListener(eventType, aName);

  if (ls) {
    mListeners.RemoveElementAt(PRUint32(ls - &mListeners.ElementAt(0)));
    mNoListenerForEvent = NS_EVENT_TYPE_NULL;
    mNoListenerForEventAtom = nsnull;
  }
}

jsid
nsEventListenerManager::sAddListenerID = JSID_VOID;

nsresult
nsEventListenerManager::RegisterScriptEventListener(nsIScriptContext *aContext,
                                                    void *aScope,
                                                    nsIAtom *aName)
{
  // Check that we have access to set an event listener. Prevents
  // snooping attacks across domains by setting onkeypress handlers,
  // for instance.
  // You'd think it'd work just to get the JSContext from aContext,
  // but that's actually the JSContext whose private object parents
  // the object in mTarget.
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
    return rv;
  JSContext *cx;
  if (NS_FAILED(rv = stack->Peek(&cx)))
    return rv;

  if (cx) {
    if (sAddListenerID == JSID_VOID) {
      JSAutoRequest ar(cx);
      sAddListenerID =
        INTERNED_STRING_TO_JSID(cx, ::JS_InternString(cx, "addEventListener"));
    }

    if (aContext->GetScriptTypeID() == nsIProgrammingLanguage::JAVASCRIPT) {
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        jsval v;
        rv = nsContentUtils::WrapNative(cx, (JSObject *)aScope, mTarget, &v,
                                        getter_AddRefs(holder));
        NS_ENSURE_SUCCESS(rv, rv);
      
        rv = nsContentUtils::GetSecurityManager()->
          CheckPropertyAccess(cx, JSVAL_TO_OBJECT(v),
                              "EventTarget",
                              sAddListenerID,
                              nsIXPCSecurityManager::ACCESS_SET_PROPERTY);
        if (NS_FAILED(rv)) {
          // XXX set pending exception on the native call context?
          return rv;
        }
    } else {
        NS_WARNING("Skipping CheckPropertyAccess for non JS language");
    }
        
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  return SetJSEventListener(aContext, aScope, aName, PR_FALSE,
                            !nsContentUtils::IsCallerChrome());
}

nsresult
nsEventListenerManager::CompileScriptEventListener(nsIScriptContext *aContext, 
                                                   void *aScope,
                                                   nsIAtom *aName,
                                                   PRBool *aDidCompile)
{
  nsresult rv = NS_OK;
  *aDidCompile = PR_FALSE;
  PRUint32 eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindJSEventListener(eventType, aName);

  if (!ls) {
    //nothing to compile
    return NS_OK;
  }

  if (ls->mHandlerIsString) {
    rv = CompileEventHandlerInternal(aContext, aScope, mTarget, aName,
                                     ls, /*XXX fixme*/nsnull, PR_TRUE);
  }

  // Set *aDidCompile to true even if we didn't really compile
  // anything right now, if we get here it means that this event
  // handler has been compiled at some point, that's good enough for
  // us.

  *aDidCompile = PR_TRUE;

  return rv;
}

nsresult
nsEventListenerManager::CompileEventHandlerInternal(nsIScriptContext *aContext,
                                                    void *aScope,
                                                    nsISupports *aObject,
                                                    nsIAtom *aName,
                                                    nsListenerStruct *aListenerStruct,
                                                    nsISupports* aCurrentTarget,
                                                    PRBool aNeedsCxPush)
{
  nsresult result = NS_OK;

  nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner =
    do_QueryInterface(aObject);
  nsScriptObjectHolder handler(aContext);

  if (handlerOwner) {
    result = handlerOwner->GetCompiledEventHandler(aName,
                                                   handler);
    if (NS_SUCCEEDED(result) && handler) {
      // XXXmarkh - why do we bind here, but not after compilation below?
      result = aContext->BindCompiledEventHandler(aObject, aScope, aName, handler);
      aListenerStruct->mHandlerIsString = PR_FALSE;
    }
  }

  if (aListenerStruct->mHandlerIsString) {
    // This should never happen for anything but content
    // XXX I don't like that we have to reference content
    // from here. The alternative is to store the event handler
    // string on the JS object itself.
    nsCOMPtr<nsIContent> content = do_QueryInterface(aObject);
    NS_ASSERTION(content, "only content should have event handler attributes");
    if (content) {
      nsAutoString handlerBody;
      nsIAtom* attrName = aName;
      if (aName == nsGkAtoms::onSVGLoad)
        attrName = nsGkAtoms::onload;
      else if (aName == nsGkAtoms::onSVGUnload)
        attrName = nsGkAtoms::onunload;
      else if (aName == nsGkAtoms::onSVGAbort)
        attrName = nsGkAtoms::onabort;
      else if (aName == nsGkAtoms::onSVGError)
        attrName = nsGkAtoms::onerror;
      else if (aName == nsGkAtoms::onSVGResize)
        attrName = nsGkAtoms::onresize;
      else if (aName == nsGkAtoms::onSVGScroll)
        attrName = nsGkAtoms::onscroll;
      else if (aName == nsGkAtoms::onSVGZoom)
        attrName = nsGkAtoms::onzoom;
#ifdef MOZ_SMIL
      else if (aName == nsGkAtoms::onbeginEvent)
        attrName = nsGkAtoms::onbegin;
      else if (aName == nsGkAtoms::onrepeatEvent)
        attrName = nsGkAtoms::onrepeat;
      else if (aName == nsGkAtoms::onendEvent)
        attrName = nsGkAtoms::onend;
#endif // MOZ_SMIL

      content->GetAttr(kNameSpaceID_None, attrName, handlerBody);

      PRUint32 lineNo = 0;
      nsCAutoString url (NS_LITERAL_CSTRING("javascript:alert('TODO: FIXME')"));
      nsIDocument* doc = nsnull;
      nsCOMPtr<nsINode> node = do_QueryInterface(aCurrentTarget);
      if (node) {
        doc = node->GetOwnerDoc();
      }
      if (doc) {
        nsIURI *uri = doc->GetDocumentURI();
        if (uri) {
          uri->GetSpec(url);
          lineNo = 1;
        }
      }

      nsCxPusher pusher;
      if (aNeedsCxPush &&
          !pusher.Push((JSContext*)aContext->GetNativeContext())) {
        return NS_ERROR_FAILURE;
      }


      if (handlerOwner) {
        // Always let the handler owner compile the event
        // handler, as it may want to use a special
        // context or scope object.
        result = handlerOwner->CompileEventHandler(aContext, aObject, aName,
                                                   handlerBody,
                                                   url.get(), lineNo,
                                                   handler);
      }
      else {
        PRUint32 argCount;
        const char **argNames;
        nsContentUtils::GetEventArgNames(content->GetNameSpaceID(), aName,
                                         &argCount, &argNames);

        result = aContext->CompileEventHandler(aName,
                                               argCount, argNames,
                                               handlerBody,
                                               url.get(), lineNo,
                                               SCRIPTVERSION_DEFAULT, // for now?
                                               handler);
        NS_ENSURE_SUCCESS(result, result);
        // And bind it.
        result = aContext->BindCompiledEventHandler(aObject, aScope,
                                                    aName, handler);
        NS_ENSURE_SUCCESS(result, result);
      }

      if (NS_SUCCEEDED(result)) {
        aListenerStruct->mHandlerIsString = PR_FALSE;
      }
    }
  }

  return result;
}

nsresult
nsEventListenerManager::HandleEventSubType(nsListenerStruct* aListenerStruct,
                                           nsIDOMEventListener* aListener,
                                           nsIDOMEvent* aDOMEvent,
                                           nsIDOMEventTarget* aCurrentTarget,
                                           PRUint32 aPhaseFlags,
                                           nsCxPusher* aPusher)
{
  nsresult result = NS_OK;

  // If this is a script handler and we haven't yet
  // compiled the event handler itself
  if ((aListenerStruct->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) &&
      aListenerStruct->mHandlerIsString) {
    nsCOMPtr<nsIJSEventListener> jslistener = do_QueryInterface(aListener);
    if (jslistener) {
      // We probably have the atom already.
      nsCOMPtr<nsIAtom> atom = aListenerStruct->mTypeAtom;
      if (!atom) {
        nsAutoString eventString;
        if (NS_SUCCEEDED(aDOMEvent->GetType(eventString))) {
          atom = do_GetAtom(NS_LITERAL_STRING("on") + eventString);
        }
      }

      if (atom) {
#ifdef DEBUG
        nsAutoString type;
        aDOMEvent->GetType(type);
        nsCOMPtr<nsIAtom> eventAtom = do_GetAtom(NS_LITERAL_STRING("on") + type);
        NS_ASSERTION(eventAtom == atom, "Something wrong with event atoms!");
#endif
        result = CompileEventHandlerInternal(jslistener->GetEventContext(),
                                             jslistener->GetEventScope(),
                                             jslistener->GetEventTarget(),
                                             atom, aListenerStruct,
                                             aCurrentTarget,
                                             !jslistener->GetEventContext() ||
                                             jslistener->GetEventContext() !=
                                             aPusher->GetCurrentScriptContext());
      }
    }
  }

  if (NS_SUCCEEDED(result)) {
    // nsIDOMEvent::currentTarget is set in nsEventDispatcher.
    result = aListener->HandleEvent(aDOMEvent);
  }

  return result;
}

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

void
nsEventListenerManager::HandleEventInternal(nsPresContext* aPresContext,
                                            nsEvent* aEvent,
                                            nsIDOMEvent** aDOMEvent,
                                            nsIDOMEventTarget* aCurrentTarget,
                                            PRUint32 aFlags,
                                            nsEventStatus* aEventStatus,
                                            nsCxPusher* aPusher)
{
  //Set the value of the internal PreventDefault flag properly based on aEventStatus
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    aEvent->flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }

  nsAutoTObserverArray<nsListenerStruct, 2>::EndLimitedIterator iter(mListeners);
  nsAutoPopupStatePusher popupStatePusher(nsDOMEvent::GetEventPopupControlState(aEvent));
  PRBool hasListener = PR_FALSE;
  while (iter.HasMore()) {
    nsListenerStruct* ls = &iter.GetNext();
    // Check that the phase is same in event and event listener.
    // Handle only trusted events, except when listener permits untrusted events.
    if (ListenerCanHandle(ls, aEvent)) {
      hasListener = PR_TRUE;
      // XXX The (mFlags & aFlags) test here seems fragile. Shouldn't we
      // specifically only test the capture/bubble flags.
      if ((ls->mFlags & aFlags & ~NS_EVENT_FLAG_SYSTEM_EVENT) &&
          (ls->mFlags & NS_EVENT_FLAG_SYSTEM_EVENT) ==
          (aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) &&
          (NS_IS_TRUSTED_EVENT(aEvent) ||
           ls->mFlags & NS_PRIV_EVENT_UNTRUSTED_PERMITTED)) {
        if (!*aDOMEvent) {
          nsEventDispatcher::CreateEvent(aPresContext, aEvent,
                                         EmptyString(), aDOMEvent);
        }
        if (*aDOMEvent) {
          if (!aEvent->currentTarget) {
            aEvent->currentTarget = aCurrentTarget->GetTargetForDOMEvent();
            if (!aEvent->currentTarget) {
              break;
            }
          }
          nsRefPtr<nsIDOMEventListener> kungFuDeathGrip = ls->mListener;
          if (aPusher->RePush(aCurrentTarget)) {
            if (NS_FAILED(HandleEventSubType(ls, ls->mListener, *aDOMEvent,
                                             aCurrentTarget, aFlags,
                                             aPusher))) {
              aEvent->flags |= NS_EVENT_FLAG_EXCEPTION_THROWN;
            }
          }
        }
      }
    }
  }

  aEvent->currentTarget = nsnull;

  if (!hasListener) {
    mNoListenerForEvent = aEvent->message;
    mNoListenerForEventAtom = aEvent->userType;
  }

  if (aEvent->flags & NS_EVENT_FLAG_NO_DEFAULT) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }
}

void
nsEventListenerManager::Disconnect()
{
  mTarget = nsnull;
  RemoveAllListeners();
}

// nsIDOMEventTarget interface
nsresult
nsEventListenerManager::AddEventListener(const nsAString& aType,
                                         nsIDOMEventListener* aListener,
                                         PRBool aUseCapture,
                                         PRBool aWantsUntrusted)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return AddEventListenerByType(aListener, aType, flags);
}

void
nsEventListenerManager::RemoveEventListener(const nsAString& aType, 
                                            nsIDOMEventListener* aListener, 
                                            PRBool aUseCapture)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  
  RemoveEventListenerByType(aListener, aType, flags);
}

PRBool
nsEventListenerManager::HasMutationListeners()
{
  if (mMayHaveMutationListeners) {
    PRUint32 count = mListeners.Length();
    for (PRUint32 i = 0; i < count; ++i) {
      nsListenerStruct* ls = &mListeners.ElementAt(i);
      if (ls->mEventType >= NS_MUTATION_START &&
          ls->mEventType <= NS_MUTATION_END) {
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

PRUint32
nsEventListenerManager::MutationListenerBits()
{
  PRUint32 bits = 0;
  if (mMayHaveMutationListeners) {
    PRUint32 count = mListeners.Length();
    for (PRUint32 i = 0; i < count; ++i) {
      nsListenerStruct* ls = &mListeners.ElementAt(i);
      if (ls->mEventType >= NS_MUTATION_START &&
          ls->mEventType <= NS_MUTATION_END) {
        if (ls->mEventType == NS_MUTATION_SUBTREEMODIFIED) {
          return kAllMutationBits;
        }
        bits |= MutationBitForEventType(ls->mEventType);
      }
    }
  }
  return bits;
}

PRBool
nsEventListenerManager::HasListenersFor(const nsAString& aEventName)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aEventName);

  PRUint32 count = mListeners.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    nsListenerStruct* ls = &mListeners.ElementAt(i);
    if (ls->mTypeAtom == atom) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
nsEventListenerManager::HasListeners()
{
  return !mListeners.IsEmpty();
}

nsresult
nsEventListenerManager::GetListenerInfo(nsCOMArray<nsIEventListenerInfo>* aList)
{
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mTarget);
  NS_ENSURE_STATE(target);
  aList->Clear();
  PRUint32 count = mListeners.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    const nsListenerStruct& ls = mListeners.ElementAt(i);
    PRBool capturing = !!(ls.mFlags & NS_EVENT_FLAG_CAPTURE);
    PRBool systemGroup = !!(ls.mFlags & NS_EVENT_FLAG_SYSTEM_EVENT);
    PRBool allowsUntrusted = !!(ls.mFlags & NS_PRIV_EVENT_UNTRUSTED_PERMITTED);
    // If this is a script handler and we haven't yet
    // compiled the event handler itself
    if ((ls.mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) && ls.mHandlerIsString) {
      nsCOMPtr<nsIJSEventListener> jslistener = do_QueryInterface(ls.mListener);
      if (jslistener) {
        CompileEventHandlerInternal(jslistener->GetEventContext(),
                                    jslistener->GetEventScope(),
                                    jslistener->GetEventTarget(),
                                    ls.mTypeAtom,
                                    const_cast<nsListenerStruct*>(&ls),
                                    mTarget,
                                    PR_TRUE);
      }
    }
    const nsDependentSubstring& eventType =
      Substring(nsDependentAtomString(ls.mTypeAtom), 2);
    nsRefPtr<nsEventListenerInfo> info =
      new nsEventListenerInfo(eventType, ls.mListener, capturing,
                              allowsUntrusted, systemGroup);
    NS_ENSURE_TRUE(info, NS_ERROR_OUT_OF_MEMORY);
    aList->AppendObject(info);
  }
  return NS_OK;
}

PRBool
nsEventListenerManager::HasUnloadListeners()
{
  PRUint32 count = mListeners.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    nsListenerStruct* ls = &mListeners.ElementAt(i);
    if (ls->mEventType == NS_PAGE_UNLOAD ||
        ls->mEventType == NS_BEFORE_PAGE_UNLOAD) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}
