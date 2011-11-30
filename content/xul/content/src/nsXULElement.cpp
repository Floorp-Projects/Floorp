/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Chris Waterson <waterson@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Brendan Eich <brendan@mozilla.org>
 *   Mike Shaver <shaver@mozilla.org>
 *   Mark Hammond <mhammond@skippinet.com.au>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsDOMString.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsHashtable.h"
#include "nsIAtom.h"
#include "nsIBaseWindow.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDocument.h"
#include "nsEventListenerManager.h"
#include "nsEventStateManager.h"
#include "nsFocusManager.h"
#include "nsHTMLStyleSheet.h"
#include "nsINameSpaceManager.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIScriptRuntime.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIServiceManager.h"
#include "mozilla/css/StyleRule.h"
#include "nsIStyleSheet.h"
#include "nsIURL.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIXULDocument.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIXBLService.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsRDFCID.h"
#include "nsStyleConsts.h"
#include "nsXPIDLString.h"
#include "nsXULControllers.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsXULDocument.h"
#include "nsXULPopupListener.h"
#include "nsRuleWalker.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsCSSParser.h"
#include "nsIListBoxObject.h"
#include "nsContentUtils.h"
#include "nsContentList.h"
#include "nsMutationEvent.h"
#include "nsPLDOMEvent.h"
#include "nsIDOMMutationEvent.h"
#include "nsPIDOMWindow.h"
#include "nsDOMAttributeMap.h"
#include "nsGkAtoms.h"
#include "nsXULContentUtils.h"
#include "nsNodeUtils.h"
#include "nsFrameLoader.h"
#include "prlog.h"
#include "rdf.h"
#include "nsIControllers.h"

// The XUL doc interface
#include "nsIDOMXULDocument.h"

#include "nsReadableUtils.h"
#include "nsIFrame.h"
#include "nsNodeInfoManager.h"
#include "nsXBLBinding.h"
#include "nsEventDispatcher.h"
#include "mozAutoDocUpdate.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsCCUncollectableMarker.h"

namespace css = mozilla::css;

// Global object maintenance
nsIXBLService * nsXULElement::gXBLService = nsnull;

/**
 * A tearoff class for nsXULElement to implement nsIScriptEventHandlerOwner.
 */
class nsScriptEventHandlerOwnerTearoff : public nsIScriptEventHandlerOwner
{
public:
    nsScriptEventHandlerOwnerTearoff(nsXULElement* aElement)
    : mElement(aElement) {}

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(nsScriptEventHandlerOwnerTearoff)

    // nsIScriptEventHandlerOwner
    virtual nsresult CompileEventHandler(nsIScriptContext* aContext,
                                         nsIAtom *aName,
                                         const nsAString& aBody,
                                         const char* aURL,
                                         PRUint32 aLineNo,
                                         nsScriptObjectHolder &aHandler);
    virtual nsresult GetCompiledEventHandler(nsIAtom *aName,
                                             nsScriptObjectHolder &aHandler);

private:
    nsRefPtr<nsXULElement> mElement;
};

//----------------------------------------------------------------------

static NS_DEFINE_CID(kXULPopupListenerCID,        NS_XULPOPUPLISTENER_CID);

//----------------------------------------------------------------------

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
PRUint32             nsXULPrototypeAttribute::gNumElements;
PRUint32             nsXULPrototypeAttribute::gNumAttributes;
PRUint32             nsXULPrototypeAttribute::gNumEventHandlers;
PRUint32             nsXULPrototypeAttribute::gNumCacheTests;
PRUint32             nsXULPrototypeAttribute::gNumCacheHits;
PRUint32             nsXULPrototypeAttribute::gNumCacheSets;
PRUint32             nsXULPrototypeAttribute::gNumCacheFills;
#endif

class nsXULElementTearoff : public nsIDOMElementCSSInlineStyle,
                            public nsIFrameLoaderOwner
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULElementTearoff,
                                           nsIDOMElementCSSInlineStyle)

  nsXULElementTearoff(nsXULElement *aElement)
    : mElement(aElement)
  {
  }

  NS_IMETHOD GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
  {
    nsresult rv;
    *aStyle = static_cast<nsXULElement*>(mElement.get())->GetStyle(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ADDREF(*aStyle);
    return NS_OK;
  }
  NS_FORWARD_NSIFRAMELOADEROWNER(static_cast<nsXULElement*>(mElement.get())->);
private:
  nsCOMPtr<nsIDOMXULElement> mElement;
};

NS_IMPL_CYCLE_COLLECTION_1(nsXULElementTearoff, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULElementTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULElementTearoff)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULElementTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoaderOwner)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElementCSSInlineStyle)
NS_INTERFACE_MAP_END_AGGREGATED(mElement)

//----------------------------------------------------------------------
// nsXULElement
//

nsXULElement::nsXULElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsStyledElement(aNodeInfo),
      mBindingParent(nsnull)
{
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumElements);

    // We may be READWRITE by default; check.
    if (IsReadWriteTextElement()) {
        AddStatesSilently(NS_EVENT_STATE_MOZ_READWRITE);
        RemoveStatesSilently(NS_EVENT_STATE_MOZ_READONLY);
    }
}

nsXULElement::nsXULSlots::nsXULSlots()
    : nsXULElement::nsDOMSlots()
{
}

nsXULElement::nsXULSlots::~nsXULSlots()
{
    NS_IF_RELEASE(mControllers); // Forces release
    if (mFrameLoader) {
        mFrameLoader->Destroy();
    }
}

void
nsXULElement::nsXULSlots::Traverse(nsCycleCollectionTraversalCallback &cb)
{
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mFrameLoader");
    cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIFrameLoader*, mFrameLoader));
}

nsINode::nsSlots*
nsXULElement::CreateSlots()
{
    return new nsXULSlots();
}

/* static */
already_AddRefed<nsXULElement>
nsXULElement::Create(nsXULPrototypeElement* aPrototype, nsINodeInfo *aNodeInfo,
                     bool aIsScriptable)
{
    nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
    nsXULElement *element = new nsXULElement(ni.forget());
    if (element) {
        NS_ADDREF(element);

        element->mPrototype = aPrototype;
        if (aPrototype->mHasIdAttribute) {
            element->SetHasID();
        }
        if (aPrototype->mHasClassAttribute) {
            element->SetFlags(NODE_MAY_HAVE_CLASS);
        }
        if (aPrototype->mHasStyleAttribute) {
            element->SetMayHaveStyle();
        }

        NS_ASSERTION(aPrototype->mScriptTypeID != nsIProgrammingLanguage::UNKNOWN,
                    "Need to know the language!");
        element->SetScriptTypeID(aPrototype->mScriptTypeID);

        if (aIsScriptable) {
            // Check each attribute on the prototype to see if we need to do
            // any additional processing and hookup that would otherwise be
            // done 'automagically' by SetAttr().
            for (PRUint32 i = 0; i < aPrototype->mNumAttributes; ++i) {
                element->AddListenerFor(aPrototype->mAttributes[i].mName,
                                        true);
            }
        }
    }

    return element;
}

nsresult
nsXULElement::Create(nsXULPrototypeElement* aPrototype,
                     nsIDocument* aDocument,
                     bool aIsScriptable,
                     Element** aResult)
{
    // Create an nsXULElement from a prototype
    NS_PRECONDITION(aPrototype != nsnull, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    if (aDocument) {
        nsINodeInfo* ni = aPrototype->mNodeInfo;
        nodeInfo = aDocument->NodeInfoManager()->
          GetNodeInfo(ni->NameAtom(), ni->GetPrefixAtom(), ni->NamespaceID(),
                      nsIDOMNode::ELEMENT_NODE);
        NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        nodeInfo = aPrototype->mNodeInfo;
    }

    nsRefPtr<nsXULElement> element = Create(aPrototype, nodeInfo,
                                            aIsScriptable);
    if (!element) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult = element.get());

    return NS_OK;
}

nsresult
NS_NewXULElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo)
{
    NS_PRECONDITION(aNodeInfo.get(), "need nodeinfo for non-proto Create");

    nsIDocument* doc = aNodeInfo.get()->GetDocument();
    if (doc && !doc->AllowXULXBL()) {
        nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
        return NS_ERROR_NOT_AVAILABLE;
    }

    NS_ADDREF(*aResult = new nsXULElement(aNodeInfo));

    return NS_OK;
}

void
NS_TrustedNewXULElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo)
{
    NS_PRECONDITION(aNodeInfo.get(), "need nodeinfo for non-proto Create");

    // Create an nsXULElement with the specified namespace and tag.
    NS_ADDREF(*aResult = new nsXULElement(aNodeInfo));
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXULElement,
                                                  nsStyledElement)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mPrototype,
                                                    nsXULPrototypeElement)
    {
        nsXULSlots* slots = static_cast<nsXULSlots*>(tmp->GetExistingSlots());
        if (slots) {
            slots->Traverse(cb);
        }
    }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsXULElement, nsStyledElement)
NS_IMPL_RELEASE_INHERITED(nsXULElement, nsStyledElement)

DOMCI_NODE_DATA(XULElement, nsXULElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsXULElement)
    NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsXULElement)
        NS_INTERFACE_TABLE_ENTRY(nsXULElement, nsIDOMNode)
        NS_INTERFACE_TABLE_ENTRY(nsXULElement, nsIDOMElement)
        NS_INTERFACE_TABLE_ENTRY(nsXULElement, nsIDOMXULElement)
    NS_OFFSET_AND_INTERFACE_TABLE_END
    NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIScriptEventHandlerOwner,
                                   new nsScriptEventHandlerOwnerTearoff(this))
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMElementCSSInlineStyle,
                                   new nsXULElementTearoff(this))
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIFrameLoaderOwner,
                                   new nsXULElementTearoff(this))
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XULElement)
NS_ELEMENT_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsIDOMNode interface

nsresult
nsXULElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
    *aResult = nsnull;

    // If we have a prototype, so will our clone.
    nsRefPtr<nsXULElement> element;
    if (mPrototype) {
        element = nsXULElement::Create(mPrototype, aNodeInfo, true);
        NS_ASSERTION(GetScriptTypeID() == mPrototype->mScriptTypeID,
                     "Didn't get the default language from proto?");
    }
    else {
        nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
        element = new nsXULElement(ni.forget());
        if (element) {
        	// If created from a prototype, we will already have the script
        	// language specified by the proto - otherwise copy it directly
        	element->SetScriptTypeID(GetScriptTypeID());
        }
    }

    if (!element) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // XXX TODO: set up RDF generic builder n' stuff if there is a
    // 'datasources' attribute? This is really kind of tricky,
    // because then we'd need to -selectively- copy children that
    // -weren't- generated from RDF. Ugh. Forget it.

    // Note that we're _not_ copying mControllers.

    nsresult rv = CopyInnerTo(element);
    if (NS_SUCCEEDED(rv)) {
        NS_ADDREF(*aResult = element);
    }

    return rv;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsXULElement::GetElementsByAttribute(const nsAString& aAttribute,
                                     const nsAString& aValue,
                                     nsIDOMNodeList** aReturn)
{
    nsCOMPtr<nsIAtom> attrAtom(do_GetAtom(aAttribute));
    NS_ENSURE_TRUE(attrAtom, NS_ERROR_OUT_OF_MEMORY);
    void* attrValue = new nsString(aValue);
    NS_ENSURE_TRUE(attrValue, NS_ERROR_OUT_OF_MEMORY);
    nsContentList *list = 
        new nsContentList(this,
                          nsXULDocument::MatchAttribute,
                          nsContentUtils::DestroyMatchString,
                          attrValue,
                          true,
                          attrAtom,
                          kNameSpaceID_Unknown);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aReturn = list);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetElementsByAttributeNS(const nsAString& aNamespaceURI,
                                       const nsAString& aAttribute,
                                       const nsAString& aValue,
                                       nsIDOMNodeList** aReturn)
{
    nsCOMPtr<nsIAtom> attrAtom(do_GetAtom(aAttribute));
    NS_ENSURE_TRUE(attrAtom, NS_ERROR_OUT_OF_MEMORY);

    PRInt32 nameSpaceId = kNameSpaceID_Wildcard;
    if (!aNamespaceURI.EqualsLiteral("*")) {
      nsresult rv =
        nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                              nameSpaceId);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    void* attrValue = new nsString(aValue);
    NS_ENSURE_TRUE(attrValue, NS_ERROR_OUT_OF_MEMORY);
    
    nsContentList *list = 
        new nsContentList(this,
                          nsXULDocument::MatchAttribute,
                          nsContentUtils::DestroyMatchString,
                          attrValue,
                          true,
                          attrAtom,
                          nameSpaceId);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aReturn = list);
    return NS_OK;
}

nsEventListenerManager*
nsXULElement::GetEventListenerManagerForAttr(nsIAtom* aAttrName, bool* aDefer)
{
    // XXXbz sXBL/XBL2 issue: should we instead use GetCurrentDoc()
    // here, override BindToTree for those classes and munge event
    // listeners there?
    nsIDocument* doc = OwnerDoc();

    nsPIDOMWindow *window;
    Element *root = doc->GetRootElement();
    if ((!root || root == this) && !mNodeInfo->Equals(nsGkAtoms::overlay) &&
        (window = doc->GetInnerWindow()) && window->IsInnerWindow()) {

        nsCOMPtr<nsIDOMEventTarget> piTarget = do_QueryInterface(window);

        *aDefer = false;
        return piTarget->GetListenerManager(true);
    }

    return nsStyledElement::GetEventListenerManagerForAttr(aAttrName, aDefer);
}

// returns true if the element is not a list
static bool IsNonList(nsINodeInfo* aNodeInfo)
{
  return !aNodeInfo->Equals(nsGkAtoms::tree) &&
         !aNodeInfo->Equals(nsGkAtoms::listbox) &&
         !aNodeInfo->Equals(nsGkAtoms::richlistbox);
}

bool
nsXULElement::IsFocusable(PRInt32 *aTabIndex, bool aWithMouse)
{
  /* 
   * Returns true if an element may be focused, and false otherwise. The inout
   * argument aTabIndex will be set to the tab order index to be used; -1 for
   * elements that should not be part of the tab order and a greater value to
   * indicate its tab order.
   *
   * Confusingly, the supplied value for the aTabIndex argument may indicate
   * whether the element may be focused as a result of the -moz-user-focus
   * property, where -1 means no and 0 means yes.
   *
   * For controls, the element cannot be focused and is not part of the tab
   * order if it is disabled.
   *
   * Controls (those that implement nsIDOMXULControlElement):
   *  *aTabIndex = -1  no tabindex     Not focusable or tabbable
   *  *aTabIndex = -1  tabindex="-1"   Not focusable or tabbable
   *  *aTabIndex = -1  tabindex=">=0"  Focusable and tabbable
   *  *aTabIndex >= 0  no tabindex     Focusable and tabbable
   *  *aTabIndex >= 0  tabindex="-1"   Focusable but not tabbable
   *  *aTabIndex >= 0  tabindex=">=0"  Focusable and tabbable
   * Non-controls:
   *  *aTabIndex = -1                  Not focusable or tabbable
   *  *aTabIndex >= 0                  Focusable and tabbable
   *
   * If aTabIndex is null, then the tabindex is not computed, and
   * true is returned for non-disabled controls and false otherwise.
   */

  // elements are not focusable by default
  bool shouldFocus = false;

#ifdef XP_MACOSX
  // on Mac, mouse interactions only focus the element if it's a list
  if (aWithMouse && IsNonList(mNodeInfo))
    return false;
#endif

  nsCOMPtr<nsIDOMXULControlElement> xulControl = do_QueryObject(this);
  if (xulControl) {
    // a disabled element cannot be focused and is not part of the tab order
    bool disabled;
    xulControl->GetDisabled(&disabled);
    if (disabled) {
      if (aTabIndex)
        *aTabIndex = -1;
      return false;
    }
    shouldFocus = true;
  }

  if (aTabIndex) {
    if (xulControl) {
      if (HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
        // if either the aTabIndex argument or a specified tabindex is non-negative,
        // the element becomes focusable.
        PRInt32 tabIndex = 0;
        xulControl->GetTabIndex(&tabIndex);
        shouldFocus = *aTabIndex >= 0 || tabIndex >= 0;
        *aTabIndex = tabIndex;
      }
      else {
        // otherwise, if there is no tabindex attribute, just use the value of
        // *aTabIndex to indicate focusability. Reset any supplied tabindex to 0.
        shouldFocus = *aTabIndex >= 0;
        if (shouldFocus)
          *aTabIndex = 0;
      }

      if (shouldFocus && sTabFocusModelAppliesToXUL &&
          !(sTabFocusModel & eTabFocus_formElementsMask)) {
        // By default, the tab focus model doesn't apply to xul element on any system but OS X.
        // on OS X we're following it for UI elements (XUL) as sTabFocusModel is based on
        // "Full Keyboard Access" system setting (see mac/nsILookAndFeel).
        // both textboxes and list elements (i.e. trees and list) should always be focusable
        // (textboxes are handled as html:input)
        // For compatibility, we only do this for controls, otherwise elements like <browser>
        // cannot take this focus.
        if (IsNonList(mNodeInfo))
          *aTabIndex = -1;
      }
    }
    else {
      shouldFocus = *aTabIndex >= 0;
    }
  }

  return shouldFocus;
}

void
nsXULElement::PerformAccesskey(bool aKeyCausesActivation,
                               bool aIsTrustedEvent)
{
    nsCOMPtr<nsIContent> content(this);

    if (Tag() == nsGkAtoms::label) {
        nsCOMPtr<nsIDOMElement> element;

        nsAutoString control;
        GetAttr(kNameSpaceID_None, nsGkAtoms::control, control);
        if (!control.IsEmpty()) {
            nsCOMPtr<nsIDOMDocument> domDocument =
                do_QueryInterface(content->GetCurrentDoc());
            if (domDocument)
                domDocument->GetElementById(control, getter_AddRefs(element));
        }
        // here we'll either change |content| to the element referenced by
        // |element|, or clear it.
        content = do_QueryInterface(element);

        if (!content)
            return;
    }

    nsIFrame* frame = content->GetPrimaryFrame();
    if (!frame || !frame->IsVisibleConsideringAncestors())
        return;

    nsXULElement* elm = FromContent(content);
    if (elm) {
        // Define behavior for each type of XUL element.
        nsIAtom *tag = content->Tag();
        if (tag != nsGkAtoms::toolbarbutton) {
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            nsCOMPtr<nsIDOMElement> element;
            // for radio buttons, focus the radiogroup instead
            if (tag == nsGkAtoms::radio) {
              nsCOMPtr<nsIDOMXULSelectControlItemElement> controlItem(do_QueryInterface(content));
              if (controlItem) {
                bool disabled;
                controlItem->GetDisabled(&disabled);
                if (!disabled) {
                  nsCOMPtr<nsIDOMXULSelectControlElement> selectControl;
                  controlItem->GetControl(getter_AddRefs(selectControl));
                  element = do_QueryInterface(selectControl);
                }
              }
            }
            else {
              element = do_QueryInterface(content);
            }
            if (element)
              fm->SetFocus(element, nsIFocusManager::FLAG_BYKEY);
          }
        }
        if (aKeyCausesActivation && tag != nsGkAtoms::textbox && tag != nsGkAtoms::menulist) {
          elm->ClickWithInputSource(nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD);
        }
    }
    else {
        content->PerformAccesskey(aKeyCausesActivation, aIsTrustedEvent);
    }
}


//----------------------------------------------------------------------
// nsIScriptEventHandlerOwner interface

NS_IMPL_CYCLE_COLLECTION_CLASS(nsScriptEventHandlerOwnerTearoff)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsScriptEventHandlerOwnerTearoff)
  tmp->mElement = nsnull;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsScriptEventHandlerOwnerTearoff)
  cb.NoteXPCOMChild(static_cast<nsIContent*>(tmp->mElement));
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsScriptEventHandlerOwnerTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIScriptEventHandlerOwner)
NS_INTERFACE_MAP_END_AGGREGATED(mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsScriptEventHandlerOwnerTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsScriptEventHandlerOwnerTearoff)

nsresult
nsScriptEventHandlerOwnerTearoff::GetCompiledEventHandler(
                                                nsIAtom *aName,
                                                nsScriptObjectHolder &aHandler)
{
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheTests);
    aHandler.drop();

    nsXULPrototypeAttribute *attr =
        mElement->FindPrototypeAttribute(kNameSpaceID_None, aName);
    if (attr) {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheHits);
        aHandler.setObject(attr->mEventHandler);
    }

    return NS_OK;
}

nsresult
nsScriptEventHandlerOwnerTearoff::CompileEventHandler(
                                                nsIScriptContext* aContext,
                                                nsIAtom *aName,
                                                const nsAString& aBody,
                                                const char* aURL,
                                                PRUint32 aLineNo,
                                                nsScriptObjectHolder &aHandler)
{
    nsresult rv;

    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheSets);

    // XXX sXBL/XBL2 issue! Owner or current document?
    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mElement->OwnerDoc());

    nsIScriptContext* context = NULL;
    nsXULPrototypeElement* elem = mElement->mPrototype;
    if (elem && xuldoc) {
        // It'll be shared among the instances of the prototype.

        // Use the prototype document's special context.  Because
        // scopeObject is null, the JS engine has no other source of
        // <the-new-shared-event-handler>.__proto__ than to look in
        // cx->globalObject for Function.prototype.  That prototype
        // keeps the global object alive, so if we use this document's
        // global object, we'll be putting something in the prototype
        // that protects this document's global object from GC.
        nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner;
        rv = xuldoc->GetScriptGlobalObjectOwner(getter_AddRefs(globalOwner));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(globalOwner, NS_ERROR_UNEXPECTED);

        nsIScriptGlobalObject* global = globalOwner->GetScriptGlobalObject();
        NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

        context = global->GetScriptContext(aContext->GetScriptTypeID());
        // It could be possible the language has been setup on aContext but
        // not on the global - we don't demand-create language contexts on the
        // nsGlobalWindow
        NS_ASSERTION(context,
                     "Failed to get a language context from the global!?");
        NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);
    }
    else {
        context = aContext;
    }

    // Compile the event handler
    PRUint32 argCount;
    const char **argNames;
    nsContentUtils::GetEventArgNames(kNameSpaceID_XUL, aName, &argCount,
                                     &argNames);

    nsCxPusher pusher;
    if (!pusher.Push(context->GetNativeContext())) {
      return NS_ERROR_FAILURE;
    }

    rv = context->CompileEventHandler(aName, argCount, argNames,
                                      aBody, aURL, aLineNo,
                                      SCRIPTVERSION_DEFAULT,  // for now?
                                      aHandler);
    if (NS_FAILED(rv)) return rv;

    nsXULPrototypeAttribute *attr =
        mElement->FindPrototypeAttribute(kNameSpaceID_None, aName);
    if (attr) {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheFills);
        // take a copy of the event handler, and tell the language about it.
        if (aHandler) {
            NS_ASSERTION(!attr->mEventHandler, "Leaking handler.");

            rv = nsContentUtils::HoldScriptObject(aContext->GetScriptTypeID(),
                                                  elem,
                                                  &NS_CYCLE_COLLECTION_NAME(nsXULPrototypeNode),
                                                  aHandler,
                                                  elem->mHoldsScriptObject);
            if (NS_FAILED(rv)) return rv;

            elem->mHoldsScriptObject = true;
        }
        attr->mEventHandler = aHandler.getObject();
    }

    return NS_OK;
}

void
nsXULElement::AddListenerFor(const nsAttrName& aName,
                             bool aCompileEventHandlers)
{
    // If appropriate, add a popup listener and/or compile the event
    // handler. Called when we change the element's document, create a
    // new element, change an attribute's value, etc.
    // Eventlistenener-attributes are always in the null namespace
    if (aName.IsAtom()) {
        nsIAtom *attr = aName.Atom();
        MaybeAddPopupListener(attr);
        if (aCompileEventHandlers &&
            nsContentUtils::IsEventAttributeName(attr, EventNameType_XUL)) {
            nsAutoString value;
            GetAttr(kNameSpaceID_None, attr, value);
            AddScriptEventListener(attr, value, true);
        }
    }
}

void
nsXULElement::MaybeAddPopupListener(nsIAtom* aLocalName)
{
    // If appropriate, add a popup listener. Called when we change the
    // element's document, create a new element, change an attribute's
    // value, etc.
    if (aLocalName == nsGkAtoms::menu ||
        aLocalName == nsGkAtoms::contextmenu ||
        // XXXdwh popup and context are deprecated
        aLocalName == nsGkAtoms::popup ||
        aLocalName == nsGkAtoms::context) {
        AddPopupListener(aLocalName);
    }
}

//----------------------------------------------------------------------
//
// nsIContent interface
//
void
nsXULElement::UpdateEditableState(bool aNotify)
{
    // Don't call through to nsGenericElement here because the things
    // it does don't work for cases when we're an editable control.
    nsIContent *parent = GetParent();

    SetEditableFlag(parent && parent->HasFlag(NODE_IS_EDITABLE));
    UpdateState(aNotify);
}

nsresult
nsXULElement::BindToTree(nsIDocument* aDocument,
                         nsIContent* aParent,
                         nsIContent* aBindingParent,
                         bool aCompileEventHandlers)
{
  nsresult rv = nsStyledElement::BindToTree(aDocument, aParent,
                                            aBindingParent,
                                            aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
      NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                   "Missing a script blocker!");
      // We're in a document now.  Kick off the frame load.
      LoadSrc();
  }

  return rv;
}

void
nsXULElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
    // mControllers can own objects that are implemented
    // in JavaScript (such as some implementations of
    // nsIControllers.  These objects prevent their global
    // object's script object from being garbage collected,
    // which means JS continues to hold an owning reference
    // to the nsGlobalWindow, which owns the document,
    // which owns this content.  That's a cycle, so we break
    // it here.  (It might be better to break this by releasing
    // mDocument in nsGlobalWindow::SetDocShell, but I'm not
    // sure whether that would fix all possible cycles through
    // mControllers.)
    nsXULSlots* slots = static_cast<nsXULSlots*>(GetExistingDOMSlots());
    if (slots) {
        NS_IF_RELEASE(slots->mControllers);
        if (slots->mFrameLoader) {
            // This element is being taken out of the document, destroy the
            // possible frame loader.
            // XXXbz we really want to only partially destroy the frame
            // loader... we don't want to tear down the docshell.  Food for
            // later bug.
            slots->mFrameLoader->Destroy();
            slots->mFrameLoader = nsnull;
        }
    }

    nsStyledElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsXULElement::RemoveChildAt(PRUint32 aIndex, bool aNotify)
{
    nsresult rv;
    nsCOMPtr<nsIContent> oldKid = mAttrsAndChildren.GetSafeChildAt(aIndex);
    if (!oldKid) {
      return NS_OK;
    }

    // On the removal of a <treeitem>, <treechildren>, or <treecell> element,
    // the possibility exists that some of the items in the removed subtree
    // are selected (and therefore need to be deselected). We need to account for this.
    nsCOMPtr<nsIDOMXULMultiSelectControlElement> controlElement;
    nsCOMPtr<nsIListBoxObject> listBox;
    bool fireSelectionHandler = false;

    // -1 = do nothing, -2 = null out current item
    // anything else = index to re-set as current
    PRInt32 newCurrentIndex = -1;

    if (oldKid->NodeInfo()->Equals(nsGkAtoms::listitem, kNameSpaceID_XUL)) {
      // This is the nasty case. We have (potentially) a slew of selected items
      // and cells going away.
      // First, retrieve the tree.
      // Check first whether this element IS the tree
      controlElement = do_QueryObject(this);

      // If it's not, look at our parent
      if (!controlElement)
        rv = GetParentTree(getter_AddRefs(controlElement));

      nsCOMPtr<nsIDOMElement> oldKidElem = do_QueryInterface(oldKid);
      if (controlElement && oldKidElem) {
        // Iterate over all of the items and find out if they are contained inside
        // the removed subtree.
        PRInt32 length;
        controlElement->GetSelectedCount(&length);
        for (PRInt32 i = 0; i < length; i++) {
          nsCOMPtr<nsIDOMXULSelectControlItemElement> node;
          controlElement->GetSelectedItem(i, getter_AddRefs(node));
          // we need to QI here to do an XPCOM-correct pointercompare
          nsCOMPtr<nsIDOMElement> selElem = do_QueryInterface(node);
          if (selElem == oldKidElem &&
              NS_SUCCEEDED(controlElement->RemoveItemFromSelection(node))) {
            length--;
            i--;
            fireSelectionHandler = true;
          }
        }

        nsCOMPtr<nsIDOMXULSelectControlItemElement> curItem;
        controlElement->GetCurrentItem(getter_AddRefs(curItem));
        nsCOMPtr<nsIContent> curNode = do_QueryInterface(curItem);
        if (curNode && nsContentUtils::ContentIsDescendantOf(curNode, oldKid)) {
            // Current item going away
            nsCOMPtr<nsIBoxObject> box;
            controlElement->GetBoxObject(getter_AddRefs(box));
            listBox = do_QueryInterface(box);
            if (listBox && oldKidElem) {
              listBox->GetIndexOfItem(oldKidElem, &newCurrentIndex);
            }

            // If any of this fails, we'll just set the current item to null
            if (newCurrentIndex == -1)
              newCurrentIndex = -2;
        }
      }
    }

    rv = nsStyledElement::RemoveChildAt(aIndex, aNotify);
    
    if (newCurrentIndex == -2)
        controlElement->SetCurrentItem(nsnull);
    else if (newCurrentIndex > -1) {
        // Make sure the index is still valid
        PRInt32 treeRows;
        listBox->GetRowCount(&treeRows);
        if (treeRows > 0) {
            newCurrentIndex = NS_MIN((treeRows - 1), newCurrentIndex);
            nsCOMPtr<nsIDOMElement> newCurrentItem;
            listBox->GetItemAtIndex(newCurrentIndex, getter_AddRefs(newCurrentItem));
            nsCOMPtr<nsIDOMXULSelectControlItemElement> xulCurItem = do_QueryInterface(newCurrentItem);
            if (xulCurItem)
                controlElement->SetCurrentItem(xulCurItem);
        } else {
            controlElement->SetCurrentItem(nsnull);
        }
    }

    nsIDocument* doc;
    if (fireSelectionHandler && (doc = GetCurrentDoc())) {
      nsContentUtils::DispatchTrustedEvent(doc,
                                           static_cast<nsIContent*>(this),
                                           NS_LITERAL_STRING("select"),
                                           false,
                                           true);
    }

    return rv;
}

void
nsXULElement::UnregisterAccessKey(const nsAString& aOldValue)
{
    // If someone changes the accesskey, unregister the old one
    //
    nsIDocument* doc = GetCurrentDoc();
    if (doc && !aOldValue.IsEmpty()) {
        nsIPresShell *shell = doc->GetShell();

        if (shell) {
            nsIContent *content = this;

            // find out what type of content node this is
            if (mNodeInfo->Equals(nsGkAtoms::label)) {
                // For anonymous labels the unregistering must
                // occur on the binding parent control.
                // XXXldb: And what if the binding parent is null?
                content = GetBindingParent();
            }

            if (content) {
                shell->GetPresContext()->EventStateManager()->
                    UnregisterAccessKey(content, aOldValue.First());
            }
        }
    }
}

nsresult
nsXULElement::BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                            const nsAString* aValue, bool aNotify)
{
    if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::accesskey &&
        IsInDoc()) {
        const nsAttrValue* attrVal = FindLocalOrProtoAttr(aNamespaceID, aName);
        if (attrVal) {
            nsAutoString oldValue;
            attrVal->ToString(oldValue);
            UnregisterAccessKey(oldValue);
        }
    } 
    else if (aNamespaceID == kNameSpaceID_None && (aName ==
             nsGkAtoms::command || aName == nsGkAtoms::observes) && IsInDoc()) {
//         XXX sXBL/XBL2 issue! Owner or current document?
        nsAutoString oldValue;
        GetAttr(kNameSpaceID_None, nsGkAtoms::observes, oldValue);
        if (oldValue.IsEmpty()) {
          GetAttr(kNameSpaceID_None, nsGkAtoms::command, oldValue);
        }

        if (!oldValue.IsEmpty()) {
          RemoveBroadcaster(oldValue);
        }
    }
    else if (aNamespaceID == kNameSpaceID_None &&
             aValue &&
             mNodeInfo->Equals(nsGkAtoms::window) &&
             aName == nsGkAtoms::chromemargin) {
      nsAttrValue attrValue;
      nsIntMargin margins;
      // Make sure the margin format is valid first
      if (!attrValue.ParseIntMarginValue(*aValue)) {
          return NS_ERROR_INVALID_ARG;
      }
    }

    return nsStyledElement::BeforeSetAttr(aNamespaceID, aName,
                                          aValue, aNotify);
}

nsresult
nsXULElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                           const nsAString* aValue, bool aNotify)
{
    if (aNamespaceID == kNameSpaceID_None) {
        // XXX UnsetAttr handles more attributes than we do. See bug 233642.

        // Add popup and event listeners. We can't call AddListenerFor since
        // the attribute isn't set yet.
        MaybeAddPopupListener(aName);
        if (nsContentUtils::IsEventAttributeName(aName, EventNameType_XUL) && aValue) {
            // If mPrototype->mScriptTypeID != GetScriptTypeID(), it means
            // we are resolving an overlay with a different default script
            // language.  We can't defer compilation of those handlers as
            // we will have lost the script language (storing it on each
            // nsXULPrototypeAttribute is expensive!)
            bool defer = mPrototype == nsnull ||
                           mPrototype->mScriptTypeID == GetScriptTypeID();
            AddScriptEventListener(aName, *aValue, defer);
        }

        // Hide chrome if needed
        if (mNodeInfo->Equals(nsGkAtoms::window) && aValue) {
          if (aName == nsGkAtoms::hidechrome) {
              HideWindowChrome(aValue->EqualsLiteral("true"));
          }
          else if (aName == nsGkAtoms::chromemargin) {
              SetChromeMargins(aValue);
          }
        }

        // title, (in)activetitlebarcolor and drawintitlebar are settable on
        // any root node (windows, dialogs, etc)
        nsIDocument *document = GetCurrentDoc();
        if (document && document->GetRootElement() == this) {
            if (aName == nsGkAtoms::title) {
                document->NotifyPossibleTitleChange(false);
            }
            else if ((aName == nsGkAtoms::activetitlebarcolor ||
                      aName == nsGkAtoms::inactivetitlebarcolor)) {
                nscolor color = NS_RGBA(0, 0, 0, 0);
                nsAttrValue attrValue;
                attrValue.ParseColor(*aValue);
                attrValue.GetColorValue(color);
                SetTitlebarColor(color, aName == nsGkAtoms::activetitlebarcolor);
            }
            else if (aName == nsGkAtoms::drawintitlebar) {
                SetDrawsInTitlebar(aValue && aValue->EqualsLiteral("true"));
            }
            else if (aName == nsGkAtoms::localedir) {
                // if the localedir changed on the root element, reset the document direction
                nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(document);
                if (xuldoc) {
                    xuldoc->ResetDocumentDirection();
                }
            }
            else if (aName == nsGkAtoms::lwtheme ||
                     aName == nsGkAtoms::lwthemetextcolor) {
                // if the lwtheme changed, make sure to reset the document lwtheme cache
                nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(document);
                if (xuldoc) {
                    xuldoc->ResetDocumentLWTheme();
                }
            }
        }

        if (aName == nsGkAtoms::src && document) {
            LoadSrc();
        }

        // XXX need to check if they're changing an event handler: if
        // so, then we need to unhook the old one.  Or something.
    }

    return nsStyledElement::AfterSetAttr(aNamespaceID, aName,
                                         aValue, aNotify);
}

bool
nsXULElement::ParseAttribute(PRInt32 aNamespaceID,
                             nsIAtom* aAttribute,
                             const nsAString& aValue,
                             nsAttrValue& aResult)
{
    // Parse into a nsAttrValue
    if (!nsStyledElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                         aResult)) {
        // Fall back to parsing as atom for short values
        aResult.ParseStringOrAtom(aValue);
    }

    return true;
}

const nsAttrName*
nsXULElement::InternalGetExistingAttrNameFromQName(const nsAString& aStr) const
{
    const nsAttrName* attrName =
        mAttrsAndChildren.GetExistingAttrNameFromQName(aStr);
    if (attrName) {
        return attrName;
    }

    if (mPrototype) {
        PRUint32 i;
        for (i = 0; i < mPrototype->mNumAttributes; ++i) {
            attrName = &mPrototype->mAttributes[i].mName;
            if (attrName->QualifiedNameEquals(aStr)) {
                return attrName;
            }
        }
    }

    return nsnull;
}

bool
nsXULElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                      nsAString& aResult) const
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
                 "must have a real namespace ID!");

    const nsAttrValue* val = FindLocalOrProtoAttr(aNameSpaceID, aName);

    if (!val) {
        // Since we are returning a success code we'd better do
        // something about the out parameters (someone may have
        // given us a non-empty string).
        aResult.Truncate();
        return false;
    }

    val->ToString(aResult);

    return true;
}

bool
nsXULElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
                 "must have a real namespace ID!");

    return mAttrsAndChildren.GetAttr(aName, aNameSpaceID) ||
           FindPrototypeAttribute(aNameSpaceID, aName);
}

bool
nsXULElement::AttrValueIs(PRInt32 aNameSpaceID,
                          nsIAtom* aName,
                          const nsAString& aValue,
                          nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");

  const nsAttrValue* val = FindLocalOrProtoAttr(aNameSpaceID, aName);
  return val && val->Equals(aValue, aCaseSensitive);
}

bool
nsXULElement::AttrValueIs(PRInt32 aNameSpaceID,
                          nsIAtom* aName,
                          nsIAtom* aValue,
                          nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValue, "Null value atom");

  const nsAttrValue* val = FindLocalOrProtoAttr(aNameSpaceID, aName);
  return val && val->Equals(aValue, aCaseSensitive);
}

PRInt32
nsXULElement::FindAttrValueIn(PRInt32 aNameSpaceID,
                              nsIAtom* aName,
                              AttrValuesArray* aValues,
                              nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValues, "Null value array");
  
  const nsAttrValue* val = FindLocalOrProtoAttr(aNameSpaceID, aName);
  if (val) {
    for (PRInt32 i = 0; aValues[i]; ++i) {
      if (val->Equals(*aValues[i], aCaseSensitive)) {
        return i;
      }
    }
    return ATTR_VALUE_NO_MATCH;
  }
  return ATTR_MISSING;
}

nsresult
nsXULElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, bool aNotify)
{
    // This doesn't call BeforeSetAttr/AfterSetAttr for now.
    
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    nsresult rv;

    // Because It's Hard to maintain a magic ``unset'' value in
    // the local attributes, we'll fault all the attributes,
    // unhook ourselves from the prototype, and then remove the
    // local copy of the attribute that we want to unset. In
    // other words, we'll become ``heavyweight''.
    //
    // We can avoid this if the attribute isn't in the prototype,
    // then we just need to remove it locally

    nsXULPrototypeAttribute *protoattr =
        FindPrototypeAttribute(aNameSpaceID, aName);
    if (protoattr) {
        // We've got an attribute on the prototype, so we need to
        // fully fault and remove the local copy.
        rv = MakeHeavyweight();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsIDocument* doc = GetCurrentDoc();
    mozAutoDocUpdate updateBatch(doc, UPDATE_CONTENT_MODEL, aNotify);

    bool isId = false;
    if (aName == nsGkAtoms::id && aNameSpaceID == kNameSpaceID_None) {
      // Have to do this before clearing flag. See RemoveFromIdTable
      RemoveFromIdTable();
      isId = true;
    }

    PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID);
    if (index < 0) {
        NS_ASSERTION(!protoattr, "we used to have a protoattr, we should now "
                                 "have a normal one");

        return NS_OK;
    }

    nsAutoString oldValue;
    GetAttr(aNameSpaceID, aName, oldValue);

    if (aNotify) {
        nsNodeUtils::AttributeWillChange(this, aNameSpaceID, aName,
                                         nsIDOMMutationEvent::REMOVAL);
    }

    bool hasMutationListeners = aNotify &&
        nsContentUtils::HasMutationListeners(this,
            NS_EVENT_BITS_MUTATION_ATTRMODIFIED, this);

    nsCOMPtr<nsIDOMAttr> attrNode;
    if (hasMutationListeners) {
        nsAutoString ns;
        nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, ns);
        GetAttributeNodeNS(ns, nsDependentAtomString(aName), getter_AddRefs(attrNode));
    }

    nsDOMSlots *slots = GetExistingDOMSlots();
    if (slots && slots->mAttributeMap) {
      slots->mAttributeMap->DropAttribute(aNameSpaceID, aName);
    }

    // The id-handling code, and in the future possibly other code, need to
    // react to unexpected attribute changes.
    nsMutationGuard::DidMutate();

    nsAttrValue ignored;
    rv = mAttrsAndChildren.RemoveAttrAt(index, ignored);
    NS_ENSURE_SUCCESS(rv, rv);

    // XXX if the RemoveAttrAt() call fails, we might end up having removed
    // the attribute from the attribute map even though the attribute is still
    // on the element
    // https://bugzilla.mozilla.org/show_bug.cgi?id=296205

    // Deal with modification of magical attributes that side-effect
    // other things.
    // XXX Know how to remove POPUP event listeners when an attribute is unset?

    if (isId) {
        ClearHasID();
    }

    if (aNameSpaceID == kNameSpaceID_None) {
        if (mNodeInfo->Equals(nsGkAtoms::window)) {
            if (aName == nsGkAtoms::hidechrome) {
                HideWindowChrome(false);
            }
            else if (aName == nsGkAtoms::chromemargin) {
                ResetChromeMargins();
            }
        }

        if (doc && doc->GetRootElement() == this) {
            if ((aName == nsGkAtoms::activetitlebarcolor ||
                 aName == nsGkAtoms::inactivetitlebarcolor)) {
                // Use 0, 0, 0, 0 as the "none" color.
                SetTitlebarColor(NS_RGBA(0, 0, 0, 0), aName == nsGkAtoms::activetitlebarcolor);
            }
            else if (aName == nsGkAtoms::localedir) {
                // if the localedir changed on the root element, reset the document direction
                nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
                if (xuldoc) {
                    xuldoc->ResetDocumentDirection();
                }
            }
            else if ((aName == nsGkAtoms::lwtheme ||
                      aName == nsGkAtoms::lwthemetextcolor)) {
                // if the lwtheme changed, make sure to restyle appropriately
                nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
                if (xuldoc) {
                    xuldoc->ResetDocumentLWTheme();
                }
            }
            else if (aName == nsGkAtoms::drawintitlebar) {
                SetDrawsInTitlebar(false);
            }
        }

        // If the accesskey attribute is removed, unregister it here
        // Also see nsXULLabelFrame, nsBoxFrame and nsTextBoxFrame's AttributeChanged
        if (aName == nsGkAtoms::accesskey || aName == nsGkAtoms::control) {
            UnregisterAccessKey(oldValue);
        }

        // Check to see if the OBSERVES attribute is being unset.  If so, we
        // need to remove our broadcaster goop completely.
        if (doc && (aName == nsGkAtoms::observes ||
                          aName == nsGkAtoms::command)) {
            RemoveBroadcaster(oldValue);
        }
    }

    if (doc) {
        nsRefPtr<nsXBLBinding> binding =
            doc->BindingManager()->GetBinding(this);
        if (binding)
            binding->AttributeChanged(aName, aNameSpaceID, true, aNotify);

    }

    UpdateState(aNotify);

    if (aNotify) {
        nsNodeUtils::AttributeChanged(this, aNameSpaceID, aName,
                                      nsIDOMMutationEvent::REMOVAL);
    }

    if (hasMutationListeners) {
        nsMutationEvent mutation(true, NS_MUTATION_ATTRMODIFIED);

        mutation.mRelatedNode = attrNode;
        mutation.mAttrName = aName;

        if (!oldValue.IsEmpty())
          mutation.mPrevAttrValue = do_GetAtom(oldValue);
        mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

        mozAutoSubtreeModified subtree(OwnerDoc(), this);
        (new nsPLDOMEvent(this, mutation))->RunDOMEventWhenSafe();
    }

    return NS_OK;
}

void
nsXULElement::RemoveBroadcaster(const nsAString & broadcasterId)
{
    nsCOMPtr<nsIDOMXULDocument> xuldoc = do_QueryInterface(OwnerDoc());
    if (xuldoc) {
        nsCOMPtr<nsIDOMElement> broadcaster;
        nsCOMPtr<nsIDOMDocument> domDoc (do_QueryInterface(xuldoc));
        domDoc->GetElementById(broadcasterId, getter_AddRefs(broadcaster));
        if (broadcaster) {
            xuldoc->RemoveBroadcastListenerFor(broadcaster, this,
              NS_LITERAL_STRING("*"));
        }
    }
}

const nsAttrName*
nsXULElement::GetAttrNameAt(PRUint32 aIndex) const
{
    PRUint32 localCount = mAttrsAndChildren.AttrCount();
    PRUint32 protoCount = mPrototype ? mPrototype->mNumAttributes : 0;

    if (localCount > protoCount) {
        // More local than proto, put local first

        // Is the index low enough to just grab a local attr?
        if (aIndex < localCount) {
            return mAttrsAndChildren.AttrNameAt(aIndex);
        }

        aIndex -= localCount;

        // Search though prototype attributes while skipping names that exist in
        // the local array.
        for (PRUint32 i = 0; i < protoCount; i++) {
            const nsAttrName* name = &mPrototype->mAttributes[i].mName;
            if (mAttrsAndChildren.GetAttr(name->LocalName(), name->NamespaceID())) {
                aIndex++;
            }
            if (i == aIndex) {
                return name;
            }
        }
    }
    else {
        // More proto than local, put proto first

        // Is the index low enough to just grab a proto attr?
        if (aIndex < protoCount) {
            return &mPrototype->mAttributes[aIndex].mName;
        }

        aIndex -= protoCount;

        // Search though local attributes while skipping names that exist in
        // the prototype array.
        for (PRUint32 i = 0; i < localCount; i++) {
            const nsAttrName* name = mAttrsAndChildren.AttrNameAt(i);

            for (PRUint32 j = 0; j < protoCount; j++) {
                if (mPrototype->mAttributes[j].mName.Equals(*name)) {
                    aIndex++;
                    break;
                }
            }
            if (i == aIndex) {
                return name;
            }
        }
    }

    return nsnull;
}

PRUint32
nsXULElement::GetAttrCount() const
{
    PRUint32 localCount = mAttrsAndChildren.AttrCount();
    PRUint32 protoCount = mPrototype ? mPrototype->mNumAttributes : 0;

    if (localCount > protoCount) {
        // More local than proto, remove dups from proto array
        PRUint32 count = localCount;

        for (PRUint32 i = 0; i < protoCount; i++) {
            const nsAttrName* name = &mPrototype->mAttributes[i].mName;
            if (!mAttrsAndChildren.GetAttr(name->LocalName(), name->NamespaceID())) {
                count++;
            }
        }

        return count;
    }

    // More proto than local, remove dups from local array
    PRUint32 count = protoCount;

    for (PRUint32 i = 0; i < localCount; i++) {
        const nsAttrName* name = mAttrsAndChildren.AttrNameAt(i);

        count++;
        for (PRUint32 j = 0; j < protoCount; j++) {
            if (mPrototype->mAttributes[j].mName.Equals(*name)) {
                count--;
                break;
            }
        }
    }

    return count;
}

void
nsXULElement::DestroyContent()
{
    nsXULSlots* slots = static_cast<nsXULSlots*>(GetExistingDOMSlots());
    if (slots) {
        NS_IF_RELEASE(slots->mControllers);
        if (slots->mFrameLoader) {
            slots->mFrameLoader->Destroy();
            slots->mFrameLoader = nsnull;
        }
    }

    nsStyledElement::DestroyContent();
}

#ifdef DEBUG
void
nsXULElement::List(FILE* out, PRInt32 aIndent) const
{
    nsCString prefix("XUL");
    if (HasSlots()) {
      prefix.Append('*');
    }
    prefix.Append(' ');

    nsStyledElement::List(out, aIndent, prefix);
}
#endif

nsresult
nsXULElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
    aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
    nsIAtom* tag = Tag();
    if (IsRootOfNativeAnonymousSubtree() &&
        (tag == nsGkAtoms::scrollbar || tag == nsGkAtoms::scrollcorner) &&
        (aVisitor.mEvent->message == NS_MOUSE_CLICK ||
         aVisitor.mEvent->message == NS_MOUSE_DOUBLECLICK ||
         aVisitor.mEvent->message == NS_XUL_COMMAND ||
         aVisitor.mEvent->message == NS_CONTEXTMENU ||
         aVisitor.mEvent->message == NS_DRAGDROP_START ||
         aVisitor.mEvent->message == NS_DRAGDROP_GESTURE)) {
        // Don't propagate these events from native anonymous scrollbar.
        aVisitor.mCanHandle = true;
        aVisitor.mParentTarget = nsnull;
        return NS_OK;
    }
    if (aVisitor.mEvent->message == NS_XUL_COMMAND &&
        aVisitor.mEvent->eventStructType == NS_INPUT_EVENT &&
        aVisitor.mEvent->originalTarget == static_cast<nsIContent*>(this) &&
        tag != nsGkAtoms::command) {
        // Check that we really have an xul command event. That will be handled
        // in a special way.
        nsCOMPtr<nsIDOMXULCommandEvent> xulEvent =
            do_QueryInterface(aVisitor.mDOMEvent);
        // See if we have a command elt.  If so, we execute on the command
        // instead of on our content element.
        nsAutoString command;
        if (xulEvent && GetAttr(kNameSpaceID_None, nsGkAtoms::command, command) &&
            !command.IsEmpty()) {
            // Stop building the event target chain for the original event.
            // We don't want it to propagate to any DOM nodes.
            aVisitor.mCanHandle = false;

            // XXX sXBL/XBL2 issue! Owner or current document?
            nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(GetCurrentDoc()));
            NS_ENSURE_STATE(domDoc);
            nsCOMPtr<nsIDOMElement> commandElt;
            domDoc->GetElementById(command, getter_AddRefs(commandElt));
            nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));
            if (commandContent) {
                // Create a new command event to dispatch to the element
                // pointed to by the command attribute.  The new event's
                // sourceEvent will be the original command event that we're
                // handling.
                nsCOMPtr<nsIDOMNSEvent> nsevent =
                    do_QueryInterface(aVisitor.mDOMEvent);
                while (nsevent) {
                    nsCOMPtr<nsIDOMEventTarget> oTarget;
                    nsevent->GetOriginalTarget(getter_AddRefs(oTarget));
                    NS_ENSURE_STATE(!SameCOMIdentity(oTarget, commandContent));
                    nsCOMPtr<nsIDOMEvent> tmp;
                    nsCOMPtr<nsIDOMXULCommandEvent> commandEvent =
                        do_QueryInterface(nsevent);
                    if (commandEvent) {
                        commandEvent->GetSourceEvent(getter_AddRefs(tmp));
                    }
                    nsevent = do_QueryInterface(tmp);
                }

                nsInputEvent* orig =
                    static_cast<nsInputEvent*>(aVisitor.mEvent);
                nsContentUtils::DispatchXULCommand(
                  commandContent,
                  NS_IS_TRUSTED_EVENT(aVisitor.mEvent),
                  aVisitor.mDOMEvent,
                  nsnull,
                  orig->isControl,
                  orig->isAlt,
                  orig->isShift,
                  orig->isMeta);
            } else {
                NS_WARNING("A XUL element is attached to a command that doesn't exist!\n");
            }
            return NS_OK;
        }
    }

    return nsStyledElement::PreHandleEvent(aVisitor);
}

// XXX This _should_ be an implementation method, _not_ publicly exposed :-(
NS_IMETHODIMP
nsXULElement::GetResource(nsIRDFResource** aResource)
{
    nsAutoString id;
    GetAttr(kNameSpaceID_None, nsGkAtoms::ref, id);
    if (id.IsEmpty()) {
        GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
    }

    if (!id.IsEmpty()) {
        return nsXULContentUtils::RDFService()->
            GetUnicodeResource(id, aResource);
    }
    *aResource = nsnull;

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetDatabase(nsIRDFCompositeDataSource** aDatabase)
{
    nsCOMPtr<nsIXULTemplateBuilder> builder;
    GetBuilder(getter_AddRefs(builder));

    if (builder)
        builder->GetDatabase(aDatabase);
    else
        *aDatabase = nsnull;

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetBuilder(nsIXULTemplateBuilder** aBuilder)
{
    *aBuilder = nsnull;

    // XXX sXBL/XBL2 issue! Owner or current document?
    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(GetCurrentDoc());
    if (xuldoc)
        xuldoc->GetTemplateBuilderFor(this, aBuilder);

    return NS_OK;
}


//----------------------------------------------------------------------
// Implementation methods

// XXX DoGetID and DoGetClasses must be defined here because we have proto
// attributes.
nsIAtom*
nsXULElement::DoGetID() const
{
    NS_ASSERTION(HasID(), "Unexpected call");
    const nsAttrValue* attr =
        FindLocalOrProtoAttr(kNameSpaceID_None, nsGkAtoms::id);

    // We need the nullcheck here because during unlink the prototype loses
    // all of its attributes. We might want to change that.
    // The nullcheck would also be needed if we make UnsetAttr use
    // nsGenericElement::UnsetAttr as that calls out to various code between
    // removing the attribute and calling ClearHasID().

    return attr ? attr->GetAtomValue() : nsnull;
}

const nsAttrValue*
nsXULElement::DoGetClasses() const
{
    NS_ASSERTION(HasFlag(NODE_MAY_HAVE_CLASS), "Unexpected call");
    return FindLocalOrProtoAttr(kNameSpaceID_None, nsGkAtoms::_class);
}

NS_IMETHODIMP
nsXULElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
    return NS_OK;
}

css::StyleRule*
nsXULElement::GetInlineStyleRule()
{
    if (!MayHaveStyle()) {
        return nsnull;
    }
    // Fetch the cached style rule from the attributes.
    const nsAttrValue* attrVal = FindLocalOrProtoAttr(kNameSpaceID_None, nsGkAtoms::style);

    if (attrVal && attrVal->Type() == nsAttrValue::eCSSStyleRule) {
        return attrVal->GetCSSStyleRuleValue();
    }

    return nsnull;
}

nsChangeHint
nsXULElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                     PRInt32 aModType) const
{
    nsChangeHint retval(NS_STYLE_HINT_NONE);

    if (aAttribute == nsGkAtoms::value &&
        (aModType == nsIDOMMutationEvent::REMOVAL ||
         aModType == nsIDOMMutationEvent::ADDITION)) {
      nsIAtom *tag = Tag();
      if (tag == nsGkAtoms::label || tag == nsGkAtoms::description)
        // Label and description dynamically morph between a normal
        // block and a cropping single-line XUL text frame.  If the
        // value attribute is being added or removed, then we need to
        // return a hint of frame change.  (See bugzilla bug 95475 for
        // details.)
        retval = NS_STYLE_HINT_FRAMECHANGE;
    } else {
        // if left or top changes we reflow. This will happen in xul
        // containers that manage positioned children such as a stack.
        if (nsGkAtoms::left == aAttribute || nsGkAtoms::top == aAttribute ||
            nsGkAtoms::right == aAttribute || nsGkAtoms::bottom == aAttribute ||
            nsGkAtoms::start == aAttribute || nsGkAtoms::end == aAttribute)
            retval = NS_STYLE_HINT_REFLOW;
    }

    return retval;
}

NS_IMETHODIMP_(bool)
nsXULElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
    return false;
}

// Controllers Methods
NS_IMETHODIMP
nsXULElement::GetControllers(nsIControllers** aResult)
{
    if (! Controllers()) {
        nsDOMSlots* slots = DOMSlots();

        nsresult rv;
        rv = NS_NewXULControllers(nsnull, NS_GET_IID(nsIControllers),
                                  reinterpret_cast<void**>(&slots->mControllers));

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a controllers");
        if (NS_FAILED(rv)) return rv;
    }

    *aResult = Controllers();
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetBoxObject(nsIBoxObject** aResult)
{
  *aResult = nsnull;

  // XXX sXBL/XBL2 issue! Owner or current document?
  return OwnerDoc()->GetBoxObjectFor(this, aResult);
}

// Methods for setting/getting attributes from nsIDOMXULElement
#define NS_IMPL_XUL_STRING_ATTR(_method, _atom)                     \
  NS_IMETHODIMP                                                     \
  nsXULElement::Get##_method(nsAString& aReturn)                    \
  {                                                                 \
    GetAttr(kNameSpaceID_None, nsGkAtoms::_atom, aReturn);         \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  nsXULElement::Set##_method(const nsAString& aValue)               \
  {                                                                 \
    return SetAttr(kNameSpaceID_None, nsGkAtoms::_atom, aValue,    \
                   true);                                        \
  }

#define NS_IMPL_XUL_BOOL_ATTR(_method, _atom)                       \
  NS_IMETHODIMP                                                     \
  nsXULElement::Get##_method(bool* aResult)                       \
  {                                                                 \
    *aResult = BoolAttrIsTrue(nsGkAtoms::_atom);                   \
                                                                    \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  nsXULElement::Set##_method(bool aValue)                         \
  {                                                                 \
    if (aValue)                                                     \
      SetAttr(kNameSpaceID_None, nsGkAtoms::_atom,                 \
              NS_LITERAL_STRING("true"), true);                  \
    else                                                            \
      UnsetAttr(kNameSpaceID_None, nsGkAtoms::_atom, true);     \
                                                                    \
    return NS_OK;                                                   \
  }


NS_IMPL_XUL_STRING_ATTR(Id, id)
NS_IMPL_XUL_STRING_ATTR(ClassName, _class)
NS_IMPL_XUL_STRING_ATTR(Align, align)
NS_IMPL_XUL_STRING_ATTR(Dir, dir)
NS_IMPL_XUL_STRING_ATTR(Flex, flex)
NS_IMPL_XUL_STRING_ATTR(FlexGroup, flexgroup)
NS_IMPL_XUL_STRING_ATTR(Ordinal, ordinal)
NS_IMPL_XUL_STRING_ATTR(Orient, orient)
NS_IMPL_XUL_STRING_ATTR(Pack, pack)
NS_IMPL_XUL_BOOL_ATTR(Hidden, hidden)
NS_IMPL_XUL_BOOL_ATTR(Collapsed, collapsed)
NS_IMPL_XUL_BOOL_ATTR(AllowEvents, allowevents)
NS_IMPL_XUL_STRING_ATTR(Observes, observes)
NS_IMPL_XUL_STRING_ATTR(Menu, menu)
NS_IMPL_XUL_STRING_ATTR(ContextMenu, contextmenu)
NS_IMPL_XUL_STRING_ATTR(Tooltip, tooltip)
NS_IMPL_XUL_STRING_ATTR(Width, width)
NS_IMPL_XUL_STRING_ATTR(Height, height)
NS_IMPL_XUL_STRING_ATTR(MinWidth, minwidth)
NS_IMPL_XUL_STRING_ATTR(MinHeight, minheight)
NS_IMPL_XUL_STRING_ATTR(MaxWidth, maxwidth)
NS_IMPL_XUL_STRING_ATTR(MaxHeight, maxheight)
NS_IMPL_XUL_STRING_ATTR(Persist, persist)
NS_IMPL_XUL_STRING_ATTR(Left, left)
NS_IMPL_XUL_STRING_ATTR(Top, top)
NS_IMPL_XUL_STRING_ATTR(Datasources, datasources)
NS_IMPL_XUL_STRING_ATTR(Ref, ref)
NS_IMPL_XUL_STRING_ATTR(TooltipText, tooltiptext)
NS_IMPL_XUL_STRING_ATTR(StatusText, statustext)

nsresult
nsXULElement::EnsureLocalStyle()
{
    // Clone the prototype rule, if we don't have a local one.
    if (mPrototype &&
        !mAttrsAndChildren.GetAttr(nsGkAtoms::style, kNameSpaceID_None)) {

        nsXULPrototypeAttribute *protoattr =
                  FindPrototypeAttribute(kNameSpaceID_None, nsGkAtoms::style);
        if (protoattr && protoattr->mValue.Type() == nsAttrValue::eCSSStyleRule) {
            nsRefPtr<css::Rule> ruleClone =
                protoattr->mValue.GetCSSStyleRuleValue()->Clone();

            nsString stringValue;
            protoattr->mValue.ToString(stringValue);

            nsAttrValue value;
            nsRefPtr<css::StyleRule> styleRule = do_QueryObject(ruleClone);
            value.SetTo(styleRule, &stringValue);

            nsresult rv =
                mAttrsAndChildren.SetAndTakeAttr(nsGkAtoms::style, value);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

nsresult
nsXULElement::LoadSrc()
{
    // Allow frame loader only on objects for which a container box object
    // can be obtained.
    nsIAtom* tag = Tag();
    if (tag != nsGkAtoms::browser &&
        tag != nsGkAtoms::editor &&
        tag != nsGkAtoms::iframe) {
        return NS_OK;
    }
    if (!IsInDoc() ||
        !OwnerDoc()->GetRootElement() ||
        OwnerDoc()->GetRootElement()->
            NodeInfo()->Equals(nsGkAtoms::overlay, kNameSpaceID_XUL)) {
        return NS_OK;
    }
    nsXULSlots* slots = static_cast<nsXULSlots*>(GetSlots());
    NS_ENSURE_TRUE(slots, NS_ERROR_OUT_OF_MEMORY);
    if (!slots->mFrameLoader) {
        // false as the last parameter so that xul:iframe/browser/editor
        // session history handling works like dynamic html:iframes.
        // Usually xul elements are used in chrome, which doesn't have
        // session history at all.
        slots->mFrameLoader = nsFrameLoader::Create(this, false);
        NS_ENSURE_TRUE(slots->mFrameLoader, NS_OK);
    }

    return slots->mFrameLoader->LoadFrame();
}

nsresult
nsXULElement::GetFrameLoader(nsIFrameLoader **aFrameLoader)
{
    *aFrameLoader = GetFrameLoader().get();
    return NS_OK;
}

already_AddRefed<nsFrameLoader>
nsXULElement::GetFrameLoader()
{
    nsXULSlots* slots = static_cast<nsXULSlots*>(GetExistingSlots());
    if (!slots)
        return nsnull;

    nsFrameLoader* loader = slots->mFrameLoader;
    NS_IF_ADDREF(loader);
    return loader;
}

nsresult
nsXULElement::SwapFrameLoaders(nsIFrameLoaderOwner* aOtherOwner)
{
    nsCOMPtr<nsIContent> otherContent(do_QueryInterface(aOtherOwner));
    NS_ENSURE_TRUE(otherContent, NS_ERROR_NOT_IMPLEMENTED);

    nsXULElement* otherEl = FromContent(otherContent);
    NS_ENSURE_TRUE(otherEl, NS_ERROR_NOT_IMPLEMENTED);

    if (otherEl == this) {
        // nothing to do
        return NS_OK;
    }

    nsXULSlots *ourSlots = static_cast<nsXULSlots*>(GetExistingDOMSlots());
    nsXULSlots *otherSlots =
        static_cast<nsXULSlots*>(otherEl->GetExistingDOMSlots());
    if (!ourSlots || !ourSlots->mFrameLoader ||
        !otherSlots || !otherSlots->mFrameLoader) {
        // Can't handle swapping when there is nothing to swap... yet.
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return
        ourSlots->mFrameLoader->SwapWithOtherLoader(otherSlots->mFrameLoader,
                                                    ourSlots->mFrameLoader,
                                                    otherSlots->mFrameLoader);
}

NS_IMETHODIMP
nsXULElement::GetParentTree(nsIDOMXULMultiSelectControlElement** aTreeElement)
{
    for (nsIContent* current = GetParent(); current;
         current = current->GetParent()) {
        if (current->NodeInfo()->Equals(nsGkAtoms::listbox,
                                        kNameSpaceID_XUL)) {
            CallQueryInterface(current, aTreeElement);
            // XXX returning NS_OK because that's what the code used to do;
            // is that the right thing, though?

            return NS_OK;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::Focus()
{
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsIDOMElement> elem = do_QueryObject(this);
    return fm ? fm->SetFocus(this, 0) : NS_OK;
}

NS_IMETHODIMP
nsXULElement::Blur()
{
    if (!ShouldBlur(this))
      return NS_OK;

    nsIDocument* doc = GetCurrentDoc();
    if (!doc)
      return NS_OK;

    nsIDOMWindow* win = doc->GetWindow();
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (win && fm)
      return fm->ClearFocus(win);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::Click()
{
  return ClickWithInputSource(nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN);
}

nsresult
nsXULElement::ClickWithInputSource(PRUint16 aInputSource)
{
    if (BoolAttrIsTrue(nsGkAtoms::disabled))
        return NS_OK;

    nsCOMPtr<nsIDocument> doc = GetCurrentDoc(); // Strong just in case
    if (doc) {
        nsCOMPtr<nsIPresShell> shell = doc->GetShell();
        if (shell) {
            // strong ref to PresContext so events don't destroy it
            nsRefPtr<nsPresContext> context = shell->GetPresContext();

            bool isCallerChrome = nsContentUtils::IsCallerChrome();

            nsMouseEvent eventDown(isCallerChrome, NS_MOUSE_BUTTON_DOWN,
                                   nsnull, nsMouseEvent::eReal);
            nsMouseEvent eventUp(isCallerChrome, NS_MOUSE_BUTTON_UP,
                                 nsnull, nsMouseEvent::eReal);
            nsMouseEvent eventClick(isCallerChrome, NS_MOUSE_CLICK, nsnull,
                                    nsMouseEvent::eReal);
            eventDown.inputSource = eventUp.inputSource = eventClick.inputSource 
                                  = aInputSource;

            // send mouse down
            nsEventStatus status = nsEventStatus_eIgnore;
            nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                        context, &eventDown,  nsnull, &status);

            // send mouse up
            status = nsEventStatus_eIgnore;  // reset status
            nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                        context, &eventUp, nsnull, &status);

            // send mouse click
            status = nsEventStatus_eIgnore;  // reset status
            nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                        context, &eventClick, nsnull, &status);
        }
    }

    // oncommand is fired when an element is clicked...
    return DoCommand();
}

NS_IMETHODIMP
nsXULElement::DoCommand()
{
    nsCOMPtr<nsIDocument> doc = GetCurrentDoc(); // strong just in case
    if (doc) {
        nsContentUtils::DispatchXULCommand(this, true);
    }

    return NS_OK;
}

nsIContent *
nsXULElement::GetBindingParent() const
{
    return mBindingParent;
}

bool
nsXULElement::IsNodeOfType(PRUint32 aFlags) const
{
    return !(aFlags & ~eCONTENT);
}

static void
PopupListenerPropertyDtor(void* aObject, nsIAtom* aPropertyName,
                          void* aPropertyValue, void* aData)
{
  nsIDOMEventListener* listener =
    static_cast<nsIDOMEventListener*>(aPropertyValue);
  if (!listener) {
    return;
  }
  nsEventListenerManager* manager = static_cast<nsINode*>(aObject)->
    GetListenerManager(false);
  if (manager) {
    manager->RemoveEventListenerByType(listener,
                                       NS_LITERAL_STRING("mousedown"),
                                       NS_EVENT_FLAG_BUBBLE |
                                       NS_EVENT_FLAG_SYSTEM_EVENT);
    manager->RemoveEventListenerByType(listener,
                                       NS_LITERAL_STRING("contextmenu"),
                                       NS_EVENT_FLAG_BUBBLE |
                                       NS_EVENT_FLAG_SYSTEM_EVENT);
  }
  NS_RELEASE(listener);
}

nsresult
nsXULElement::AddPopupListener(nsIAtom* aName)
{
    // Add a popup listener to the element
    bool isContext = (aName == nsGkAtoms::context ||
                        aName == nsGkAtoms::contextmenu);
    nsIAtom* listenerAtom = isContext ?
                            nsGkAtoms::contextmenulistener :
                            nsGkAtoms::popuplistener;

    nsCOMPtr<nsIDOMEventListener> popupListener =
        static_cast<nsIDOMEventListener*>(GetProperty(listenerAtom));
    if (popupListener) {
        // Popup listener is already installed.
        return NS_OK;
    }

    popupListener = new nsXULPopupListener(this, isContext);

    // Add the popup as a listener on this element.
    nsEventListenerManager* manager = GetListenerManager(true);
    NS_ENSURE_TRUE(manager, NS_ERROR_FAILURE);
    nsresult rv = SetProperty(listenerAtom, popupListener,
                              PopupListenerPropertyDtor, true);
    NS_ENSURE_SUCCESS(rv, rv);
    // Want the property to have a reference to the listener.
    nsIDOMEventListener* listener = nsnull;
    popupListener.swap(listener);

    if (isContext) {
      manager->AddEventListenerByType(listener,
                                      NS_LITERAL_STRING("contextmenu"),
                                      NS_EVENT_FLAG_BUBBLE |
                                      NS_EVENT_FLAG_SYSTEM_EVENT);
    } else {
      manager->AddEventListenerByType(listener,
                                      NS_LITERAL_STRING("mousedown"),
                                      NS_EVENT_FLAG_BUBBLE |
                                      NS_EVENT_FLAG_SYSTEM_EVENT);
    }
    return NS_OK;
}

nsEventStates
nsXULElement::IntrinsicState() const
{
    nsEventStates state = nsStyledElement::IntrinsicState();

    if (IsReadWriteTextElement()) {
        state |= NS_EVENT_STATE_MOZ_READWRITE;
        state &= ~NS_EVENT_STATE_MOZ_READONLY;
    }

    return state;
}

//----------------------------------------------------------------------

nsGenericElement::nsAttrInfo
nsXULElement::GetAttrInfo(PRInt32 aNamespaceID, nsIAtom *aName) const
{

    nsAttrInfo info(nsStyledElement::GetAttrInfo(aNamespaceID, aName));
    if (!info.mValue) {
        nsXULPrototypeAttribute *protoattr =
            FindPrototypeAttribute(aNamespaceID, aName);
        if (protoattr) {
            return nsAttrInfo(&protoattr->mName, &protoattr->mValue);
        }
    }

    return info;
}


nsXULPrototypeAttribute *
nsXULElement::FindPrototypeAttribute(PRInt32 aNamespaceID,
                                     nsIAtom* aLocalName) const
{
    if (!mPrototype) {
        return nsnull;
    }

    PRUint32 i, count = mPrototype->mNumAttributes;
    if (aNamespaceID == kNameSpaceID_None) {
        // Common case so optimize for this
        for (i = 0; i < count; ++i) {
            nsXULPrototypeAttribute *protoattr = &mPrototype->mAttributes[i];
            if (protoattr->mName.Equals(aLocalName)) {
                return protoattr;
            }
        }
    }
    else {
        for (i = 0; i < count; ++i) {
            nsXULPrototypeAttribute *protoattr = &mPrototype->mAttributes[i];
            if (protoattr->mName.Equals(aLocalName, aNamespaceID)) {
                return protoattr;
            }
        }
    }

    return nsnull;
}

nsresult nsXULElement::MakeHeavyweight()
{
    if (!mPrototype)
        return NS_OK;           // already heavyweight

    nsRefPtr<nsXULPrototypeElement> proto;
    proto.swap(mPrototype);

    bool hadAttributes = mAttrsAndChildren.AttrCount() > 0;

    PRUint32 i;
    nsresult rv;
    for (i = 0; i < proto->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &proto->mAttributes[i];

        // We might have a local value for this attribute, in which case
        // we don't want to copy the prototype's value.
        if (hadAttributes &&
            mAttrsAndChildren.GetAttr(protoattr->mName.LocalName(),
                                      protoattr->mName.NamespaceID())) {
            continue;
        }

        nsAttrValue attrValue;
        
        // Style rules need to be cloned.
        if (protoattr->mValue.Type() == nsAttrValue::eCSSStyleRule) {
            nsRefPtr<css::Rule> ruleClone =
                protoattr->mValue.GetCSSStyleRuleValue()->Clone();

            nsString stringValue;
            protoattr->mValue.ToString(stringValue);

            nsRefPtr<css::StyleRule> styleRule = do_QueryObject(ruleClone);
            attrValue.SetTo(styleRule, &stringValue);
        }
        else {
            attrValue.SetTo(protoattr->mValue);
        }

        // XXX we might wanna have a SetAndTakeAttr that takes an nsAttrName
        if (protoattr->mName.IsAtom()) {
            rv = mAttrsAndChildren.SetAndTakeAttr(protoattr->mName.Atom(), attrValue);
        }
        else {
            rv = mAttrsAndChildren.SetAndTakeAttr(protoattr->mName.NodeInfo(),
                                                  attrValue);
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}

nsresult
nsXULElement::HideWindowChrome(bool aShouldHide)
{
    nsIDocument* doc = GetCurrentDoc();
    if (!doc || doc->GetRootElement() != this)
      return NS_ERROR_UNEXPECTED;

    // only top level chrome documents can hide the window chrome
    if (!doc->IsRootDisplayDocument())
      return NS_OK;

    nsIPresShell *shell = doc->GetShell();

    if (shell) {
        nsIFrame* frame = GetPrimaryFrame();

        nsPresContext *presContext = shell->GetPresContext();

        if (frame && presContext && presContext->IsChrome()) {
            nsIView* view = frame->GetClosestView();

            if (view) {
                nsIWidget* w = view->GetWidget();
                NS_ENSURE_STATE(w);
                w->HideWindowChrome(aShouldHide);
            }
        }
    }

    return NS_OK;
}

nsIWidget*
nsXULElement::GetWindowWidget()
{
    nsIDocument* doc = GetCurrentDoc();

    // only top level chrome documents can set the titlebar color
    if (doc->IsRootDisplayDocument()) {
        nsCOMPtr<nsISupports> container = doc->GetContainer();
        nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
        if (baseWindow) {
            nsCOMPtr<nsIWidget> mainWidget;
            baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
            return mainWidget;
        }
    }
    return nsnull;
}

void
nsXULElement::SetTitlebarColor(nscolor aColor, bool aActive)
{
    nsIWidget* mainWidget = GetWindowWidget();
    if (mainWidget) {
        mainWidget->SetWindowTitlebarColor(aColor, aActive);
    }
}

class SetDrawInTitleBarEvent : public nsRunnable
{
public:
  SetDrawInTitleBarEvent(nsIWidget* aWidget, bool aState)
    : mWidget(aWidget)
    , mState(aState)
  {}

  NS_IMETHOD Run() {
    NS_ASSERTION(mWidget, "You shouldn't call this runnable with a null widget!");

    mWidget->SetDrawsInTitlebar(mState);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIWidget> mWidget;
  bool mState;
};

void
nsXULElement::SetDrawsInTitlebar(bool aState)
{
    nsIWidget* mainWidget = GetWindowWidget();
    if (mainWidget) {
        nsContentUtils::AddScriptRunner(new SetDrawInTitleBarEvent(mainWidget, aState));
    }
}

class MarginSetter : public nsRunnable
{
public:
    MarginSetter(nsIWidget* aWidget) :
        mWidget(aWidget), mMargin(-1, -1, -1, -1)
    {}
    MarginSetter(nsIWidget *aWidget, const nsIntMargin& aMargin) :
        mWidget(aWidget), mMargin(aMargin)
    {}

    NS_IMETHOD Run()
    {
        // SetNonClientMargins can dispatch native events, hence doing
        // it off a script runner.
        mWidget->SetNonClientMargins(mMargin);
        return NS_OK;
    }

private:
    nsCOMPtr<nsIWidget> mWidget;
    nsIntMargin mMargin;
};

void
nsXULElement::SetChromeMargins(const nsAString* aValue)
{
    if (!aValue)
        return;

    nsIWidget* mainWidget = GetWindowWidget();
    if (!mainWidget)
        return;

    // top, right, bottom, left - see nsAttrValue
    nsAttrValue attrValue;
    nsIntMargin margins;

    nsAutoString data;
    data.Assign(*aValue);
    if (attrValue.ParseIntMarginValue(data) &&
        attrValue.GetIntMarginValue(margins)) {
        nsContentUtils::AddScriptRunner(new MarginSetter(mainWidget, margins));
    }
}

void
nsXULElement::ResetChromeMargins()
{
    nsIWidget* mainWidget = GetWindowWidget();
    if (!mainWidget)
        return;
    // See nsIWidget
    nsContentUtils::AddScriptRunner(new MarginSetter(mainWidget));
}

bool
nsXULElement::BoolAttrIsTrue(nsIAtom* aName)
{
    const nsAttrValue* attr =
        FindLocalOrProtoAttr(kNameSpaceID_None, aName);

    return attr && attr->Type() == nsAttrValue::eAtom &&
           attr->GetAtomValue() == nsGkAtoms::_true;
}

void
nsXULElement::RecompileScriptEventListeners()
{
    PRInt32 i, count = mAttrsAndChildren.AttrCount();
    bool haveLocalAttributes = (count > 0);
    for (i = 0; i < count; ++i) {
        const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);

        // Eventlistenener-attributes are always in the null namespace
        if (!name->IsAtom()) {
            continue;
        }

        nsIAtom *attr = name->Atom();
        if (!nsContentUtils::IsEventAttributeName(attr, EventNameType_XUL)) {
            continue;
        }

        nsAutoString value;
        GetAttr(kNameSpaceID_None, attr, value);
        AddScriptEventListener(attr, value, true);
    }

    if (mPrototype) {
        // If we have a prototype, the node we are binding to should
        // have the same script-type - otherwise we will compile the
        // event handlers incorrectly.
        NS_ASSERTION(mPrototype->mScriptTypeID == GetScriptTypeID(),
                     "Prototype and node confused about default language?");

        count = mPrototype->mNumAttributes;
        for (i = 0; i < count; ++i) {
            const nsAttrName &name = mPrototype->mAttributes[i].mName;

            // Eventlistenener-attributes are always in the null namespace
            if (!name.IsAtom()) {
                continue;
            }

            nsIAtom *attr = name.Atom();

            // Don't clobber a locally modified attribute.
            if (haveLocalAttributes && mAttrsAndChildren.GetAttr(attr)) {
                continue;
            }

            if (!nsContentUtils::IsEventAttributeName(attr, EventNameType_XUL)) {
                continue;
            }

            nsAutoString value;
            GetAttr(kNameSpaceID_None, attr, value);
            AddScriptEventListener(attr, value, true);
        }
    }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULPrototypeNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(nsXULPrototypeNode)
    if (tmp->mType == nsXULPrototypeNode::eType_Element) {
        static_cast<nsXULPrototypeElement*>(tmp)->Unlink();
    }
    else if (tmp->mType == nsXULPrototypeNode::eType_Script) {
        static_cast<nsXULPrototypeScript*>(tmp)->UnlinkJSObjects();
    }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsXULPrototypeNode)
    if (tmp->mType == nsXULPrototypeNode::eType_Element) {
        nsXULPrototypeElement *elem =
            static_cast<nsXULPrototypeElement*>(tmp);
        cb.NoteXPCOMChild(elem->mNodeInfo);
        PRUint32 i;
        for (i = 0; i < elem->mNumAttributes; ++i) {
            const nsAttrName& name = elem->mAttributes[i].mName;
            if (!name.IsAtom())
                cb.NoteXPCOMChild(name.NodeInfo());
        }
        for (i = 0; i < elem->mChildren.Length(); ++i) {
            NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_PTR(elem->mChildren[i].get(),
                                                         nsXULPrototypeNode,
                                                         "mChildren[i]")
        }
    }
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_NATIVE_BEGIN(nsXULPrototypeNode)
    if (tmp->mType == nsXULPrototypeNode::eType_Element) {
        nsXULPrototypeElement *elem =
            static_cast<nsXULPrototypeElement*>(tmp);
        if (elem->mHoldsScriptObject) {
            PRUint32 i;
            for (i = 0; i < elem->mNumAttributes; ++i) {
                JSObject* handler = elem->mAttributes[i].mEventHandler;
                NS_IMPL_CYCLE_COLLECTION_TRACE_CALLBACK(elem->mScriptTypeID,
                                                        handler,
                                                        "mAttributes[i].mEventHandler")
            }
        }
    }
    else if (tmp->mType == nsXULPrototypeNode::eType_Script) {
        nsXULPrototypeScript *script =
            static_cast<nsXULPrototypeScript*>(tmp);
        NS_IMPL_CYCLE_COLLECTION_TRACE_CALLBACK(script->mScriptObject.mLangID,
                                                script->mScriptObject.mObject,
                                                "mScriptObject.mObject")
    }
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXULPrototypeNode, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXULPrototypeNode, Release)

//----------------------------------------------------------------------
//
// nsXULPrototypeAttribute
//

nsXULPrototypeAttribute::~nsXULPrototypeAttribute()
{
    MOZ_COUNT_DTOR(nsXULPrototypeAttribute);
}


//----------------------------------------------------------------------
//
// nsXULPrototypeElement
//

nsresult
nsXULPrototypeElement::Serialize(nsIObjectOutputStream* aStream,
                                 nsIScriptGlobalObject* aGlobal,
                                 const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    // Write script language
    rv |= aStream->Write32(mScriptTypeID);

    // Write Node Info
    PRInt32 index = aNodeInfos->IndexOf(mNodeInfo);
    NS_ASSERTION(index >= 0, "unknown nsINodeInfo index");
    rv |= aStream->Write32(index);

    // Write Attributes
    rv |= aStream->Write32(mNumAttributes);

    nsAutoString attributeValue;
    PRUint32 i;
    for (i = 0; i < mNumAttributes; ++i) {
        nsCOMPtr<nsINodeInfo> ni;
        if (mAttributes[i].mName.IsAtom()) {
            ni = mNodeInfo->NodeInfoManager()->
                GetNodeInfo(mAttributes[i].mName.Atom(), nsnull,
                            kNameSpaceID_None,
                            nsIDOMNode::ATTRIBUTE_NODE);
            NS_ASSERTION(ni, "the nodeinfo should already exist");
        }
        else {
            ni = mAttributes[i].mName.NodeInfo();
        }

        index = aNodeInfos->IndexOf(ni);
        NS_ASSERTION(index >= 0, "unknown nsINodeInfo index");
        rv |= aStream->Write32(index);

        mAttributes[i].mValue.ToString(attributeValue);
        rv |= aStream->WriteWStringZ(attributeValue.get());
    }

    // Now write children
    rv |= aStream->Write32(PRUint32(mChildren.Length()));
    for (i = 0; i < mChildren.Length(); i++) {
        nsXULPrototypeNode* child = mChildren[i].get();
        switch (child->mType) {
        case eType_Element:
        case eType_Text:
        case eType_PI:
            rv |= child->Serialize(aStream, aGlobal, aNodeInfos);
            break;
        case eType_Script:
            rv |= aStream->Write32(child->mType);
            nsXULPrototypeScript* script = static_cast<nsXULPrototypeScript*>(child);

            rv |= aStream->Write32(script->mScriptObject.mLangID);

            rv |= aStream->Write8(script->mOutOfLine);
            if (! script->mOutOfLine) {
                rv |= script->Serialize(aStream, aGlobal, aNodeInfos);
            } else {
                rv |= aStream->WriteCompoundObject(script->mSrcURI,
                                                   NS_GET_IID(nsIURI),
                                                   true);

                if (script->mScriptObject.mObject) {
                    // This may return NS_OK without muxing script->mSrcURI's
                    // data into the cache file, in the case where that
                    // muxed document is already there (written by a prior
                    // session, or by an earlier cache episode during this
                    // session).
                    rv |= script->SerializeOutOfLine(aStream, aGlobal);
                }
            }
            break;
        }
    }

    return rv;
}

nsresult
nsXULPrototypeElement::Deserialize(nsIObjectInputStream* aStream,
                                   nsIScriptGlobalObject* aGlobal,
                                   nsIURI* aDocumentURI,
                                   const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    NS_PRECONDITION(aNodeInfos, "missing nodeinfo array");
    nsresult rv;

    // Read script language
    PRUint32 scriptId = 0;
    rv = aStream->Read32(&scriptId);
    mScriptTypeID = scriptId;

    // Read Node Info
    PRUint32 number;
    rv |= aStream->Read32(&number);
    mNodeInfo = aNodeInfos->SafeObjectAt(number);
    if (!mNodeInfo)
        return NS_ERROR_UNEXPECTED;

    // Read Attributes
    rv |= aStream->Read32(&number);
    mNumAttributes = PRInt32(number);

    PRUint32 i;
    if (mNumAttributes > 0) {
        mAttributes = new nsXULPrototypeAttribute[mNumAttributes];
        if (! mAttributes)
            return NS_ERROR_OUT_OF_MEMORY;

        nsAutoString attributeValue;
        for (i = 0; i < mNumAttributes; ++i) {
            rv |= aStream->Read32(&number);
            nsINodeInfo* ni = aNodeInfos->SafeObjectAt(number);
            if (!ni)
                return NS_ERROR_UNEXPECTED;

            mAttributes[i].mName.SetTo(ni);

            rv |= aStream->ReadString(attributeValue);
            rv |= SetAttrAt(i, attributeValue, aDocumentURI);
        }
    }

    rv |= aStream->Read32(&number);
    PRUint32 numChildren = PRInt32(number);

    if (numChildren > 0) {
        if (!mChildren.SetCapacity(numChildren))
            return NS_ERROR_OUT_OF_MEMORY;

        for (i = 0; i < numChildren; i++) {
            rv |= aStream->Read32(&number);
            Type childType = (Type)number;

            nsRefPtr<nsXULPrototypeNode> child;

            switch (childType) {
            case eType_Element:
                child = new nsXULPrototypeElement();
                if (! child)
                    return NS_ERROR_OUT_OF_MEMORY;
                child->mType = childType;

                rv |= child->Deserialize(aStream, aGlobal, aDocumentURI,
                                         aNodeInfos);
                break;
            case eType_Text:
                child = new nsXULPrototypeText();
                if (! child)
                    return NS_ERROR_OUT_OF_MEMORY;
                child->mType = childType;

                rv |= child->Deserialize(aStream, aGlobal, aDocumentURI,
                                         aNodeInfos);
                break;
            case eType_PI:
                child = new nsXULPrototypePI();
                if (! child)
                    return NS_ERROR_OUT_OF_MEMORY;
                child->mType = childType;

                rv |= child->Deserialize(aStream, aGlobal, aDocumentURI,
                                         aNodeInfos);
                break;
            case eType_Script: {
                PRUint32 langID = nsIProgrammingLanguage::UNKNOWN;
                rv |= aStream->Read32(&langID);

                // language version/options obtained during deserialization.
                nsXULPrototypeScript* script = new nsXULPrototypeScript(langID, 0, 0);
                if (! script)
                    return NS_ERROR_OUT_OF_MEMORY;
                child = script;
                child->mType = childType;

                rv |= aStream->ReadBoolean(&script->mOutOfLine);
                if (! script->mOutOfLine) {
                    rv |= script->Deserialize(aStream, aGlobal, aDocumentURI,
                                              aNodeInfos);
                } else {
                    rv |= aStream->ReadObject(true, getter_AddRefs(script->mSrcURI));

                    rv |= script->DeserializeOutOfLine(aStream, aGlobal);
                }
                // If we failed to deserialize, consider deleting 'script'?
                break;
            }
            default:
                NS_NOTREACHED("Unexpected child type!");
                rv = NS_ERROR_UNEXPECTED;
            }

            mChildren.AppendElement(child);

            // Oh dear. Something failed during the deserialization.
            // We don't know what.  But likely consequences of failed
            // deserializations included calls to |AbortCaching| which
            // shuts down the cache and closes our streams.
            // If that happens, next time through this loop, we die a messy
            // death. So, let's just fail now, and propagate that failure
            // upward so that the ChromeProtocolHandler knows it can't use
            // a cached chrome channel for this.
            if (NS_FAILED(rv))
                return rv;
        }
    }

    return rv;
}

nsresult
nsXULPrototypeElement::SetAttrAt(PRUint32 aPos, const nsAString& aValue,
                                 nsIURI* aDocumentURI)
{
    NS_PRECONDITION(aPos < mNumAttributes, "out-of-bounds");

    // WARNING!!
    // This code is largely duplicated in nsXULElement::SetAttr.
    // Any changes should be made to both functions.

    if (!mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        mAttributes[aPos].mValue.ParseStringOrAtom(aValue);

        return NS_OK;
    }

    if (mAttributes[aPos].mName.Equals(nsGkAtoms::id) &&
        !aValue.IsEmpty()) {
        mHasIdAttribute = true;
        // Store id as atom.
        // id="" means that the element has no id. Not that it has
        // emptystring as id.
        mAttributes[aPos].mValue.ParseAtom(aValue);

        return NS_OK;
    }
    else if (mAttributes[aPos].mName.Equals(nsGkAtoms::_class)) {
        mHasClassAttribute = true;
        // Compute the element's class list
        mAttributes[aPos].mValue.ParseAtomArray(aValue);

        return NS_OK;
    }
    else if (mAttributes[aPos].mName.Equals(nsGkAtoms::style)) {
        mHasStyleAttribute = true;
        // Parse the element's 'style' attribute
        nsRefPtr<css::StyleRule> rule;

        nsCSSParser parser;

        // XXX Get correct Base URI (need GetBaseURI on *prototype* element)
        parser.ParseStyleAttribute(aValue, aDocumentURI, aDocumentURI,
                                   // This is basically duplicating what
                                   // nsINode::NodePrincipal() does
                                   mNodeInfo->NodeInfoManager()->
                                     DocumentPrincipal(),
                                   getter_AddRefs(rule));
        if (rule) {
            mAttributes[aPos].mValue.SetTo(rule, &aValue);

            return NS_OK;
        }
        // Don't abort if parsing failed, it could just be malformed css.
    }

    mAttributes[aPos].mValue.ParseStringOrAtom(aValue);

    return NS_OK;
}

void
nsXULPrototypeElement::Unlink()
{
    if (mHoldsScriptObject) {
        nsContentUtils::DropScriptObjects(mScriptTypeID, this,
                                          &NS_CYCLE_COLLECTION_NAME(nsXULPrototypeNode));
        mHoldsScriptObject = false;
    }
    mNumAttributes = 0;
    delete[] mAttributes;
    mAttributes = nsnull;
}

//----------------------------------------------------------------------
//
// nsXULPrototypeScript
//

nsXULPrototypeScript::nsXULPrototypeScript(PRUint32 aLangID, PRUint32 aLineNo, PRUint32 aVersion)
    : nsXULPrototypeNode(eType_Script),
      mLineNo(aLineNo),
      mSrcLoading(false),
      mOutOfLine(true),
      mSrcLoadWaiters(nsnull),
      mLangVersion(aVersion),
      mScriptObject(aLangID)
{
    NS_ASSERTION(aLangID != nsIProgrammingLanguage::UNKNOWN,
                 "The language ID must be known and constant");
}


nsXULPrototypeScript::~nsXULPrototypeScript()
{
    UnlinkJSObjects();
}

nsresult
nsXULPrototypeScript::Serialize(nsIObjectOutputStream* aStream,
                                nsIScriptGlobalObject* aGlobal,
                                const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsIScriptContext *context = aGlobal->GetScriptContext(
                                        mScriptObject.mLangID);
    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nsnull ||
                 !mScriptObject.mObject,
                 "script source still loading when serializing?!");
    if (!mScriptObject.mObject)
        return NS_ERROR_FAILURE;

    // Write basic prototype data
    nsresult rv;
    rv = aStream->Write32(mLineNo);
    if (NS_FAILED(rv)) return rv;
    rv = aStream->Write32(mLangVersion);
    if (NS_FAILED(rv)) return rv;
    // And delegate the writing to the nsIScriptContext
    rv = context->Serialize(aStream, mScriptObject.mObject);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsXULPrototypeScript::SerializeOutOfLine(nsIObjectOutputStream* aStream,
                                         nsIScriptGlobalObject* aGlobal)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

    bool isChrome = false;
    if (NS_FAILED(mSrcURI->SchemeIs("chrome", &isChrome)) || !isChrome)
       // Don't cache scripts that don't come from chrome uris.
       return rv;

    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
    if (!cache)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ASSERTION(cache->IsEnabled(),
                 "writing to the cache file, but the XUL cache is off?");
    bool exists;
    cache->HasData(mSrcURI, &exists);
    
    
    /* return will be NS_OK from GetAsciiSpec.
     * that makes no sense.
     * nor does returning NS_OK from HasMuxedDocument.
     * XXX return something meaningful.
     */
    if (exists)
        return NS_OK;

    nsCOMPtr<nsIObjectOutputStream> oos;
    rv = cache->GetOutputStream(mSrcURI, getter_AddRefs(oos));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv |= Serialize(oos, aGlobal, nsnull);
    rv |= cache->FinishOutputStream(mSrcURI);

    if (NS_FAILED(rv))
        cache->AbortCaching();
    return rv;
}


nsresult
nsXULPrototypeScript::Deserialize(nsIObjectInputStream* aStream,
                                  nsIScriptGlobalObject* aGlobal,
                                  nsIURI* aDocumentURI,
                                  const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nsnull ||
                 !mScriptObject.mObject,
                 "prototype script not well-initialized when deserializing?!");

    // Read basic prototype data
    aStream->Read32(&mLineNo);
    aStream->Read32(&mLangVersion);

    nsIScriptContext *context = aGlobal->GetScriptContext(
                                            mScriptObject.mLangID);
    NS_ASSERTION(context != nsnull, "Have no context for deserialization");
    NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);
    nsScriptObjectHolder newScriptObject(context);
    rv = context->Deserialize(aStream, newScriptObject);
    if (NS_FAILED(rv)) {
        NS_WARNING("Language deseralization failed");
        return rv;
    }
    Set(newScriptObject);
    return NS_OK;
}


nsresult
nsXULPrototypeScript::DeserializeOutOfLine(nsIObjectInputStream* aInput,
                                           nsIScriptGlobalObject* aGlobal)
{
    // Keep track of failure via rv, so we can
    // AbortCaching if things look bad.
    nsresult rv = NS_OK;
    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
  
    nsCOMPtr<nsIObjectInputStream> objectInput = aInput;
    if (cache) {
        bool useXULCache = true;
        if (mSrcURI) {
            // NB: we must check the XUL script cache early, to avoid
            // multiple deserialization attempts for a given script.            
            // Note that nsXULDocument::LoadScript
            // checks the XUL script cache too, in order to handle the
            // serialization case.
            //
            // We need do this only for <script src='strres.js'> and the
            // like, i.e., out-of-line scripts that are included by several
            // different XUL documents stored in the cache file.
            useXULCache = cache->IsEnabled();

            if (useXULCache) {
                PRUint32 newLangID = nsIProgrammingLanguage::UNKNOWN;
                JSScript* newScriptObject =
                    cache->GetScript(mSrcURI, &newLangID);
                if (newScriptObject) {
                    // Things may blow here if we simply change the script
                    // language - other code may already have pre-fetched the
                    // global for the language. (You can see this code by
                    // setting langID to UNKNOWN in the nsXULPrototypeScript
                    // ctor and not setting it until the scriptObject is set -
                    // code that pre-fetches these globals will then start
                    // asserting.)
                    if (mScriptObject.mLangID != newLangID) {
                        NS_ERROR("XUL cache gave different language?");
                        return NS_ERROR_UNEXPECTED;
                    }
                    Set(newScriptObject);
                }
            }
        }

        if (! mScriptObject.mObject) {
            if (mSrcURI) {
                rv = cache->GetInputStream(mSrcURI, getter_AddRefs(objectInput));
            } 
            // If !mSrcURI, we have an inline script. We shouldn't have 
            // to do anything else in that case, I think.
 
            // We do reflect errors into rv, but our caller may want to
            // ignore our return value, because mScriptObject will be null
            // after any error, and that suffices to cause the script to
            // be reloaded (from the src= URI, if any) and recompiled.
            // We're better off slow-loading than bailing out due to a
            // error.
            if (NS_SUCCEEDED(rv))
                rv = Deserialize(objectInput, aGlobal, nsnull, nsnull);

            if (NS_SUCCEEDED(rv)) {
                if (useXULCache && mSrcURI) {
                    bool isChrome = false;
                    mSrcURI->SchemeIs("chrome", &isChrome);
                    if (isChrome) {
                        cache->PutScript(mSrcURI,
                                         mScriptObject.mLangID,
                                         mScriptObject.mObject);
                    }
                }
                cache->FinishInputStream(mSrcURI);
            } else {
                // If mSrcURI is not in the cache,
                // rv will be NS_ERROR_NOT_AVAILABLE and we'll try to
                // update the cache file to hold a serialization of
                // this script, once it has finished loading.
                if (rv != NS_ERROR_NOT_AVAILABLE)
                    cache->AbortCaching();
            }
        }
    }
    return rv;
}

nsresult
nsXULPrototypeScript::Compile(const PRUnichar* aText,
                              PRInt32 aTextLength,
                              nsIURI* aURI,
                              PRUint32 aLineNo,
                              nsIDocument* aDocument,
                              nsIScriptGlobalObjectOwner* aGlobalOwner)
{
    // We'll compile the script using the prototype document's special
    // script object as the parent. This ensures that we won't end up
    // with an uncollectable reference.
    //
    // Compiling it using (for example) the first document's global
    // object would cause JS to keep a reference via the __proto__ or
    // parent pointer to the first document's global. If that happened,
    // our script object would reference the first document, and the
    // first document would indirectly reference the prototype document
    // because it keeps the prototype cache alive. Circularity!
    nsresult rv;

    // Use the prototype document's special context
    nsIScriptContext *context;

    {
        nsIScriptGlobalObject* global = aGlobalOwner->GetScriptGlobalObject();
        NS_ASSERTION(global != nsnull, "prototype doc has no script global");
        if (! global)
            return NS_ERROR_UNEXPECTED;

        context = global->GetScriptContext(mScriptObject.mLangID);
        NS_ASSERTION(context != nsnull, "no context for script global");
        if (! context)
            return NS_ERROR_UNEXPECTED;
    }

    nsCAutoString urlspec;
    nsContentUtils::GetWrapperSafeScriptFilename(aDocument, aURI, urlspec);

    // Ok, compile it to create a prototype script object!

    nsScriptObjectHolder newScriptObject(context);
    rv = context->CompileScript(aText,
                                aTextLength,
                                // Use the enclosing document's principal
                                // XXX is this right? or should we use the
                                // protodoc's?
                                // If we start using the protodoc's, make sure
                                // the DowngradePrincipalIfNeeded stuff in
                                // nsXULDocument::OnStreamComplete still works!
                                aDocument->NodePrincipal(),
                                urlspec.get(),
                                aLineNo,
                                mLangVersion,
                                newScriptObject);
    if (NS_FAILED(rv))
        return rv;

    Set(newScriptObject);
    return rv;
}

void
nsXULPrototypeScript::UnlinkJSObjects()
{
    if (mScriptObject.mObject) {
        nsContentUtils::DropScriptObjects(mScriptObject.mLangID, this,
                                          &NS_CYCLE_COLLECTION_NAME(nsXULPrototypeNode));
        mScriptObject.mObject = nsnull;
    }
}

void
nsXULPrototypeScript::Set(JSScript* aObject)
{
    NS_ASSERTION(!mScriptObject.mObject, "Leaking script object.");
    if (!aObject) {
        mScriptObject.mObject = nsnull;

        return;
    }

    nsresult rv = nsContentUtils::HoldScriptObject(mScriptObject.mLangID,
                                                   this,
                                                   &NS_CYCLE_COLLECTION_NAME(nsXULPrototypeNode),
                                                   aObject, false);
    if (NS_SUCCEEDED(rv)) {
        mScriptObject.mObject = aObject;
    }
}

//----------------------------------------------------------------------
//
// nsXULPrototypeText
//

nsresult
nsXULPrototypeText::Serialize(nsIObjectOutputStream* aStream,
                              nsIScriptGlobalObject* aGlobal,
                              const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    rv |= aStream->WriteWStringZ(mValue.get());

    return rv;
}

nsresult
nsXULPrototypeText::Deserialize(nsIObjectInputStream* aStream,
                                nsIScriptGlobalObject* aGlobal,
                                nsIURI* aDocumentURI,
                                const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    rv = aStream->ReadString(mValue);

    return rv;
}

//----------------------------------------------------------------------
//
// nsXULPrototypePI
//

nsresult
nsXULPrototypePI::Serialize(nsIObjectOutputStream* aStream,
                            nsIScriptGlobalObject* aGlobal,
                            const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    rv |= aStream->WriteWStringZ(mTarget.get());
    rv |= aStream->WriteWStringZ(mData.get());

    return rv;
}

nsresult
nsXULPrototypePI::Deserialize(nsIObjectInputStream* aStream,
                              nsIScriptGlobalObject* aGlobal,
                              nsIURI* aDocumentURI,
                              const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    rv = aStream->ReadString(mTarget);
    rv |= aStream->ReadString(mData);

    return rv;
}
