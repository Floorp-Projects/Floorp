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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsXMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsNetUtil.h"
#include "nsIEventListenerManager.h"
#include "nsIDocShell.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIScriptSecurityManager.h"
#include "nsIRefreshURI.h"
#include "nsStyleConsts.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"
#include "nsPresContext.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsIXBLService.h"
#include "nsIXBLBinding.h"
#include "nsIBindingManager.h"

nsresult
NS_NewXMLElement(nsIContent** aInstancePtrResult, nsINodeInfo *aNodeInfo)
{
  nsXMLElement* it = new nsXMLElement(aNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = it);

  return NS_OK;
}

nsXMLElement::nsXMLElement(nsINodeInfo *aNodeInfo)
  : nsGenericElement(aNodeInfo),
    mIsLink(PR_FALSE)
{
}

nsXMLElement::~nsXMLElement()
{
}


// QueryInterface implementation for nsXMLElement
NS_IMETHODIMP 
nsXMLElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  *aInstancePtr = nsnull;

  nsresult rv = nsGenericElement::QueryInterface(aIID, aInstancePtr);

  if (NS_SUCCEEDED(rv))
    return rv;

  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {
    inst = NS_STATIC_CAST(nsIDOMNode *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMElement))) {
    inst = NS_STATIC_CAST(nsIDOMElement *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIXMLContent))) {
    inst = NS_STATIC_CAST(nsIXMLContent *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    inst = nsContentUtils::GetClassInfoInstance(eDOMClassInfo_Element_id);
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else {
    return PostQueryInterface(aIID, aInstancePtr);
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}


NS_IMPL_ADDREF_INHERITED(nsXMLElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsXMLElement, nsGenericElement)

nsresult
nsXMLElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                      const nsAString& aValue, PRBool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_XLink && aName == nsHTMLAtoms::type) { 

    // NOTE: This really is a link according to the XLink spec,
    //       we do not need to check other attributes. If there
    //       is no href attribute, then this link is simply
    //       untraversible [XLink 3.2].
    mIsLink = aValue.EqualsLiteral("simple");

    // We will check for actuate="onLoad" in MaybeTriggerAutoLink
  }

  return nsGenericElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                   aNotify);
}

static nsresult
DocShellToPresContext(nsIDocShell *aShell, nsPresContext **aPresContext)
{
  *aPresContext = nsnull;

  nsresult rv;
  nsCOMPtr<nsIDocShell> ds = do_QueryInterface(aShell,&rv);
  if (NS_FAILED(rv))
    return rv;

  return ds->GetPresContext(aPresContext);
}

static inline
nsresult SpecialAutoLoadReturn(nsresult aRv, nsLinkVerb aVerb)
{
  if (NS_SUCCEEDED(aRv)) {
    switch(aVerb) {
      case eLinkVerb_Embed:
        aRv = NS_XML_AUTOLINK_EMBED;
        break;
      case eLinkVerb_New:
        aRv = NS_XML_AUTOLINK_NEW;
        break;
      case eLinkVerb_Replace:
        aRv = NS_XML_AUTOLINK_REPLACE;
        break;
      default:
        aRv = NS_XML_AUTOLINK_UNDEFINED;
        break;
    }
  }
  return aRv;
}

NS_IMETHODIMP
nsXMLElement::MaybeTriggerAutoLink(nsIDocShell *aShell)
{
  NS_ENSURE_ARG_POINTER(aShell);

  nsresult rv = NS_OK;

  if (mIsLink) {
    NS_NAMED_LITERAL_STRING(onloadString, "onLoad");
    do {
      // actuate="onLoad" ?
      nsAutoString value;
      rv = nsGenericElement::GetAttr(kNameSpaceID_XLink,
                                     nsLayoutAtoms::actuate, value);
      if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
          value.Equals(onloadString)) {

        // Disable in Mail/News for now. We may want a pref to control
        // this at some point.
        nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(aShell));
        if (docShellItem) {
          nsCOMPtr<nsIDocShellTreeItem> rootItem;
          docShellItem->GetRootTreeItem(getter_AddRefs(rootItem));
          nsCOMPtr<nsIDocShell> docshell(do_QueryInterface(rootItem));
          if (docshell) {
            PRUint32 appType;
            if (NS_SUCCEEDED(docshell->GetAppType(&appType)) &&
                appType == nsIDocShell::APP_TYPE_MAIL) {
              return NS_OK;
            }
          }
        }

        // show= ?
        nsLinkVerb verb = eLinkVerb_Undefined; // basically means same as replace
        rv = nsGenericElement::GetAttr(kNameSpaceID_XLink,
                                       nsLayoutAtoms::show, value);
        if (NS_FAILED(rv))
          break;

        // XXX Should probably do this using atoms 
        if (value.EqualsLiteral("new")) {
          if (nsContentUtils::GetBoolPref("dom.disable_open_during_load")) {
            // disabling open during load

            return NS_OK;
          }

          if (!nsContentUtils::GetBoolPref("browser.block.target_new_window")) {
            // not blocking new windows
            verb = eLinkVerb_New;
          }
        } else if (value.EqualsLiteral("replace")) {
          // We want to actually stop processing the current document now.
          // We do this by returning the correct value so that the one
          // that called us knows to stop processing.
          verb = eLinkVerb_Replace;
        } else if (value.EqualsLiteral("embed")) {
          // XXX TODO
          break;
        }

        // base
        nsCOMPtr<nsIURI> base = GetBaseURI();
        if (!base)
          break;

        // href= ?
        rv = nsGenericElement::GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::href,
                                       value);
        if (rv == NS_CONTENT_ATTR_HAS_VALUE && !value.IsEmpty()) {
          nsCOMPtr<nsIURI> uri;
          rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                         value,
                                                         mDocument,
                                                         base);
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsPresContext> pc;
            rv = DocShellToPresContext(aShell, getter_AddRefs(pc));
            if (NS_SUCCEEDED(rv)) {
              rv = TriggerLink(pc, verb, base, uri,
                               EmptyString(), PR_TRUE, PR_FALSE);

              return SpecialAutoLoadReturn(rv,verb);
            }
          }
        } // href
      }
    } while (0);
  }

  return rv;
}

nsresult
nsXMLElement::HandleDOMEvent(nsPresContext* aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  // Try script event handlers first
  nsresult ret = nsGenericElement::HandleDOMEvent(aPresContext, aEvent,
                                                  aDOMEvent, aFlags,
                                                  aEventStatus);

  if (mIsLink && (NS_OK == ret) && (nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE) && !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT)) {
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        aPresContext->EventStateManager()->
          SetContentState(this, NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);

        *aEventStatus = nsEventStatus_eConsumeDoDefault;
      }
      break;

    case NS_MOUSE_LEFT_CLICK:
      {
        if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
          nsInputEvent* inputEvent = NS_STATIC_CAST(nsInputEvent*, aEvent);
          if (inputEvent->isControl || inputEvent->isMeta ||
              inputEvent->isAlt || inputEvent->isShift) {
            break;  // let the click go through so we can handle it in JS/XUL
          }
          nsAutoString show, href;
          nsLinkVerb verb = eLinkVerb_Undefined; // basically means same as replace
          nsCOMPtr<nsIURI> uri = nsContentUtils::GetXLinkURI(this);
          if (!uri) {
            *aEventStatus = nsEventStatus_eConsumeDoDefault; 
            break;
          }

          nsGenericElement::GetAttr(kNameSpaceID_XLink, nsLayoutAtoms::show,
                                    show);

          // XXX Should probably do this using atoms 
          if (show.EqualsLiteral("new")) {
            if (!nsContentUtils::GetBoolPref("browser.block.target_new_window")) {
              verb = eLinkVerb_New;
            }
          } else if (show.EqualsLiteral("replace")) {
            verb = eLinkVerb_Replace;
          } else if (show.EqualsLiteral("embed")) {
            verb = eLinkVerb_Embed;
          }

          nsCOMPtr<nsIURI> baseURI = GetBaseURI();
          ret = TriggerLink(aPresContext, verb, baseURI, uri,
                            EmptyString(), PR_TRUE, PR_TRUE);

          *aEventStatus = nsEventStatus_eConsumeDoDefault; 
        }
      }
      break;

    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // XXX Bring up a contextual menu provided by the application
      break;

    case NS_KEY_PRESS:
      if (aEvent->eventStructType == NS_KEY_EVENT) {
        nsKeyEvent* keyEvent = NS_STATIC_CAST(nsKeyEvent*, aEvent);
        if (keyEvent->keyCode == NS_VK_RETURN) {
          nsEventStatus status = nsEventStatus_eIgnore;

          //fire click
          nsGUIEvent* guiEvent = NS_STATIC_CAST(nsGUIEvent*, aEvent);
          nsMouseEvent event(NS_MOUSE_LEFT_CLICK, guiEvent->widget);
          event.point = aEvent->point;
          event.refPoint = aEvent->refPoint;
          event.clickCount = 1;
          event.isShift = keyEvent->isShift;
          event.isControl = keyEvent->isControl;
          event.isAlt = keyEvent->isAlt;
          event.isMeta = keyEvent->isMeta;

          nsIPresShell *presShell = aPresContext->GetPresShell();
          if (presShell) {
            ret = presShell->HandleDOMEventWithTarget(this, &event, &status);
            // presShell may no longer be alive, don't use it here
            // unless you keep a reference.
          }
        }
      }
      break;

    case NS_MOUSE_ENTER_SYNTH:
      {
        nsAutoString href;
        nsGenericElement::GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::href,
                                  href);
        if (href.IsEmpty()) {
          *aEventStatus = nsEventStatus_eConsumeDoDefault; 
          break;
        }

        nsCOMPtr<nsIURI> baseURI = GetBaseURI();

        nsCOMPtr<nsIURI> uri;
        ret = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                        href,
                                                        mDocument,
                                                        baseURI);
        if (NS_SUCCEEDED(ret)) {
          ret = TriggerLink(aPresContext, eLinkVerb_Replace, baseURI, uri,
                            EmptyString(), PR_FALSE, PR_TRUE);
        }
        
        *aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT_SYNTH:
      {
        ret = LeaveLink(aPresContext);
        *aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

    default:
      break;
    }
  }

  return ret;
}

NS_IMETHODIMP
nsXMLElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  nsXMLElement* it = new nsXMLElement(mNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  CopyInnerTo(it, aDeep);

  kungFuDeathGrip.swap(*aReturn);

  return NS_OK;
}

// nsIStyledContent implementation

NS_IMETHODIMP
nsXMLElement::GetID(nsIAtom** aResult) const
{
  nsIAtom* atom = GetIDAttributeName();

  *aResult = nsnull;
  nsresult rv = NS_OK;
  if (atom) {
    nsAutoString value;
    rv = nsGenericElement::GetAttr(kNameSpaceID_None, atom, value);
    if (NS_SUCCEEDED(rv)) {
      *aResult = NS_NewAtom(value);
    }
  }

  return rv;
}
