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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Olli Pettay.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXFormsAtoms.h"
#include "nsXFormsStubElement.h"
#include "nsXFormsDelegateStub.h"
#include "nsXFormsActionElement.h"
#include "nsIXFormsActionModuleElement.h"

#include "nsIDOMText.h"
#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDOMImplementation.h"

#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"

#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleDeclaration.h"

#include "nsITimer.h"
#include "nsIDocument.h"
#include "nsIBoxObject.h"
#include "nsIServiceManager.h"

#include "prmem.h"
#include "plbase64.h"
#include "nsAutoPtr.h"
#include "nsIStringBundle.h"
#include "nsIDOMSerializer.h"
#include "nsIServiceManager.h"
#include "nsIDelegateInternal.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIChannelEventSink.h"
#include "nsIXFormsEphemeralMessageUI.h"

#define MESSAGE_WINDOW_PROPERTIES \
  "centerscreen,chrome,dependent,dialog"

#define MESSAGE_WINDOW_URL \
  "chrome://xforms/content/xforms-message.xul"

// Defining a simple dialog for modeless and modal messages.

#define SHOW_EPHEMERAL_TIMEOUT           750
#define HIDE_EPHEMERAL_TIMEOUT           5000
#define EPHEMERAL_POSITION_RESET_TIMEOUT 100

class nsXFormsEventListener;
/**
 * Implementation of the XForms \<message\> element.
 *
 * @see http://www.w3.org/TR/xforms/slice10.html#action-info
 */
class nsXFormsMessageElement : public nsXFormsDelegateStub,
                               public nsIDOMEventListener,
                               public nsIXFormsActionModuleElement,
                               public nsIStreamListener,
                               public nsIInterfaceRequestor,
                               public nsIChannelEventSink
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXFormsDelegate
  NS_IMETHOD GetValue(nsAString& aValue);

  // nsIXTFElement overrides
  NS_IMETHOD OnCreated(nsIXTFElementWrapper *aWrapper);
  NS_IMETHOD WillChangeDocument(nsIDOMDocument *aNewDocument);
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD WillChangeParent(nsIDOMElement *aNewParent);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aSrc);
  NS_IMETHOD AttributeRemoved(nsIAtom *aName);
  NS_IMETHOD DoneAddingChildren();

  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIXFORMSACTIONMODULEELEMENT

  // Start the timer, which is used to set the message visible
  void StartEphemeral();
  // Set the message visible and start timer to hide it later.
  void ShowEphemeral();
  // Hide the ephemeral message.
  void HideEphemeral();
  // Reset the position of the ephemeral message.
  void ResetEphemeralPosition()
  {
    mPosX = mPosY = -1;
  }

  enum MessageType {
    eType_Normal,
    eType_Hint,
    eType_Help,
    eType_Alert
  };

  enum StopType {
    eStopType_None,
    eStopType_Security,
    eStopType_LinkError
  };


  nsXFormsMessageElement(MessageType aType) :
    mType(aType), mPosX(-1), mPosY(-1), mDocument(nsnull),
    mStopType(eStopType_None), mSrcAttrText(""),
    mDoneAddingChildren(PR_FALSE) {}
private:
  nsresult HandleEphemeralMessage(nsIDOMDocument* aDoc, nsIDOMEvent* aEvent);
  nsresult HandleModalAndModelessMessage(nsIDOMDocument* aDoc, nsAString& aLevel);
  void ImportNode(nsIDOMNode* aSrc, nsIDOMDocument* aDestDoc,
                  nsIDOMNode** aTarget);
  PRBool HandleInlineAlert(nsIDOMEvent* aEvent);
  nsresult ConstructMessageWindowURL(const nsAString& aData,
                                     PRBool aIsLink,
                                     /*out*/ nsAString& aURL);

  /**
   * Begin the process of testing to see if this message element could get held
   * up later by linking to an external resource.  Run this when the element is
   * set up and it will try to access any external resource that this message
   * may later try to access.  If we can't get to it by the time the message
   * is triggered, then throw a xforms-link-error.
   */
  nsresult TestExternalFile();

  /**
   * Either add or subtract from the number of messages with external resources
   * currently loading in this document.  aAdd == PR_TRUE will add to the total
   * otherwise we will subtract from the total (as long as it isn't already 0).
   */
  void AddRemoveExternalResource(PRBool aAdd);

  /**
   * Determine if the message is Ephemeral.
   */
  PRBool IsEphemeral();

  MessageType          mType;

  // The position of the ephemeral message
  PRInt32              mPosX;
  PRInt32              mPosY;

  nsCOMPtr<nsITimer>   mEphemeralTimer;
  nsIDOMDocument*      mDocument;
  nsCOMPtr<nsIChannel> mChannel;
  StopType             mStopType;
  nsCString            mSrcAttrText;
  PRBool               mDoneAddingChildren;
};

NS_IMPL_ADDREF_INHERITED(nsXFormsMessageElement, nsXFormsDelegateStub)
NS_IMPL_RELEASE_INHERITED(nsXFormsMessageElement, nsXFormsDelegateStub)

NS_INTERFACE_MAP_BEGIN(nsXFormsMessageElement)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsActionModuleElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END_INHERITING(nsXFormsDelegateStub)

// nsIXTFElement

NS_IMETHODIMP
nsXFormsMessageElement::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsDelegateStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);
  
  aWrapper->SetNotificationMask(kStandardNotificationMask |
                                nsIXTFElement::NOTIFY_WILL_CHANGE_PARENT |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::WillChangeDocument(nsIDOMDocument *aNewDocument)
{
  if (mDocument) {
    if (mEphemeralTimer) {
      mEphemeralTimer->Cancel();
      mEphemeralTimer = nsnull;
    }

    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    if (doc) {
      nsXFormsMessageElement *msg =
        NS_STATIC_CAST(nsXFormsMessageElement*,
                       doc->GetProperty(nsXFormsAtoms::messageProperty));
      if (msg == this)
        doc->UnsetProperty(nsXFormsAtoms::messageProperty);
    }

    // If we are currently trying to load an external message, cancel the
    // request.
    if (mChannel) {
      mChannel->Cancel(NS_BINDING_ABORTED);
    }
  }

  mDocument = aNewDocument;
  return nsXFormsDelegateStub::WillChangeDocument(aNewDocument);
}

NS_IMETHODIMP
nsXFormsMessageElement::OnDestroyed()
{
  mChannel = nsnull;
  mDocument = nsnull;
  return nsXFormsDelegateStub::OnDestroyed();
}

NS_IMETHODIMP
nsXFormsMessageElement::HandleEvent(nsIDOMEvent* aEvent)
{
  return nsXFormsUtils::EventHandlingAllowed(aEvent, mElement) ?
           HandleAction(aEvent, nsnull) : NS_OK;
}

void
nsXFormsMessageElement::ImportNode(nsIDOMNode* aSrc, nsIDOMDocument* aDestDoc,
                                   nsIDOMNode** aTarget)
{
  nsAutoString ns;
  nsAutoString localName;
  aSrc->GetNamespaceURI(ns);
  aSrc->GetLocalName(localName);
  // Clone the visual content of the <output>.
  // According to the XForms Schema it is enough 
  // to support <output> here.
  if (ns.EqualsLiteral(NS_NAMESPACE_XFORMS) &&
      localName.EqualsLiteral("output")) {
    nsCOMPtr<nsIDelegateInternal> outEl(do_QueryInterface(aSrc));
    if (outEl) {
      nsCOMPtr<nsIDOMText> text;
      nsAutoString value;
      outEl->GetValue(value);
      aDestDoc->CreateTextNode(value, getter_AddRefs(text));
      NS_IF_ADDREF(*aTarget = text);
    }
    return;
  }

  // Clone other elements
  aDestDoc->ImportNode(aSrc, PR_FALSE, aTarget);

  if (!*aTarget)
    return;

  // Add the new children
  nsCOMPtr<nsIDOMNode> tmp;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  aSrc->GetChildNodes(getter_AddRefs(childNodes));

  PRUint32 count = 0;
  if (childNodes)
    childNodes->GetLength(&count);

  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMNode> child;
    childNodes->Item(i, getter_AddRefs(child));
    
    if (child) {
      nsCOMPtr<nsIDOMNode> clone;
      ImportNode(child, aDestDoc, getter_AddRefs(clone));
      if (clone)
        (*aTarget)->AppendChild(clone, getter_AddRefs(tmp));
    }
  }
}

NS_IMETHODIMP
nsXFormsMessageElement::WillChangeParent(nsIDOMElement *aNewParent)
{
  if (mType == eType_Normal)
    return nsXFormsDelegateStub::WillChangeParent(aNewParent);
  
  nsCOMPtr<nsIDOMNode> parent;
  mElement->GetParentNode(getter_AddRefs(parent));
  if (!parent)
    return nsXFormsDelegateStub::WillChangeParent(aNewParent);

  nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(parent));
  NS_ENSURE_STATE(targ);
  
  if (mType == eType_Hint) {
    targ->RemoveEventListener(NS_LITERAL_STRING("xforms-hint"), this, PR_FALSE);
    targ->RemoveEventListener(NS_LITERAL_STRING("xforms-moz-hint-off"),
                              this, PR_FALSE);
  } else if (mType == eType_Help) {
    targ->RemoveEventListener(NS_LITERAL_STRING("xforms-help"), this, PR_FALSE);      
  } else if (mType == eType_Alert) {
    targ->RemoveEventListener(NS_LITERAL_STRING("xforms-invalid"), this, PR_TRUE);
    targ->RemoveEventListener(NS_LITERAL_STRING("xforms-out-of-range"), this, PR_TRUE);
    targ->RemoveEventListener(NS_LITERAL_STRING("xforms-binding-exception"),this, PR_TRUE);
  }

  return nsXFormsDelegateStub::WillChangeParent(aNewParent);
}

NS_IMETHODIMP
nsXFormsMessageElement::ParentChanged(nsIDOMElement *aNewParent)
{
  if (mType == eType_Normal || !aNewParent)
    return nsXFormsDelegateStub::ParentChanged(aNewParent);

  nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(aNewParent));
  NS_ENSURE_STATE(targ);

  if (mType == eType_Hint) {
    targ->AddEventListener(NS_LITERAL_STRING("xforms-hint"), this, PR_FALSE);
    targ->AddEventListener(NS_LITERAL_STRING("xforms-moz-hint-off"),
                           this, PR_FALSE);
  } else if (mType == eType_Help) {
    targ->AddEventListener(NS_LITERAL_STRING("xforms-help"), this, PR_FALSE);
  } else if (mType == eType_Alert) {
    // Adding listeners for error events, which have a form control as a target.
    targ->AddEventListener(NS_LITERAL_STRING("xforms-invalid"), this, PR_TRUE);
    targ->AddEventListener(NS_LITERAL_STRING("xforms-out-of-range"), this, PR_TRUE);
    targ->AddEventListener(NS_LITERAL_STRING("xforms-binding-exception"), this, PR_TRUE);
  }

  return nsXFormsDelegateStub::ParentChanged(aNewParent);
}

NS_IMETHODIMP
nsXFormsMessageElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (mDoneAddingChildren) {
    if (aName == nsXFormsAtoms::src) {
      // If we are currently trying to load an external message, cancel the
      // request.
      if (mChannel) {
        mChannel->Cancel(NS_BINDING_ABORTED);
      }

      mStopType = eStopType_None;

      // The src attribute has changed so retest the external file.
      TestExternalFile();
    }
  }

  return nsXFormsDelegateStub::AttributeSet(aName, aValue);
}

NS_IMETHODIMP
nsXFormsMessageElement::AttributeRemoved(nsIAtom *aName)
{
  if (mDoneAddingChildren) {
    if (aName == nsXFormsAtoms::src) {
      // If we are currently trying to test an external resource, cancel the
      // request.
      if (mChannel) {
        mChannel->Cancel(NS_BINDING_ABORTED);
      }

      mSrcAttrText.Truncate();
      mStopType = eStopType_None;
    }
  }

  return nsXFormsDelegateStub::AttributeRemoved(aName);
}

NS_IMETHODIMP
nsXFormsMessageElement::DoneAddingChildren()
{
  mDoneAddingChildren = PR_TRUE;
  TestExternalFile();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::HandleAction(nsIDOMEvent* aEvent, 
                                     nsIXFormsActionElement *aParentAction)
{
  if (!mElement)
    return NS_OK;

  // If TestExternalFile fails, then there is an external link that we need
  // to use that we can't reach right now.  If it won't load, then might as
  // well stop here.  We don't want to be popping up empty windows
  // or windows that will just end up showing 404 messages.
  // We also stop if we were not allowed to access the given resource.
  if (mStopType == eStopType_LinkError ||
      mStopType == eStopType_Security) {
    // we couldn't successfully link to our external resource.  Better throw
    // the xforms-link-error event
    nsCOMPtr<nsIModelElementPrivate> modelPriv =
      nsXFormsUtils::GetModel(mElement);
    nsCOMPtr<nsIDOMNode> model = do_QueryInterface(modelPriv);
    nsXFormsUtils::DispatchEvent(model, eEvent_LinkError);
    return NS_OK;
  }

  // If there is still a channel, then someone must have changed the value of
  // the src attribute since the document finished loading and we haven't yet
  // determined whether the new link is valid or not.  For now we'll assume
  // that the value is good rather than returning a link error.  Also
  // canceling the channel since it is too late now.
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }

  if (mType != eType_Normal) {
    nsCOMPtr<nsIDOMEventTarget> target;

    if (mType == eType_Alert) {
      // Alert should fire only if target is the parent element.
      aEvent->GetTarget(getter_AddRefs(target));
    } else {
      // If <help> or <hint> is inside <action>, we don't want to fire them, 
      // unless xforms-help/xforms-hint is dispatched to the <action>.
      aEvent->GetCurrentTarget(getter_AddRefs(target));
    }
    nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(target));
    nsCOMPtr<nsIDOMNode> parent;
    mElement->GetParentNode(getter_AddRefs(parent));
    if (!parent || targetNode != parent)
      return NS_OK;
  }

  nsAutoString level;
  
  switch (mType) {
    case eType_Normal:
      mElement->GetAttribute(NS_LITERAL_STRING("level"), level);
      break;
    case eType_Hint:
      level.AssignLiteral("ephemeral");
      // Using the innermost <hint>.
      aEvent->StopPropagation();
      break;
    case eType_Help:
      level.AssignLiteral("modeless");
      // <help> is equivalent to a
      // <message level="modeless" ev:event="xforms-help" ev:propagate="stop>.
      aEvent->StopPropagation();
      aEvent->PreventDefault();
      break;
    case eType_Alert:
      if (HandleInlineAlert(aEvent))
        return NS_OK;

      level.AssignLiteral("modal");
      break;
  }

  if (level.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIDOMDocument> doc;
  mElement->GetOwnerDocument(getter_AddRefs(doc));

  return level.EqualsLiteral("ephemeral")
    ? HandleEphemeralMessage(doc, aEvent)
    : HandleModalAndModelessMessage(doc, level);
}

PRBool
nsXFormsMessageElement::HandleInlineAlert(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMDocument> doc;
  nsCOMPtr<nsIDOMWindowInternal> internal;
  mElement->GetOwnerDocument(getter_AddRefs(doc));
  nsXFormsUtils::GetWindowFromDocument(doc, getter_AddRefs(internal));
  if (!internal) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMViewCSS> cssView(do_QueryInterface(internal));
  if (!cssView)
    return PR_FALSE;
  
  nsAutoString tmp;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> styles;
  cssView->GetComputedStyle(mElement, tmp, getter_AddRefs(styles));
  nsCOMPtr<nsIDOMCSSValue> display;
  styles->GetPropertyCSSValue(NS_LITERAL_STRING("display"),
                              getter_AddRefs(display));
  if (display) {
    nsCOMPtr<nsIDOMCSSPrimitiveValue> displayValue(do_QueryInterface(display));
    if (displayValue) {
      nsAutoString type;
      displayValue->GetStringValue(type);
      return !type.EqualsLiteral("none");
    }
  }
  return PR_FALSE;
}

nsresult
nsXFormsMessageElement::HandleEphemeralMessage(nsIDOMDocument* aDoc,
                                               nsIDOMEvent* aEvent)
{
  if (!aEvent)
    return NS_OK;

  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (mType == eType_Hint) {
    // If this is a <hint> element, try to make it work more like a tooltip:
    // - if we get an xforms-moz-hint-off event, hide the element.
    // - if the <hint> is active and we get a new xforms-hint, then do nothing.
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
    if (!doc)
      return NS_OK;

    nsXFormsMessageElement *msg =
    NS_STATIC_CAST(nsXFormsMessageElement*,
                   doc->GetProperty(nsXFormsAtoms::messageProperty));
    if (msg == this) {
      if (eventType.EqualsLiteral("xforms-moz-hint-off")) {
        if (mEphemeralTimer) {
          mEphemeralTimer->Cancel();
          mEphemeralTimer = nsnull;
        }
        doc->UnsetProperty(nsXFormsAtoms::messageProperty);

        nsCOMPtr<nsIXFormsEphemeralMessageUI> ui(do_QueryInterface(mElement));
        if (ui) {
          ui->Hide();
        }
        ResetEphemeralPosition();
      }

      return NS_OK;
    }
  }

  /// @bug How to handle the following:
  ///      <message level="ephemeral" src="http://mozilla.org"/>
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  nsCOMPtr<nsIDOMElement> targetEl(do_QueryInterface(target));
  if (targetEl) {
    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(aDoc));
    if (nsDoc) {
      PRInt32 oldX = mPosX;
      PRInt32 oldY = mPosY;
      nsCOMPtr<nsIBoxObject> box;
      nsDoc->GetBoxObjectFor(targetEl, getter_AddRefs(box));
      if (box) {
        box->GetX(&mPosX);
        box->GetY(&mPosY);
        PRInt32 height;
        box->GetHeight(&height);

        mPosX += 10;
        mPosY = mPosY + height;

        // Move the ephemeral message a bit upwards if the
        // box object of the event target is large enough.
        // This makes it more clear to which element the 
        // message is related to.
        if (height > 20)
          mPosY -= height > 30 ? 10 : 10 - (30 - height);
      }

      // A special case for hints to make them work more like
      // normal tooltips.
      if (eventType.EqualsLiteral("xforms-hint") &&
          mPosX == oldX && mPosY == oldY) {
        return NS_OK;
      }

      StartEphemeral();
    }
  }
  return NS_OK;
}

nsresult
nsXFormsMessageElement::HandleModalAndModelessMessage(nsIDOMDocument* aDoc,
                                                      nsAString& aLevel)
{
  nsCOMPtr<nsIDOMWindowInternal> internal;
  nsXFormsUtils::GetWindowFromDocument(aDoc, getter_AddRefs(internal));
  if (!internal) {
    return NS_OK;
  }

  nsAutoString messageURL;

  // Order of precedence is single-node binding, linking attribute then
  // inline text.

  nsAutoString instanceData;
  PRBool hasBinding = nsXFormsUtils::GetSingleNodeBindingValue(mElement,
                                                               instanceData);

  nsAutoString options;
  options.AssignLiteral(MESSAGE_WINDOW_PROPERTIES);

  nsAutoString src;
  if (!hasBinding) {
    mElement->GetAttribute(NS_LITERAL_STRING("src"), src);
  }

  nsresult rv;
  if (!hasBinding && !src.IsEmpty()) {
    // Creating a normal window for messages with src attribute.
    options.AppendLiteral(",resizable");
    // Create a new URI so that we properly convert relative urls to absolute.
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
    NS_ENSURE_STATE(doc);
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), src, doc->GetDocumentCharacterSet().get(),
              doc->GetDocumentURI());
    NS_ENSURE_STATE(uri);
    nsCAutoString uriSpec;
    uri->GetSpec(uriSpec);
    messageURL = NS_ConvertUTF8toUTF16(uriSpec);
  } else {
    // Cloning the content of the xf:message and creating a
    // dialog for it.
    nsCOMPtr<nsIDOMDocument> ddoc;
    nsCOMPtr<nsIDOMDOMImplementation> domImpl;
    rv = aDoc->GetImplementation(getter_AddRefs(domImpl));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = domImpl->CreateDocument(EmptyString(), EmptyString(), nsnull,
                                 getter_AddRefs(ddoc));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!ddoc)
      return NS_OK;

    nsCOMPtr<nsIDOMNode> tmp;
    nsCOMPtr<nsIDOMElement> htmlEl;
    rv = ddoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                               NS_LITERAL_STRING("html"),
                               getter_AddRefs(htmlEl));
    NS_ENSURE_SUCCESS(rv, rv);
    htmlEl->SetAttribute(NS_LITERAL_STRING("style"),
                         NS_LITERAL_STRING("background-color: -moz-Dialog;"));
    ddoc->AppendChild(htmlEl, getter_AddRefs(tmp));

    nsCOMPtr<nsIDOMElement> bodyEl;
    rv = ddoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                               NS_LITERAL_STRING("body"),
                               getter_AddRefs(bodyEl));
    NS_ENSURE_SUCCESS(rv, rv);
    htmlEl->AppendChild(bodyEl, getter_AddRefs(tmp));

    // If we have a binding, it is enough to show a simple message
    if (hasBinding) {
      nsCOMPtr<nsIDOM3Node> body3(do_QueryInterface(bodyEl));
      if (body3)
        body3->SetTextContent(instanceData);
    } else {
      // Otherwise copying content from the original document to
      // the modeless/modal message document.
      nsCOMPtr<nsIDOMNode> tmp;
      nsCOMPtr<nsIDOMNodeList> childNodes;
      mElement->GetChildNodes(getter_AddRefs(childNodes));

      PRUint32 count = 0;
      if (childNodes)
        childNodes->GetLength(&count);

      for (PRUint32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        childNodes->Item(i, getter_AddRefs(child));
        
        if (child) {
          nsCOMPtr<nsIDOMNode> clone;
          ImportNode(child, ddoc, getter_AddRefs(clone));
          if (clone)
            bodyEl->AppendChild(clone, getter_AddRefs(tmp));
        }
      }
    }

    nsCOMPtr<nsIDOMSerializer> serializer =
      do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString docString;
    rv = serializer->SerializeToString(ddoc, docString);
    NS_ENSURE_SUCCESS(rv, rv);

    char* b64 =
      PL_Base64Encode(NS_ConvertUTF16toUTF8(docString).get(), 0, nsnull);
    if (!b64) {
      return NS_ERROR_FAILURE;
    }

    nsCAutoString b64String;
    b64String.AppendLiteral("data:application/vnd.mozilla.xul+xml;base64,");
    b64String.Append(b64);
    PR_Free(b64);

    CopyUTF8toUTF16(b64String, messageURL);
  }

  if (aLevel.EqualsLiteral("modal")) {
    options.AppendLiteral(",modal");
  } else if (aLevel.EqualsLiteral("modeless")) {
    options.AppendLiteral(",minimizable");
  }

  nsCOMPtr<nsISupportsString> arg(
    do_CreateInstance("@mozilla.org/supports-string;1", &rv));
  if (!arg)
    return rv;

  arg->SetData(messageURL);

  nsCOMPtr<nsISupportsArray> args(
    do_CreateInstance("@mozilla.org/supports-array;1", &rv));
  if (!args)
    return rv;

  args->AppendElement(arg);

  nsCOMPtr<nsIDOMWindow> messageWindow;
  // The 2nd argument is the window name, and if a window with the name exists,
  // it gets reused.  Using "_blank" makes sure we get a new window each time.
  internal->OpenDialog(NS_LITERAL_STRING(MESSAGE_WINDOW_URL),
                       NS_LITERAL_STRING("_blank"), options, args,
                       getter_AddRefs(messageWindow));
  return NS_OK;
}

void
sEphemeralCallbackShow(nsITimer *aTimer, void *aListener)
{
  nsXFormsMessageElement* self =
    NS_STATIC_CAST(nsXFormsMessageElement*, aListener);
  if (self)
    self->ShowEphemeral();
}

void
sEphemeralCallbackHide(nsITimer *aTimer, void *aListener)
{
  nsXFormsMessageElement* self =
    NS_STATIC_CAST(nsXFormsMessageElement*, aListener);
  if (self)
    self->HideEphemeral();
}

void
sEphemeralCallbackResetPosition(nsITimer *aTimer, void *aListener)
{
  nsXFormsMessageElement* self =
    NS_STATIC_CAST(nsXFormsMessageElement*, aListener);
  if (self)
    self->ResetEphemeralPosition();
}

void
nsXFormsMessageElement::StartEphemeral()
{
  HideEphemeral();
  if (!mElement)
    return;
  nsCOMPtr<nsIDOMDocument> domdoc;
  mElement->GetOwnerDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  if (!doc)
    return;
  doc->SetProperty(nsXFormsAtoms::messageProperty, this);
  mEphemeralTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (mEphemeralTimer)
    mEphemeralTimer->InitWithFuncCallback(sEphemeralCallbackShow, this, 
                                          SHOW_EPHEMERAL_TIMEOUT,
                                          nsITimer::TYPE_ONE_SHOT);
}

void
nsXFormsMessageElement::ShowEphemeral()
{
  if (mEphemeralTimer) {
    mEphemeralTimer->Cancel();
    mEphemeralTimer = nsnull;
  }
  if (!mElement)
    return;

  nsCOMPtr<nsIXFormsEphemeralMessageUI> ui(do_QueryInterface(mElement));
  if (ui) {
    ui->Show(mPosX, mPosY);
  }

  mEphemeralTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (mEphemeralTimer)
    mEphemeralTimer->InitWithFuncCallback(sEphemeralCallbackHide, this,
                                          HIDE_EPHEMERAL_TIMEOUT,
                                          nsITimer::TYPE_ONE_SHOT);
}

void
nsXFormsMessageElement::HideEphemeral()
{
  if (mEphemeralTimer) {
    mEphemeralTimer->Cancel();
    mEphemeralTimer = nsnull;
  }
  if (!mElement)
    return;

  nsCOMPtr<nsIDOMDocument> domdoc;
  mElement->GetOwnerDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  if (!doc)
    return;
  nsXFormsMessageElement *msg =
    NS_STATIC_CAST(nsXFormsMessageElement*,
                   doc->GetProperty(nsXFormsAtoms::messageProperty));
  if (msg && msg != this) {
    msg->HideEphemeral();
    return;
  }
  doc->UnsetProperty(nsXFormsAtoms::messageProperty);

  nsCOMPtr<nsIXFormsEphemeralMessageUI> ui(do_QueryInterface(mElement));
  if (ui) {
    ui->Hide();
  }

  mEphemeralTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (mEphemeralTimer)
    mEphemeralTimer->InitWithFuncCallback(sEphemeralCallbackResetPosition,
                                          this, 
                                          EPHEMERAL_POSITION_RESET_TIMEOUT,
                                          nsITimer::TYPE_ONE_SHOT);
}

nsresult
nsXFormsMessageElement::TestExternalFile()
{
  // Let's see if checking for any external resources is even necessary.  Single
  // node binding trumps linking attributes in order of precendence.  If we
  // find single node binding in evidence, then return NS_OK to show that
  // this message element has access to the info that it needs.
  nsAutoString snb;
  mElement->GetAttribute(NS_LITERAL_STRING("bind"), snb);
  if (!snb.IsEmpty()) {
    return NS_OK;
  }
  mElement->GetAttribute(NS_LITERAL_STRING("ref"), snb);
  if (!snb.IsEmpty()) {
    return NS_OK;
  }

  // if no linking attribute, no need to go on
  nsAutoString src;
  mElement->GetAttribute(NS_LITERAL_STRING("src"), src);
  if (src.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_STATE(doc);
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), src, doc->GetDocumentCharacterSet().get(),
            doc->GetDocumentURI());
  NS_ENSURE_STATE(uri);

  if (!nsXFormsUtils::CheckConnectionAllowed(mElement, uri)) {
    nsAutoString tagName;
    mElement->GetLocalName(tagName);
    const PRUnichar *strings[] = { tagName.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLinkLoadOrigin"),
                               strings, 1, mElement, mElement);
    // Keep the the dialog from popping up.  Won't be able to reach the
    // resource anyhow.
    mStopType = eStopType_Security;
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
  NS_WARN_IF_FALSE(loadGroup, "No load group!");

  // Using the same load group as the main document and creating
  // the channel with LOAD_NORMAL flag delays the dispatching of
  // the 'load' event until message data document has been loaded.
  nsresult rv = NS_NewChannel(getter_AddRefs(mChannel), uri, nsnull, loadGroup,
                              this, nsIRequest::LOAD_NORMAL);
  NS_ENSURE_TRUE(mChannel, rv);
  
  // See if it's an http channel.  We'll look at the http status code more
  // closely and only request the "HEAD" to keep the response small.
  // Especially since we are requesting this for every message in the document
  // and in most cases we pass off the URL to another browser service to
  // get and display the message so no good to try to look at the contents
  // anyway.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (httpChannel) {
    if (!IsEphemeral()) {
      PRBool isReallyHTTP = PR_FALSE;
      uri->SchemeIs("http", &isReallyHTTP);
      if (!isReallyHTTP) {
        uri->SchemeIs("https", &isReallyHTTP);
      }
      if (isReallyHTTP) {
        httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("HEAD"));
      }
    }
  }

  rv = mChannel->AsyncOpen(this, nsnull);
  if (NS_FAILED(rv)) {
    mChannel = nsnull;
  
    // URI doesn't exist; report error.
    // set up the error strings
    nsAutoString tagName;
    mElement->GetLocalName(tagName);
    const PRUnichar *strings[] = { src.get(), tagName.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLink1Error"),
                               strings, 2, mElement, mElement);
    mStopType = eStopType_LinkError;
    return NS_ERROR_FAILURE;
  }

  // channel should be running along smoothly, increment the count
  AddRemoveExternalResource(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::GetValue(nsAString& aValue)
{
  // The order of precedence for determining the text of the message
  // is: single node binding, linking, inline text (8.3.5).  We cache
  // the value of the external message (via mSrcAttrText) for hints
  // and ephemeral messages.
  
  nsXFormsDelegateStub::GetValue(aValue);
  if (aValue.IsVoid()) {
    if (!mSrcAttrText.IsEmpty()) {
      // handle linking ('src') attribute
      aValue = NS_ConvertUTF8toUTF16(mSrcAttrText);
    }
    else {
      // Return inline value
      nsCOMPtr<nsIDOM3Node> node = do_QueryInterface(mElement);
      if (node) {
        node->GetTextContent(aValue);
      }
    }
  }

  return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsXFormsMessageElement::GetInterface(const nsIID &aIID, void **aResult)
{
  *aResult = nsnull;
  return QueryInterface(aIID, aResult);
}

// nsIChannelEventSink

NS_IMETHODIMP
nsXFormsMessageElement::OnChannelRedirect(nsIChannel *OldChannel,
                                          nsIChannel *aNewChannel,
                                          PRUint32    aFlags)
{
  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");

  nsCOMPtr<nsIURI> newURI;
  nsresult rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!nsXFormsUtils::CheckConnectionAllowed(mElement, newURI)) {
    nsAutoString tagName;
    mElement->GetLocalName(tagName);
    const PRUnichar *strings[] = { tagName.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLinkLoadOrigin"),
                               strings, 1, mElement, mElement);
    mStopType = eStopType_Security;
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

// nsIStreamListener

NS_IMETHODIMP
nsXFormsMessageElement::OnStartRequest(nsIRequest *aRequest,
                                       nsISupports *aContext)
{
  // Make sure to null out mChannel before we return.  Keep in mind that
  // if this is the last message channel to be loaded for the xforms
  // document then when AddRemoveExternalResource is called, it may result
  // in xforms-ready firing. Should there be a message acting as a handler
  // for xforms-ready, it will start the logic to display itself
  // (HandleAction()).  So we can't call AddRemoveExternalResource to remove
  // this channel from the count until we've set the mStopType to be the
  // proper value.  Entering this function, mStopType will be eStopType_None,
  // so if we need mStopType to be any other value (like in an error
  // condition), please make sure it is set before AddRemoveExternalResource
  // is called.
  NS_ASSERTION(aRequest == mChannel, "unexpected request");
  NS_ASSERTION(mChannel, "no channel");

  if (!mElement) {
    AddRemoveExternalResource(PR_FALSE);
    mChannel = nsnull;
    return NS_BINDING_ABORTED;
  }

  nsresult status;
  nsresult rv = mChannel->GetStatus(&status);
  // DNS errors and other obvious problems will return failure status
  if (NS_FAILED(rv) || NS_FAILED(status)) {
    // NS_BINDING_ABORTED means that we have been cancelled by a later
    // AttributeSet() call (which will also reset mStopType), so don't
    // treat it like an error.
    if (status != NS_BINDING_ABORTED) {
      nsAutoString src, tagName;
      mElement->GetLocalName(tagName);
      mElement->GetAttribute(NS_LITERAL_STRING("src"), src);
      const PRUnichar *strings[] = { tagName.get(), src.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLink2Error"),
                                 strings, 2, mElement, mElement);
      mStopType = eStopType_LinkError;
    }

    AddRemoveExternalResource(PR_FALSE);
    mChannel = nsnull;

    return NS_BINDING_ABORTED;
  }

  // If status is zero, it might still be an error if it's http:
  // http has data even when there's an error like a 404.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (httpChannel) {
    // If responseStatus is 2xx, it is valid.
    // 3xx (various flavors of redirection) COULD be successful.  Can't really
    // follow those to conclusion so we'll assume they were successful.
    // If responseStatus is 4xx or 5xx, it is an error.
    PRUint32 responseStatus;
    rv = httpChannel->GetResponseStatus(&responseStatus);
    if (NS_FAILED(rv) || (responseStatus >= 400)) {
      nsAutoString src, tagName;
      mElement->GetLocalName(tagName);
      mElement->GetAttribute(NS_LITERAL_STRING("src"), src);
      const PRUnichar *strings[] = { tagName.get(), src.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLink2Error"),
                                 strings, 2, mElement, mElement);
      mStopType = eStopType_LinkError;
    }
  }

  // If the type is Hint or Ephemeral we want to return ns_ok so that callbacks
  // continue and we can read the contents of the src attribute during
  // OnDataAvailable. For all others, we return ns_binding_aborted because we
  // don't care to actually read the data at this point. The external content
  // for these other elements will be retrieved by the services that actually
  // display the popups.
  if (IsEphemeral() && mStopType == eStopType_None)
    return NS_OK;

  AddRemoveExternalResource(PR_FALSE);
  mChannel = nsnull;

  return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
nsXFormsMessageElement::OnDataAvailable(nsIRequest *aRequest,
                                        nsISupports *aContext,
                                        nsIInputStream *aInputStream,
                                        PRUint32 aOffset,
                                        PRUint32 aCount)
{
  if (!mElement) {
    AddRemoveExternalResource(PR_FALSE);
    mChannel = nsnull;
    return NS_BINDING_ABORTED;
  }

  if (!IsEphemeral())
    return NS_BINDING_ABORTED;

  nsresult rv;
  PRUint32 size, bytesRead;
  char buffer[256];

  while (aCount) {
    size = PR_MIN(aCount, sizeof(buffer));
    rv = aInputStream->Read(buffer, size, &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    mSrcAttrText.Append(buffer, bytesRead);
    aCount -= bytesRead;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::OnStopRequest(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsresult aStatusCode)
{
  if (IsEphemeral()) {
    nsCOMPtr<nsIXFormsUIWidget> widget = do_QueryInterface(mElement);
    if (widget)
      widget->Refresh();

    AddRemoveExternalResource(PR_FALSE);
    mChannel = nsnull;
  }

  return NS_OK;
}

void
nsXFormsMessageElement::AddRemoveExternalResource(PRBool aAdd)
{
  // if this message doesn't have a channel established already or it has
  // already returned, then no sense bumping the counter.
  if (!mChannel) {
    return;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  PRUint32 loadingMessages = NS_PTR_TO_UINT32(
    doc->GetProperty(nsXFormsAtoms::externalMessagesProperty));
  if (aAdd) {
    loadingMessages++;
  } else {
    if (loadingMessages) {
      loadingMessages--;
    }
  }
  doc->SetProperty(nsXFormsAtoms::externalMessagesProperty,
                   NS_REINTERPRET_CAST(void *, loadingMessages), nsnull);

  if (!loadingMessages) {
    // no outstanding loads left, let the model in the document know in case
    // the models are waiting to send out the xforms-ready event

    nsCOMPtr<nsIModelElementPrivate> modelPriv =
      nsXFormsUtils::GetModel(mElement);
    if (modelPriv) {
      // if there are no more messages loading then it is probably the case
      // that my mChannel is going to get nulled out as soon as this function
      // returns.  If the model is waiting for this notification, then it may
      // kick off the message right away and we should probably ensure that
      // mChannel is gone before HandleAction is called.  So...if the channel
      // isn't pending, let's null it out right here.
      PRBool isPending = PR_TRUE;
      mChannel->IsPending(&isPending);
      if (!isPending) {
        mChannel = nsnull;
      }
      modelPriv->MessageLoadFinished();
    }
  }
}

PRBool nsXFormsMessageElement::IsEphemeral()
{
  if (mType == eType_Hint)
    return PR_TRUE;

  nsAutoString level;
  mElement->GetAttribute(NS_LITERAL_STRING("level"), level);
  return level.Equals(NS_LITERAL_STRING("ephemeral"));
}

NS_HIDDEN_(nsresult)
NS_NewXFormsMessageElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsMessageElement(nsXFormsMessageElement::eType_Normal);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsHintElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsMessageElement(nsXFormsMessageElement::eType_Hint);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsHelpElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsMessageElement(nsXFormsMessageElement::eType_Help);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsAlertElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsMessageElement(nsXFormsMessageElement::eType_Alert);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
