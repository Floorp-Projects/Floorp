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
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"

static NS_DEFINE_IID(kIDOMHTMLAnchorElementIID, NS_IDOMHTMLANCHORELEMENT_IID);

class nsHTMLAnchor : public nsIDOMHTMLAnchorElement,
                     public nsIScriptObjectOwner,
                     public nsIDOMEventReceiver,
                     public nsIHTMLContent
{
public:
  nsHTMLAnchor(nsIAtom* aTag);
  ~nsHTMLAnchor();

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

protected:
  nsHTMLGenericContainerContent mInner;
};

nsresult
NS_NewHTMLAnchor(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLAnchor(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLAnchor::nsHTMLAnchor(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLAnchor::~nsHTMLAnchor()
{
}

NS_IMPL_ADDREF(nsHTMLAnchor)

NS_IMPL_RELEASE(nsHTMLAnchor)

nsresult
nsHTMLAnchor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLAnchorElementIID)) {
    nsIDOMHTMLAnchorElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLAnchor::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLAnchor* it = new nsHTMLAnchor(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLAnchor::GetAccessKey(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::accesskey, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetAccessKey(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::accesskey, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetCharset(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetCharset(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::charset, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetCoords(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::coords, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetCoords(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::coords, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetHref(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::href, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetHref(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::href, aValue, eSetAttrNotify_Render);
}

NS_IMETHODIMP
nsHTMLAnchor::GetHreflang(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::hreflang, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetHreflang(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::hreflang, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetName(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::name, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetName(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::name, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetRel(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::rel, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetRel(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::rel, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetRev(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::rev, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetRev(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::rev, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetShape(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::shape, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetShape(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::shape, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetTabIndex(PRInt32* aValue)
{
  nsHTMLValue value;
  *aValue = -1;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mInner.GetAttribute(nsHTMLAtoms::tabindex, value)) {
    if (value.GetUnit() == eHTMLUnit_Integer) {
      *aValue = value.GetIntValue();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetTabIndex(PRInt32 aValue)
{
  nsHTMLValue value(aValue, eHTMLUnit_Integer);
  return mInner.SetAttr(nsHTMLAtoms::tabindex, value, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetTarget(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::target, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetTarget(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::target, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::GetType(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::type, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::SetType(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::type, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLAnchor::Blur()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::Focus()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::StringToAttribute(nsIAtom* aAttribute,
                                const nsString& aValue,
                                nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::tabindex) {
    nsHTMLGenericContent::ParseValue(aValue, 0, 32767, aResult,
                                     eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLAnchor::AttributeToString(nsIAtom* aAttribute,
                                nsHTMLValue& aValue,
                                nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLAnchor::MapAttributesInto(nsIStyleContext* aContext,
                                nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchor::HandleDOMEvent(nsIPresContext& aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus& aEventStatus)
{
  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags, aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == aEventStatus)) {
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

    case NS_MOUSE_LEFT_BUTTON_UP:
      {
        nsIEventStateManager *stateManager;
        nsIContent *activeLink;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->GetActiveLink(&activeLink);
          NS_RELEASE(stateManager);
        }

        if (activeLink == this) {
          nsEventStatus status;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_LEFT_CLICK;
          HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status);

          if (nsEventStatus_eConsumeNoDefault != status) {
            nsAutoString base, href, target;
            GetAttribute(nsString(NS_HTML_BASE_HREF), base);
            GetAttribute(nsString("href"), href);
            GetAttribute(nsString("target"), target);
            if (target.Length() == 0) {
              GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
            }
            mInner.TriggerLink(aPresContext, base, href, target, PR_TRUE);
            aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
      }
      break;

    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // XXX Bring up a contextual menu provided by the application
      break;

    case NS_MOUSE_ENTER:
      //mouse enter doesn't work yet.  Use move until then.
      {
        nsAutoString base, href, target;
        GetAttribute(nsString(NS_HTML_BASE_HREF), base);
        GetAttribute(nsString("href"), href);
        GetAttribute(nsString("target"), target);
        if (target.Length() == 0) {
          GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
        }
        mInner.TriggerLink(aPresContext, base, href, target, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT:
      {
        nsAutoString empty;
        mInner.TriggerLink(aPresContext, empty, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

    default:
      break;
    }
  }
  return ret;
}
