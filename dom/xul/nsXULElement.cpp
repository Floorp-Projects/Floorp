/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsError.h"
#include "nsDOMString.h"
#include "nsAtom.h"
#include "nsIBaseWindow.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDocument.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "nsFocusManager.h"
#include "nsHTMLStyleSheet.h"
#include "nsNameSpaceManager.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsViewManager.h"
#include "nsIWidget.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "mozilla/dom/Event.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsXULControllers.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "XULDocument.h"
#include "nsXULPopupListener.h"
#include "ListBoxObject.h"
#include "nsContentUtils.h"
#include "nsContentList.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MouseEvents.h"
#include "nsPIDOMWindow.h"
#include "nsJSPrincipals.h"
#include "nsDOMAttributeMap.h"
#include "nsGkAtoms.h"
#include "nsNodeUtils.h"
#include "nsFrameLoader.h"
#include "mozilla/Logging.h"
#include "nsIControllers.h"
#include "nsAttrValueOrString.h"
#include "nsAttrValueInlines.h"
#include "mozilla/Attributes.h"
#include "nsIController.h"
#include "nsQueryObject.h"
#include <algorithm>
#include "nsIDOMChromeWindow.h"

#include "nsReadableUtils.h"
#include "nsIFrame.h"
#include "nsNodeInfoManager.h"
#include "nsXBLBinding.h"
#include "nsXULTooltipListener.h"
#include "mozilla/EventDispatcher.h"
#include "mozAutoDocUpdate.h"
#include "nsCCUncollectableMarker.h"
#include "nsICSSDeclaration.h"
#include "nsLayoutUtils.h"
#include "XULPopupElement.h"

#include "mozilla/dom/XULElementBinding.h"
#include "mozilla/dom/BoxObject.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/XULCommandEvent.h"

using namespace mozilla;
using namespace mozilla::dom;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
uint32_t             nsXULPrototypeAttribute::gNumElements;
uint32_t             nsXULPrototypeAttribute::gNumAttributes;
uint32_t             nsXULPrototypeAttribute::gNumCacheTests;
uint32_t             nsXULPrototypeAttribute::gNumCacheHits;
uint32_t             nsXULPrototypeAttribute::gNumCacheSets;
uint32_t             nsXULPrototypeAttribute::gNumCacheFills;
#endif

#define NS_DISPATCH_XUL_COMMAND     (1 << 0)

class nsXULElementTearoff final : public nsIFrameLoaderOwner
{
  ~nsXULElementTearoff() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsXULElementTearoff)

  explicit nsXULElementTearoff(nsXULElement* aElement)
    : mElement(aElement)
  {
  }

  NS_FORWARD_NSIFRAMELOADEROWNER(static_cast<nsXULElement*>(mElement.get())->)
private:
  RefPtr<nsXULElement> mElement;
};

NS_IMPL_CYCLE_COLLECTION(nsXULElementTearoff, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULElementTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULElementTearoff)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULElementTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoaderOwner)
NS_INTERFACE_MAP_END_AGGREGATED(mElement)

//----------------------------------------------------------------------
// nsXULElement
//

nsXULElement::nsXULElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsStyledElement(aNodeInfo),
      mBindingParent(nullptr)
{
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumElements);

    // We may be READWRITE by default; check.
    if (IsReadWriteTextElement()) {
        AddStatesSilently(NS_EVENT_STATE_MOZ_READWRITE);
        RemoveStatesSilently(NS_EVENT_STATE_MOZ_READONLY);
    }
}

nsXULElement::~nsXULElement()
{
}

void
nsXULElement::MaybeUpdatePrivateLifetime()
{
    if (AttrValueIs(kNameSpaceID_None, nsGkAtoms::windowtype,
                    NS_LITERAL_STRING("navigator:browser"),
                    eCaseMatters)) {
        return;
    }

    nsPIDOMWindowOuter* win = OwnerDoc()->GetWindow();
    nsCOMPtr<nsIDocShell> docShell = win ? win->GetDocShell() : nullptr;
    if (docShell) {
        docShell->SetAffectPrivateSessionLifetime(false);
    }
}

/* static */
nsXULElement* NS_NewBasicXULElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
{
  return new nsXULElement(aNodeInfo);
}

 /* static */
nsXULElement* nsXULElement::Construct(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo = aNodeInfo;
  if (nodeInfo->Equals(nsGkAtoms::menupopup) ||
      nodeInfo->Equals(nsGkAtoms::popup) ||
      nodeInfo->Equals(nsGkAtoms::panel) ||
      nodeInfo->Equals(nsGkAtoms::tooltip)) {
    return NS_NewXULPopupElement(nodeInfo.forget());
  }

  return NS_NewBasicXULElement(nodeInfo.forget());
}

/* static */
already_AddRefed<nsXULElement>
nsXULElement::CreateFromPrototype(nsXULPrototypeElement* aPrototype,
                                  mozilla::dom::NodeInfo *aNodeInfo,
                                  bool aIsScriptable,
                                  bool aIsRoot)
{
    RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
    nsCOMPtr<Element> baseElement;
    NS_NewXULElement(getter_AddRefs(baseElement), ni.forget(), dom::FROM_PARSER_NETWORK);

    if (baseElement) {
        nsXULElement* element = FromNode(baseElement);

        if (aPrototype->mHasIdAttribute) {
            element->SetHasID();
        }
        if (aPrototype->mHasClassAttribute) {
            element->SetMayHaveClass();
        }
        if (aPrototype->mHasStyleAttribute) {
            element->SetMayHaveStyle();
        }

        element->MakeHeavyweight(aPrototype);
        if (aIsScriptable) {
            // Check each attribute on the prototype to see if we need to do
            // any additional processing and hookup that would otherwise be
            // done 'automagically' by SetAttr().
            for (uint32_t i = 0; i < aPrototype->mNumAttributes; ++i) {
                element->AddListenerFor(aPrototype->mAttributes[i].mName,
                                        true);
            }
        }

        if (aIsRoot && aPrototype->mNodeInfo->Equals(nsGkAtoms::window)) {
            for (uint32_t i = 0; i < aPrototype->mNumAttributes; ++i) {
                if (aPrototype->mAttributes[i].mName.Equals(nsGkAtoms::windowtype)) {
                    element->MaybeUpdatePrivateLifetime();
                }
            }
        }

        return baseElement.forget().downcast<nsXULElement>();
    }

    return nullptr;
}

nsresult
nsXULElement::CreateFromPrototype(nsXULPrototypeElement* aPrototype,
                                  nsIDocument* aDocument,
                                  bool aIsScriptable,
                                  bool aIsRoot,
                                  Element** aResult)
{
    // Create an nsXULElement from a prototype
    MOZ_ASSERT(aPrototype != nullptr, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    MOZ_ASSERT(aResult != nullptr, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    RefPtr<mozilla::dom::NodeInfo> nodeInfo;
    if (aDocument) {
        mozilla::dom::NodeInfo* ni = aPrototype->mNodeInfo;
        nodeInfo = aDocument->NodeInfoManager()->
          GetNodeInfo(ni->NameAtom(), ni->GetPrefixAtom(), ni->NamespaceID(),
                      ELEMENT_NODE);
    } else {
        nodeInfo = aPrototype->mNodeInfo;
    }

    RefPtr<nsXULElement> element = CreateFromPrototype(aPrototype, nodeInfo,
                                                       aIsScriptable, aIsRoot);
    element.forget(aResult);

    return NS_OK;
}

nsresult
NS_NewXULElement(Element** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                 FromParser aFromParser)
{
    RefPtr<mozilla::dom::NodeInfo> nodeInfo = aNodeInfo;

    MOZ_ASSERT(nodeInfo, "need nodeinfo for non-proto Create");

    NS_ASSERTION(nodeInfo->NamespaceEquals(kNameSpaceID_XUL),
                 "Trying to create XUL elements that don't have the XUL namespace");

    nsIDocument* doc = nodeInfo->GetDocument();
    if (doc && !doc->AllowXULXBL()) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    return nsContentUtils::NewXULOrHTMLElement(aResult, nodeInfo, aFromParser, nullptr, nullptr);
}

void
NS_TrustedNewXULElement(Element** aResult,
                        already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
{
    RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
    MOZ_ASSERT(ni, "need nodeinfo for non-proto Create");

    // Create an nsXULElement with the specified namespace and tag.
    NS_ADDREF(*aResult = nsXULElement::Construct(ni.forget()));
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXULElement,
                                                  nsStyledElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXULElement,
                                                nsStyledElement)
    // Why aren't we unlinking the prototype?
    tmp->ClearHasID();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsXULElement, nsStyledElement)
NS_IMPL_RELEASE_INHERITED(nsXULElement, nsStyledElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsXULElement)
    NS_INTERFACE_TABLE_INHERITED(nsXULElement, nsIDOMNode)
    NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIFrameLoaderOwner,
                                   new nsXULElementTearoff(this))
NS_INTERFACE_MAP_END_INHERITING(nsStyledElement)

//----------------------------------------------------------------------
// nsIDOMNode interface

nsresult
nsXULElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                    bool aPreallocateChildren) const
{
    *aResult = nullptr;

    RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
    RefPtr<nsXULElement> element = Construct(ni.forget());

    nsresult rv = element->mAttrsAndChildren.EnsureCapacityToClone(mAttrsAndChildren,
                                                                   aPreallocateChildren);
    NS_ENSURE_SUCCESS(rv, rv);

    // Note that we're _not_ copying mControllers.

    uint32_t count = mAttrsAndChildren.AttrCount();
    rv = NS_OK;
    for (uint32_t i = 0; i < count; ++i) {
        const nsAttrName* originalName = mAttrsAndChildren.AttrNameAt(i);
        const nsAttrValue* originalValue = mAttrsAndChildren.AttrAt(i);
        nsAttrValue attrValue;

        // Style rules need to be cloned.
        if (originalValue->Type() == nsAttrValue::eCSSDeclaration) {
            DeclarationBlock* decl = originalValue->GetCSSDeclarationValue();
            RefPtr<DeclarationBlock> declClone = decl->Clone();

            nsString stringValue;
            originalValue->ToString(stringValue);

            attrValue.SetTo(declClone.forget(), &stringValue);
        } else {
            attrValue.SetTo(*originalValue);
        }

        bool oldValueSet;
        if (originalName->IsAtom()) {
           rv = element->mAttrsAndChildren.SetAndSwapAttr(originalName->Atom(),
                                                          attrValue,
                                                          &oldValueSet);
        } else {
            rv = element->mAttrsAndChildren.SetAndSwapAttr(originalName->NodeInfo(),
                                                           attrValue,
                                                           &oldValueSet);
        }
        NS_ENSURE_SUCCESS(rv, rv);
        element->AddListenerFor(*originalName, true);
        if (originalName->Equals(nsGkAtoms::id) &&
            !originalValue->IsEmptyString()) {
            element->SetHasID();
        }
        if (originalName->Equals(nsGkAtoms::_class)) {
            element->SetMayHaveClass();
        }
        if (originalName->Equals(nsGkAtoms::style)) {
            element->SetMayHaveStyle();
        }
    }

    element.forget(aResult);
    return rv;
}

//----------------------------------------------------------------------

already_AddRefed<nsINodeList>
nsXULElement::GetElementsByAttribute(const nsAString& aAttribute,
                                     const nsAString& aValue)
{
    RefPtr<nsAtom> attrAtom(NS_Atomize(aAttribute));
    void* attrValue = new nsString(aValue);
    RefPtr<nsContentList> list =
        new nsContentList(this,
                          XULDocument::MatchAttribute,
                          nsContentUtils::DestroyMatchString,
                          attrValue,
                          true,
                          attrAtom,
                          kNameSpaceID_Unknown);
    return list.forget();
}

already_AddRefed<nsINodeList>
nsXULElement::GetElementsByAttributeNS(const nsAString& aNamespaceURI,
                                       const nsAString& aAttribute,
                                       const nsAString& aValue,
                                       ErrorResult& rv)
{
    RefPtr<nsAtom> attrAtom(NS_Atomize(aAttribute));

    int32_t nameSpaceId = kNameSpaceID_Wildcard;
    if (!aNamespaceURI.EqualsLiteral("*")) {
      rv =
        nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                              nameSpaceId);
      if (rv.Failed()) {
          return nullptr;
      }
    }

    void* attrValue = new nsString(aValue);
    RefPtr<nsContentList> list =
        new nsContentList(this,
                          XULDocument::MatchAttribute,
                          nsContentUtils::DestroyMatchString,
                          attrValue,
                          true,
                          attrAtom,
                          nameSpaceId);

    return list.forget();
}

EventListenerManager*
nsXULElement::GetEventListenerManagerForAttr(nsAtom* aAttrName, bool* aDefer)
{
    // XXXbz sXBL/XBL2 issue: should we instead use GetComposedDoc()
    // here, override BindToTree for those classes and munge event
    // listeners there?
    nsIDocument* doc = OwnerDoc();

    nsPIDOMWindowInner *window;
    Element *root = doc->GetRootElement();
    if ((!root || root == this) && !mNodeInfo->Equals(nsGkAtoms::overlay) &&
        (window = doc->GetInnerWindow())) {

        nsCOMPtr<EventTarget> piTarget = do_QueryInterface(window);

        *aDefer = false;
        return piTarget->GetOrCreateListenerManager();
    }

    return nsStyledElement::GetEventListenerManagerForAttr(aAttrName, aDefer);
}

// returns true if the element is not a list
static bool IsNonList(mozilla::dom::NodeInfo* aNodeInfo)
{
  return !aNodeInfo->Equals(nsGkAtoms::tree) &&
         !aNodeInfo->Equals(nsGkAtoms::listbox) &&
         !aNodeInfo->Equals(nsGkAtoms::richlistbox);
}

bool
nsXULElement::IsFocusableInternal(int32_t *aTabIndex, bool aWithMouse)
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
  // on Mac, mouse interactions only focus the element if it's a list,
  // or if it's a remote target, since the remote target must handle
  // the focus.
  if (aWithMouse &&
      IsNonList(mNodeInfo) &&
      !EventStateManager::IsRemoteTarget(this))
  {
    return false;
  }
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
        int32_t tabIndex = 0;
        xulControl->GetTabIndex(&tabIndex);
        shouldFocus = *aTabIndex >= 0 || tabIndex >= 0;
        *aTabIndex = tabIndex;
      } else {
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
    } else {
      shouldFocus = *aTabIndex >= 0;
    }
  }

  return shouldFocus;
}

bool
nsXULElement::PerformAccesskey(bool aKeyCausesActivation,
                               bool aIsTrustedEvent)
{
    RefPtr<Element> content(this);

    if (IsXULElement(nsGkAtoms::label)) {
        nsAutoString control;
        GetAttr(kNameSpaceID_None, nsGkAtoms::control, control);
        if (control.IsEmpty()) {
            return false;
        }

        //XXXsmaug Should we use ShadowRoot::GetElementById in case
        //         content is in Shadow DOM?
        nsCOMPtr<nsIDocument> document = content->GetUncomposedDoc();
        if (!document) {
            return false;
        }

        content = document->GetElementById(control);
        if (!content) {
            return false;
        }
    }

    nsIFrame* frame = content->GetPrimaryFrame();
    if (!frame || !frame->IsVisibleConsideringAncestors()) {
        return false;
    }

    bool focused = false;
    nsXULElement* elm = FromNode(content);
    if (elm) {
        // Define behavior for each type of XUL element.
        if (!content->IsXULElement(nsGkAtoms::toolbarbutton)) {
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            nsCOMPtr<Element> elementToFocus;
            // for radio buttons, focus the radiogroup instead
            if (content->IsXULElement(nsGkAtoms::radio)) {
              nsCOMPtr<nsIDOMXULSelectControlItemElement> controlItem(do_QueryInterface(content));
              if (controlItem) {
                bool disabled;
                controlItem->GetDisabled(&disabled);
                if (!disabled) {
                  nsCOMPtr<nsIDOMXULSelectControlElement> selectControl;
                  controlItem->GetControl(getter_AddRefs(selectControl));
                  elementToFocus = do_QueryInterface(selectControl);
                }
              }
            } else {
              elementToFocus = content;
            }
            if (elementToFocus) {
              fm->SetFocus(elementToFocus, nsIFocusManager::FLAG_BYKEY);

              // Return true if the element became focused.
              nsPIDOMWindowOuter* window = OwnerDoc()->GetWindow();
              focused = (window && window->GetFocusedElement());
            }
          }
        }
        if (aKeyCausesActivation &&
            !content->IsAnyOfXULElements(nsGkAtoms::textbox, nsGkAtoms::menulist)) {
          elm->ClickWithInputSource(MouseEventBinding::MOZ_SOURCE_KEYBOARD, aIsTrustedEvent);
        }
    } else {
        return content->PerformAccesskey(aKeyCausesActivation, aIsTrustedEvent);
    }

    return focused;
}

//----------------------------------------------------------------------

void
nsXULElement::AddListenerFor(const nsAttrName& aName,
                             bool aCompileEventHandlers)
{
    // If appropriate, add a popup listener and/or compile the event
    // handler. Called when we change the element's document, create a
    // new element, change an attribute's value, etc.
    // Eventlistenener-attributes are always in the null namespace
    if (aName.IsAtom()) {
        nsAtom *attr = aName.Atom();
        MaybeAddPopupListener(attr);
        if (aCompileEventHandlers &&
            nsContentUtils::IsEventAttributeName(attr, EventNameType_XUL)) {
            nsAutoString value;
            GetAttr(kNameSpaceID_None, attr, value);
            SetEventHandler(attr, value, true);
        }
    }
}

void
nsXULElement::MaybeAddPopupListener(nsAtom* aLocalName)
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
    // Don't call through to Element here because the things
    // it does don't work for cases when we're an editable control.
    nsIContent *parent = GetParent();

    SetEditableFlag(parent && parent->HasFlag(NODE_IS_EDITABLE));
    UpdateState(aNotify);
}

#ifdef DEBUG
/**
 * Returns true if the user-agent style sheet rules for this XUL element are
 * in minimal-xul.css instead of xul.css.
 */
static inline bool XULElementsRulesInMinimalXULSheet(nsAtom* aTag)
{
  return // scrollbar parts:
         aTag == nsGkAtoms::scrollbar ||
         aTag == nsGkAtoms::scrollbarbutton ||
         aTag == nsGkAtoms::scrollcorner ||
         aTag == nsGkAtoms::slider ||
         aTag == nsGkAtoms::thumb ||
         // other
         aTag == nsGkAtoms::datetimebox ||
         aTag == nsGkAtoms::resizer ||
         aTag == nsGkAtoms::label ||
         aTag == nsGkAtoms::videocontrols;
}
#endif

class XULInContentErrorReporter : public Runnable
{
public:
  explicit XULInContentErrorReporter(nsIDocument* aDocument)
    : mozilla::Runnable("XULInContentErrorReporter")
    , mDocument(aDocument)
  {
  }

  NS_IMETHOD Run() override
  {
    mDocument->WarnOnceAbout(nsIDocument::eImportXULIntoContent, false);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
};

static bool
NeedTooltipSupport(const nsXULElement& aXULElement)
{
  if (aXULElement.NodeInfo()->Equals(nsGkAtoms::treechildren)) {
    // treechildren always get tooltip support, since cropped tree cells show
    // their full text in a tooltip.
    return true;
  }

  return aXULElement.GetBoolAttr(nsGkAtoms::tooltip) ||
         aXULElement.GetBoolAttr(nsGkAtoms::tooltiptext);
}

nsresult
nsXULElement::BindToTree(nsIDocument* aDocument,
                         nsIContent* aParent,
                         nsIContent* aBindingParent,
                         bool aCompileEventHandlers)
{
  if (!aBindingParent &&
      aDocument &&
      !aDocument->IsLoadedAsInteractiveData() &&
      !aDocument->AllowXULXBL() &&
      !aDocument->HasWarnedAbout(nsIDocument::eImportXULIntoContent)) {
    nsContentUtils::AddScriptRunner(new XULInContentErrorReporter(aDocument));
  }

  nsresult rv = nsStyledElement::BindToTree(aDocument, aParent,
                                            aBindingParent,
                                            aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument* doc = GetComposedDoc();
#ifdef DEBUG
  if (doc &&
      !doc->LoadsFullXULStyleSheetUpFront() &&
      !doc->IsUnstyledDocument()) {

    // To save CPU cycles and memory, non-XUL documents only load the user
    // agent style sheet rules for a minimal set of XUL elements such as
    // 'scrollbar' that may be created implicitly for their content (those
    // rules being in minimal-xul.css).
    //
    // This assertion makes sure no other XUL element than the ones in the
    // minimal XUL sheet is used in the bindings.
    if (!XULElementsRulesInMinimalXULSheet(NodeInfo()->NameAtom())) {
      NS_ERROR("Unexpected XUL element in non-XUL doc");
    }
  }
#endif

  if (doc && NeedTooltipSupport(*this)) {
      AddTooltipSupport();
  }

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
    if (NeedTooltipSupport(*this)) {
        RemoveTooltipSupport();
    }

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
    nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
    if (slots) {
        slots->mControllers = nullptr;
        RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
        if (frameLoader) {
            frameLoader->Destroy();
        }
        slots->mFrameLoaderOrOpener = nullptr;
    }

    nsStyledElement::UnbindFromTree(aDeep, aNullParent);
}

void
nsXULElement::RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify)
{
    nsCOMPtr<nsIContent> oldKid = mAttrsAndChildren.GetSafeChildAt(aIndex);
    if (!oldKid) {
      return;
    }

    // On the removal of a <treeitem>, <treechildren>, or <treecell> element,
    // the possibility exists that some of the items in the removed subtree
    // are selected (and therefore need to be deselected). We need to account for this.
    nsCOMPtr<nsIDOMXULMultiSelectControlElement> controlElement;
    nsCOMPtr<nsIListBoxObject> listBox;
    bool fireSelectionHandler = false;

    // -1 = do nothing, -2 = null out current item
    // anything else = index to re-set as current
    int32_t newCurrentIndex = -1;

    if (oldKid->NodeInfo()->Equals(nsGkAtoms::listitem, kNameSpaceID_XUL)) {
      // This is the nasty case. We have (potentially) a slew of selected items
      // and cells going away.
      // First, retrieve the tree.
      // Check first whether this element IS the tree
      controlElement = do_QueryObject(this);

      // If it's not, look at our parent
      if (!controlElement)
        GetParentTree(getter_AddRefs(controlElement));
      nsCOMPtr<nsIContent> controlContent(do_QueryInterface(controlElement));
      RefPtr<nsXULElement> xulElement = FromNodeOrNull(controlContent);

      if (xulElement) {
        // Iterate over all of the items and find out if they are contained inside
        // the removed subtree.
        int32_t length;
        controlElement->GetSelectedCount(&length);
        for (int32_t i = 0; i < length; i++) {
          nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
          controlElement->MultiGetSelectedItem(i, getter_AddRefs(item));
          nsCOMPtr<nsINode> node = do_QueryInterface(item);
          if (node == oldKid &&
              NS_SUCCEEDED(controlElement->RemoveItemFromSelection(item))) {
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
            nsCOMPtr<nsIBoxObject> box = xulElement->GetBoxObject(IgnoreErrors());
            listBox = do_QueryInterface(box);
            if (listBox) {
              listBox->GetIndexOfItem(oldKid->AsElement(), &newCurrentIndex);
            }

            // If any of this fails, we'll just set the current item to null
            if (newCurrentIndex == -1)
              newCurrentIndex = -2;
        }
      }
    }

    nsStyledElement::RemoveChildAt_Deprecated(aIndex, aNotify);

    if (newCurrentIndex == -2) {
        controlElement->SetCurrentItem(nullptr);
    } else if (newCurrentIndex > -1) {
        // Make sure the index is still valid
        int32_t treeRows;
        listBox->GetRowCount(&treeRows);
        if (treeRows > 0) {
            newCurrentIndex = std::min((treeRows - 1), newCurrentIndex);
            RefPtr<Element> newCurrentItem;
            listBox->GetItemAtIndex(newCurrentIndex, getter_AddRefs(newCurrentItem));
            nsCOMPtr<nsIDOMXULSelectControlItemElement> xulCurItem = do_QueryInterface(newCurrentItem);
            if (xulCurItem)
                controlElement->SetCurrentItem(xulCurItem);
        } else {
            controlElement->SetCurrentItem(nullptr);
        }
    }

    nsIDocument* doc;
    if (fireSelectionHandler && (doc = GetComposedDoc())) {
      nsContentUtils::DispatchTrustedEvent(doc,
                                           static_cast<nsIContent*>(this),
                                           NS_LITERAL_STRING("select"),
                                           false,
                                           true);
    }
}

void
nsXULElement::RemoveChildNode(nsIContent* aKid, bool aNotify)
{
    // On the removal of a <treeitem>, <treechildren>, or <treecell> element,
    // the possibility exists that some of the items in the removed subtree
    // are selected (and therefore need to be deselected). We need to account for this.
    nsCOMPtr<nsIDOMXULMultiSelectControlElement> controlElement;
    nsCOMPtr<nsIListBoxObject> listBox;
    bool fireSelectionHandler = false;

    // -1 = do nothing, -2 = null out current item
    // anything else = index to re-set as current
    int32_t newCurrentIndex = -1;

    if (aKid->NodeInfo()->Equals(nsGkAtoms::listitem, kNameSpaceID_XUL)) {
      // This is the nasty case. We have (potentially) a slew of selected items
      // and cells going away.
      // First, retrieve the tree.
      // Check first whether this element IS the tree
      controlElement = do_QueryObject(this);

      // If it's not, look at our parent
      if (!controlElement)
        GetParentTree(getter_AddRefs(controlElement));
      nsCOMPtr<nsIContent> controlContent(do_QueryInterface(controlElement));
      RefPtr<nsXULElement> xulElement = FromNodeOrNull(controlContent);

      if (xulElement) {
        // Iterate over all of the items and find out if they are contained inside
        // the removed subtree.
        int32_t length;
        controlElement->GetSelectedCount(&length);
        for (int32_t i = 0; i < length; i++) {
          nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
          controlElement->MultiGetSelectedItem(i, getter_AddRefs(item));
          nsCOMPtr<nsINode> node = do_QueryInterface(item);
          if (node == aKid &&
              NS_SUCCEEDED(controlElement->RemoveItemFromSelection(item))) {
            length--;
            i--;
            fireSelectionHandler = true;
          }
        }

        nsCOMPtr<nsIDOMXULSelectControlItemElement> curItem;
        controlElement->GetCurrentItem(getter_AddRefs(curItem));
        nsCOMPtr<nsIContent> curNode = do_QueryInterface(curItem);
        if (curNode && nsContentUtils::ContentIsDescendantOf(curNode, aKid)) {
            // Current item going away
            nsCOMPtr<nsIBoxObject> box = xulElement->GetBoxObject(IgnoreErrors());
            listBox = do_QueryInterface(box);
            if (listBox) {
              listBox->GetIndexOfItem(aKid->AsElement(), &newCurrentIndex);
            }

            // If any of this fails, we'll just set the current item to null
            if (newCurrentIndex == -1)
              newCurrentIndex = -2;
        }
      }
    }

    nsStyledElement::RemoveChildNode(aKid, aNotify);

    if (newCurrentIndex == -2) {
        controlElement->SetCurrentItem(nullptr);
    } else if (newCurrentIndex > -1) {
        // Make sure the index is still valid
        int32_t treeRows;
        listBox->GetRowCount(&treeRows);
        if (treeRows > 0) {
            newCurrentIndex = std::min((treeRows - 1), newCurrentIndex);
            RefPtr<Element> newCurrentItem;
            listBox->GetItemAtIndex(newCurrentIndex, getter_AddRefs(newCurrentItem));
            nsCOMPtr<nsIDOMXULSelectControlItemElement> xulCurItem = do_QueryInterface(newCurrentItem);
            if (xulCurItem)
                controlElement->SetCurrentItem(xulCurItem);
        } else {
            controlElement->SetCurrentItem(nullptr);
        }
    }

    nsIDocument* doc;
    if (fireSelectionHandler && (doc = GetComposedDoc())) {
      nsContentUtils::DispatchTrustedEvent(doc,
                                           static_cast<nsIContent*>(this),
                                           NS_LITERAL_STRING("select"),
                                           false,
                                           true);
    }
}

void
nsXULElement::UnregisterAccessKey(const nsAString& aOldValue)
{
    // If someone changes the accesskey, unregister the old one
    //
    nsIDocument* doc = GetComposedDoc();
    if (doc && !aOldValue.IsEmpty()) {
        nsIPresShell *shell = doc->GetShell();

        if (shell) {
            Element* element = this;

            // find out what type of content node this is
            if (mNodeInfo->Equals(nsGkAtoms::label)) {
                // For anonymous labels the unregistering must
                // occur on the binding parent control.
                // XXXldb: And what if the binding parent is null?
                nsIContent* bindingParent = GetBindingParent();
                element = bindingParent ? bindingParent->AsElement() : nullptr;
            }

            if (element) {
                shell->GetPresContext()->EventStateManager()->
                    UnregisterAccessKey(element, aOldValue.First());
            }
        }
    }
}

nsresult
nsXULElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                            const nsAttrValueOrString* aValue, bool aNotify)
{
    if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::accesskey &&
        IsInUncomposedDoc()) {
        nsAutoString oldValue;
        if (GetAttr(aNamespaceID, aName, oldValue)) {
            UnregisterAccessKey(oldValue);
        }
    } else if (aNamespaceID == kNameSpaceID_None &&
               (aName == nsGkAtoms::command || aName == nsGkAtoms::observes) &&
               IsInUncomposedDoc()) {
//         XXX sXBL/XBL2 issue! Owner or current document?
        nsAutoString oldValue;
        GetAttr(kNameSpaceID_None, nsGkAtoms::observes, oldValue);
        if (oldValue.IsEmpty()) {
          GetAttr(kNameSpaceID_None, nsGkAtoms::command, oldValue);
        }

        if (!oldValue.IsEmpty()) {
          RemoveBroadcaster(oldValue);
        }
    } else if (aNamespaceID == kNameSpaceID_None &&
               aValue &&
               mNodeInfo->Equals(nsGkAtoms::window) &&
               aName == nsGkAtoms::chromemargin) {
      nsAttrValue attrValue;
      // Make sure the margin format is valid first
      if (!attrValue.ParseIntMarginValue(aValue->String())) {
        return NS_ERROR_INVALID_ARG;
      }
    } else if (aNamespaceID == kNameSpaceID_None &&
               aName == nsGkAtoms::usercontextid) {
        nsAutoString oldValue;
        bool hasAttribute = GetAttr(kNameSpaceID_None, nsGkAtoms::usercontextid, oldValue);
        if (hasAttribute && (!aValue || !aValue->String().Equals(oldValue))) {
          MOZ_ASSERT(false, "Changing usercontextid is not allowed.");
          return NS_ERROR_INVALID_ARG;
        }
    }

    return nsStyledElement::BeforeSetAttr(aNamespaceID, aName,
                                          aValue, aNotify);
}

nsresult
nsXULElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                           const nsAttrValue* aValue,
                           const nsAttrValue* aOldValue,
                           nsIPrincipal* aSubjectPrincipal,
                           bool aNotify)
{
    if (aNamespaceID == kNameSpaceID_None) {
        if (aValue) {
            // Add popup and event listeners. We can't call AddListenerFor since
            // the attribute isn't set yet.
            MaybeAddPopupListener(aName);
            if (nsContentUtils::IsEventAttributeName(aName, EventNameType_XUL)) {
                if (aValue->Type() == nsAttrValue::eString) {
                    SetEventHandler(aName, aValue->GetStringValue(), true);
                } else {
                    nsAutoString body;
                    aValue->ToString(body);
                    SetEventHandler(aName, body, true);
                }
            }

            nsIDocument* document = GetUncomposedDoc();

            // Hide chrome if needed
            if (mNodeInfo->Equals(nsGkAtoms::window)) {
                if (aName == nsGkAtoms::hidechrome) {
                    HideWindowChrome(
                      aValue->Equals(NS_LITERAL_STRING("true"), eCaseMatters));
                } else if (aName == nsGkAtoms::chromemargin) {
                    SetChromeMargins(aValue);
                } else if (aName == nsGkAtoms::windowtype &&
                           document && document->GetRootElement() == this) {
                    MaybeUpdatePrivateLifetime();
                }
            }
            // title and drawintitlebar are settable on
            // any root node (windows, dialogs, etc)
            if (document && document->GetRootElement() == this) {
                if (aName == nsGkAtoms::title) {
                    document->NotifyPossibleTitleChange(false);
                } else if (aName == nsGkAtoms::drawintitlebar) {
                    SetDrawsInTitlebar(
                        aValue->Equals(NS_LITERAL_STRING("true"), eCaseMatters));
                } else if (aName == nsGkAtoms::drawtitle) {
                    SetDrawsTitle(
                        aValue->Equals(NS_LITERAL_STRING("true"), eCaseMatters));
                } else if (aName == nsGkAtoms::localedir) {
                    // if the localedir changed on the root element, reset the document direction
                    if (document->IsXULDocument()) {
                        document->AsXULDocument()->ResetDocumentDirection();
                    }
                } else if (aName == nsGkAtoms::lwtheme ||
                         aName == nsGkAtoms::lwthemetextcolor) {
                    // if the lwtheme changed, make sure to reset the document lwtheme cache
                    if (document->IsXULDocument()) {
                        document->AsXULDocument()->ResetDocumentLWTheme();
                        UpdateBrightTitlebarForeground(document);
                    }
                } else if (aName == nsGkAtoms::brighttitlebarforeground) {
                    UpdateBrightTitlebarForeground(document);
                }
            }

            if (aName == nsGkAtoms::src && document) {
                LoadSrc();
            }
        } else {
            if (mNodeInfo->Equals(nsGkAtoms::window)) {
                if (aName == nsGkAtoms::hidechrome) {
                    HideWindowChrome(false);
                } else if (aName == nsGkAtoms::chromemargin) {
                    ResetChromeMargins();
                }
            }

            nsIDocument* doc = GetUncomposedDoc();
            if (doc && doc->GetRootElement() == this) {
                if (aName == nsGkAtoms::localedir) {
                    // if the localedir changed on the root element, reset the document direction
                    if (doc->IsXULDocument()) {
                        doc->AsXULDocument()->ResetDocumentDirection();
                    }
                } else if ((aName == nsGkAtoms::lwtheme ||
                            aName == nsGkAtoms::lwthemetextcolor)) {
                    // if the lwtheme changed, make sure to restyle appropriately
                    if (doc->IsXULDocument()) {
                        doc->AsXULDocument()->ResetDocumentLWTheme();
                        UpdateBrightTitlebarForeground(doc);
                    }
                } else if (aName == nsGkAtoms::brighttitlebarforeground) {
                    UpdateBrightTitlebarForeground(doc);
                } else if (aName == nsGkAtoms::drawintitlebar) {
                    SetDrawsInTitlebar(false);
                } else if (aName == nsGkAtoms::drawtitle) {
                    SetDrawsTitle(false);
                }
            }
        }

        if (aName == nsGkAtoms::tooltip || aName == nsGkAtoms::tooltiptext) {
            if (!!aValue != !!aOldValue &&
                IsInComposedDoc() &&
                !NodeInfo()->Equals(nsGkAtoms::treechildren)) {
                if (aValue) {
                    AddTooltipSupport();
                } else {
                    RemoveTooltipSupport();
                }
            }
        }
        // XXX need to check if they're changing an event handler: if
        // so, then we need to unhook the old one.  Or something.
    }

    return nsStyledElement::AfterSetAttr(aNamespaceID, aName,
                                         aValue, aOldValue, aSubjectPrincipal, aNotify);
}

void
nsXULElement::AddTooltipSupport()
{
  nsXULTooltipListener* listener = nsXULTooltipListener::GetInstance();
  if (!listener) {
    return;
  }

  listener->AddTooltipSupport(this);
}

void
nsXULElement::RemoveTooltipSupport()
{
  nsXULTooltipListener* listener = nsXULTooltipListener::GetInstance();
  if (!listener) {
    return;
  }

  listener->RemoveTooltipSupport(this);
}

bool
nsXULElement::ParseAttribute(int32_t aNamespaceID,
                             nsAtom* aAttribute,
                             const nsAString& aValue,
                             nsIPrincipal* aMaybeScriptedPrincipal,
                             nsAttrValue& aResult)
{
    // Parse into a nsAttrValue
    if (!nsStyledElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                         aMaybeScriptedPrincipal, aResult)) {
        // Fall back to parsing as atom for short values
        aResult.ParseStringOrAtom(aValue);
    }

    return true;
}

void
nsXULElement::RemoveBroadcaster(const nsAString & broadcasterId)
{
    nsIDocument* doc = OwnerDoc();
    if (!doc->IsXULDocument()) {
      return;
    }
    if (Element* broadcaster = doc->GetElementById(broadcasterId)) {
        doc->AsXULDocument()->RemoveBroadcastListenerFor(
           *broadcaster, *this, NS_LITERAL_STRING("*"));
    }
}

void
nsXULElement::DestroyContent()
{
    nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
    if (slots) {
        slots->mControllers = nullptr;
        RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
        if (frameLoader) {
            frameLoader->Destroy();
        }
        slots->mFrameLoaderOrOpener = nullptr;
    }

    nsStyledElement::DestroyContent();
}

#ifdef DEBUG
void
nsXULElement::List(FILE* out, int32_t aIndent) const
{
    nsCString prefix("XUL");
    if (HasSlots()) {
      prefix.Append('*');
    }
    prefix.Append(' ');

    nsStyledElement::List(out, aIndent, prefix);
}
#endif

bool
nsXULElement::IsEventStoppedFromAnonymousScrollbar(EventMessage aMessage)
{
    return (IsRootOfNativeAnonymousSubtree() &&
            IsAnyOfXULElements(nsGkAtoms::scrollbar, nsGkAtoms::scrollcorner) &&
            (aMessage == eMouseClick || aMessage == eMouseDoubleClick ||
             aMessage == eXULCommand || aMessage == eContextMenu ||
             aMessage == eDragStart  || aMessage == eMouseAuxClick));
}

nsresult
nsXULElement::DispatchXULCommand(const EventChainVisitor& aVisitor,
                                 nsAutoString& aCommand)
{
    // XXX sXBL/XBL2 issue! Owner or current document?
    nsCOMPtr<nsIDocument> doc = GetUncomposedDoc();
    NS_ENSURE_STATE(doc);
    RefPtr<Element> commandElt = doc->GetElementById(aCommand);
    if (commandElt) {
        // Create a new command event to dispatch to the element
        // pointed to by the command attribute. The new event's
        // sourceEvent will be the original command event that we're
        // handling.
        RefPtr<Event> event = aVisitor.mDOMEvent;
        uint16_t inputSource = MouseEventBinding::MOZ_SOURCE_UNKNOWN;
        while (event) {
            NS_ENSURE_STATE(event->GetOriginalTarget() != commandElt);
            RefPtr<XULCommandEvent> commandEvent = event->AsXULCommandEvent();
            if (commandEvent) {
                event = commandEvent->GetSourceEvent();
                inputSource = commandEvent->InputSource();
            } else {
                event = nullptr;
            }
        }
        WidgetInputEvent* orig = aVisitor.mEvent->AsInputEvent();
        nsContentUtils::DispatchXULCommand(
          commandElt,
          orig->IsTrusted(),
          aVisitor.mDOMEvent,
          nullptr,
          orig->IsControl(),
          orig->IsAlt(),
          orig->IsShift(),
          orig->IsMeta(),
          inputSource);
    } else {
        NS_WARNING("A XUL element is attached to a command that doesn't exist!\n");
    }
    return NS_OK;
}

void
nsXULElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
    aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
    if (IsEventStoppedFromAnonymousScrollbar(aVisitor.mEvent->mMessage)) {
        // Don't propagate these events from native anonymous scrollbar.
        aVisitor.mCanHandle = true;
        aVisitor.SetParentTarget(nullptr, false);
        return;
    }
    if (aVisitor.mEvent->mMessage == eXULCommand &&
        aVisitor.mEvent->mClass == eInputEventClass &&
        aVisitor.mEvent->mOriginalTarget == static_cast<nsIContent*>(this) &&
        !IsXULElement(nsGkAtoms::command)) {
        // Check that we really have an xul command event. That will be handled
        // in a special way.
        // See if we have a command elt.  If so, we execute on the command
        // instead of on our content element.
        nsAutoString command;
        if (aVisitor.mDOMEvent &&
            aVisitor.mDOMEvent->AsXULCommandEvent() &&
            GetAttr(kNameSpaceID_None, nsGkAtoms::command, command) &&
            !command.IsEmpty()) {
            // Stop building the event target chain for the original event.
            // We don't want it to propagate to any DOM nodes.
            aVisitor.mCanHandle = false;
            aVisitor.mAutomaticChromeDispatch = false;
            // Dispatch XUL command in PreHandleEvent to prevent it breaks event
            // target chain creation
            aVisitor.mWantsPreHandleEvent = true;
            aVisitor.mItemFlags |= NS_DISPATCH_XUL_COMMAND;
            return;
        }
    }

    nsStyledElement::GetEventTargetParent(aVisitor);
}

nsresult
nsXULElement::PreHandleEvent(EventChainVisitor& aVisitor)
{
    if (aVisitor.mItemFlags & NS_DISPATCH_XUL_COMMAND) {
        nsAutoString command;
        GetAttr(kNameSpaceID_None, nsGkAtoms::command, command);
        MOZ_ASSERT(!command.IsEmpty());
        return DispatchXULCommand(aVisitor, command);
    }
    return nsStyledElement::PreHandleEvent(aVisitor);
}

//----------------------------------------------------------------------
// Implementation methods


nsChangeHint
nsXULElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                     int32_t aModType) const
{
    nsChangeHint retval(nsChangeHint(0));

    if (aAttribute == nsGkAtoms::value &&
        (aModType == MutationEventBinding::REMOVAL ||
         aModType == MutationEventBinding::ADDITION)) {
      if (IsAnyOfXULElements(nsGkAtoms::label, nsGkAtoms::description))
        // Label and description dynamically morph between a normal
        // block and a cropping single-line XUL text frame.  If the
        // value attribute is being added or removed, then we need to
        // return a hint of frame change.  (See bugzilla bug 95475 for
        // details.)
        retval = nsChangeHint_ReconstructFrame;
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
nsXULElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
    return false;
}

nsIControllers*
nsXULElement::GetControllers(ErrorResult& rv)
{
    if (! Controllers()) {
        nsExtendedDOMSlots* slots = ExtendedDOMSlots();

        rv = NS_NewXULControllers(nullptr, NS_GET_IID(nsIControllers),
                                  reinterpret_cast<void**>(&slots->mControllers));

        NS_ASSERTION(!rv.Failed(), "unable to create a controllers");
        if (rv.Failed()) {
            return nullptr;
        }
    }

    return Controllers();
}

already_AddRefed<BoxObject>
nsXULElement::GetBoxObject(ErrorResult& rv)
{
    // XXX sXBL/XBL2 issue! Owner or current document?
    return OwnerDoc()->GetBoxObjectFor(this, rv);
}

void
nsXULElement::LoadSrc()
{
    // Allow frame loader only on objects for which a container box object
    // can be obtained.
    if (!IsAnyOfXULElements(nsGkAtoms::browser, nsGkAtoms::editor,
                            nsGkAtoms::iframe)) {
        return;
    }
    if (!IsInUncomposedDoc() ||
        !OwnerDoc()->GetRootElement() ||
        OwnerDoc()->GetRootElement()->
            NodeInfo()->Equals(nsGkAtoms::overlay, kNameSpaceID_XUL)) {
        return;
    }
    RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
    if (!frameLoader) {
        // Check if we have an opener we need to be setting
        nsExtendedDOMSlots* slots = ExtendedDOMSlots();
        nsCOMPtr<nsPIDOMWindowOuter> opener = do_QueryInterface(slots->mFrameLoaderOrOpener);
        if (!opener) {
            // If we are a primary xul-browser, we want to take the opener property!
            nsCOMPtr<nsPIDOMWindowOuter> window = OwnerDoc()->GetWindow();
            if (AttrValueIs(kNameSpaceID_None, nsGkAtoms::primary,
                            nsGkAtoms::_true, eIgnoreCase) && window) {
                opener = window->TakeOpenerForInitialContentBrowser();
            }
        }

        // false as the last parameter so that xul:iframe/browser/editor
        // session history handling works like dynamic html:iframes.
        // Usually xul elements are used in chrome, which doesn't have
        // session history at all.
        frameLoader = nsFrameLoader::Create(this, opener, false);
        slots->mFrameLoaderOrOpener = ToSupports(frameLoader);
        if (NS_WARN_IF(!frameLoader)) {
            return;
        }

        (new AsyncEventDispatcher(this,
                                  NS_LITERAL_STRING("XULFrameLoaderCreated"),
                                  /* aBubbles */ true))->RunDOMEventWhenSafe();
    }

    frameLoader->LoadFrame(false);
}

already_AddRefed<nsFrameLoader>
nsXULElement::GetFrameLoader()
{
    nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
    if (!slots)
        return nullptr;

    RefPtr<nsFrameLoader> loader = do_QueryObject(slots->mFrameLoaderOrOpener);
    return loader.forget();
}

void
nsXULElement::PresetOpenerWindow(mozIDOMWindowProxy* aWindow, ErrorResult& aRv)
{
    nsExtendedDOMSlots* slots = ExtendedDOMSlots();
    MOZ_ASSERT(!slots->mFrameLoaderOrOpener, "A frameLoader or opener is present when calling PresetOpenerWindow");

    slots->mFrameLoaderOrOpener = aWindow;
}

void
nsXULElement::InternalSetFrameLoader(nsFrameLoader* aNewFrameLoader)
{
    nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
    MOZ_ASSERT(slots);

    slots->mFrameLoaderOrOpener = ToSupports(aNewFrameLoader);
}

void
nsXULElement::SwapFrameLoaders(HTMLIFrameElement& aOtherLoaderOwner,
                               ErrorResult& rv)
{
    if (!GetExistingDOMSlots()) {
        rv.Throw(NS_ERROR_NOT_IMPLEMENTED);
        return;
    }

    nsCOMPtr<nsIFrameLoaderOwner> flo = do_QueryInterface(ToSupports(this));
    aOtherLoaderOwner.SwapFrameLoaders(flo, rv);
}

void
nsXULElement::SwapFrameLoaders(nsXULElement& aOtherLoaderOwner,
                               ErrorResult& rv)
{
    if (&aOtherLoaderOwner == this) {
        // nothing to do
        return;
    }

    if (!GetExistingDOMSlots()) {
        rv.Throw(NS_ERROR_NOT_IMPLEMENTED);
        return;
    }

    nsCOMPtr<nsIFrameLoaderOwner> flo = do_QueryInterface(ToSupports(this));
    aOtherLoaderOwner.SwapFrameLoaders(flo, rv);
}

void
nsXULElement::SwapFrameLoaders(nsIFrameLoaderOwner* aOtherLoaderOwner,
                               mozilla::ErrorResult& rv)
{
    if (!GetExistingDOMSlots()) {
        rv.Throw(NS_ERROR_NOT_IMPLEMENTED);
        return;
    }

    RefPtr<nsFrameLoader> loader = GetFrameLoader();
    RefPtr<nsFrameLoader> otherLoader = aOtherLoaderOwner->GetFrameLoader();
    if (!loader || !otherLoader) {
        rv.Throw(NS_ERROR_NOT_IMPLEMENTED);
        return;
    }

    nsCOMPtr<nsIFrameLoaderOwner> flo = do_QueryInterface(ToSupports(this));
    rv = loader->SwapWithOtherLoader(otherLoader, flo, aOtherLoaderOwner);
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

void
nsXULElement::Click(CallerType aCallerType)
{
  ClickWithInputSource(MouseEventBinding::MOZ_SOURCE_UNKNOWN,
                       aCallerType == CallerType::System);
}

void
nsXULElement::ClickWithInputSource(uint16_t aInputSource, bool aIsTrustedEvent)
{
    if (BoolAttrIsTrue(nsGkAtoms::disabled))
        return;

    nsCOMPtr<nsIDocument> doc = GetComposedDoc(); // Strong just in case
    if (doc) {
        RefPtr<nsPresContext> context = doc->GetPresContext();
        if (context) {
            // strong ref to PresContext so events don't destroy it

            WidgetMouseEvent eventDown(aIsTrustedEvent, eMouseDown,
                                       nullptr, WidgetMouseEvent::eReal);
            WidgetMouseEvent eventUp(aIsTrustedEvent, eMouseUp,
                                     nullptr, WidgetMouseEvent::eReal);
            WidgetMouseEvent eventClick(aIsTrustedEvent, eMouseClick, nullptr,
                                        WidgetMouseEvent::eReal);
            eventDown.inputSource = eventUp.inputSource = eventClick.inputSource
                                  = aInputSource;

            // send mouse down
            nsEventStatus status = nsEventStatus_eIgnore;
            EventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                      context, &eventDown,  nullptr, &status);

            // send mouse up
            status = nsEventStatus_eIgnore;  // reset status
            EventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                      context, &eventUp, nullptr, &status);

            // send mouse click
            status = nsEventStatus_eIgnore;  // reset status
            EventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                      context, &eventClick, nullptr, &status);

            // If the click has been prevented, lets skip the command call
            // this is how a physical click works
            if (status == nsEventStatus_eConsumeNoDefault) {
                return;
            }
        }
    }

    // oncommand is fired when an element is clicked...
    DoCommand();
}

void
nsXULElement::DoCommand()
{
    nsCOMPtr<nsIDocument> doc = GetComposedDoc(); // strong just in case
    if (doc) {
        nsContentUtils::DispatchXULCommand(this, true);
    }
}

bool
nsXULElement::IsNodeOfType(uint32_t aFlags) const
{
    return false;
}

nsresult
nsXULElement::AddPopupListener(nsAtom* aName)
{
    // Add a popup listener to the element
    bool isContext = (aName == nsGkAtoms::context ||
                        aName == nsGkAtoms::contextmenu);
    uint32_t listenerFlag = isContext ?
                            XUL_ELEMENT_HAS_CONTENTMENU_LISTENER :
                            XUL_ELEMENT_HAS_POPUP_LISTENER;

    if (HasFlag(listenerFlag)) {
        return NS_OK;
    }

    nsCOMPtr<nsIDOMEventListener> listener =
      new nsXULPopupListener(this, isContext);

    // Add the popup as a listener on this element.
    EventListenerManager* manager = GetOrCreateListenerManager();
    SetFlags(listenerFlag);

    if (isContext) {
      manager->AddEventListenerByType(listener,
                                      NS_LITERAL_STRING("contextmenu"),
                                      TrustedEventsAtSystemGroupBubble());
    } else {
      manager->AddEventListenerByType(listener,
                                      NS_LITERAL_STRING("mousedown"),
                                      TrustedEventsAtSystemGroupBubble());
    }
    return NS_OK;
}

EventStates
nsXULElement::IntrinsicState() const
{
    EventStates state = nsStyledElement::IntrinsicState();

    if (IsReadWriteTextElement()) {
        state |= NS_EVENT_STATE_MOZ_READWRITE;
        state &= ~NS_EVENT_STATE_MOZ_READONLY;
    }

    return state;
}

//----------------------------------------------------------------------

nsresult
nsXULElement::MakeHeavyweight(nsXULPrototypeElement* aPrototype)
{
    if (!aPrototype) {
        return NS_OK;
    }

    uint32_t i;
    nsresult rv;
    for (i = 0; i < aPrototype->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &aPrototype->mAttributes[i];
        nsAttrValue attrValue;

        // Style rules need to be cloned.
        if (protoattr->mValue.Type() == nsAttrValue::eCSSDeclaration) {
            DeclarationBlock* decl = protoattr->mValue.GetCSSDeclarationValue();
            RefPtr<DeclarationBlock> declClone = decl->Clone();

            nsString stringValue;
            protoattr->mValue.ToString(stringValue);

            attrValue.SetTo(declClone.forget(), &stringValue);
        } else {
            attrValue.SetTo(protoattr->mValue);
        }

        bool oldValueSet;
        // XXX we might wanna have a SetAndTakeAttr that takes an nsAttrName
        if (protoattr->mName.IsAtom()) {
            rv = mAttrsAndChildren.SetAndSwapAttr(protoattr->mName.Atom(),
                                                  attrValue, &oldValueSet);
        } else {
            rv = mAttrsAndChildren.SetAndSwapAttr(protoattr->mName.NodeInfo(),
                                                  attrValue, &oldValueSet);
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}

nsresult
nsXULElement::HideWindowChrome(bool aShouldHide)
{
    nsIDocument* doc = GetUncomposedDoc();
    if (!doc || doc->GetRootElement() != this)
      return NS_ERROR_UNEXPECTED;

    // only top level chrome documents can hide the window chrome
    if (!doc->IsRootDisplayDocument())
      return NS_OK;

    nsPresContext* presContext = doc->GetPresContext();

    if (presContext && presContext->IsChrome()) {
        nsIFrame* frame = GetPrimaryFrame();

        if (frame) {
            nsView* view = frame->GetClosestView();

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
    nsIDocument* doc = GetComposedDoc();

    // only top level chrome documents can set the titlebar color
    if (doc && doc->IsRootDisplayDocument()) {
        nsCOMPtr<nsISupports> container = doc->GetContainer();
        nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
        if (baseWindow) {
            nsCOMPtr<nsIWidget> mainWidget;
            baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
            return mainWidget;
        }
    }
    return nullptr;
}

class SetDrawInTitleBarEvent : public Runnable
{
public:
  SetDrawInTitleBarEvent(nsIWidget* aWidget, bool aState)
    : mozilla::Runnable("SetDrawInTitleBarEvent")
    , mWidget(aWidget)
    , mState(aState)
  {}

  NS_IMETHOD Run() override {
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

void
nsXULElement::SetDrawsTitle(bool aState)
{
    nsIWidget* mainWidget = GetWindowWidget();
    if (mainWidget) {
        // We can do this synchronously because SetDrawsTitle doesn't have any
        // synchronous effects apart from a harmless invalidation.
        mainWidget->SetDrawsTitle(aState);
    }
}

void
nsXULElement::UpdateBrightTitlebarForeground(nsIDocument* aDoc)
{
    nsIWidget* mainWidget = GetWindowWidget();
    if (mainWidget) {
        // We can do this synchronously because SetBrightTitlebarForeground doesn't have any
        // synchronous effects apart from a harmless invalidation.
        mainWidget->SetUseBrightTitlebarForeground(
          aDoc->GetDocumentLWTheme() == nsIDocument::Doc_Theme_Bright ||
          aDoc->GetRootElement()->AttrValueIs(kNameSpaceID_None,
                                              nsGkAtoms::brighttitlebarforeground,
                                              NS_LITERAL_STRING("true"),
                                              eCaseMatters));
    }
}

class MarginSetter : public Runnable
{
public:
  explicit MarginSetter(nsIWidget* aWidget)
    : mozilla::Runnable("MarginSetter")
    , mWidget(aWidget)
    , mMargin(-1, -1, -1, -1)
  {
  }
  MarginSetter(nsIWidget* aWidget, const LayoutDeviceIntMargin& aMargin)
    : mozilla::Runnable("MarginSetter")
    , mWidget(aWidget)
    , mMargin(aMargin)
  {
  }

  NS_IMETHOD Run() override
  {
    // SetNonClientMargins can dispatch native events, hence doing
    // it off a script runner.
    mWidget->SetNonClientMargins(mMargin);
    return NS_OK;
    }

private:
    nsCOMPtr<nsIWidget> mWidget;
    LayoutDeviceIntMargin mMargin;
};

void
nsXULElement::SetChromeMargins(const nsAttrValue* aValue)
{
    if (!aValue)
        return;

    nsIWidget* mainWidget = GetWindowWidget();
    if (!mainWidget)
        return;

    // top, right, bottom, left - see nsAttrValue
    nsIntMargin margins;
    bool gotMargins = false;

    if (aValue->Type() == nsAttrValue::eIntMarginValue) {
        gotMargins = aValue->GetIntMarginValue(margins);
    } else {
        nsAutoString tmp;
        aValue->ToString(tmp);
        gotMargins = nsContentUtils::ParseIntMarginValue(tmp, margins);
    }
    if (gotMargins) {
        nsContentUtils::AddScriptRunner(
            new MarginSetter(
                mainWidget, LayoutDeviceIntMargin::FromUnknownMargin(margins)));
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
nsXULElement::BoolAttrIsTrue(nsAtom* aName) const
{
    const nsAttrValue* attr =
        GetAttrInfo(kNameSpaceID_None, aName).mValue;

    return attr && attr->Type() == nsAttrValue::eAtom &&
           attr->GetAtomValue() == nsGkAtoms::_true;
}

void
nsXULElement::RecompileScriptEventListeners()
{
    int32_t i, count = mAttrsAndChildren.AttrCount();
    for (i = 0; i < count; ++i) {
        const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);

        // Eventlistenener-attributes are always in the null namespace
        if (!name->IsAtom()) {
            continue;
        }

        nsAtom *attr = name->Atom();
        if (!nsContentUtils::IsEventAttributeName(attr, EventNameType_XUL)) {
            continue;
        }

        nsAutoString value;
        GetAttr(kNameSpaceID_None, attr, value);
        SetEventHandler(attr, value, true);
    }
}

bool
nsXULElement::IsEventAttributeNameInternal(nsAtom *aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_XUL);
}

JSObject*
nsXULElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
    return dom::XULElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULPrototypeNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULPrototypeNode)
    if (tmp->mType == nsXULPrototypeNode::eType_Element) {
        static_cast<nsXULPrototypeElement*>(tmp)->Unlink();
    } else if (tmp->mType == nsXULPrototypeNode::eType_Script) {
        static_cast<nsXULPrototypeScript*>(tmp)->UnlinkJSObjects();
    }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULPrototypeNode)
    if (tmp->mType == nsXULPrototypeNode::eType_Element) {
        nsXULPrototypeElement *elem =
            static_cast<nsXULPrototypeElement*>(tmp);
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mNodeInfo");
        cb.NoteNativeChild(elem->mNodeInfo,
                           NS_CYCLE_COLLECTION_PARTICIPANT(NodeInfo));
        uint32_t i;
        for (i = 0; i < elem->mNumAttributes; ++i) {
            const nsAttrName& name = elem->mAttributes[i].mName;
            if (!name.IsAtom()) {
                NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                    "mAttributes[i].mName.NodeInfo()");
                cb.NoteNativeChild(name.NodeInfo(),
                                   NS_CYCLE_COLLECTION_PARTICIPANT(NodeInfo));
            }
        }
        ImplCycleCollectionTraverse(cb, elem->mChildren, "mChildren");
    }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsXULPrototypeNode)
    if (tmp->mType == nsXULPrototypeNode::eType_Script) {
        nsXULPrototypeScript *script =
            static_cast<nsXULPrototypeScript*>(tmp);
        script->Trace(aCallbacks, aClosure);
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
                                 nsXULPrototypeDocument* aProtoDoc,
                                 const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    // Write Node Info
    int32_t index = aNodeInfos->IndexOf(mNodeInfo);
    NS_ASSERTION(index >= 0, "unknown mozilla::dom::NodeInfo index");
    nsresult tmp = aStream->Write32(index);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    // Write Attributes
    tmp = aStream->Write32(mNumAttributes);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    nsAutoString attributeValue;
    uint32_t i;
    for (i = 0; i < mNumAttributes; ++i) {
        RefPtr<mozilla::dom::NodeInfo> ni;
        if (mAttributes[i].mName.IsAtom()) {
            ni = mNodeInfo->NodeInfoManager()->
                GetNodeInfo(mAttributes[i].mName.Atom(), nullptr,
                            kNameSpaceID_None, nsINode::ATTRIBUTE_NODE);
            NS_ASSERTION(ni, "the nodeinfo should already exist");
        } else {
            ni = mAttributes[i].mName.NodeInfo();
        }

        index = aNodeInfos->IndexOf(ni);
        NS_ASSERTION(index >= 0, "unknown mozilla::dom::NodeInfo index");
        tmp = aStream->Write32(index);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }

        mAttributes[i].mValue.ToString(attributeValue);
        tmp = aStream->WriteWStringZ(attributeValue.get());
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
    }

    // Now write children
    tmp = aStream->Write32(uint32_t(mChildren.Length()));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    for (i = 0; i < mChildren.Length(); i++) {
        nsXULPrototypeNode* child = mChildren[i].get();
        switch (child->mType) {
        case eType_Element:
        case eType_Text:
        case eType_PI:
            tmp = child->Serialize(aStream, aProtoDoc, aNodeInfos);
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
            break;
        case eType_Script:
            tmp = aStream->Write32(child->mType);
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
            nsXULPrototypeScript* script = static_cast<nsXULPrototypeScript*>(child);

            tmp = aStream->Write8(script->mOutOfLine);
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
            if (! script->mOutOfLine) {
                tmp = script->Serialize(aStream, aProtoDoc, aNodeInfos);
                if (NS_FAILED(tmp)) {
                  rv = tmp;
                }
            } else {
                tmp = aStream->WriteCompoundObject(script->mSrcURI,
                                                   NS_GET_IID(nsIURI),
                                                   true);
                if (NS_FAILED(tmp)) {
                  rv = tmp;
                }

                if (script->HasScriptObject()) {
                    // This may return NS_OK without muxing script->mSrcURI's
                    // data into the cache file, in the case where that
                    // muxed document is already there (written by a prior
                    // session, or by an earlier cache episode during this
                    // session).
                    tmp = script->SerializeOutOfLine(aStream, aProtoDoc);
                    if (NS_FAILED(tmp)) {
                      rv = tmp;
                    }
                }
            }
            break;
        }
    }

    return rv;
}

nsresult
nsXULPrototypeElement::Deserialize(nsIObjectInputStream* aStream,
                                   nsXULPrototypeDocument* aProtoDoc,
                                   nsIURI* aDocumentURI,
                                   const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    MOZ_ASSERT(aNodeInfos, "missing nodeinfo array");

    // Read Node Info
    uint32_t number = 0;
    nsresult rv = aStream->Read32(&number);
    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
    mNodeInfo = aNodeInfos->SafeElementAt(number, nullptr);
    if (!mNodeInfo) {
        return NS_ERROR_UNEXPECTED;
    }

    // Read Attributes
    rv = aStream->Read32(&number);
    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
    mNumAttributes = int32_t(number);

    if (mNumAttributes > 0) {
        mAttributes = new (fallible) nsXULPrototypeAttribute[mNumAttributes];
        if (!mAttributes) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        nsAutoString attributeValue;
        for (uint32_t i = 0; i < mNumAttributes; ++i) {
            rv = aStream->Read32(&number);
            if (NS_WARN_IF(NS_FAILED(rv))) return rv;
            mozilla::dom::NodeInfo* ni = aNodeInfos->SafeElementAt(number, nullptr);
            if (!ni) {
                return NS_ERROR_UNEXPECTED;
            }

            mAttributes[i].mName.SetTo(ni);

            rv = aStream->ReadString(attributeValue);
            if (NS_WARN_IF(NS_FAILED(rv))) return rv;
            rv = SetAttrAt(i, attributeValue, aDocumentURI);
            if (NS_WARN_IF(NS_FAILED(rv))) return rv;
        }
    }

    rv = aStream->Read32(&number);
    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
    uint32_t numChildren = int32_t(number);

    if (numChildren > 0) {
        if (!mChildren.SetCapacity(numChildren, fallible)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        for (uint32_t i = 0; i < numChildren; i++) {
            rv = aStream->Read32(&number);
            if (NS_WARN_IF(NS_FAILED(rv))) return rv;
            Type childType = (Type)number;

            RefPtr<nsXULPrototypeNode> child;

            switch (childType) {
            case eType_Element:
                child = new nsXULPrototypeElement();
                rv = child->Deserialize(aStream, aProtoDoc, aDocumentURI,
                                        aNodeInfos);
                if (NS_WARN_IF(NS_FAILED(rv))) return rv;
                break;
            case eType_Text:
                child = new nsXULPrototypeText();
                rv = child->Deserialize(aStream, aProtoDoc, aDocumentURI,
                                        aNodeInfos);
                if (NS_WARN_IF(NS_FAILED(rv))) return rv;
                break;
            case eType_PI:
                child = new nsXULPrototypePI();
                rv = child->Deserialize(aStream, aProtoDoc, aDocumentURI,
                                        aNodeInfos);
                if (NS_WARN_IF(NS_FAILED(rv))) return rv;
                break;
            case eType_Script: {
                // language version/options obtained during deserialization.
                RefPtr<nsXULPrototypeScript> script = new nsXULPrototypeScript(0);

                rv = aStream->ReadBoolean(&script->mOutOfLine);
                if (NS_WARN_IF(NS_FAILED(rv))) return rv;
                if (!script->mOutOfLine) {
                    rv = script->Deserialize(aStream, aProtoDoc, aDocumentURI,
                                             aNodeInfos);
                    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
                } else {
                    nsCOMPtr<nsISupports> supports;
                    rv = aStream->ReadObject(true, getter_AddRefs(supports));
                    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
                    script->mSrcURI = do_QueryInterface(supports);

                    rv = script->DeserializeOutOfLine(aStream, aProtoDoc);
                    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
                }

                child = script.forget();
                break;
            }
            default:
                MOZ_ASSERT(false, "Unexpected child type!");
                return NS_ERROR_UNEXPECTED;
            }

            MOZ_ASSERT(child, "Don't append null to mChildren");
            MOZ_ASSERT(child->mType == childType);
            mChildren.AppendElement(child);

            // Oh dear. Something failed during the deserialization.
            // We don't know what.  But likely consequences of failed
            // deserializations included calls to |AbortCaching| which
            // shuts down the cache and closes our streams.
            // If that happens, next time through this loop, we die a messy
            // death. So, let's just fail now, and propagate that failure
            // upward so that the ChromeProtocolHandler knows it can't use
            // a cached chrome channel for this.
            if (NS_WARN_IF(NS_FAILED(rv)))
                return rv;
        }
    }

    return rv;
}

nsresult
nsXULPrototypeElement::SetAttrAt(uint32_t aPos, const nsAString& aValue,
                                 nsIURI* aDocumentURI)
{
    MOZ_ASSERT(aPos < mNumAttributes, "out-of-bounds");

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
    } else if (mAttributes[aPos].mName.Equals(nsGkAtoms::_class)) {
        mHasClassAttribute = true;
        // Compute the element's class list
        mAttributes[aPos].mValue.ParseAtomArray(aValue);

        return NS_OK;
    } else if (mAttributes[aPos].mName.Equals(nsGkAtoms::style)) {
        mHasStyleAttribute = true;
        // Parse the element's 'style' attribute

        // This is basically duplicating what nsINode::NodePrincipal() does
        nsIPrincipal* principal =
          mNodeInfo->NodeInfoManager()->DocumentPrincipal();
        // XXX Get correct Base URI (need GetBaseURI on *prototype* element)
        // TODO: If we implement Content Security Policy for chrome documents
        // as has been discussed, the CSP should be checked here to see if
        // inline styles are allowed to be applied.
        RefPtr<URLExtraData> data =
          new URLExtraData(aDocumentURI, aDocumentURI, principal);
        RefPtr<DeclarationBlock> declaration =
          ServoDeclarationBlock::FromCssText(
            aValue, data, eCompatibility_FullStandards, nullptr);
        if (declaration) {
            mAttributes[aPos].mValue.SetTo(declaration.forget(), &aValue);

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
    mNumAttributes = 0;
    delete[] mAttributes;
    mAttributes = nullptr;
    mChildren.Clear();
}

void
nsXULPrototypeElement::TraceAllScripts(JSTracer* aTrc)
{
    for (uint32_t i = 0; i < mChildren.Length(); ++i) {
        nsXULPrototypeNode* child = mChildren[i];
        if (child->mType == nsXULPrototypeNode::eType_Element) {
            static_cast<nsXULPrototypeElement*>(child)->TraceAllScripts(aTrc);
        } else if (child->mType == nsXULPrototypeNode::eType_Script) {
            static_cast<nsXULPrototypeScript*>(child)->TraceScriptObject(aTrc);
        }
    }
}

//----------------------------------------------------------------------
//
// nsXULPrototypeScript
//

nsXULPrototypeScript::nsXULPrototypeScript(uint32_t aLineNo)
    : nsXULPrototypeNode(eType_Script),
      mLineNo(aLineNo),
      mSrcLoading(false),
      mOutOfLine(true),
      mSrcLoadWaiters(nullptr),
      mScriptObject(nullptr)
{
}


nsXULPrototypeScript::~nsXULPrototypeScript()
{
    UnlinkJSObjects();
}

nsresult
nsXULPrototypeScript::Serialize(nsIObjectOutputStream* aStream,
                                nsXULPrototypeDocument* aProtoDoc,
                                const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    NS_ENSURE_TRUE(aProtoDoc, NS_ERROR_UNEXPECTED);

    AutoJSAPI jsapi;
    if (!jsapi.Init(xpc::CompilationScope())) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nullptr ||
                 !mScriptObject,
                 "script source still loading when serializing?!");
    if (!mScriptObject)
        return NS_ERROR_FAILURE;

    // Write basic prototype data
    nsresult rv;
    rv = aStream->Write32(mLineNo);
    if (NS_FAILED(rv)) return rv;
    rv = aStream->Write32(0); // See bug 1418294.
    if (NS_FAILED(rv)) return rv;

    JSContext* cx = jsapi.cx();
    JS::Rooted<JSScript*> script(cx, mScriptObject);
    MOZ_ASSERT(xpc::CompilationScope() == JS::CurrentGlobalOrNull(cx));
    return nsContentUtils::XPConnect()->WriteScript(aStream, cx, script);
}

nsresult
nsXULPrototypeScript::SerializeOutOfLine(nsIObjectOutputStream* aStream,
                                         nsXULPrototypeDocument* aProtoDoc)
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

    nsresult tmp = Serialize(oos, aProtoDoc, nullptr);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = cache->FinishOutputStream(mSrcURI);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    if (NS_FAILED(rv))
        cache->AbortCaching();
    return rv;
}


nsresult
nsXULPrototypeScript::Deserialize(nsIObjectInputStream* aStream,
                                  nsXULPrototypeDocument* aProtoDoc,
                                  nsIURI* aDocumentURI,
                                  const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    nsresult rv;
    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nullptr ||
                 !mScriptObject,
                 "prototype script not well-initialized when deserializing?!");

    // Read basic prototype data
    rv = aStream->Read32(&mLineNo);
    if (NS_FAILED(rv)) return rv;
    uint32_t dummy;
    rv = aStream->Read32(&dummy); // See bug 1418294.
    if (NS_FAILED(rv)) return rv;

    AutoJSAPI jsapi;
    if (!jsapi.Init(xpc::CompilationScope())) {
        return NS_ERROR_UNEXPECTED;
    }
    JSContext* cx = jsapi.cx();

    JS::Rooted<JSScript*> newScriptObject(cx);
    rv = nsContentUtils::XPConnect()->ReadScript(aStream, cx,
                                                 newScriptObject.address());
    NS_ENSURE_SUCCESS(rv, rv);
    Set(newScriptObject);
    return NS_OK;
}


nsresult
nsXULPrototypeScript::DeserializeOutOfLine(nsIObjectInputStream* aInput,
                                           nsXULPrototypeDocument* aProtoDoc)
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
            // Note that XULDocument::LoadScript
            // checks the XUL script cache too, in order to handle the
            // serialization case.
            //
            // We need do this only for <script src='strres.js'> and the
            // like, i.e., out-of-line scripts that are included by several
            // different XUL documents stored in the cache file.
            useXULCache = cache->IsEnabled();

            if (useXULCache) {
                JSScript* newScriptObject =
                    cache->GetScript(mSrcURI);
                if (newScriptObject)
                    Set(newScriptObject);
            }
        }

        if (!mScriptObject) {
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
                rv = Deserialize(objectInput, aProtoDoc, nullptr, nullptr);

            if (NS_SUCCEEDED(rv)) {
                if (useXULCache && mSrcURI) {
                    bool isChrome = false;
                    mSrcURI->SchemeIs("chrome", &isChrome);
                    if (isChrome) {
                        JS::Rooted<JSScript*> script(RootingCx(), GetScriptObject());
                        cache->PutScript(mSrcURI, script);
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

class NotifyOffThreadScriptCompletedRunnable : public Runnable
{
    // An array of all outstanding script receivers. All reference counting of
    // these objects happens on the main thread. When we return to the main
    // thread from script compilation we make sure our receiver is still in
    // this array (still alive) before proceeding. This array is cleared during
    // shutdown, potentially before all outstanding script compilations have
    // finished. We do not need to worry about pointer replay here, because
    // a) we should not be starting script compilation after clearing this
    // array and b) in all other cases the receiver will still be alive.
    static StaticAutoPtr<nsTArray<nsCOMPtr<nsIOffThreadScriptReceiver>>> sReceivers;
    static bool sSetupClearOnShutdown;

    nsIOffThreadScriptReceiver* mReceiver;
    JS::OffThreadToken* mToken;

public:
  NotifyOffThreadScriptCompletedRunnable(nsIOffThreadScriptReceiver* aReceiver,
                                         JS::OffThreadToken* aToken)
    : mozilla::Runnable("NotifyOffThreadScriptCompletedRunnable")
    , mReceiver(aReceiver)
    , mToken(aToken)
  {
  }

  static void NoteReceiver(nsIOffThreadScriptReceiver* aReceiver)
  {
    if (!sSetupClearOnShutdown) {
      ClearOnShutdown(&sReceivers);
      sSetupClearOnShutdown = true;
      sReceivers = new nsTArray<nsCOMPtr<nsIOffThreadScriptReceiver>>();
    }

    // If we ever crash here, it's because we tried to lazy compile script
    // too late in shutdown.
    sReceivers->AppendElement(aReceiver);
    }

    NS_DECL_NSIRUNNABLE
};

StaticAutoPtr<nsTArray<nsCOMPtr<nsIOffThreadScriptReceiver>>> NotifyOffThreadScriptCompletedRunnable::sReceivers;
bool NotifyOffThreadScriptCompletedRunnable::sSetupClearOnShutdown = false;

NS_IMETHODIMP
NotifyOffThreadScriptCompletedRunnable::Run()
{
    MOZ_ASSERT(NS_IsMainThread());

    JS::Rooted<JSScript*> script(RootingCx());
    {
        AutoJSAPI jsapi;
        if (!jsapi.Init(xpc::CompilationScope())) {
            // Now what?  I guess we just leak... this should probably never
            // happen.
            return NS_ERROR_UNEXPECTED;
        }
        JSContext* cx = jsapi.cx();
        script = JS::FinishOffThreadScript(cx, mToken);
    }

    if (!sReceivers) {
        // We've already shut down.
        return NS_OK;
    }

    auto index = sReceivers->IndexOf(mReceiver);
    MOZ_RELEASE_ASSERT(index != sReceivers->NoIndex);
    nsCOMPtr<nsIOffThreadScriptReceiver> receiver = (*sReceivers)[index].forget();
    sReceivers->RemoveElementAt(index);

    return receiver->OnScriptCompileComplete(script, script ? NS_OK : NS_ERROR_FAILURE);
}

static void
OffThreadScriptReceiverCallback(JS::OffThreadToken* aToken, void* aCallbackData)
{
    // Be careful not to adjust the refcount on the receiver, as this callback
    // may be invoked off the main thread.
    nsIOffThreadScriptReceiver* aReceiver = static_cast<nsIOffThreadScriptReceiver*>(aCallbackData);
    RefPtr<NotifyOffThreadScriptCompletedRunnable> notify =
        new NotifyOffThreadScriptCompletedRunnable(aReceiver, aToken);
    NS_DispatchToMainThread(notify);
}

nsresult
nsXULPrototypeScript::Compile(JS::SourceBufferHolder& aSrcBuf,
                              nsIURI* aURI, uint32_t aLineNo,
                              nsIDocument* aDocument,
                              nsIOffThreadScriptReceiver *aOffThreadReceiver /* = nullptr */)
{
    // We'll compile the script in the compilation scope.
    AutoJSAPI jsapi;
    if (!jsapi.Init(xpc::CompilationScope())) {
        return NS_ERROR_UNEXPECTED;
    }
    JSContext* cx = jsapi.cx();

    nsresult rv;
    nsAutoCString urlspec;
    nsContentUtils::GetWrapperSafeScriptFilename(aDocument, aURI, urlspec, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Ok, compile it to create a prototype script object!
    JS::CompileOptions options(cx);
    options.setIntroductionType("scriptElement")
           .setFileAndLine(urlspec.get(), aLineNo);
    // If the script was inline, tell the JS parser to save source for
    // Function.prototype.toSource(). If it's out of line, we retrieve the
    // source from the files on demand.
    options.setSourceIsLazy(mOutOfLine);
    JS::Rooted<JSObject*> scope(cx, JS::CurrentGlobalOrNull(cx));
    if (scope) {
      JS::ExposeObjectToActiveJS(scope);
    }

    if (aOffThreadReceiver && JS::CanCompileOffThread(cx, options, aSrcBuf.length())) {
        if (!JS::CompileOffThread(cx, options,
                                  aSrcBuf.get(), aSrcBuf.length(),
                                  OffThreadScriptReceiverCallback,
                                  static_cast<void*>(aOffThreadReceiver))) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        NotifyOffThreadScriptCompletedRunnable::NoteReceiver(aOffThreadReceiver);
    } else {
        JS::Rooted<JSScript*> script(cx);
        if (!JS::Compile(cx, options, aSrcBuf, &script))
            return NS_ERROR_OUT_OF_MEMORY;
        Set(script);
    }
    return NS_OK;
}

nsresult
nsXULPrototypeScript::Compile(const char16_t* aText,
                              int32_t aTextLength,
                              nsIURI* aURI,
                              uint32_t aLineNo,
                              nsIDocument* aDocument,
                              nsIOffThreadScriptReceiver *aOffThreadReceiver /* = nullptr */)
{
  JS::SourceBufferHolder srcBuf(aText, aTextLength,
                                JS::SourceBufferHolder::NoOwnership);
  return Compile(srcBuf, aURI, aLineNo, aDocument, aOffThreadReceiver);
}

void
nsXULPrototypeScript::UnlinkJSObjects()
{
    if (mScriptObject) {
        mScriptObject = nullptr;
        mozilla::DropJSObjects(this);
    }
}

void
nsXULPrototypeScript::Set(JSScript* aObject)
{
    MOZ_ASSERT(!mScriptObject, "Leaking script object.");
    if (!aObject) {
        mScriptObject = nullptr;
        return;
    }

    mScriptObject = aObject;
    mozilla::HoldJSObjects(this);
}

//----------------------------------------------------------------------
//
// nsXULPrototypeText
//

nsresult
nsXULPrototypeText::Serialize(nsIObjectOutputStream* aStream,
                              nsXULPrototypeDocument* aProtoDoc,
                              const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    nsresult tmp = aStream->WriteWStringZ(mValue.get());
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    return rv;
}

nsresult
nsXULPrototypeText::Deserialize(nsIObjectInputStream* aStream,
                                nsXULPrototypeDocument* aProtoDoc,
                                nsIURI* aDocumentURI,
                                const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    nsresult rv = aStream->ReadString(mValue);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
    }
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsXULPrototypePI
//

nsresult
nsXULPrototypePI::Serialize(nsIObjectOutputStream* aStream,
                            nsXULPrototypeDocument* aProtoDoc,
                            const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    nsresult tmp = aStream->WriteWStringZ(mTarget.get());
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = aStream->WriteWStringZ(mData.get());
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    return rv;
}

nsresult
nsXULPrototypePI::Deserialize(nsIObjectInputStream* aStream,
                              nsXULPrototypeDocument* aProtoDoc,
                              nsIURI* aDocumentURI,
                              const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos)
{
    nsresult rv;

    rv = aStream->ReadString(mTarget);
    if (NS_FAILED(rv)) return rv;
    rv = aStream->ReadString(mData);
    if (NS_FAILED(rv)) return rv;

    return rv;
}
