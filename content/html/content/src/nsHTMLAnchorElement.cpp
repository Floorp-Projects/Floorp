/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsINameSpaceManager.h"
#include "nsIURL.h"
#include "nsIFocusableContent.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"

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
    mRefCnt++;
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
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Href, href)
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
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetFocus(nsIPresContext* aPresContext)
{
  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {
    esm->SetFocusedContent(this);
    NS_RELEASE(esm);
  }

  // XXX write me
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
    nsGenericHTMLElement::ParseValue(aValue, 0, 32767, aResult,
                                     eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if (aAttribute == nsHTMLAtoms::href) {
    nsAutoString href(aValue);
    href.StripWhitespace();
    aResult.SetStringValue(href);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if (aAttribute == nsHTMLAtoms::suppress) {
    if (aValue.EqualsIgnoreCase("true")) {
      aResult.SetEmptyValue();
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
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

// XXX support suppress in here
NS_IMETHODIMP
nsHTMLAnchorElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus& aEventStatus)
{
  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags, aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == aEventStatus)) {
    // If this anchor element has an HREF then it is sensitive to
    // mouse events (otherwise ignore them).
    nsAutoString href;
    nsresult result = GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, href);
    if (NS_CONTENT_ATTR_HAS_VALUE == result) {
      switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->SetActiveLink(this);
          NS_RELEASE(stateManager);
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

      case NS_MOUSE_LEFT_CLICK:
      {
        nsIEventStateManager *stateManager;
        nsLinkEventState linkState;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->GetLinkState(this, linkState);
          NS_RELEASE(stateManager);
        }

        if (eLinkState_Active == linkState) {
          if (nsEventStatus_eConsumeNoDefault != aEventStatus) {
            nsAutoString target;
            nsIURL* baseURL = nsnull;
            GetBaseURL(baseURL);
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
            if (target.Length() == 0) {
              GetBaseTarget(target);
            }
            mInner.TriggerLink(aPresContext, eLinkVerb_Replace,
                               baseURL, href, target, PR_TRUE);
            NS_IF_RELEASE(baseURL);
            aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
      }
      break;

      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        // XXX Bring up a contextual menu provided by the application
        break;

      case NS_MOUSE_ENTER:
      {
        nsAutoString target;
        nsIURL* baseURL = nsnull;
        GetBaseURL(baseURL);
        GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
        if (target.Length() == 0) {
          GetBaseTarget(target);
        }
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace,
                           baseURL, href, target, PR_FALSE);
        NS_IF_RELEASE(baseURL);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      case NS_MOUSE_EXIT:
      {
        nsAutoString empty;
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace, nsnull, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
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
nsHTMLAnchorElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  if ((aAttribute == nsHTMLAtoms::charset) ||
      (aAttribute == nsHTMLAtoms::coords) ||
      (aAttribute == nsHTMLAtoms::href) ||
      (aAttribute == nsHTMLAtoms::hreflang) ||
      (aAttribute == nsHTMLAtoms::name) ||
      (aAttribute == nsHTMLAtoms::rel) ||
      (aAttribute == nsHTMLAtoms::rev) ||
      (aAttribute == nsHTMLAtoms::shape) ||
      (aAttribute == nsHTMLAtoms::tabindex) ||
      (aAttribute == nsHTMLAtoms::target) ||
      (aAttribute == nsHTMLAtoms::type)) {
    *aHint = NS_STYLE_HINT_CONTENT;
  }
  else if (aAttribute == nsHTMLAtoms::accesskey) {
    // XXX Notification needs to happen for this attribute
    *aHint = NS_STYLE_HINT_CONTENT;
  }
  else {
    nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  }
  return NS_OK;
}
