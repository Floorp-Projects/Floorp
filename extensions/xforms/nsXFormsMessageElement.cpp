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
#include "nsXFormsActionElement.h"
#include "nsIXFormsActionModuleElement.h"

#include "nsIXTFXMLVisual.h"
#include "nsIXTFXMLVisualWrapper.h"

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

#define EPHEMERAL_STYLE \
  "position:absolute;z-index:2147483647; \
  background:inherit;color:inherit; \
  border:inherit;visibility:visible;"

#define EPHEMERAL_STYLE_HIDDEN \
  "position:absolute;z-index:2147483647;visibility:hidden;"

#define MESSAGE_WINDOW_PROPERTIES \
  "location=false,scrollbars=yes,centerscreen"


// Defining a simple dialog for modeless and modal messages.

#define MESSAGE_WINDOW_UI_PART1 \
  "<?xml version='1.0'?> \
   <?xml-stylesheet href='chrome://global/skin/' type='text/css'?> \
   <window title='[XForms]'\
     xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul' \
     onload='document.documentElement.lastChild.previousSibling \
             .firstChild.nextSibling.focus();'>"

#define MESSAGE_WINDOW_UI_PART2 \
  "<separator class='thin'/><hbox><spacer flex='1'/><button label='"

#define MESSAGE_WINDOW_UI_PART3 \
  "' oncommand='window.close();'/><spacer flex='1'/> \
  </hbox><separator class='thin'/></window>"

#define SHOW_EPHEMERAL_TIMEOUT           750
#define HIDE_EPHEMERAL_TIMEOUT           5000
#define EPHEMERAL_POSITION_RESET_TIMEOUT 100

class nsXFormsEventListener;
/**
 * Implementation of the XForms \<message\> element.
 *
 * @see http://www.w3.org/TR/xforms/slice10.html#action-info
 */
class nsXFormsMessageElement : public nsXFormsXMLVisualStub,
                               public nsIDOMEventListener,
                               public nsIXFormsActionModuleElement
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  
  // nsIXTFElement overrides
  NS_IMETHOD WillChangeDocument(nsIDOMDocument *aNewDocument);
  NS_IMETHOD OnDestroyed();
  
  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD WillChangeParent(nsIDOMElement *aNewParent);

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

  nsXFormsMessageElement(MessageType aType) :
    mType(aType), mElement(nsnull), mPosX(-1), mPosY(-1) {}
private:
  nsresult HandleEphemeralMessage(nsIDOMDocument* aDoc, nsIDOMEvent* aEvent);
  nsresult HandleModalAndModelessMessage(nsIDOMDocument* aDoc, nsAString& aLevel);
  void CloneNode(nsIDOMNode* aSrc, nsIDOMNode** aTarget);
  void AppendCSSOptions(nsIDOMViewCSS* aViewCSS, nsAString& aOptions);
  PRBool HandleInlineAlert(nsIDOMEvent* aEvent);
  nsresult ConstructMessageWindowURL(nsAString& aData,
                                     PRBool aIsLink,
                                     /*out*/ nsAString& aURL);

  MessageType mType;

  nsCOMPtr<nsIDOMElement> mVisualElement;
  nsIDOMElement *mElement;

  // The position of the ephemeral message
  PRInt32 mPosX;
  PRInt32 mPosY;

  nsCOMPtr<nsITimer> mEphemeralTimer;
  nsCOMPtr<nsIDOMDocument> mDocument;
};

NS_IMPL_ADDREF_INHERITED(nsXFormsMessageElement, nsXFormsXMLVisualStub)
NS_IMPL_RELEASE_INHERITED(nsXFormsMessageElement, nsXFormsXMLVisualStub)

NS_INTERFACE_MAP_BEGIN(nsXFormsMessageElement)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsActionModuleElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END_INHERITING(nsXFormsXMLVisualStub)

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsMessageElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  nsresult rv = nsXFormsXMLVisualStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);
  
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_WILL_CHANGE_DOCUMENT |
                                nsIXTFElement::NOTIFY_PARENT_CHANGED);

  nsCOMPtr<nsIDOMElement> node;
  rv = aWrapper->GetElementNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          mType == eType_Alert
                          ? NS_LITERAL_STRING("span")
                          : NS_LITERAL_STRING("div"),
                          getter_AddRefs(mVisualElement));
  if (mVisualElement && mType != eType_Alert)
      mVisualElement->SetAttribute(NS_LITERAL_STRING("style"),
                                   NS_LITERAL_STRING(EPHEMERAL_STYLE_HIDDEN));

  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsMessageElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mVisualElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mVisualElement);
  return NS_OK;
}

// nsIXTFElement

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
  }

  mDocument = aNewDocument;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::OnDestroyed()
{
  mElement = nsnull;
  mVisualElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::HandleEvent(nsIDOMEvent* aEvent)
{
  return nsXFormsUtils::EventHandlingAllowed(aEvent, mElement) ?
           HandleAction(aEvent, nsnull) : NS_OK;
}

void
nsXFormsMessageElement::CloneNode(nsIDOMNode* aSrc, nsIDOMNode** aTarget)
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
      nsCOMPtr<nsIDOMDocument> doc;
      aSrc->GetOwnerDocument(getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIDOMText> text;
        nsAutoString value;
        outEl->GetValue(value);
        doc->CreateTextNode(value, getter_AddRefs(text));
        NS_IF_ADDREF(*aTarget = text);
      }
    }
    return;
  }

  // Clone other elements
  aSrc->CloneNode(PR_FALSE, aTarget);

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
      CloneNode(child, getter_AddRefs(clone));
      if (clone)
        (*aTarget)->AppendChild(clone, getter_AddRefs(tmp));
    }
  }
}

NS_IMETHODIMP
nsXFormsMessageElement::WillChangeParent(nsIDOMElement *aNewParent)
{
  if (mType == eType_Normal)
    return NS_OK;
  
  nsCOMPtr<nsIDOMNode> parent;
  mElement->GetParentNode(getter_AddRefs(parent));
  if (!parent)
    return NS_OK;

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

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::ParentChanged(nsIDOMElement *aNewParent)
{
  if (mType == eType_Normal || !aNewParent)
    return NS_OK;

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

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsMessageElement::HandleAction(nsIDOMEvent* aEvent, 
                                     nsIXFormsActionElement *aParentAction)
{
  if (!mElement)
    return NS_OK;

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
  mElement->GetOwnerDocument(getter_AddRefs(doc));
  
  nsCOMPtr<nsIDOMDocumentView> dview(do_QueryInterface(doc));
  if (!dview)
    return PR_FALSE;

  nsCOMPtr<nsIDOMAbstractView> aview;
  dview->GetDefaultView(getter_AddRefs(aview));
  if (!aview)
    return PR_FALSE;

  nsCOMPtr<nsIDOMWindowInternal> internal(do_QueryInterface(aview));
  if (!internal)
    return PR_FALSE;

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
      if (type.EqualsLiteral("none"))
        return PR_FALSE;

      nsAutoString instanceData;
      PRBool hasBinding = nsXFormsUtils::GetSingleNodeBindingValue(mElement,
                                                                   instanceData);
      if (hasBinding) {
        nsCOMPtr<nsIDOM3Node> visualElement3(do_QueryInterface(mVisualElement));
        if (visualElement3) {
          visualElement3->SetTextContent(instanceData);
        }
      }
      return PR_TRUE;
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
  
        if (mVisualElement) {
          mVisualElement->SetAttribute(NS_LITERAL_STRING("style"),
                                       NS_LITERAL_STRING(EPHEMERAL_STYLE_HIDDEN));
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

      nsAutoString instanceData;
      PRBool hasBinding = nsXFormsUtils::GetSingleNodeBindingValue(mElement,
                                                                   instanceData);
      if (hasBinding) {
        nsCOMPtr<nsIDOM3Node> visualElement3(do_QueryInterface(mVisualElement));
        if (visualElement3) {
          visualElement3->SetTextContent(instanceData);
        }
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
  nsCOMPtr<nsIDOMDocumentView> dview(do_QueryInterface(aDoc));
  if (!dview)
    return NS_OK;

  nsCOMPtr<nsIDOMAbstractView> aview;
  dview->GetDefaultView(getter_AddRefs(aview));

  nsCOMPtr<nsIDOMWindowInternal> internal(do_QueryInterface(aview));
  if (!internal)
    return NS_OK;

  nsAutoString instanceData;
  PRBool hasBinding = nsXFormsUtils::GetSingleNodeBindingValue(mElement,
                                                               instanceData);

  nsAutoString options;
  options.AppendLiteral(MESSAGE_WINDOW_PROPERTIES);

  nsAutoString src;
  if (!hasBinding) {
    mElement->GetAttribute(NS_LITERAL_STRING("src"), src);
  }

  nsCOMPtr<nsIDOMViewCSS> cssView(do_QueryInterface(internal));
  AppendCSSOptions(cssView, options);

  // order of precedence is single-node binding, linking attribute then
  // inline text
  nsresult rv;
  if (!hasBinding && !src.IsEmpty()) {
    // Creating a normal window for messages with src attribute.
    options.AppendLiteral(",chrome=no");
    rv = ConstructMessageWindowURL(src, PR_TRUE, src);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Cloning the content of the xf:message and creating a
    // dialog for it.
    options.AppendLiteral(",dialog,chrome,dependent");
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
          CloneNode(child, getter_AddRefs(clone));
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
    
    rv = ConstructMessageWindowURL(docString, PR_FALSE, src);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  nsCOMPtr<nsISupports> arg;
  PRBool isModal = aLevel.EqualsLiteral("modal");
  if (isModal) {
    options.AppendLiteral(",modal");
    // We need to have an argument to set the window modal.
    // Using nsXFormsAtoms::messageProperty so that we don't create new
    // cycles between the windows.
    arg = nsXFormsAtoms::messageProperty;
  }
  
  //XXX Add support for xforms-link-error.
  nsCOMPtr<nsIDOMWindow> messageWindow;
  internal->OpenDialog(src, aLevel, options, arg, getter_AddRefs(messageWindow));
  if (!isModal) {
    nsCOMPtr<nsIDOMWindowInternal> msgWinInternal =
      do_QueryInterface(messageWindow);
    if (msgWinInternal)
      msgWinInternal->Focus();
  }
  return NS_OK;
}

void
nsXFormsMessageElement::AppendCSSOptions(nsIDOMViewCSS* aViewCSS, nsAString& aOptions)
{
  if (!aViewCSS)
    return;
  // Try to get the calculated size of the message element. It will
  // be used for the new window.
  //XXX This could be extended also for 'top', 'left' etc. properties.
  PRInt32 computedWidth = 0;
  PRInt32 computedHeight = 0;
  
  nsAutoString tmp;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> styles;
  aViewCSS->GetComputedStyle(mElement, tmp, getter_AddRefs(styles));
  if (styles) {
    nsCOMPtr<nsIDOMCSSValue> cssWidth;
    styles->GetPropertyCSSValue(NS_LITERAL_STRING("width"),
                                getter_AddRefs(cssWidth));
    nsCOMPtr<nsIDOMCSSPrimitiveValue> pvalueWidth(do_QueryInterface(cssWidth));
    float width = 0;
    if (pvalueWidth) {
      PRUint16 type;
      pvalueWidth->GetPrimitiveType(&type);
      if (type == nsIDOMCSSPrimitiveValue::CSS_PX)
        pvalueWidth->GetFloatValue(type, &width);
    }

    nsCOMPtr<nsIDOMCSSValue> cssHeight;
    styles->GetPropertyCSSValue(NS_LITERAL_STRING("height"),
                                getter_AddRefs(cssHeight));
    nsCOMPtr<nsIDOMCSSPrimitiveValue> pvalueHeight(do_QueryInterface(cssHeight));
    float height = 0;
    if (pvalueHeight) {
      PRUint16 type;
      pvalueHeight->GetPrimitiveType(&type);
      if (type == nsIDOMCSSPrimitiveValue::CSS_PX)
        pvalueHeight->GetFloatValue(type, &height);
    }
    computedWidth = NS_STATIC_CAST(PRInt32, width);
    computedHeight = NS_STATIC_CAST(PRInt32, height);
  }

  if (computedWidth > 0 && computedHeight > 0) {
    nsAutoString options;
    options.AppendLiteral(",innerWidth=");
    options.AppendInt(computedWidth);
    options.AppendLiteral(",innerHeight=");
    options.AppendInt(computedHeight);
    aOptions.Append(options);
  }
}

nsresult
nsXFormsMessageElement::ConstructMessageWindowURL(nsAString& aData,
                                                  PRBool aIsLink,
                                                  nsAString& aURL)
{
  nsXPIDLString messageOK;
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (bundleService) {
    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle("chrome://global/locale/dialog.properties",
                                 getter_AddRefs(bundle));
    if (bundle) {
      bundle->GetStringFromName(NS_LITERAL_STRING("button-accept").get(),
                                getter_Copies(messageOK));
    }
  }
  if (messageOK.IsEmpty())
    messageOK.AssignLiteral("OK");

  nsAutoString xul;
  xul.AssignLiteral(MESSAGE_WINDOW_UI_PART1);
  if (aIsLink)
    xul.AppendLiteral("<browser flex='1' src='");

  xul.Append(aData);

  if (aIsLink)
    xul.AppendLiteral("'/>");
  else
    xul.AppendLiteral("<spacer flex='1'/>");

  xul.AppendLiteral(MESSAGE_WINDOW_UI_PART2);
  xul.Append(messageOK);
  xul.AppendLiteral(MESSAGE_WINDOW_UI_PART3);

  char* b64 = PL_Base64Encode(NS_ConvertUTF16toUTF8(xul).get(), 0, nsnull);
  if (!b64)
    return NS_ERROR_FAILURE;

  nsCAutoString b64String;
  b64String.AppendLiteral("data:application/vnd.mozilla.xul+xml;base64,");
  b64String.Append(b64);
  PR_Free(b64);

  CopyUTF8toUTF16(b64String, aURL);
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

  nsAutoString style;
  style.AppendLiteral(EPHEMERAL_STYLE);
  style.AppendLiteral("left:");
  style.AppendInt(mPosX);
  style.AppendLiteral("px;top:");
  style.AppendInt(mPosY);
  style.AppendLiteral("px;");
  mVisualElement->SetAttribute(NS_LITERAL_STRING("style"), style);
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

  if (mVisualElement)
    mVisualElement->SetAttribute(NS_LITERAL_STRING("style"),
                                 NS_LITERAL_STRING(EPHEMERAL_STYLE_HIDDEN));

  mEphemeralTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (mEphemeralTimer)
    mEphemeralTimer->InitWithFuncCallback(sEphemeralCallbackResetPosition,
                                          this, 
                                          EPHEMERAL_POSITION_RESET_TIMEOUT,
                                          nsITimer::TYPE_ONE_SHOT);
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
