/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsINameSpaceManager.h"
#include "nsIURL.h"
#include "nsIFocusableContent.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsNetUtil.h"

#include "nsCOMPtr.h"
#include "nsIFrameManager.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"

// XXX suppress

// XXX either suppress is handled in the event code below OR we need a
// custom frame

static NS_DEFINE_IID(kIDOMHTMLAnchorElementIID, NS_IDOMHTMLANCHORELEMENT_IID);
static NS_DEFINE_IID(kIFocusableContentIID, NS_IFOCUSABLECONTENT_IID);

class nsHTMLAnchorElement : public nsIDOMHTMLAnchorElement,
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent,
                            public nsIFocusableContent
{
public:
  nsHTMLAnchorElement(nsIAtom* aTag);
  virtual ~nsHTMLAnchorElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLAnchorElement
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetCharset(nsString& aCharset);
  NS_IMETHOD SetCharset(const nsString& aCharset);
  NS_IMETHOD GetCoords(nsString& aCoords);
  NS_IMETHOD SetCoords(const nsString& aCoords);
  NS_IMETHOD GetHref(nsString& aHref);
  NS_IMETHOD SetHref(const nsString& aHref);
  NS_IMETHOD GetHreflang(nsString& aHreflang);
  NS_IMETHOD SetHreflang(const nsString& aHreflang);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetRel(nsString& aRel);
  NS_IMETHOD SetRel(const nsString& aRel);
  NS_IMETHOD GetRev(nsString& aRev);
  NS_IMETHOD SetRev(const nsString& aRev);
  NS_IMETHOD GetShape(nsString& aShape);
  NS_IMETHOD SetShape(const nsString& aShape);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetTarget(nsString& aTarget);
  NS_IMETHOD SetTarget(const nsString& aTarget);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD SetType(const nsString& aType);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIFocusableContent
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

protected:
  nsGenericHTMLContainerElement mInner;
};

nsresult
NS_NewHTMLAnchorElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLAnchorElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLAnchorElement::nsHTMLAnchorElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLAnchorElement::~nsHTMLAnchorElement()
{
}

NS_IMPL_ADDREF(nsHTMLAnchorElement)

NS_IMPL_RELEASE(nsHTMLAnchorElement)

nsresult
nsHTMLAnchorElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLAnchorElementIID)) {
    nsIDOMHTMLAnchorElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kIFocusableContentIID)) {
    *aInstancePtr = (void*)(nsIFocusableContent*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLAnchorElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLAnchorElement* it = new nsHTMLAnchorElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Charset, charset)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Coords, coords)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Shape, shape)
NS_IMPL_INT_ATTR(nsHTMLAnchorElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Target, target)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Type, type)

NS_IMETHODIMP
nsHTMLAnchorElement::Blur()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::Focus()
{
  nsIDocument* doc; // Strong
  nsresult rv = GetDocument(doc);
  if (NS_FAILED(rv)) { return rv; }
  if (!doc) { return NS_ERROR_NULL_POINTER; }

  PRInt32 numShells = doc->GetNumberOfShells();
  nsIPresShell* shell = nsnull; // Strong
  nsCOMPtr<nsIPresContext> context;
  for (PRInt32 i=0; i<numShells; i++) 
  {
    shell = doc->GetShellAt(i);
    if (!shell) { return NS_ERROR_NULL_POINTER; }

    rv = shell->GetPresContext(getter_AddRefs(context));
    if (NS_FAILED(rv)) { return rv; }
    if (!context) { return NS_ERROR_NULL_POINTER; }

    rv = SetFocus(context);
    if (NS_FAILED(rv)) { return rv; }

    NS_RELEASE(shell);
  }
  NS_RELEASE(doc);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetFocus(nsIPresContext* aPresContext)
{

  // don't make the link grab the focus if there is no link handler
  nsILinkHandler* handler;
  nsresult rv = aPresContext->GetLinkHandler(&handler);
  if (NS_SUCCEEDED(rv) && (nsnull != handler)) {
    nsIEventStateManager *stateManager;
    if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
      stateManager->SetContentState(this, NS_EVENT_STATE_FOCUS);

      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      if (presShell) {
        nsCOMPtr<nsIFrameManager> frameManager;
        presShell->GetFrameManager(getter_AddRefs(frameManager));
        if (frameManager) {

          nsIFrame* frame = nsnull;
          frameManager->GetPrimaryFrameFor(this, &frame);
          if (frame) {
            presShell->ScrollFrameIntoView(frame,
                           NS_PRESSHELL_SCROLL_ANYWHERE,NS_PRESSHELL_SCROLL_ANYWHERE);
          }
        }
      }

      NS_RELEASE(stateManager);
    }
    NS_RELEASE(handler);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::RemoveFocus(nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::tabindex) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, 32767, aResult,
                                         eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::suppress) {
    if (aValue.EqualsIgnoreCase("true")) {
      aResult.SetEmptyValue();  // XXX? shouldn't just leave "true"
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLAnchorElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

// XXX support suppress in here
NS_IMETHODIMP
nsHTMLAnchorElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aEventStatus);
  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags, aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE)) {
    // If this anchor element has an HREF then it is sensitive to
    // mouse events (otherwise ignore them).
    nsAutoString href;
    nsresult result = GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, href);
    if (NS_CONTENT_ATTR_HAS_VALUE == result) {
      switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
        {
          // don't make the link grab the focus if there is no link handler
          nsILinkHandler* handler;
          nsresult rv = aPresContext->GetLinkHandler(&handler);
          if (NS_SUCCEEDED(rv) && (nsnull != handler)) {
            nsIEventStateManager *stateManager;
            if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
              stateManager->SetContentState(this, NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);
              NS_RELEASE(stateManager);
            }
            NS_RELEASE(handler);
            *aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
        break;

      case NS_MOUSE_LEFT_CLICK:
      case NS_KEY_PRESS:
      {
        if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {

          nsKeyEvent * keyEvent;
          if (aEvent->eventStructType == NS_KEY_EVENT) {
            //Handle key commands from keys with char representation here, not on KeyDown
            keyEvent = (nsKeyEvent *)aEvent;
          }

          //Click or return key
          if (aEvent->message == NS_MOUSE_LEFT_CLICK || keyEvent->keyCode == NS_VK_RETURN) {
            nsAutoString target;
            nsIURI* baseURL = nsnull;
            GetBaseURL(baseURL);
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
            if (target.Length() == 0) {
              GetBaseTarget(target);
            }
            mInner.TriggerLink(aPresContext, eLinkVerb_Replace,
                               baseURL, href, target, PR_TRUE);
            NS_IF_RELEASE(baseURL);
            *aEventStatus = nsEventStatus_eConsumeDoDefault;
          }
        }
      }
      break;

      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        // XXX Bring up a contextual menu provided by the application
        break;

      case NS_MOUSE_ENTER:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(this, NS_EVENT_STATE_HOVER);
          NS_RELEASE(stateManager);
        }

        nsAutoString target;
        nsIURI* baseURL = nsnull;
        GetBaseURL(baseURL);
        GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
        if (target.Length() == 0) {
          GetBaseTarget(target);
        }
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace,
                           baseURL, href, target, PR_FALSE);
        NS_IF_RELEASE(baseURL);
        *aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

      case NS_MOUSE_EXIT:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(nsnull, NS_EVENT_STATE_HOVER);
          NS_RELEASE(stateManager);
        }

        nsAutoString empty;
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace, nsnull, empty, empty, PR_FALSE);
        *aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

      default:
        break;
      }
    }
  }
  return ret;
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetHref(nsString& aValue)
{
  // Resolve url to an absolute url
  nsresult rv = NS_OK;
  nsAutoString relURLSpec;
  nsIURI* baseURL = nsnull;

  // Get base URL.
  mInner.GetBaseURL(baseURL);

  // Get href= attribute (relative URL).
  mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, relURLSpec);

  // If there is no href=, then use base target.
  if (relURLSpec.Length() == 0) {
    mInner.GetBaseTarget(relURLSpec);
  }

  if (nsnull != baseURL) {
    // Get absolute URL.
    rv = NS_MakeAbsoluteURI(relURLSpec, baseURL, aValue);
  }
  else {
    // Absolute URL is same as relative URL.
    aValue = relURLSpec;
  }
  NS_IF_RELEASE(baseURL);
  return rv;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetHref(const nsString& aValue)
{
  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLAnchorElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}
