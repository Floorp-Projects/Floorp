/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULElement.h"

#include <new>
#include <utility>
#include "AttrArray.h"
#include "MainThreadUtils.h"
#include "ReferrerInfo.h"
#include "Units.h"
#include "XULFrameElement.h"
#include "XULMenuElement.h"
#include "XULPopupElement.h"
#include "XULTextElement.h"
#include "XULTooltipElement.h"
#include "XULTreeElement.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/experimental/JSStencil.h"
#include "js/OffThreadScriptCompilation.h"
#include "js/SourceText.h"
#include "js/Transcoding.h"
#include "js/Utility.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/FlushType.h"
#include "mozilla/GlobalKeyListener.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/PresShell.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticAnalysisFunctions.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/BorrowedAttrInfo.h"
#include "mozilla/dom/CSSRuleBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/XULBroadcastManager.h"
#include "mozilla/dom/XULCommandEvent.h"
#include "mozilla/dom/XULElementBinding.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/fallible.h"
#include "nsAtom.h"
#include "nsAttrValueInlines.h"
#include "nsAttrValueOrString.h"
#include "nsCaseTreatment.h"
#include "nsChangeHint.h"
#include "nsCOMPtr.h"
#include "nsCompatibility.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIControllers.h"
#include "nsID.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMXULControlElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDocShell.h"
#include "nsIFocusManager.h"
#include "nsIFrame.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIRunnable.h"
#include "nsIScriptContext.h"
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsIXPConnect.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsNodeInfoManager.h"
#include "nsPIDOMWindow.h"
#include "nsPIDOMWindowInlines.h"
#include "nsPresContext.h"
#include "nsQueryFrame.h"
#include "nsString.h"
#include "nsStyledElement.h"
#include "nsThreadUtils.h"
#include "nsXULControllers.h"
#include "nsXULPopupListener.h"
#include "nsXULPopupManager.h"
#include "nsXULPrototypeCache.h"
#include "nsXULTooltipListener.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
uint32_t nsXULPrototypeAttribute::gNumElements;
uint32_t nsXULPrototypeAttribute::gNumAttributes;
uint32_t nsXULPrototypeAttribute::gNumCacheTests;
uint32_t nsXULPrototypeAttribute::gNumCacheHits;
uint32_t nsXULPrototypeAttribute::gNumCacheSets;
uint32_t nsXULPrototypeAttribute::gNumCacheFills;
#endif

#define NS_DISPATCH_XUL_COMMAND (1 << 0)

//----------------------------------------------------------------------
// nsXULElement
//

nsXULElement::nsXULElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsStyledElement(std::move(aNodeInfo)) {
  XUL_PROTOTYPE_ATTRIBUTE_METER(gNumElements);
}

nsXULElement::~nsXULElement() = default;

/* static */
nsXULElement* NS_NewBasicXULElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(std::move(aNodeInfo));
  auto* nim = nodeInfo->NodeInfoManager();
  return new (nim) nsXULElement(nodeInfo.forget());
}

/* static */
nsXULElement* nsXULElement::Construct(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {
  // NOTE: If you add elements here, you probably also want to change
  // mozilla::dom::binding_detail::HTMLConstructor in BindingUtils.cpp to take
  // them into account, otherwise you'll start getting "Illegal constructor"
  // exceptions in chrome code.
  RefPtr<mozilla::dom::NodeInfo> nodeInfo = aNodeInfo;
  if (nodeInfo->Equals(nsGkAtoms::label) ||
      nodeInfo->Equals(nsGkAtoms::description)) {
    auto* nim = nodeInfo->NodeInfoManager();
    return new (nim) XULTextElement(nodeInfo.forget());
  }

  if (nodeInfo->Equals(nsGkAtoms::menupopup) ||
      nodeInfo->Equals(nsGkAtoms::popup) ||
      nodeInfo->Equals(nsGkAtoms::panel)) {
    return NS_NewXULPopupElement(nodeInfo.forget());
  }

  if (nodeInfo->Equals(nsGkAtoms::tooltip)) {
    return NS_NewXULTooltipElement(nodeInfo.forget());
  }

  if (nodeInfo->Equals(nsGkAtoms::iframe) ||
      nodeInfo->Equals(nsGkAtoms::browser) ||
      nodeInfo->Equals(nsGkAtoms::editor)) {
    auto* nim = nodeInfo->NodeInfoManager();
    return new (nim) XULFrameElement(nodeInfo.forget());
  }

  if (nodeInfo->Equals(nsGkAtoms::menu) ||
      nodeInfo->Equals(nsGkAtoms::menulist)) {
    auto* nim = nodeInfo->NodeInfoManager();
    return new (nim) XULMenuElement(nodeInfo.forget());
  }

  if (nodeInfo->Equals(nsGkAtoms::tree)) {
    auto* nim = nodeInfo->NodeInfoManager();
    return new (nim) XULTreeElement(nodeInfo.forget());
  }

  return NS_NewBasicXULElement(nodeInfo.forget());
}

/* static */
already_AddRefed<nsXULElement> nsXULElement::CreateFromPrototype(
    nsXULPrototypeElement* aPrototype, mozilla::dom::NodeInfo* aNodeInfo,
    bool aIsScriptable, bool aIsRoot) {
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  nsCOMPtr<Element> baseElement;
  NS_NewXULElement(getter_AddRefs(baseElement), ni.forget(),
                   dom::FROM_PARSER_NETWORK, aPrototype->mIsAtom);

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
      for (const auto& attribute : aPrototype->mAttributes) {
        element->AddListenerForAttributeIfNeeded(attribute.mName);
      }
    }

    return baseElement.forget().downcast<nsXULElement>();
  }

  return nullptr;
}

nsresult nsXULElement::CreateFromPrototype(nsXULPrototypeElement* aPrototype,
                                           Document* aDocument,
                                           bool aIsScriptable, bool aIsRoot,
                                           Element** aResult) {
  // Create an nsXULElement from a prototype
  MOZ_ASSERT(aPrototype != nullptr, "null ptr");
  if (!aPrototype) return NS_ERROR_NULL_POINTER;

  MOZ_ASSERT(aResult != nullptr, "null ptr");
  if (!aResult) return NS_ERROR_NULL_POINTER;

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  if (aDocument) {
    mozilla::dom::NodeInfo* ni = aPrototype->mNodeInfo;
    nodeInfo = aDocument->NodeInfoManager()->GetNodeInfo(
        ni->NameAtom(), ni->GetPrefixAtom(), ni->NamespaceID(), ELEMENT_NODE);
  } else {
    nodeInfo = aPrototype->mNodeInfo;
  }

  RefPtr<nsXULElement> element =
      CreateFromPrototype(aPrototype, nodeInfo, aIsScriptable, aIsRoot);
  element.forget(aResult);

  return NS_OK;
}

nsresult NS_NewXULElement(Element** aResult,
                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                          FromParser aFromParser, nsAtom* aIsAtom,
                          mozilla::dom::CustomElementDefinition* aDefinition) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo = aNodeInfo;

  MOZ_ASSERT(nodeInfo, "need nodeinfo for non-proto Create");

  NS_ASSERTION(
      nodeInfo->NamespaceEquals(kNameSpaceID_XUL),
      "Trying to create XUL elements that don't have the XUL namespace");

  Document* doc = nodeInfo->GetDocument();
  if (doc && !doc->AllowXULXBL()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return nsContentUtils::NewXULOrHTMLElement(aResult, nodeInfo, aFromParser,
                                             aIsAtom, aDefinition);
}

void NS_TrustedNewXULElement(
    Element** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  MOZ_ASSERT(ni, "need nodeinfo for non-proto Create");

  // Create an nsXULElement with the specified namespace and tag.
  NS_ADDREF(*aResult = nsXULElement::Construct(ni.forget()));
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsXULElement, nsStyledElement)

NS_IMPL_ADDREF_INHERITED(nsXULElement, nsStyledElement)
NS_IMPL_RELEASE_INHERITED(nsXULElement, nsStyledElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsXULElement)
  NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE
NS_INTERFACE_MAP_END_INHERITING(nsStyledElement)

//----------------------------------------------------------------------
// nsINode interface

nsresult nsXULElement::Clone(mozilla::dom::NodeInfo* aNodeInfo,
                             nsINode** aResult) const {
  *aResult = nullptr;

  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  RefPtr<nsXULElement> element = Construct(ni.forget());

  nsresult rv = const_cast<nsXULElement*>(this)->CopyInnerTo(
      element, ReparseAttributes::No);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note that we're _not_ copying mControllers.

  element.forget(aResult);
  return rv;
}

//----------------------------------------------------------------------

EventListenerManager* nsXULElement::GetEventListenerManagerForAttr(
    nsAtom* aAttrName, bool* aDefer) {
  // XXXbz sXBL/XBL2 issue: should we instead use GetComposedDoc()
  // here, override BindToTree for those classes and munge event
  // listeners there?
  Document* doc = OwnerDoc();

  nsPIDOMWindowInner* window;
  Element* root = doc->GetRootElement();
  if ((!root || root == this) && (window = doc->GetInnerWindow())) {
    nsCOMPtr<EventTarget> piTarget = do_QueryInterface(window);

    *aDefer = false;
    return piTarget->GetOrCreateListenerManager();
  }

  return nsStyledElement::GetEventListenerManagerForAttr(aAttrName, aDefer);
}

// returns true if the element is not a list
static bool IsNonList(mozilla::dom::NodeInfo* aNodeInfo) {
  return !aNodeInfo->Equals(nsGkAtoms::tree) &&
         !aNodeInfo->Equals(nsGkAtoms::richlistbox);
}

bool nsXULElement::IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) {
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
   * -moz-user-focus is overridden if a tabindex (even -1) is specified.
   *
   * Specifically, the behaviour for all XUL elements is as follows:
   *  *aTabIndex = -1  no tabindex     Not focusable or tabbable
   *  *aTabIndex = -1  tabindex="-1"   Focusable but not tabbable
   *  *aTabIndex = -1  tabindex=">=0"  Focusable and tabbable
   *  *aTabIndex >= 0  no tabindex     Focusable and tabbable
   *  *aTabIndex >= 0  tabindex="-1"   Focusable but not tabbable
   *  *aTabIndex >= 0  tabindex=">=0"  Focusable and tabbable
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
  if (aWithMouse && IsNonList(mNodeInfo) &&
      !EventStateManager::IsTopLevelRemoteTarget(this)) {
    return false;
  }
#endif

  nsCOMPtr<nsIDOMXULControlElement> xulControl = AsXULControl();
  if (xulControl) {
    // a disabled element cannot be focused and is not part of the tab order
    bool disabled;
    xulControl->GetDisabled(&disabled);
    if (disabled) {
      if (aTabIndex) *aTabIndex = -1;
      return false;
    }
    shouldFocus = true;
  }

  if (aTabIndex) {
    Maybe<int32_t> attrVal = GetTabIndexAttrValue();
    if (attrVal.isSome()) {
      // The tabindex attribute was specified, so the element becomes
      // focusable.
      shouldFocus = true;
      *aTabIndex = attrVal.value();
    } else {
      // otherwise, if there is no tabindex attribute, just use the value of
      // *aTabIndex to indicate focusability. Reset any supplied tabindex to 0.
      shouldFocus = *aTabIndex >= 0;
      if (shouldFocus) {
        *aTabIndex = 0;
      }
    }

    if (xulControl && shouldFocus && sTabFocusModelAppliesToXUL &&
        !(sTabFocusModel & eTabFocus_formElementsMask)) {
      // By default, the tab focus model doesn't apply to xul element on any
      // system but OS X. on OS X we're following it for UI elements (XUL) as
      // sTabFocusModel is based on "Full Keyboard Access" system setting (see
      // mac/nsILookAndFeel). both textboxes and list elements (i.e. trees and
      // list) should always be focusable (textboxes are handled as html:input)
      // For compatibility, we only do this for controls, otherwise elements
      // like <browser> cannot take this focus.
      if (IsNonList(mNodeInfo)) {
        *aTabIndex = -1;
      }
    }
  }

  return shouldFocus;
}

int32_t nsXULElement::ScreenX() {
  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);
  return frame ? frame->GetScreenRect().x : 0;
}

int32_t nsXULElement::ScreenY() {
  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);
  return frame ? frame->GetScreenRect().y : 0;
}

bool nsXULElement::HasMenu() {
  nsMenuFrame* menu = do_QueryFrame(GetPrimaryFrame(FlushType::Frames));
  return !!menu;
}

void nsXULElement::OpenMenu(bool aOpenFlag) {
  // Flush frames first. It's not clear why this is needed, see bug 1704670.
  if (Document* doc = GetComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    if (aOpenFlag) {
      // Nothing will happen if this element isn't a menu.
      pm->ShowMenu(this, false, false);
    } else {
      // Nothing will happen if this element isn't a menu.
      pm->HideMenu(this);
    }
  }
}

Result<bool, nsresult> nsXULElement::PerformAccesskey(bool aKeyCausesActivation,
                                                      bool aIsTrustedEvent) {
  if (IsXULElement(nsGkAtoms::label)) {
    nsAutoString control;
    GetAttr(kNameSpaceID_None, nsGkAtoms::control, control);
    if (control.IsEmpty()) {
      return Err(NS_ERROR_UNEXPECTED);
    }

    // XXXsmaug Should we use ShadowRoot::GetElementById in case
    //          element is in Shadow DOM?
    RefPtr<Document> document = GetUncomposedDoc();
    if (!document) {
      return Err(NS_ERROR_UNEXPECTED);
    }

    RefPtr<Element> element = document->GetElementById(control);
    if (!element) {
      return Err(NS_ERROR_UNEXPECTED);
    }

    // XXXedgar, This is mainly for HTMLElement which doesn't do visible
    // check in PerformAccesskey. We probably should always do visible
    // check on HTMLElement even if the PerformAccesskey is not redirected from
    // label XULelement per spec.
    nsIFrame* frame = element->GetPrimaryFrame();
    if (!frame || !frame->IsVisibleConsideringAncestors()) {
      return Err(NS_ERROR_UNEXPECTED);
    }

    return element->PerformAccesskey(aKeyCausesActivation, aIsTrustedEvent);
  }

  nsIFrame* frame = GetPrimaryFrame();
  if (!frame || !frame->IsVisibleConsideringAncestors()) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  bool focused = false;
  // Define behavior for each type of XUL element.
  if (!IsXULElement(nsGkAtoms::toolbarbutton)) {
    if (RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager()) {
      RefPtr<Element> elementToFocus = this;
      // for radio buttons, focus the radiogroup instead
      if (IsXULElement(nsGkAtoms::radio)) {
        if (nsCOMPtr<nsIDOMXULSelectControlItemElement> controlItem =
                AsXULSelectControlItem()) {
          bool disabled;
          controlItem->GetDisabled(&disabled);
          if (!disabled) {
            controlItem->GetControl(getter_AddRefs(elementToFocus));
          }
        }
      }

      if (elementToFocus) {
        fm->SetFocus(elementToFocus, nsIFocusManager::FLAG_BYKEY);

        // Return true if the element became focused.
        nsPIDOMWindowOuter* window = OwnerDoc()->GetWindow();
        focused = (window && window->GetFocusedElement() == elementToFocus);
      }
    }
  }

  if (aKeyCausesActivation && !IsXULElement(nsGkAtoms::menulist)) {
    ClickWithInputSource(MouseEvent_Binding::MOZ_SOURCE_KEYBOARD,
                         aIsTrustedEvent);
    return focused;
  }

  // If the accesskey won't cause the activation and the focus isn't changed,
  // either. Return error so EventStateManager would try to find next element
  // to handle the accesskey.
  return focused ? Result<bool, nsresult>{focused} : Err(NS_ERROR_ABORT);
}

//----------------------------------------------------------------------

void nsXULElement::AddListenerForAttributeIfNeeded(nsAtom* aLocalName) {
  // If appropriate, add a popup listener and/or compile the event
  // handler. Called when we change the element's document, create a
  // new element, change an attribute's value, etc.
  // Eventlistenener-attributes are always in the null namespace.
  if (aLocalName == nsGkAtoms::menu || aLocalName == nsGkAtoms::contextmenu ||
      // XXXdwh popup and context are deprecated
      aLocalName == nsGkAtoms::popup || aLocalName == nsGkAtoms::context) {
    AddPopupListener(aLocalName);
  }
  if (nsContentUtils::IsEventAttributeName(aLocalName, EventNameType_XUL)) {
    nsAutoString value;
    GetAttr(kNameSpaceID_None, aLocalName, value);
    SetEventHandler(aLocalName, value, true);
  }
}

void nsXULElement::AddListenerForAttributeIfNeeded(const nsAttrName& aName) {
  if (aName.IsAtom()) {
    AddListenerForAttributeIfNeeded(aName.Atom());
  }
}

//----------------------------------------------------------------------
//
// nsIContent interface
//
void nsXULElement::UpdateEditableState(bool aNotify) {
  // Don't call through to Element here because the things
  // it does don't work for cases when we're an editable control.
  nsIContent* parent = GetParent();

  SetEditableFlag(parent && parent->HasFlag(NODE_IS_EDITABLE));
  UpdateState(aNotify);
}

class XULInContentErrorReporter : public Runnable {
 public:
  explicit XULInContentErrorReporter(Document& aDocument)
      : mozilla::Runnable("XULInContentErrorReporter"), mDocument(aDocument) {}

  NS_IMETHOD Run() override {
    mDocument->WarnOnceAbout(DeprecatedOperations::eImportXULIntoContent,
                             false);
    return NS_OK;
  }

 private:
  OwningNonNull<Document> mDocument;
};

static bool NeedTooltipSupport(const nsXULElement& aXULElement) {
  if (aXULElement.NodeInfo()->Equals(nsGkAtoms::treechildren)) {
    // treechildren always get tooltip support, since cropped tree cells show
    // their full text in a tooltip.
    return true;
  }

  return aXULElement.GetBoolAttr(nsGkAtoms::tooltip) ||
         aXULElement.GetBoolAttr(nsGkAtoms::tooltiptext);
}

nsresult nsXULElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = nsStyledElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsInComposedDoc()) {
    return rv;
  }

  Document& doc = aContext.OwnerDoc();
  if (!IsInNativeAnonymousSubtree() && !doc.AllowXULXBL() &&
      !doc.HasWarnedAbout(DeprecatedOperations::eImportXULIntoContent)) {
    nsContentUtils::AddScriptRunner(new XULInContentErrorReporter(doc));
  }

#ifdef DEBUG
  if (!doc.AllowXULXBL() && !doc.IsUnstyledDocument()) {
    // To save CPU cycles and memory, non-XUL documents only load the user
    // agent style sheet rules for a minimal set of XUL elements such as
    // 'scrollbar' that may be created implicitly for their content (those
    // rules being in minimal-xul.css).
    //
    // This assertion makes sure no other XUL element is used in a non-XUL
    // document.
    nsAtom* tag = NodeInfo()->NameAtom();
    MOZ_ASSERT(
        // scrollbar parts
        tag == nsGkAtoms::scrollbar || tag == nsGkAtoms::scrollbarbutton ||
            tag == nsGkAtoms::scrollcorner || tag == nsGkAtoms::slider ||
            tag == nsGkAtoms::thumb ||
            // other
            tag == nsGkAtoms::resizer || tag == nsGkAtoms::label,
        "Unexpected XUL element in non-XUL doc");
  }
#endif

  // Within Bug 1492063 and its dependencies we started to apply a
  // CSP to system privileged about pages. Since some about: pages
  // are implemented in *.xul files we added this workaround to
  // apply a CSP to them. To do so, we check the introduced custom
  // attribute 'csp' on the root element.
  if (doc.GetRootElement() == this) {
    nsAutoString cspPolicyStr;
    GetAttr(kNameSpaceID_None, nsGkAtoms::csp, cspPolicyStr);

#ifdef DEBUG
    {
      nsCOMPtr<nsIContentSecurityPolicy> docCSP = doc.GetCsp();
      uint32_t policyCount = 0;
      if (docCSP) {
        docCSP->GetPolicyCount(&policyCount);
      }
      MOZ_ASSERT(policyCount == 0, "how come we already have a policy?");
    }
#endif

    CSP_ApplyMetaCSPToDoc(doc, cspPolicyStr);
  }

  if (NodeInfo()->Equals(nsGkAtoms::keyset, kNameSpaceID_XUL)) {
    // Create our XUL key listener and hook it up.
    XULKeySetGlobalKeyListener::AttachKeyHandler(this);
  }

  RegUnRegAccessKey(true);

  if (NeedTooltipSupport(*this)) {
    AddTooltipSupport();
  }

  if (XULBroadcastManager::MayNeedListener(*this)) {
    if (!doc.HasXULBroadcastManager()) {
      doc.InitializeXULBroadcastManager();
    }
    XULBroadcastManager* broadcastManager = doc.GetXULBroadcastManager();
    broadcastManager->AddListener(this);
  }
  return rv;
}

void nsXULElement::UnbindFromTree(bool aNullParent) {
  if (NodeInfo()->Equals(nsGkAtoms::keyset, kNameSpaceID_XUL)) {
    XULKeySetGlobalKeyListener::DetachKeyHandler(this);
  }

  RegUnRegAccessKey(false);

  if (NeedTooltipSupport(*this)) {
    RemoveTooltipSupport();
  }

  Document* doc = GetComposedDoc();
  if (doc && doc->HasXULBroadcastManager() &&
      XULBroadcastManager::MayNeedListener(*this)) {
    RefPtr<XULBroadcastManager> broadcastManager =
        doc->GetXULBroadcastManager();
    broadcastManager->RemoveListener(this);
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
  }

  nsStyledElement::UnbindFromTree(aNullParent);
}

void nsXULElement::DoneAddingChildren(bool aHaveNotified) {
  if (IsXULElement(nsGkAtoms::linkset)) {
    Document* doc = GetComposedDoc();
    if (doc) {
      doc->OnL10nResourceContainerParsed();
    }
  }
}

void nsXULElement::RegUnRegAccessKey(bool aDoReg) {
  // Don't try to register for unsupported elements
  if (!SupportsAccessKey()) {
    return;
  }

  nsStyledElement::RegUnRegAccessKey(aDoReg);
}

bool nsXULElement::SupportsAccessKey() const {
  if (NodeInfo()->Equals(nsGkAtoms::label) && HasAttr(nsGkAtoms::control)) {
    return true;
  }

  // XXX(ntim): check if description[value] or description[accesskey] are
  // actually used, remove `value` from {Before/After}SetAttr if not the case
  if (NodeInfo()->Equals(nsGkAtoms::description) && HasAttr(nsGkAtoms::value) &&
      HasAttr(nsGkAtoms::control)) {
    return true;
  }

  return IsAnyOfXULElements(nsGkAtoms::button, nsGkAtoms::toolbarbutton,
                            nsGkAtoms::checkbox, nsGkAtoms::tab,
                            nsGkAtoms::radio);
}

nsresult nsXULElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                     const nsAttrValueOrString* aValue,
                                     bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::accesskey || aName == nsGkAtoms::control ||
       aName == nsGkAtoms::value)) {
    RegUnRegAccessKey(false);
  } else if (aNamespaceID == kNameSpaceID_None &&
             (aName == nsGkAtoms::command || aName == nsGkAtoms::observes) &&
             IsInUncomposedDoc()) {
    //         XXX sXBL/XBL2 issue! Owner or current document?
    // XXX Why does this not also remove broadcast listeners if the
    // "element" attribute was changed on an <observer>?
    nsAutoString oldValue;
    GetAttr(kNameSpaceID_None, nsGkAtoms::observes, oldValue);
    if (oldValue.IsEmpty()) {
      GetAttr(kNameSpaceID_None, nsGkAtoms::command, oldValue);
    }

    Document* doc = GetUncomposedDoc();
    if (!oldValue.IsEmpty() && doc->HasXULBroadcastManager()) {
      RefPtr<XULBroadcastManager> broadcastManager =
          doc->GetXULBroadcastManager();
      broadcastManager->RemoveListener(this);
    }
  } else if (aNamespaceID == kNameSpaceID_None && aValue &&
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
    bool hasAttribute =
        GetAttr(kNameSpaceID_None, nsGkAtoms::usercontextid, oldValue);
    if (hasAttribute && (!aValue || !aValue->String().Equals(oldValue))) {
      MOZ_ASSERT(false, "Changing usercontextid is not allowed.");
      return NS_ERROR_INVALID_ARG;
    }
  }

  return nsStyledElement::BeforeSetAttr(aNamespaceID, aName, aValue, aNotify);
}

nsresult nsXULElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                    const nsAttrValue* aValue,
                                    const nsAttrValue* aOldValue,
                                    nsIPrincipal* aSubjectPrincipal,
                                    bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aValue) {
      AddListenerForAttributeIfNeeded(aName);
    }

    if (aName == nsGkAtoms::accesskey || aName == nsGkAtoms::control ||
        aName == nsGkAtoms::value) {
      RegUnRegAccessKey(true);
    } else if (aName == nsGkAtoms::tooltip || aName == nsGkAtoms::tooltiptext) {
      if (!!aValue != !!aOldValue && IsInComposedDoc() &&
          !NodeInfo()->Equals(nsGkAtoms::treechildren)) {
        if (aValue) {
          AddTooltipSupport();
        } else {
          RemoveTooltipSupport();
        }
      }
    }
    Document* doc = GetComposedDoc();
    if (doc && doc->HasXULBroadcastManager()) {
      RefPtr<XULBroadcastManager> broadcastManager =
          doc->GetXULBroadcastManager();
      broadcastManager->AttributeChanged(this, aNamespaceID, aName);
    }
    if (doc && XULBroadcastManager::MayNeedListener(*this)) {
      if (!doc->HasXULBroadcastManager()) {
        doc->InitializeXULBroadcastManager();
      }
      XULBroadcastManager* broadcastManager = doc->GetXULBroadcastManager();
      broadcastManager->AddListener(this);
    }

    // XXX need to check if they're changing an event handler: if
    // so, then we need to unhook the old one.  Or something.
  }

  return nsStyledElement::AfterSetAttr(aNamespaceID, aName, aValue, aOldValue,
                                       aSubjectPrincipal, aNotify);
}

void nsXULElement::AddTooltipSupport() {
  nsXULTooltipListener* listener = nsXULTooltipListener::GetInstance();
  if (!listener) {
    return;
  }

  listener->AddTooltipSupport(this);
}

void nsXULElement::RemoveTooltipSupport() {
  nsXULTooltipListener* listener = nsXULTooltipListener::GetInstance();
  if (!listener) {
    return;
  }

  listener->RemoveTooltipSupport(this);
}

bool nsXULElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsIPrincipal* aMaybeScriptedPrincipal,
                                  nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::tabindex) {
    return aResult.ParseIntValue(aValue);
  }

  // Parse into a nsAttrValue
  if (!nsStyledElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                       aMaybeScriptedPrincipal, aResult)) {
    // Fall back to parsing as atom for short values
    aResult.ParseStringOrAtom(aValue);
  }

  return true;
}

void nsXULElement::DestroyContent() {
  nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
  if (slots) {
    slots->mControllers = nullptr;
  }

  nsStyledElement::DestroyContent();
}

#ifdef MOZ_DOM_LIST
void nsXULElement::List(FILE* out, int32_t aIndent) const {
  nsCString prefix("XUL");
  if (HasSlots()) {
    prefix.Append('*');
  }
  prefix.Append(' ');

  nsStyledElement::List(out, aIndent, prefix);
}
#endif

bool nsXULElement::IsEventStoppedFromAnonymousScrollbar(EventMessage aMessage) {
  return (IsRootOfNativeAnonymousSubtree() &&
          IsAnyOfXULElements(nsGkAtoms::scrollbar, nsGkAtoms::scrollcorner) &&
          (aMessage == eMouseClick || aMessage == eMouseDoubleClick ||
           aMessage == eXULCommand || aMessage == eContextMenu ||
           aMessage == eDragStart || aMessage == eMouseAuxClick));
}

nsresult nsXULElement::DispatchXULCommand(const EventChainVisitor& aVisitor,
                                          nsAutoString& aCommand) {
  // XXX sXBL/XBL2 issue! Owner or current document?
  nsCOMPtr<Document> doc = GetUncomposedDoc();
  NS_ENSURE_STATE(doc);
  RefPtr<Element> commandElt = doc->GetElementById(aCommand);
  if (commandElt) {
    // Create a new command event to dispatch to the element
    // pointed to by the command attribute. The new event's
    // sourceEvent will be the original command event that we're
    // handling.
    RefPtr<Event> event = aVisitor.mDOMEvent;
    uint16_t inputSource = MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
    int16_t button = 0;
    while (event) {
      NS_ENSURE_STATE(event->GetOriginalTarget() != commandElt);
      RefPtr<XULCommandEvent> commandEvent = event->AsXULCommandEvent();
      if (commandEvent) {
        event = commandEvent->GetSourceEvent();
        inputSource = commandEvent->InputSource();
        button = commandEvent->Button();
      } else {
        event = nullptr;
      }
    }
    WidgetInputEvent* orig = aVisitor.mEvent->AsInputEvent();
    nsContentUtils::DispatchXULCommand(
        commandElt, orig->IsTrusted(), MOZ_KnownLive(aVisitor.mDOMEvent),
        nullptr, orig->IsControl(), orig->IsAlt(), orig->IsShift(),
        orig->IsMeta(), inputSource, button);
  } else {
    NS_WARNING("A XUL element is attached to a command that doesn't exist!\n");
  }
  return NS_OK;
}

void nsXULElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  aVisitor.mForceContentDispatch = true;  // FIXME! Bug 329119
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
    if (aVisitor.mDOMEvent && aVisitor.mDOMEvent->AsXULCommandEvent() &&
        HasNonEmptyAttr(nsGkAtoms::command)) {
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

nsresult nsXULElement::PreHandleEvent(EventChainVisitor& aVisitor) {
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

nsChangeHint nsXULElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                  int32_t aModType) const {
  if (aAttribute == nsGkAtoms::value &&
      (aModType == MutationEvent_Binding::REMOVAL ||
       aModType == MutationEvent_Binding::ADDITION) &&
      IsAnyOfXULElements(nsGkAtoms::label, nsGkAtoms::description)) {
    // Label and description dynamically morph between a normal
    // block and a cropping single-line XUL text frame.  If the
    // value attribute is being added or removed, then we need to
    // return a hint of frame change.  (See bugzilla bug 95475 for
    // details.)
    return nsChangeHint_ReconstructFrame;
  }

  if (aAttribute == nsGkAtoms::type &&
      IsAnyOfXULElements(nsGkAtoms::toolbarbutton, nsGkAtoms::button)) {
    // type=menu switches from a button frame to a menu frame.
    return nsChangeHint_ReconstructFrame;
  }

  return nsChangeHint(0);
}

NS_IMETHODIMP_(bool)
nsXULElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  return false;
}

nsIControllers* nsXULElement::GetControllers(ErrorResult& rv) {
  if (!Controllers()) {
    nsExtendedDOMSlots* slots = ExtendedDOMSlots();

    slots->mControllers = new nsXULControllers();
  }

  return Controllers();
}

void nsXULElement::Click(CallerType aCallerType) {
  ClickWithInputSource(MouseEvent_Binding::MOZ_SOURCE_UNKNOWN,
                       aCallerType == CallerType::System);
}

void nsXULElement::ClickWithInputSource(uint16_t aInputSource,
                                        bool aIsTrustedEvent) {
  if (BoolAttrIsTrue(nsGkAtoms::disabled)) return;

  nsCOMPtr<Document> doc = GetComposedDoc();  // Strong just in case
  if (doc) {
    RefPtr<nsPresContext> context = doc->GetPresContext();
    if (context) {
      // strong ref to PresContext so events don't destroy it

      WidgetMouseEvent eventDown(aIsTrustedEvent, eMouseDown, nullptr,
                                 WidgetMouseEvent::eReal);
      WidgetMouseEvent eventUp(aIsTrustedEvent, eMouseUp, nullptr,
                               WidgetMouseEvent::eReal);
      WidgetMouseEvent eventClick(aIsTrustedEvent, eMouseClick, nullptr,
                                  WidgetMouseEvent::eReal);
      eventDown.mInputSource = eventUp.mInputSource = eventClick.mInputSource =
          aInputSource;

      // send mouse down
      nsEventStatus status = nsEventStatus_eIgnore;
      EventDispatcher::Dispatch(static_cast<nsIContent*>(this), context,
                                &eventDown, nullptr, &status);

      // send mouse up
      status = nsEventStatus_eIgnore;  // reset status
      EventDispatcher::Dispatch(static_cast<nsIContent*>(this), context,
                                &eventUp, nullptr, &status);

      // send mouse click
      status = nsEventStatus_eIgnore;  // reset status
      EventDispatcher::Dispatch(static_cast<nsIContent*>(this), context,
                                &eventClick, nullptr, &status);

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

void nsXULElement::DoCommand() {
  nsCOMPtr<Document> doc = GetComposedDoc();  // strong just in case
  if (doc) {
    RefPtr<nsXULElement> self = this;
    nsContentUtils::DispatchXULCommand(self, true);
  }
}

bool nsXULElement::IsNodeOfType(uint32_t aFlags) const { return false; }

nsresult nsXULElement::AddPopupListener(nsAtom* aName) {
  // Add a popup listener to the element
  bool isContext =
      (aName == nsGkAtoms::context || aName == nsGkAtoms::contextmenu);
  uint32_t listenerFlag = isContext ? XUL_ELEMENT_HAS_CONTENTMENU_LISTENER
                                    : XUL_ELEMENT_HAS_POPUP_LISTENER;

  if (HasFlag(listenerFlag)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventListener> listener =
      new nsXULPopupListener(this, isContext);

  // Add the popup as a listener on this element.
  EventListenerManager* manager = GetOrCreateListenerManager();
  SetFlags(listenerFlag);

  if (isContext) {
    manager->AddEventListenerByType(listener, u"contextmenu"_ns,
                                    TrustedEventsAtSystemGroupBubble());
  } else {
    manager->AddEventListenerByType(listener, u"mousedown"_ns,
                                    TrustedEventsAtSystemGroupBubble());
  }
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsXULElement::MakeHeavyweight(nsXULPrototypeElement* aPrototype) {
  if (!aPrototype) {
    return NS_OK;
  }

  size_t i;
  nsresult rv;
  for (i = 0; i < aPrototype->mAttributes.Length(); ++i) {
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
      rv = mAttrs.SetAndSwapAttr(protoattr->mName.Atom(), attrValue,
                                 &oldValueSet);
    } else {
      rv = mAttrs.SetAndSwapAttr(protoattr->mName.NodeInfo(), attrValue,
                                 &oldValueSet);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

bool nsXULElement::BoolAttrIsTrue(nsAtom* aName) const {
  const nsAttrValue* attr = GetAttrInfo(kNameSpaceID_None, aName).mValue;

  return attr && attr->Type() == nsAttrValue::eAtom &&
         attr->GetAtomValue() == nsGkAtoms::_true;
}

bool nsXULElement::IsEventAttributeNameInternal(nsAtom* aName) {
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_XUL);
}

JSObject* nsXULElement::WrapNode(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return dom::XULElement_Binding::Wrap(aCx, this, aGivenProto);
}

bool nsXULElement::IsInteractiveHTMLContent() const {
  return IsXULElement(nsGkAtoms::menupopup) ||
         Element::IsInteractiveHTMLContent();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULPrototypeNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULPrototypeNode)
  if (tmp->mType == nsXULPrototypeNode::eType_Element) {
    static_cast<nsXULPrototypeElement*>(tmp)->Unlink();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULPrototypeNode)
  if (tmp->mType == nsXULPrototypeNode::eType_Element) {
    nsXULPrototypeElement* elem = static_cast<nsXULPrototypeElement*>(tmp);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mNodeInfo");
    cb.NoteNativeChild(elem->mNodeInfo,
                       NS_CYCLE_COLLECTION_PARTICIPANT(NodeInfo));
    size_t i;
    for (i = 0; i < elem->mAttributes.Length(); ++i) {
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
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXULPrototypeNode, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXULPrototypeNode, Release)

//----------------------------------------------------------------------
//
// nsXULPrototypeAttribute
//

nsXULPrototypeAttribute::~nsXULPrototypeAttribute() {
  MOZ_COUNT_DTOR(nsXULPrototypeAttribute);
}

//----------------------------------------------------------------------
//
// nsXULPrototypeElement
//

nsresult nsXULPrototypeElement::Serialize(
    nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
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
  tmp = aStream->Write32(mAttributes.Length());
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

  nsAutoString attributeValue;
  size_t i;
  for (i = 0; i < mAttributes.Length(); ++i) {
    RefPtr<mozilla::dom::NodeInfo> ni;
    if (mAttributes[i].mName.IsAtom()) {
      ni = mNodeInfo->NodeInfoManager()->GetNodeInfo(
          mAttributes[i].mName.Atom(), nullptr, kNameSpaceID_None,
          nsINode::ATTRIBUTE_NODE);
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
        nsXULPrototypeScript* script =
            static_cast<nsXULPrototypeScript*>(child);

        tmp = aStream->Write8(script->mOutOfLine);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
        if (!script->mOutOfLine) {
          tmp = script->Serialize(aStream, aProtoDoc, aNodeInfos);
          if (NS_FAILED(tmp)) {
            rv = tmp;
          }
        } else {
          tmp = aStream->WriteCompoundObject(script->mSrcURI,
                                             NS_GET_IID(nsIURI), true);
          if (NS_FAILED(tmp)) {
            rv = tmp;
          }

          if (script->HasStencil()) {
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

nsresult nsXULPrototypeElement::Deserialize(
    nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    nsIURI* aDocumentURI,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
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
  int32_t attributes = int32_t(number);

  if (attributes > 0) {
    mAttributes.AppendElements(attributes);

    nsAutoString attributeValue;
    for (size_t i = 0; i < mAttributes.Length(); ++i) {
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
          rv = child->Deserialize(aStream, aProtoDoc, aDocumentURI, aNodeInfos);
          if (NS_WARN_IF(NS_FAILED(rv))) return rv;
          break;
        case eType_Text:
          child = new nsXULPrototypeText();
          rv = child->Deserialize(aStream, aProtoDoc, aDocumentURI, aNodeInfos);
          if (NS_WARN_IF(NS_FAILED(rv))) return rv;
          break;
        case eType_PI:
          child = new nsXULPrototypePI();
          rv = child->Deserialize(aStream, aProtoDoc, aDocumentURI, aNodeInfos);
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

          child = std::move(script);
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
      if (NS_WARN_IF(NS_FAILED(rv))) return rv;
    }
  }

  return rv;
}

nsresult nsXULPrototypeElement::SetAttrAt(uint32_t aPos,
                                          const nsAString& aValue,
                                          nsIURI* aDocumentURI) {
  MOZ_ASSERT(aPos < mAttributes.Length(), "out-of-bounds");

  // WARNING!!
  // This code is largely duplicated in nsXULElement::SetAttr.
  // Any changes should be made to both functions.

  if (!mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
    if (mNodeInfo->NamespaceEquals(kNameSpaceID_XHTML) &&
        mAttributes[aPos].mName.Equals(nsGkAtoms::is)) {
      // We still care about the is attribute set on HTML elements.
      mAttributes[aPos].mValue.ParseAtom(aValue);
      mIsAtom = mAttributes[aPos].mValue.GetAtomValue();

      return NS_OK;
    }

    mAttributes[aPos].mValue.ParseStringOrAtom(aValue);

    return NS_OK;
  }

  if (mAttributes[aPos].mName.Equals(nsGkAtoms::id) && !aValue.IsEmpty()) {
    mHasIdAttribute = true;
    // Store id as atom.
    // id="" means that the element has no id. Not that it has
    // emptystring as id.
    mAttributes[aPos].mValue.ParseAtom(aValue);

    return NS_OK;
  } else if (mAttributes[aPos].mName.Equals(nsGkAtoms::is)) {
    // Store is as atom.
    mAttributes[aPos].mValue.ParseAtom(aValue);
    mIsAtom = mAttributes[aPos].mValue.GetAtomValue();

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
    nsIPrincipal* principal = mNodeInfo->NodeInfoManager()->DocumentPrincipal();
    // XXX Get correct Base URI (need GetBaseURI on *prototype* element)
    // TODO: If we implement Content Security Policy for chrome documents
    // as has been discussed, the CSP should be checked here to see if
    // inline styles are allowed to be applied.
    // XXX No specific specs talk about xul and referrer policy, pass Unset
    auto referrerInfo =
        MakeRefPtr<ReferrerInfo>(aDocumentURI, ReferrerPolicy::_empty);
    auto data = MakeRefPtr<URLExtraData>(aDocumentURI, referrerInfo, principal);
    RefPtr<DeclarationBlock> declaration = DeclarationBlock::FromCssText(
        aValue, data, eCompatibility_FullStandards, nullptr,
        StyleCssRuleType::Style);
    if (declaration) {
      mAttributes[aPos].mValue.SetTo(declaration.forget(), &aValue);

      return NS_OK;
    }
    // Don't abort if parsing failed, it could just be malformed css.
  } else if (mAttributes[aPos].mName.Equals(nsGkAtoms::tabindex)) {
    mAttributes[aPos].mValue.ParseIntValue(aValue);

    return NS_OK;
  }

  mAttributes[aPos].mValue.ParseStringOrAtom(aValue);

  return NS_OK;
}

void nsXULPrototypeElement::Unlink() {
  mAttributes.Clear();
  mChildren.Clear();
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
      mStencil(nullptr) {}

static nsresult WriteStencil(nsIObjectOutputStream* aStream, JSContext* aCx,
                             JS::Stencil* aStencil) {
  JS::TranscodeBuffer buffer;
  JS::TranscodeResult code;
  code = JS::EncodeStencil(aCx, aStencil, buffer);

  if (code != JS::TranscodeResult::Ok) {
    if (code == JS::TranscodeResult::Throw) {
      JS_ClearPendingException(aCx);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    MOZ_ASSERT(IsTranscodeFailureResult(code));
    return NS_ERROR_FAILURE;
  }

  size_t size = buffer.length();
  if (size > UINT32_MAX) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = aStream->Write32(size);
  if (NS_SUCCEEDED(rv)) {
    // Ideally we could just pass "buffer" here.  See bug 1566574.
    rv = aStream->WriteBytes(Span(buffer.begin(), size));
  }

  return rv;
}

static nsresult ReadStencil(nsIObjectInputStream* aStream, JSContext* aCx,
                            const JS::DecodeOptions& aOptions,
                            JS::Stencil** aStencilOut) {
  // We don't serialize mutedError-ness of scripts, which is fine as long as
  // we only serialize system and XUL-y things. We can detect this by checking
  // where the caller wants us to deserialize.
  //
  // CompilationScope() could theoretically GC, so get that out of the way
  // before comparing to the cx global.
  JSObject* loaderGlobal = xpc::CompilationScope();
  MOZ_RELEASE_ASSERT(nsContentUtils::IsSystemCaller(aCx) ||
                     JS::CurrentGlobalOrNull(aCx) == loaderGlobal);

  uint32_t size;
  nsresult rv = aStream->Read32(&size);
  if (NS_FAILED(rv)) {
    return rv;
  }

  char* data;
  rv = aStream->ReadBytes(size, &data);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // The decoded stencil shouldn't borrow from the XDR buffer.
  MOZ_ASSERT(!aOptions.borrowBuffer);
  auto cleanupData = MakeScopeExit([&]() { free(data); });

  JS::TranscodeRange range(reinterpret_cast<uint8_t*>(data), size);

  {
    JS::TranscodeResult code;
    RefPtr<JS::Stencil> stencil;
    code = JS::DecodeStencil(aCx, aOptions, range, getter_AddRefs(stencil));
    if (code != JS::TranscodeResult::Ok) {
      if (code == JS::TranscodeResult::Throw) {
        JS_ClearPendingException(aCx);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      MOZ_ASSERT(IsTranscodeFailureResult(code));
      return NS_ERROR_FAILURE;
    }

    stencil.forget(aStencilOut);
  }

  return rv;
}

void nsXULPrototypeScript::FillCompileOptions(JS::CompileOptions& options) {
  // If the script was inline, tell the JS parser to save source for
  // Function.prototype.toSource(). If it's out of line, we retrieve the
  // source from the files on demand.
  options.setSourceIsLazy(mOutOfLine);
}

nsresult nsXULPrototypeScript::Serialize(
    nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
  NS_ENSURE_TRUE(aProtoDoc, NS_ERROR_UNEXPECTED);

  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::CompilationScope())) {
    return NS_ERROR_UNEXPECTED;
  }

  NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nullptr || !mStencil,
               "script source still loading when serializing?!");
  if (!mStencil) return NS_ERROR_FAILURE;

  // Write basic prototype data
  nsresult rv;
  rv = aStream->Write32(mLineNo);
  if (NS_FAILED(rv)) return rv;
  rv = aStream->Write32(0);  // See bug 1418294.
  if (NS_FAILED(rv)) return rv;

  JSContext* cx = jsapi.cx();
  MOZ_ASSERT(xpc::CompilationScope() == JS::CurrentGlobalOrNull(cx));

  return WriteStencil(aStream, cx, mStencil);
}

nsresult nsXULPrototypeScript::SerializeOutOfLine(
    nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc) {
  if (!mSrcURI->SchemeIs("chrome"))
    // Don't cache scripts that don't come from chrome uris.
    return NS_ERROR_NOT_IMPLEMENTED;

  nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
  if (!cache) return NS_ERROR_OUT_OF_MEMORY;

  NS_ASSERTION(cache->IsEnabled(),
               "writing to the cache file, but the XUL cache is off?");
  bool exists;
  cache->HasData(mSrcURI, &exists);

  /* return will be NS_OK from GetAsciiSpec.
   * that makes no sense.
   * nor does returning NS_OK from HasMuxedDocument.
   * XXX return something meaningful.
   */
  if (exists) return NS_OK;

  nsCOMPtr<nsIObjectOutputStream> oos;
  nsresult rv = cache->GetOutputStream(mSrcURI, getter_AddRefs(oos));
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult tmp = Serialize(oos, aProtoDoc, nullptr);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = cache->FinishOutputStream(mSrcURI);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

  if (NS_FAILED(rv)) cache->AbortCaching();
  return rv;
}

nsresult nsXULPrototypeScript::Deserialize(
    nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    nsIURI* aDocumentURI,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
  nsresult rv;
  NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nullptr || !mStencil,
               "prototype script not well-initialized when deserializing?!");

  // Read basic prototype data
  rv = aStream->Read32(&mLineNo);
  if (NS_FAILED(rv)) return rv;
  uint32_t dummy;
  rv = aStream->Read32(&dummy);  // See bug 1418294.
  if (NS_FAILED(rv)) return rv;

  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::CompilationScope())) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();

  JS::DecodeOptions options;
  RefPtr<JS::Stencil> newStencil;
  rv = ReadStencil(aStream, cx, options, getter_AddRefs(newStencil));
  NS_ENSURE_SUCCESS(rv, rv);
  Set(newStencil);
  return NS_OK;
}

nsresult nsXULPrototypeScript::DeserializeOutOfLine(
    nsIObjectInputStream* aInput, nsXULPrototypeDocument* aProtoDoc) {
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
      // Note that PrototypeDocumentContentSink::LoadScript
      // checks the XUL script cache too, in order to handle the
      // serialization case.
      //
      // We need do this only for <script src='strres.js'> and the
      // like, i.e., out-of-line scripts that are included by several
      // different XUL documents stored in the cache file.
      useXULCache = cache->IsEnabled();

      if (useXULCache) {
        RefPtr<JS::Stencil> newStencil = cache->GetStencil(mSrcURI);
        if (newStencil) {
          Set(newStencil);
        }
      }
    }

    if (!mStencil) {
      if (mSrcURI) {
        rv = cache->GetInputStream(mSrcURI, getter_AddRefs(objectInput));
      }
      // If !mSrcURI, we have an inline script. We shouldn't have
      // to do anything else in that case, I think.

      // We do reflect errors into rv, but our caller may want to
      // ignore our return value, because mStencil will be null
      // after any error, and that suffices to cause the script to
      // be reloaded (from the src= URI, if any) and recompiled.
      // We're better off slow-loading than bailing out due to a
      // error.
      if (NS_SUCCEEDED(rv))
        rv = Deserialize(objectInput, aProtoDoc, nullptr, nullptr);

      if (NS_SUCCEEDED(rv)) {
        if (useXULCache && mSrcURI && mSrcURI->SchemeIs("chrome")) {
          cache->PutStencil(mSrcURI, GetStencil());
        }
        cache->FinishInputStream(mSrcURI);
      } else {
        // If mSrcURI is not in the cache,
        // rv will be NS_ERROR_NOT_AVAILABLE and we'll try to
        // update the cache file to hold a serialization of
        // this script, once it has finished loading.
        if (rv != NS_ERROR_NOT_AVAILABLE) cache->AbortCaching();
      }
    }
  }
  return rv;
}

class NotifyOffThreadScriptCompletedRunnable : public Runnable {
  // An array of all outstanding script receivers. All reference counting of
  // these objects happens on the main thread. When we return to the main
  // thread from script compilation we make sure our receiver is still in
  // this array (still alive) before proceeding. This array is cleared during
  // shutdown, potentially before all outstanding script compilations have
  // finished. We do not need to worry about pointer replay here, because
  // a) we should not be starting script compilation after clearing this
  // array and b) in all other cases the receiver will still be alive.
  static StaticAutoPtr<nsTArray<nsCOMPtr<nsIOffThreadScriptReceiver>>>
      sReceivers;
  static bool sSetupClearOnShutdown;

  nsIOffThreadScriptReceiver* mReceiver;
  JS::OffThreadToken* mToken;

 public:
  NotifyOffThreadScriptCompletedRunnable(nsIOffThreadScriptReceiver* aReceiver,
                                         JS::OffThreadToken* aToken)
      : mozilla::Runnable("NotifyOffThreadScriptCompletedRunnable"),
        mReceiver(aReceiver),
        mToken(aToken) {}

  static void NoteReceiver(nsIOffThreadScriptReceiver* aReceiver) {
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

StaticAutoPtr<nsTArray<nsCOMPtr<nsIOffThreadScriptReceiver>>>
    NotifyOffThreadScriptCompletedRunnable::sReceivers;
bool NotifyOffThreadScriptCompletedRunnable::sSetupClearOnShutdown = false;

NS_IMETHODIMP
NotifyOffThreadScriptCompletedRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<JS::Stencil> stencil;
  {
    AutoJSAPI jsapi;
    if (!jsapi.Init(xpc::CompilationScope())) {
      // Now what?  I guess we just leak... this should probably never
      // happen.
      return NS_ERROR_UNEXPECTED;
    }
    JSContext* cx = jsapi.cx();
    stencil = JS::FinishOffThreadCompileToStencil(cx, mToken);
  }

  if (!sReceivers) {
    // We've already shut down.
    return NS_OK;
  }

  auto index = sReceivers->IndexOf(mReceiver);
  MOZ_RELEASE_ASSERT(index != sReceivers->NoIndex);
  nsCOMPtr<nsIOffThreadScriptReceiver> receiver =
      std::move((*sReceivers)[index]);
  sReceivers->RemoveElementAt(index);

  return receiver->OnScriptCompileComplete(stencil,
                                           stencil ? NS_OK : NS_ERROR_FAILURE);
}

static void OffThreadScriptReceiverCallback(JS::OffThreadToken* aToken,
                                            void* aCallbackData) {
  // Be careful not to adjust the refcount on the receiver, as this callback
  // may be invoked off the main thread.
  nsIOffThreadScriptReceiver* aReceiver =
      static_cast<nsIOffThreadScriptReceiver*>(aCallbackData);
  RefPtr<NotifyOffThreadScriptCompletedRunnable> notify =
      new NotifyOffThreadScriptCompletedRunnable(aReceiver, aToken);
  NS_DispatchToMainThread(notify);
}

nsresult nsXULPrototypeScript::Compile(
    const char16_t* aText, size_t aTextLength, JS::SourceOwnership aOwnership,
    nsIURI* aURI, uint32_t aLineNo, Document* aDocument,
    nsIOffThreadScriptReceiver* aOffThreadReceiver /* = nullptr */) {
  // We'll compile the script in the compilation scope.
  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::CompilationScope())) {
    if (aOwnership == JS::SourceOwnership::TakeOwnership) {
      // In this early-exit case -- before the |srcBuf.init| call will
      // own |aText| -- we must relinquish ownership manually.
      js_free(const_cast<char16_t*>(aText));
    }

    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();

  JS::SourceText<char16_t> srcBuf;
  if (NS_WARN_IF(!srcBuf.init(cx, aText, aTextLength, aOwnership))) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString urlspec;
  nsresult rv = aURI->GetSpec(urlspec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Ok, compile it to create a prototype script object!
  JS::CompileOptions options(cx);
  FillCompileOptions(options);
  options.setIntroductionType(mOutOfLine ? "srcScript" : "inlineScript")
      .setFileAndLine(urlspec.get(), mOutOfLine ? 1 : aLineNo);

  JS::Rooted<JSObject*> scope(cx, JS::CurrentGlobalOrNull(cx));

  if (aOffThreadReceiver && JS::CanCompileOffThread(cx, options, aTextLength)) {
    if (!JS::CompileToStencilOffThread(
            cx, options, srcBuf, OffThreadScriptReceiverCallback,
            static_cast<void*>(aOffThreadReceiver))) {
      JS_ClearPendingException(cx);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NotifyOffThreadScriptCompletedRunnable::NoteReceiver(aOffThreadReceiver);
  } else {
    RefPtr<JS::Stencil> stencil =
        JS::CompileGlobalScriptToStencil(cx, options, srcBuf);
    if (!stencil) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    Set(stencil);
  }
  return NS_OK;
}

nsresult nsXULPrototypeScript::InstantiateScript(
    JSContext* aCx, JS::MutableHandleScript aScript) {
  MOZ_ASSERT(mStencil);

  JS::CompileOptions options(aCx);
  FillCompileOptions(options);
  JS::InstantiateOptions instantiateOptions(options);
  aScript.set(JS::InstantiateGlobalStencil(aCx, instantiateOptions, mStencil));
  if (!aScript) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void nsXULPrototypeScript::Set(JS::Stencil* aStencil) { mStencil = aStencil; }

//----------------------------------------------------------------------
//
// nsXULPrototypeText
//

nsresult nsXULPrototypeText::Serialize(
    nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
  nsresult rv;

  // Write basic prototype data
  rv = aStream->Write32(mType);

  nsresult tmp = aStream->WriteWStringZ(mValue.get());
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

  return rv;
}

nsresult nsXULPrototypeText::Deserialize(
    nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    nsIURI* aDocumentURI,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
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

nsresult nsXULPrototypePI::Serialize(
    nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
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

nsresult nsXULPrototypePI::Deserialize(
    nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
    nsIURI* aDocumentURI,
    const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) {
  nsresult rv;

  rv = aStream->ReadString(mTarget);
  if (NS_FAILED(rv)) return rv;
  rv = aStream->ReadString(mData);
  if (NS_FAILED(rv)) return rv;

  return rv;
}
