/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
#include "nsScriptElement.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

#include "nsUnicharUtils.h"  // for nsCaseInsensitiveStringComparator()
#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "nsIScriptEventHandler.h"
#include "nsIDOMDocument.h"
#include "nsContentErrors.h"
#include "nsIArray.h"
#include "nsTArray.h"
#include "nsDOMJSUtils.h"

//
// Helper class used to support <SCRIPT FOR=object EVENT=handler ...>
// style script tags...
//
class nsHTMLScriptEventHandler : public nsIScriptEventHandler
{
public:
  nsHTMLScriptEventHandler(nsIDOMHTMLScriptElement *aOuter);
  virtual ~nsHTMLScriptEventHandler() {}

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
  nsTArray<nsCString> mArgNames;

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
  NS_LossyConvertUTF16toASCII sig(Substring(next, end));

  // Store each (comma separated) argument in mArgNames
  ParseString(sig, ',', mArgNames);

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
    lineNumber = sele->GetScriptLineNumber();
  }

  // Get the script context...
  nsCOMPtr<nsIDOMDocument> domdoc;
  nsCOMPtr<nsIScriptContext> scriptContext;
  nsIScriptGlobalObject *sgo = nsnull;

  mOuter->GetOwnerDocument(getter_AddRefs(domdoc));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  if (doc) {
    sgo = doc->GetScriptGlobalObject();
    if (sgo) {
      scriptContext = sgo->GetContext();
    }
  }
  // Fail if is no script context is available...
  if (!scriptContext) {
    return NS_ERROR_FAILURE;
  }

  // wrap the target object...
  JSContext *cx = (JSContext *)scriptContext->GetNativeContext();
  JSObject *scope = sgo->GetGlobalJSObject();

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  jsval v;
  rv = nsContentUtils::WrapNative(cx, scope, aTargetObject, &v,
                                  getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

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
  argc = PRInt32(mArgNames.Length());

  // If there are too many arguments then allocate the array from the heap
  // otherwise build it up on the stack...
  if (argc >= kMaxArgsOnStack) {
    args = new const char*[argc+1];
    if (!args) return NS_ERROR_OUT_OF_MEMORY;
  }

  for(i=0; i<argc; i++) {
    args[i] = mArgNames[i].get();
  }

  // Null terminate for good luck ;-)
  args[i] = nsnull;

  // Compile the event handler script...
  void* funcObject = nsnull;
  NS_NAMED_LITERAL_CSTRING(funcName, "anonymous");

  rv = scriptContext->CompileFunction(JSVAL_TO_OBJECT(v),
                                      funcName,   // method name
                                      argc,       // no of arguments
                                      args,       // argument names
                                      scriptBody, // script text
                                      nsnull,     // XXX: URL
                                      lineNumber, // line no (for errors)
                                      JSVERSION_DEFAULT, // Default for now?
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

  // Create an nsIArray for the args (the JS context will efficiently
  // re-fetch the jsvals from this object)
  nsCOMPtr<nsIArray> argarray;
  rv = NS_CreateJSArgv(cx, aArgCount, (jsval *)aArgs, getter_AddRefs(argarray));
  if (NS_FAILED(rv))
    return rv;

  // Invoke the event handler script...
  nsCOMPtr<nsIVariant> ret;
  return scriptContext->CallEventHandler(aTargetObject, scope, funcObject,
                                         argarray, getter_AddRefs(ret));
}


class nsHTMLScriptElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLScriptElement,
                            public nsScriptElement
{
public:
  nsHTMLScriptElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      PRUint32 aFromParser);
  virtual ~nsHTMLScriptElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  NS_DECL_NSIDOMHTMLSCRIPTELEMENT

  // nsIScriptElement
  virtual void GetScriptType(nsAString& type);
  virtual void GetScriptText(nsAString& text);
  virtual void GetScriptCharset(nsAString& charset);
  virtual void FreezeUriAsyncDefer();

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);

  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);
  virtual nsresult SetInnerHTML(const nsAString& aInnerHTML);
  virtual nsresult DoneAddingChildren(PRBool aHaveNotified);
  virtual PRBool IsDoneAddingChildren();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  PRBool IsOnloadEventForWindow();


  // Pointer to the script handler helper object (OWNING reference)
  nsCOMPtr<nsHTMLScriptEventHandler> mScriptEventHandler;

  // nsScriptElement
  virtual PRBool HasScriptContent();
  virtual nsresult MaybeProcessScript();
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Script)


nsHTMLScriptElement::nsHTMLScriptElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                         PRUint32 aFromParser)
  : nsGenericHTMLElement(aNodeInfo)
  , nsScriptElement(aFromParser)
{
  mDoneAddingChildren = !aFromParser;
  AddMutationObserver(this);
}

nsHTMLScriptElement::~nsHTMLScriptElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLScriptElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLScriptElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLScriptElement, nsHTMLScriptElement)

// QueryInterface implementation for nsHTMLScriptElement
NS_INTERFACE_TABLE_HEAD(nsHTMLScriptElement)
  NS_HTML_CONTENT_INTERFACE_TABLE4(nsHTMLScriptElement,
                                   nsIDOMHTMLScriptElement,
                                   nsIScriptLoaderObserver,
                                   nsIScriptElement,
                                   nsIMutationObserver)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLScriptElement,
                                               nsGenericHTMLElement)
  if (mScriptEventHandler && aIID.Equals(NS_GET_IID(nsIScriptEventHandler)))
    foundInterface = static_cast<nsIScriptEventHandler*>
                                (mScriptEventHandler);
  else
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(HTMLScriptElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLScriptElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    MaybeProcessScript();
  }

  return NS_OK;
}

nsresult
nsHTMLScriptElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsHTMLScriptElement* it = new nsHTMLScriptElement(ni.forget(), PR_FALSE);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  // The clone should be marked evaluated if we are.
  it->mAlreadyStarted = mAlreadyStarted;
  it->mLineNumber = mLineNumber;
  it->mMalformed = mMalformed;

  kungFuDeathGrip.swap(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetText(nsAString& aValue)
{
  nsContentUtils::GetNodeTextContent(this, PR_FALSE, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetText(const nsAString& aValue)
{
  return nsContentUtils::SetNodeTextContent(this, aValue, PR_TRUE);
}


NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Charset, charset)
NS_IMPL_BOOL_ATTR(nsHTMLScriptElement, Defer, defer)
NS_IMPL_BOOL_ATTR(nsHTMLScriptElement, Async, async)
NS_IMPL_URI_ATTR(nsHTMLScriptElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, HtmlFor, _for)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Event, event)

nsresult
nsHTMLScriptElement::GetInnerHTML(nsAString& aInnerHTML)
{
  nsContentUtils::GetNodeTextContent(this, PR_FALSE, aInnerHTML);
  return NS_OK;
}

nsresult
nsHTMLScriptElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  return nsContentUtils::SetNodeTextContent(this, aInnerHTML, PR_TRUE);
}

nsresult
nsHTMLScriptElement::DoneAddingChildren(PRBool aHaveNotified)
{
  mDoneAddingChildren = PR_TRUE;
  nsresult rv = MaybeProcessScript();
  if (!mAlreadyStarted) {
    // Need to lose parser-insertedness here to allow another script to cause
    // execution later.
    LoseParserInsertedness();
  }
  return rv;
}

PRBool
nsHTMLScriptElement::IsDoneAddingChildren()
{
  return mDoneAddingChildren;
}

// variation of this code in nsSVGScriptElement - check if changes
// need to be transfered when modifying

void
nsHTMLScriptElement::GetScriptType(nsAString& type)
{
  GetType(type);
}

void
nsHTMLScriptElement::GetScriptText(nsAString& text)
{
  GetText(text);
}

void
nsHTMLScriptElement::GetScriptCharset(nsAString& charset)
{
  GetCharset(charset);
}

void
nsHTMLScriptElement::FreezeUriAsyncDefer()
{
  if (mFrozen) {
    return;
  }
  
  // variation of this code in nsSVGScriptElement - check if changes
  // need to be transfered when modifying
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
    nsAutoString src;
    GetSrc(src);
    NS_NewURI(getter_AddRefs(mUri), src);

    PRBool defer, async;
    GetAsync(&async);
    GetDefer(&defer);

    mDefer = !async && defer;
    mAsync = async;
  }
  
  mFrozen = PR_TRUE;
}

PRBool
nsHTMLScriptElement::HasScriptContent()
{
  return (mFrozen ? !!mUri : HasAttr(kNameSpaceID_None, nsGkAtoms::src)) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}

nsresult
nsHTMLScriptElement::MaybeProcessScript()
{
  nsresult rv = nsScriptElement::MaybeProcessScript();
  if (rv == NS_CONTENT_SCRIPT_IS_EVENTHANDLER) {
    // Don't return NS_CONTENT_SCRIPT_IS_EVENTHANDLER since callers can't deal
    rv = NS_OK;

    // We tried to evaluate the script but realized it was an eventhandler
    // mEvaluated will already be set at this point
    NS_ASSERTION(mAlreadyStarted, "should have set mIsEvaluated already");
    NS_ASSERTION(!mScriptEventHandler, "how could we have an SEH already?");

    mScriptEventHandler = new nsHTMLScriptEventHandler(this);
    NS_ENSURE_TRUE(mScriptEventHandler, NS_ERROR_OUT_OF_MEMORY);

    nsAutoString event_val;
    GetAttr(kNameSpaceID_None, nsGkAtoms::event, event_val);
    mScriptEventHandler->ParseEventString(event_val);
  }

  return rv;
}
