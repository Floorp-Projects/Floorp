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
 * The Original Code is Mozilla Communicator client code.
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
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsITextContent.h"
#include "nsIDocument.h"
#include "nsIScriptLoader.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIScriptElement.h"
#include "nsGUIEvent.h"
#include "nsIURI.h"

#include "nsUnicharUtils.h"  // for nsCaseInsensitiveCaseComparator()
#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsIServiceManagerUtils.h"
#include "nsIScriptEventHandler.h"
#include "nsIDOMDocument.h"


//
// Helper class used to support <SCRIPT FOR=object EVENT=handler ...>
// style script tags...
//
class nsHTMLScriptEventHandler : public nsIScriptEventHandler
{
public:
  nsHTMLScriptEventHandler(nsIDOMHTMLScriptElement *aOuter);
  virtual ~nsHTMLScriptEventHandler() {};

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIScriptEventHandler interface...
  NS_DECL_NSISCRIPTEVENTHANDLER

  // Helper method called by nsHTMLScriptElement
  nsresult ParseEventString(const nsAString &aValue);

protected:
  // WEAK reference to outer object.
  nsIDOMHTMLScriptElement *mOuter;

  // Javascript argument names must be ASCII...
  nsCStringArray mArgNames;

  // The event name is kept UCS2 for 'quick comparisions'...
  nsString mEventName;
};


nsHTMLScriptEventHandler::nsHTMLScriptEventHandler(nsIDOMHTMLScriptElement *aOuter)
{

  // Weak reference...
  mOuter = aOuter;
}

//
// nsISupports Implementation
//
NS_IMPL_ADDREF (nsHTMLScriptEventHandler)
NS_IMPL_RELEASE(nsHTMLScriptEventHandler)

NS_INTERFACE_MAP_BEGIN(nsHTMLScriptEventHandler)
// All interfaces are delegated to the outer object...
NS_INTERFACE_MAP_END_AGGREGATED(mOuter)


//
// Parse the EVENT attribute into an array of argument names...
//
nsresult nsHTMLScriptEventHandler::ParseEventString(const nsAString &aValue)
{
  nsAutoString eventSig(aValue);
  nsAutoString::const_iterator start, next, end;

  // Clear out the arguments array...
  mArgNames.Clear();

  // Eliminate all whitespace.
  eventSig.StripWhitespace();

  // Parse out the event name from the signature...
  eventSig.BeginReading(start);
  eventSig.EndReading(end);

  next = start;
  if (FindCharInReadable('(', next, end)) {
    mEventName = Substring(start, next);
  } else {
    // There is no opening parenthesis...
    return NS_ERROR_FAILURE;
  }

  ++next;  // skip over the '('
  --end;   // Move back 1 character -- hopefully to the ')'
  if (*end != ')') {
    // The arguments are not enclosed in parentheses...
    return NS_ERROR_FAILURE;
  }

  // Javascript expects all argument names to be ASCII.
  NS_LossyConvertUCS2toASCII sig(Substring(next, end));

  // Store each (comma separated) argument in mArgNames
  mArgNames.ParseString(sig.get(), ",");

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptEventHandler::IsSameEvent(const nsAString &aObjectName,
                                      const nsAString &aEventName,
                                      PRUint32 aArgCount,
                                      PRBool *aResult)
{
  *aResult = PR_FALSE;

  // First compare the event name.
  // Currently, both the case and number of arguments associated with the
  // event are ignored...
  if (aEventName.Equals(mEventName, nsCaseInsensitiveStringComparator())) {
    nsAutoString id;

    // Next compare the target object...
    mOuter->GetHtmlFor(id);
    if (aObjectName.Equals(id)) {
      *aResult = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptEventHandler::Invoke(nsISupports *aTargetObject,
                                 void *aArgs,
                                 PRUint32 aArgCount)
{
  nsresult rv;
  nsAutoString scriptBody;

  // Initial sanity checking
  //
  // It's 'ok' for aArgCount to be different from mArgNames.Count().
  // This just means that the number of args being passed in is
  // different from the number it is compiled with...
  if (!aTargetObject || (aArgCount && !aArgs) ) {
    return NS_ERROR_FAILURE;
  }

  // Get the text of the script to execute...
  rv = mOuter->GetText(scriptBody);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the line number of the script (used when compiling)
  PRUint32 lineNumber = 0;
  nsCOMPtr<nsIScriptElement> sele(do_QueryInterface(mOuter));
  if (sele) {
    (void) sele->GetLineNumber(&lineNumber);
  }

  // Get the script context...
  nsCOMPtr<nsIDOMDocument> domdoc;
  nsCOMPtr<nsIScriptContext> scriptContext;

  mOuter->GetOwnerDocument(getter_AddRefs(domdoc));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  if (doc) {
    nsIScriptGlobalObject *sgo = doc->GetScriptGlobalObject();
    if (sgo) {
      scriptContext = sgo->GetContext();
    }
  }
  // Fail if is no script context is available...
  if (!scriptContext) {
    return NS_ERROR_FAILURE;
  }

  // wrap the target object...
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  JSContext *cx = (JSContext *)scriptContext->GetNativeContext();
  JSObject *scriptObject = nsnull;

  if (xpc) {
    rv = xpc->WrapNative(cx,
                         ::JS_GetGlobalObject(cx),
                         aTargetObject,
                         NS_GET_IID(nsISupports),
                         getter_AddRefs(holder));
    if (holder) {
      rv = holder->GetJSObject(&scriptObject);
    }
  }
  // Fail if wrapping the native object failed...
  if (!scriptObject) {
    return NS_ERROR_FAILURE;
  }

  // Build up the array of argument names...
  //
  // Since this array is temporary (and const) the 'argument name' strings
  // are NOT copied.  Instead each element points into the underlying buffer
  // of the corresponding element in the mArgNames array...
  //
  // Remember, this is the number of arguments to compile the function with...
  // So, use mArgNames.Count()
  //
  const int kMaxArgsOnStack = 10;

  PRInt32 argc, i;
  const char** args;
  const char*  stackArgs[kMaxArgsOnStack];

  args = stackArgs;
  argc = mArgNames.Count();

  // If there are too many arguments then allocate the array from the heap
  // otherwise build it up on the stack...
  if (argc >= kMaxArgsOnStack) {
    args = new const char*[argc+1];
    if (!args) return NS_ERROR_OUT_OF_MEMORY;
  }

  for(i=0; i<argc; i++) {
    args[i] = mArgNames[i]->get();
  }

  // Null terminate for good luck ;-)
  args[i] = nsnull;

  // Compile the event handler script...
  void* funcObject = nsnull;
  NS_NAMED_LITERAL_CSTRING(funcName, "anonymous");

  rv = scriptContext->CompileFunction(scriptObject,
                                      funcName,   // method name
                                      argc,       // no of arguments
                                      args,       // argument names
                                      scriptBody, // script text
                                      nsnull,     // XXX: URL
                                      lineNumber, // line no (for errors)
                                      PR_FALSE,   // shared ?
                                      &funcObject);
  // Free the argument names array if it was heap allocated...
  if (args != stackArgs) {
    delete [] args;
  }

  // Fail if there was an error compiling the script.
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Invoke the event handler script...
  jsval dummy;
  return scriptContext->CallEventHandler(scriptObject, (JSObject *)funcObject,
                                         aArgCount, (jsval *)aArgs, &dummy);
}


class nsHTMLScriptElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLScriptElement,
                            public nsIScriptLoaderObserver,
                            public nsIScriptElement
{
public:
  nsHTMLScriptElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLScriptElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLScriptElement
  NS_DECL_NSIDOMHTMLSCRIPTELEMENT

  // nsIScriptLoaderObserver
  NS_DECL_NSISCRIPTLOADEROBSERVER

  // nsIScriptElement
  NS_IMETHOD SetLineNumber(PRUint32 aLineNumber);
  NS_IMETHOD GetLineNumber(PRUint32* aLineNumber);

  // nsIContent
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify, PRBool aDeepSetDocument);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument);

  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);
  virtual nsresult SetInnerHTML(const nsAString& aInnerHTML);

protected:
  PRBool IsOnloadEventForWindow();

  PRUint32 mLineNumber;
  PRPackedBool mIsEvaluated;
  PRPackedBool mEvaluating;

  // Pointer to the script handler helper object (OWNING reference)
  nsHTMLScriptEventHandler *mScriptEventHandler;

  /**
   * Processes the script if it's in the document-tree and links to or
   * contains a script. Once it has been evaluated there is no way to make it
   * reevaluate the script, you'll have to create a new element. This also means
   * that when adding a src attribute to an element that already contains an
   * inline script, the script referenced by the src attribute will not be
   * loaded.
   *
   * In order to be able to use multiple childNodes, or to use the
   * fallback-mechanism of using both inline script and linked script you have
   * to add all attributes and childNodes before adding the element to the
   * document-tree.
   */
  void MaybeProcessScript();
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Script)


nsHTMLScriptElement::nsHTMLScriptElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  mLineNumber = 0;
  mIsEvaluated = PR_FALSE;
  mEvaluating = PR_FALSE;
  mScriptEventHandler = nsnull;
}

nsHTMLScriptElement::~nsHTMLScriptElement()
{
  NS_IF_RELEASE(mScriptEventHandler);
}


NS_IMPL_ADDREF_INHERITED(nsHTMLScriptElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLScriptElement, nsGenericElement)

// QueryInterface implementation for nsHTMLScriptElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLScriptElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLScriptElement)
  NS_INTERFACE_MAP_ENTRY(nsIScriptLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIScriptElement)
  if (mScriptEventHandler && aIID.Equals(NS_GET_IID(nsIScriptEventHandler)))
    foundInterface = NS_STATIC_CAST(nsIScriptEventHandler*,
                                    mScriptEventHandler);
  else
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLScriptElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLScriptElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNameSpaceID != kNameSpaceID_None) {
    return rv;
  }

  if (aNotify && aName == nsHTMLAtoms::src) {
    MaybeProcessScript();
  }

  return rv;
}

void
nsHTMLScriptElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                 PRBool aCompileEventHandlers)
{
  nsGenericHTMLElement::SetDocument(aDocument, aDeep, aCompileEventHandlers);
  if (aDocument) {
    MaybeProcessScript();
  }
}

nsresult
nsHTMLScriptElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                   PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv = nsGenericHTMLElement::InsertChildAt(aKid, aIndex, aNotify,
                                                    aDeepSetDocument);
  if (NS_SUCCEEDED(rv) && aNotify) {
    MaybeProcessScript();
  }

  return rv;
}

nsresult
nsHTMLScriptElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                   PRBool aDeepSetDocument)
{
  nsresult rv = nsGenericHTMLElement::AppendChildTo(aKid, aNotify,
                                                    aDeepSetDocument);
  if (NS_SUCCEEDED(rv) && aNotify) {
    MaybeProcessScript();
  }

  return rv;
}

nsresult
nsHTMLScriptElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  nsHTMLScriptElement* it = new nsHTMLScriptElement(mNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  CopyInnerTo(it, aDeep);

  // The clone should be marked evaluated if we are.  It should also be marked
  // evaluated if we're evaluating, to handle the case when this script node's
  // script clones the node.
  it->mIsEvaluated = mIsEvaluated || mEvaluating;
  it->mLineNumber = mLineNumber;

  kungFuDeathGrip.swap(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetText(nsAString& aValue)
{
  return GetContentsAsText(aValue);
}

NS_IMETHODIMP
nsHTMLScriptElement::SetText(const nsAString& aValue)
{
  return ReplaceContentsWithText(aValue, PR_TRUE);
}


NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Charset, charset)
NS_IMPL_BOOL_ATTR(nsHTMLScriptElement, Defer, defer)
NS_IMPL_URI_ATTR(nsHTMLScriptElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, HtmlFor, _for)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Event, _event)

nsresult
nsHTMLScriptElement::GetInnerHTML(nsAString& aInnerHTML)
{
  return GetContentsAsText(aInnerHTML);
}

nsresult
nsHTMLScriptElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  return ReplaceContentsWithText(aInnerHTML, PR_TRUE);
}

/* void scriptAvailable (in nsresult aResult, in nsIDOMHTMLScriptElement aElement, in nsIURI aURI, in PRInt32 aLineNo, in PRUint32 aScriptLength, [size_is (aScriptLength)] in wstring aScript); */
NS_IMETHODIMP
nsHTMLScriptElement::ScriptAvailable(nsresult aResult,
                                     nsIDOMHTMLScriptElement *aElement,
                                     PRBool aIsInline,
                                     PRBool aWasPending,
                                     nsIURI *aURI,
                                     PRInt32 aLineNo,
                                     const nsAString& aScript)
{
  if (!aIsInline && NS_FAILED(aResult)) {
    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext));

    nsEventStatus status = nsEventStatus_eIgnore;
    nsScriptErrorEvent event(NS_SCRIPT_ERROR);

    event.lineNr = aLineNo;

    NS_NAMED_LITERAL_STRING(errorString, "Error loading script");
    event.errorMsg = errorString.get();

    nsCAutoString spec;
    aURI->GetSpec(spec);

    NS_ConvertUTF8toUCS2 fileName(spec);
    event.fileName = fileName.get();

    HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT,
                   &status);
  }

  return NS_OK;
}

/* void scriptEvaluated (in nsresult aResult, in nsIDOMHTMLScriptElement aElement); */
NS_IMETHODIMP
nsHTMLScriptElement::ScriptEvaluated(nsresult aResult,
                                     nsIDOMHTMLScriptElement *aElement,
                                     PRBool aIsInline,
                                     PRBool aWasPending)
{
  nsresult rv = NS_OK;
  if (!aIsInline) {
    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext));

    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(NS_SUCCEEDED(aResult) ? NS_SCRIPT_LOAD : NS_SCRIPT_ERROR);
    rv = HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT,
                        &status);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetLineNumber(PRUint32 aLineNumber)
{
  mLineNumber = aLineNumber;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetLineNumber(PRUint32* aLineNumber)
{
  NS_ENSURE_ARG_POINTER(aLineNumber);

  *aLineNumber = mLineNumber;

  return NS_OK;
}

void
nsHTMLScriptElement::MaybeProcessScript()
{
  if (mIsEvaluated || mEvaluating || !mDocument || !GetParent()) {
    return;
  }

  // We'll always call this to make sure that
  // ScriptAvailable/ScriptEvaluated gets called. See bug 153600
  nsresult rv = NS_OK;
  nsCOMPtr<nsIScriptLoader> loader = mDocument->GetScriptLoader();
  if (loader) {
    mEvaluating = PR_TRUE;
    rv = loader->ProcessScriptElement(this, this);
    mEvaluating = PR_FALSE;
  }

  if (rv == NS_CONTENT_SCRIPT_IS_EVENTHANDLER) {

    // If the script has NOT been executed yet then create a script
    // event handler if necessary...
    if (!mIsEvaluated && !mScriptEventHandler) {
      // Set mIsEvaluated, this element will be handled by the
      // nsIScriptEventManager
      mIsEvaluated = PR_TRUE;

      mScriptEventHandler = new nsHTMLScriptEventHandler(this);
      if (!mScriptEventHandler) {
        return;
      }

      NS_ADDREF(mScriptEventHandler);

      // The script-loader will make sure that the script is not evaluated
      // right away.
    }

    if (mScriptEventHandler) {
      nsAutoString event_val;
      GetAttr(kNameSpaceID_None, nsHTMLAtoms::_event, event_val);
      mScriptEventHandler->ParseEventString(event_val);
    }
  }

  // But we'll only set mIsEvaluated if we did really load or evaluate
  // something
  if (HasAttr(kNameSpaceID_None, nsHTMLAtoms::src) ||
      mAttrsAndChildren.ChildCount()) {
    mIsEvaluated = PR_TRUE;
  }
}
