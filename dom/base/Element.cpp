/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes; this provides an implementation
 * of DOM Core's nsIDOMElement, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#include "mozilla/dom/ElementInlines.h"

#include "AnimationCommon.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Flex.h"
#include "mozilla/dom/Grid.h"
#include "mozilla/gfx/Matrix.h"
#include "nsAtom.h"
#include "nsCSSFrameConstructor.h"
#include "nsDOMAttributeMap.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsIDocumentInlines.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIContentIterator.h"
#include "nsFlexContainerFrame.h"
#include "nsFocusManager.h"
#include "nsILinkHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsIDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsNameSpaceManager.h"
#include "nsContentList.h"
#include "nsVariant.h"
#include "nsDOMTokenList.h"
#include "nsXBLPrototypeBinding.h"
#include "nsError.h"
#include "nsDOMString.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/HTMLDivElement.h"
#include "mozilla/dom/HTMLSpanElement.h"
#include "mozilla/dom/KeyframeAnimationOptionsBinding.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/AnimationComparator.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/SizeOfState.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TextEvents.h"
#include "nsNodeUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsDocument.h"
#include "nsAttrValueOrString.h"
#include "nsAttrValueInlines.h"
#include "nsCSSPseudoElements.h"
#include "nsWindowSizes.h"
#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif /* MOZ_XUL */
#include "nsSVGElement.h"
#include "nsFrameSelection.h"
#ifdef DEBUG
#include "nsRange.h"
#endif

#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsPIDOMWindow.h"
#include "nsPIBoxObject.h"
#include "mozilla/dom/DOMRect.h"
#include "nsSVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "ChildIterator.h"

#include "nsIDOMEventListener.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"

#include "nsNodeInfoManager.h"
#include "nsICategoryManager.h"
#include "nsGenericHTMLElement.h"
#include "nsContentCreatorFunctions.h"
#include "nsIControllers.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsTextNode.h"

#include "nsCycleCollectionParticipant.h"
#include "nsCCUncollectableMarker.h"

#include "mozAutoDocUpdate.h"

#include "nsCSSParser.h"
#include "nsDOMMutationObserver.h"
#include "nsWrapperCacheInlines.h"
#include "xpcpublic.h"
#include "nsIScriptError.h"
#include "mozilla/Telemetry.h"

#include "mozilla/CORSMode.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/NodeListBinding.h"

#include "nsStyledElement.h"
#include "nsXBLService.h"
#include "nsITextControlElement.h"
#include "nsITextControlFrame.h"
#include "nsISupportsImpl.h"
#include "mozilla/dom/CSSPseudoElement.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "nsComputedDOMStyle.h"
#include "nsDOMStringMap.h"
#include "DOMIntersectionObserver.h"

#include "nsISpeculativeConnect.h"
#include "nsIIOService.h"

#include "DOMMatrix.h"

using namespace mozilla;
using namespace mozilla::dom;

using mozilla::gfx::Matrix4x4;

//
// Verify sizes of elements on 64-bit platforms. This should catch most memory
// regressions, and is easy to verify locally since most developers are on
// 64-bit machines. We use a template rather than a direct static assert so
// that the error message actually displays the sizes.
//

// We need different numbers on certain build types to deal with the owning
// thread pointer that comes with the non-threadsafe refcount on
// FragmentOrElement.
#ifdef MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED
#define EXTRA_DOM_ELEMENT_BYTES 8
#else
#define EXTRA_DOM_ELEMENT_BYTES 0
#endif

#define ASSERT_ELEMENT_SIZE(type, opt_size) \
template<int a, int b> struct Check##type##Size \
{ \
  static_assert(sizeof(void*) != 8 || a == b, "DOM size changed"); \
}; \
Check##type##Size<sizeof(type), opt_size + EXTRA_DOM_ELEMENT_BYTES> g##type##CES;

// Note that mozjemalloc uses a 16 byte quantum, so 128 is a bin/bucket size.
ASSERT_ELEMENT_SIZE(Element, 120);
ASSERT_ELEMENT_SIZE(HTMLDivElement, 128);
ASSERT_ELEMENT_SIZE(HTMLSpanElement, 128);

#undef ASSERT_ELEMENT_SIZE
#undef EXTRA_DOM_ELEMENT_BYTES

nsAtom*
nsIContent::DoGetID() const
{
  MOZ_ASSERT(HasID(), "Unexpected call");
  MOZ_ASSERT(IsElement(), "Only elements can have IDs");

  return AsElement()->GetParsedAttr(nsGkAtoms::id)->GetAtomValue();
}

const nsAttrValue*
Element::GetSVGAnimatedClass() const
{
  MOZ_ASSERT(MayHaveClass() && IsSVGElement(), "Unexpected call");
  return static_cast<const nsSVGElement*>(this)->GetAnimatedClassName();
}

NS_IMETHODIMP
Element::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(Element))) {
    NS_ADDREF_THIS();
    *aInstancePtr = this;
    return NS_OK;
  }

  NS_ASSERTION(aInstancePtr,
               "QueryInterface requires a non-NULL destination!");
  nsresult rv = FragmentOrElement::QueryInterface(aIID, aInstancePtr);
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  // Give the binding manager a chance to get an interface for this element.
  return OwnerDoc()->BindingManager()->GetBindingImplementation(this, aIID,
                                                                aInstancePtr);
}

EventStates
Element::IntrinsicState() const
{
  return IsEditable() ? NS_EVENT_STATE_MOZ_READWRITE :
                        NS_EVENT_STATE_MOZ_READONLY;
}

void
Element::NotifyStateChange(EventStates aStates)
{
  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    nsAutoScriptBlocker scriptBlocker;
    doc->ContentStateChanged(this, aStates);
  }
}

void
Element::UpdateLinkState(EventStates aState)
{
  MOZ_ASSERT(!aState.HasAtLeastOneOfStates(~(NS_EVENT_STATE_VISITED |
                                             NS_EVENT_STATE_UNVISITED)),
             "Unexpected link state bits");
  mState =
    (mState & ~(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED)) |
    aState;
}

void
Element::UpdateState(bool aNotify)
{
  EventStates oldState = mState;
  mState = IntrinsicState() | (oldState & EXTERNALLY_MANAGED_STATES);
  if (aNotify) {
    EventStates changedStates = oldState ^ mState;
    if (!changedStates.IsEmpty()) {
      nsIDocument* doc = GetComposedDoc();
      if (doc) {
        nsAutoScriptBlocker scriptBlocker;
        doc->ContentStateChanged(this, changedStates);
      }
    }
  }
}

void
nsIContent::UpdateEditableState(bool aNotify)
{
  // Guaranteed to be non-element content
  NS_ASSERTION(!IsElement(), "What happened here?");
  nsIContent *parent = GetParent();

  // Skip over unknown native anonymous content to avoid setting a flag we
  // can't clear later
  bool isUnknownNativeAnon = false;
  if (IsInNativeAnonymousSubtree()) {
    isUnknownNativeAnon = true;
    nsCOMPtr<nsIContent> root = this;
    while (root && !root->IsRootOfNativeAnonymousSubtree()) {
      root = root->GetParent();
    }
    // root should always be true here, but isn't -- bug 999416
    if (root) {
      nsIFrame* rootFrame = root->GetPrimaryFrame();
      if (rootFrame) {
        nsContainerFrame* parentFrame = rootFrame->GetParent();
        nsITextControlFrame* textCtrl = do_QueryFrame(parentFrame);
        isUnknownNativeAnon = !textCtrl;
      }
    }
  }

  SetEditableFlag(parent && parent->HasFlag(NODE_IS_EDITABLE) &&
                  !isUnknownNativeAnon);
}

void
Element::UpdateEditableState(bool aNotify)
{
  nsIContent *parent = GetParent();

  SetEditableFlag(parent && parent->HasFlag(NODE_IS_EDITABLE));
  if (aNotify) {
    UpdateState(aNotify);
  } else {
    // Avoid calling UpdateState in this very common case, because
    // this gets called for pretty much every single element on
    // insertion into the document and UpdateState can be slow for
    // some kinds of elements even when not notifying.
    if (IsEditable()) {
      RemoveStatesSilently(NS_EVENT_STATE_MOZ_READONLY);
      AddStatesSilently(NS_EVENT_STATE_MOZ_READWRITE);
    } else {
      RemoveStatesSilently(NS_EVENT_STATE_MOZ_READWRITE);
      AddStatesSilently(NS_EVENT_STATE_MOZ_READONLY);
    }
  }
}

int32_t
Element::TabIndex()
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsGkAtoms::tabindex);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    return attrVal->GetIntegerValue();
  }

  return TabIndexDefault();
}

void
Element::Focus(mozilla::ErrorResult& aError)
{
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(this);
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  // Also other browsers seem to have the hack to not re-focus (and flush) when
  // the element is already focused.
  if (fm && domElement) {
    if (fm->CanSkipFocus(this)) {
      fm->NeedsFlushBeforeEventHandling(this);
    } else {
      aError = fm->SetFocus(domElement, 0);
    }
  }
}

void
Element::SetTabIndex(int32_t aTabIndex, mozilla::ErrorResult& aError)
{
  nsAutoString value;
  value.AppendInt(aTabIndex);

  SetAttr(nsGkAtoms::tabindex, value, aError);
}

void
Element::SetXBLBinding(nsXBLBinding* aBinding,
                       nsBindingManager* aOldBindingManager)
{
  nsBindingManager* bindingManager;
  if (aOldBindingManager) {
    MOZ_ASSERT(!aBinding, "aOldBindingManager should only be provided "
                          "when removing a binding.");
    bindingManager = aOldBindingManager;
  } else {
    bindingManager = OwnerDoc()->BindingManager();
  }

  // After this point, aBinding will be the most-derived binding for aContent.
  // If we already have a binding for aContent, make sure to
  // remove it from the attached stack.  Otherwise we might end up firing its
  // constructor twice (if aBinding inherits from it) or firing its constructor
  // after aContent has been deleted (if aBinding is null and the content node
  // dies before we process mAttachedStack).
  RefPtr<nsXBLBinding> oldBinding = GetXBLBinding();
  if (oldBinding) {
    bindingManager->RemoveFromAttachedQueue(oldBinding);
  }

  if (aBinding) {
    SetFlags(NODE_MAY_BE_IN_BINDING_MNGR);
    nsExtendedDOMSlots* slots = ExtendedDOMSlots();
    slots->mXBLBinding = aBinding;
    bindingManager->AddBoundContent(this);
  } else {
    nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
    if (slots) {
      slots->mXBLBinding = nullptr;
    }
    bindingManager->RemoveBoundContent(this);
    if (oldBinding) {
      oldBinding->SetBoundElement(nullptr);
    }
  }
}

void
Element::SetShadowRoot(ShadowRoot* aShadowRoot)
{
  nsExtendedDOMSlots* slots = ExtendedDOMSlots();
  slots->mShadowRoot = aShadowRoot;
}

void
Element::Blur(mozilla::ErrorResult& aError)
{
  if (!ShouldBlur(this)) {
    return;
  }

  nsIDocument* doc = GetComposedDoc();
  if (!doc) {
    return;
  }

  nsPIDOMWindowOuter* win = doc->GetWindow();
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (win && fm) {
    aError = fm->ClearFocus(win);
  }
}

EventStates
Element::StyleStateFromLocks() const
{
  StyleStateLocks locksAndValues = LockedStyleStates();
  EventStates locks = locksAndValues.mLocks;
  EventStates values = locksAndValues.mValues;
  EventStates state = (mState & ~locks) | (locks & values);

  if (state.HasState(NS_EVENT_STATE_VISITED)) {
    return state & ~NS_EVENT_STATE_UNVISITED;
  }
  if (state.HasState(NS_EVENT_STATE_UNVISITED)) {
    return state & ~NS_EVENT_STATE_VISITED;
  }

  return state;
}

Element::StyleStateLocks
Element::LockedStyleStates() const
{
  StyleStateLocks* locks =
    static_cast<StyleStateLocks*>(GetProperty(nsGkAtoms::lockedStyleStates));
  if (locks) {
    return *locks;
  }
  return StyleStateLocks();
}

void
Element::NotifyStyleStateChange(EventStates aStates)
{
  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    nsIPresShell *presShell = doc->GetShell();
    if (presShell) {
      nsAutoScriptBlocker scriptBlocker;
      presShell->ContentStateChanged(doc, this, aStates);
    }
  }
}

void
Element::LockStyleStates(EventStates aStates, bool aEnabled)
{
  StyleStateLocks* locks = new StyleStateLocks(LockedStyleStates());

  locks->mLocks |= aStates;
  if (aEnabled) {
    locks->mValues |= aStates;
  } else {
    locks->mValues &= ~aStates;
  }

  if (aStates.HasState(NS_EVENT_STATE_VISITED)) {
    locks->mLocks &= ~NS_EVENT_STATE_UNVISITED;
  }
  if (aStates.HasState(NS_EVENT_STATE_UNVISITED)) {
    locks->mLocks &= ~NS_EVENT_STATE_VISITED;
  }

  SetProperty(nsGkAtoms::lockedStyleStates, locks,
              nsINode::DeleteProperty<StyleStateLocks>);
  SetHasLockedStyleStates();

  NotifyStyleStateChange(aStates);
}

void
Element::UnlockStyleStates(EventStates aStates)
{
  StyleStateLocks* locks = new StyleStateLocks(LockedStyleStates());

  locks->mLocks &= ~aStates;

  if (locks->mLocks.IsEmpty()) {
    DeleteProperty(nsGkAtoms::lockedStyleStates);
    ClearHasLockedStyleStates();
    delete locks;
  }
  else {
    SetProperty(nsGkAtoms::lockedStyleStates, locks,
                nsINode::DeleteProperty<StyleStateLocks>);
  }

  NotifyStyleStateChange(aStates);
}

void
Element::ClearStyleStateLocks()
{
  StyleStateLocks locks = LockedStyleStates();

  DeleteProperty(nsGkAtoms::lockedStyleStates);
  ClearHasLockedStyleStates();

  NotifyStyleStateChange(locks.mLocks);
}

bool
Element::GetBindingURL(nsIDocument *aDocument, css::URLValue **aResult)
{
  // If we have a frame the frame has already loaded the binding.  And
  // otherwise, don't do anything else here unless we're dealing with
  // XUL or an HTML element that may have a plugin-related overlay
  // (i.e. object or embed).
  bool isXULorPluginElement = (IsXULElement() ||
                               IsHTMLElement(nsGkAtoms::object) ||
                               IsHTMLElement(nsGkAtoms::embed));
  if (!aDocument->GetShell() || GetPrimaryFrame() || !isXULorPluginElement) {
    *aResult = nullptr;
    return true;
  }

  // Get the computed -moz-binding directly from the ComputedStyle
  RefPtr<ComputedStyle> sc =
    nsComputedDOMStyle::GetComputedStyleNoFlush(this, nullptr);
  NS_ENSURE_TRUE(sc, false);

  NS_IF_ADDREF(*aResult = sc->StyleDisplay()->mBinding);
  return true;
}

JSObject*
Element::WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  JS::Rooted<JSObject*> obj(aCx, nsINode::WrapObject(aCx, aGivenProto));
  if (!obj) {
    return nullptr;
  }

  nsIDocument* doc = GetComposedDoc();
  if (!doc) {
    // There's no baseclass that cares about this call so we just
    // return here.
    return obj;
  }

  // We must ensure that the XBL Binding is installed before we hand
  // back this object.

  if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR) && GetXBLBinding()) {
    // There's already a binding for this element so nothing left to
    // be done here.

    // In theory we could call ExecuteAttachedHandler here when it's safe to
    // run script if we also removed the binding from the PAQ queue, but that
    // seems like a scary change that would mosly just add more
    // inconsistencies.
    return obj;
  }

  // Make sure the ComputedStyle goes away _before_ we load the binding
  // since that can destroy the relevant presshell.

  {
    // Make a scope so that ~nsRefPtr can GC before returning obj.
    RefPtr<css::URLValue> bindingURL;
    bool ok = GetBindingURL(doc, getter_AddRefs(bindingURL));
    if (!ok) {
      dom::Throw(aCx, NS_ERROR_FAILURE);
      return nullptr;
    }

    if (bindingURL) {
      nsCOMPtr<nsIURI> uri = bindingURL->GetURI();
      nsCOMPtr<nsIPrincipal> principal = bindingURL->mExtraData->GetPrincipal();

      // We have a binding that must be installed.
      bool dummy;

      nsXBLService* xblService = nsXBLService::GetInstance();
      if (!xblService) {
        dom::Throw(aCx, NS_ERROR_NOT_AVAILABLE);
        return nullptr;
      }

      RefPtr<nsXBLBinding> binding;
      xblService->LoadBindings(this, uri, principal, getter_AddRefs(binding),
                               &dummy);

      if (binding) {
        if (nsContentUtils::IsSafeToRunScript()) {
          binding->ExecuteAttachedHandler();
        } else {
          nsContentUtils::AddScriptRunner(
            NewRunnableMethod("nsXBLBinding::ExecuteAttachedHandler",
                              binding,
                              &nsXBLBinding::ExecuteAttachedHandler));
        }
      }
    }
  }

  return obj;
}

/* virtual */
nsINode*
Element::GetScopeChainParent() const
{
  return OwnerDoc();
}

nsDOMTokenList*
Element::ClassList()
{
  Element::nsDOMSlots* slots = DOMSlots();

  if (!slots->mClassList) {
    slots->mClassList = new nsDOMTokenList(this, nsGkAtoms::_class);
  }

  return slots->mClassList;
}

void
Element::GetAttributeNames(nsTArray<nsString>& aResult)
{
  uint32_t count = mAttrsAndChildren.AttrCount();
  for (uint32_t i = 0; i < count; ++i) {
    const nsAttrName* name = mAttrsAndChildren.AttrNameAt(i);
    name->GetQualifiedName(*aResult.AppendElement());
  }
}

already_AddRefed<nsIHTMLCollection>
Element::GetElementsByTagName(const nsAString& aLocalName)
{
  return NS_GetContentList(this, kNameSpaceID_Unknown, aLocalName);
}

nsIFrame*
Element::GetStyledFrame()
{
  nsIFrame *frame = GetPrimaryFrame(FlushType::Layout);
  return frame ? nsLayoutUtils::GetStyleFrame(frame) : nullptr;
}

nsIScrollableFrame*
Element::GetScrollFrame(nsIFrame **aStyledFrame, FlushType aFlushType)
{
  // it isn't clear what to return for SVG nodes, so just return nothing
  if (IsSVGElement()) {
    if (aStyledFrame) {
      *aStyledFrame = nullptr;
    }
    return nullptr;
  }

  // Inline version of GetStyledFrame to use the given FlushType.
  nsIFrame* frame = GetPrimaryFrame(aFlushType);
  if (frame) {
    frame = nsLayoutUtils::GetStyleFrame(frame);
  }

  if (aStyledFrame) {
    *aStyledFrame = frame;
  }
  if (frame) {
    // menu frames implement GetScrollTargetFrame but we don't want
    // to use it here.  Similar for comboboxes.
    LayoutFrameType type = frame->Type();
    if (type != LayoutFrameType::Menu &&
        type != LayoutFrameType::ComboboxControl) {
      nsIScrollableFrame *scrollFrame = frame->GetScrollTargetFrame();
      if (scrollFrame) {
        MOZ_ASSERT(!OwnerDoc()->IsScrollingElement(this),
                   "How can we have a scrollframe if we're the "
                   "scrollingElement for our document?");
        return scrollFrame;
      }
    }
  }

  nsIDocument* doc = OwnerDoc();
  // Note: This IsScrollingElement() call can flush frames, if we're the body of
  // a quirks mode document.
  bool isScrollingElement = OwnerDoc()->IsScrollingElement(this);
  // Now reget *aStyledFrame if the caller asked for it, because that frame
  // flush can kill it.
  if (aStyledFrame) {
    nsIFrame* frame = GetPrimaryFrame(FlushType::None);
    if (frame) {
      *aStyledFrame = nsLayoutUtils::GetStyleFrame(frame);
    } else {
      *aStyledFrame = nullptr;
    }
  }

  if (isScrollingElement) {
    // Our scroll info should map to the root scrollable frame if there is one.
    if (nsIPresShell* shell = doc->GetShell()) {
      return shell->GetRootScrollFrameAsScrollable();
    }
  }

  return nullptr;
}

void
Element::ScrollIntoView(const BooleanOrScrollIntoViewOptions& aObject)
{
  if (aObject.IsScrollIntoViewOptions()) {
    return ScrollIntoView(aObject.GetAsScrollIntoViewOptions());
  }

  MOZ_DIAGNOSTIC_ASSERT(aObject.IsBoolean());

  ScrollIntoViewOptions options;
  if (aObject.GetAsBoolean()) {
    options.mBlock = ScrollLogicalPosition::Start;
    options.mInline = ScrollLogicalPosition::Nearest;
  } else {
    options.mBlock = ScrollLogicalPosition::End;
    options.mInline = ScrollLogicalPosition::Nearest;
  }
  return ScrollIntoView(options);
}

void
Element::ScrollIntoView(const ScrollIntoViewOptions &aOptions)
{
  nsIDocument *document = GetComposedDoc();
  if (!document) {
    return;
  }

  // Get the presentation shell
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (!presShell) {
    return;
  }

  int16_t vpercent = nsIPresShell::SCROLL_CENTER;
  switch (aOptions.mBlock) {
    case ScrollLogicalPosition::Start:
      vpercent = nsIPresShell::SCROLL_TOP;
      break;
    case ScrollLogicalPosition::Center:
      vpercent = nsIPresShell::SCROLL_CENTER;
      break;
    case ScrollLogicalPosition::End:
      vpercent = nsIPresShell::SCROLL_BOTTOM;
      break;
    case ScrollLogicalPosition::Nearest:
      vpercent = nsIPresShell::SCROLL_MINIMUM;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected ScrollLogicalPosition value");
  }

  int16_t hpercent = nsIPresShell::SCROLL_CENTER;
  switch (aOptions.mInline) {
    case ScrollLogicalPosition::Start:
      hpercent = nsIPresShell::SCROLL_LEFT;
      break;
    case ScrollLogicalPosition::Center:
      hpercent = nsIPresShell::SCROLL_CENTER;
      break;
    case ScrollLogicalPosition::End:
      hpercent = nsIPresShell::SCROLL_RIGHT;
      break;
    case ScrollLogicalPosition::Nearest:
      hpercent = nsIPresShell::SCROLL_MINIMUM;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected ScrollLogicalPosition value");
  }

  uint32_t flags = nsIPresShell::SCROLL_OVERFLOW_HIDDEN;
  if (aOptions.mBehavior == ScrollBehavior::Smooth) {
    flags |= nsIPresShell::SCROLL_SMOOTH;
  } else if (aOptions.mBehavior == ScrollBehavior::Auto) {
    flags |= nsIPresShell::SCROLL_SMOOTH_AUTO;
  }

  presShell->ScrollContentIntoView(this,
                                   nsIPresShell::ScrollAxis(
                                     vpercent,
                                     nsIPresShell::SCROLL_ALWAYS),
                                   nsIPresShell::ScrollAxis(
                                     hpercent,
                                     nsIPresShell::SCROLL_ALWAYS),
                                   flags);
}

void
Element::Scroll(const CSSIntPoint& aScroll, const ScrollOptions& aOptions)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (sf) {
    nsIScrollableFrame::ScrollMode scrollMode = nsIScrollableFrame::INSTANT;
    if (aOptions.mBehavior == ScrollBehavior::Smooth) {
      scrollMode = nsIScrollableFrame::SMOOTH_MSD;
    } else if (aOptions.mBehavior == ScrollBehavior::Auto) {
      ScrollbarStyles styles = sf->GetScrollbarStyles();
      if (styles.mScrollBehavior == NS_STYLE_SCROLL_BEHAVIOR_SMOOTH) {
        scrollMode = nsIScrollableFrame::SMOOTH_MSD;
      }
    }

    sf->ScrollToCSSPixels(aScroll, scrollMode);
  }
}

void
Element::Scroll(double aXScroll, double aYScroll)
{
  // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
  auto scrollPos = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScroll),
                                         mozilla::ToZeroIfNonfinite(aYScroll));

  Scroll(scrollPos, ScrollOptions());
}

void
Element::Scroll(const ScrollToOptions& aOptions)
{
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    CSSIntPoint scrollPos = sf->GetScrollPositionCSSPixels();
    if (aOptions.mLeft.WasPassed()) {
      scrollPos.x = mozilla::ToZeroIfNonfinite(aOptions.mLeft.Value());
    }
    if (aOptions.mTop.WasPassed()) {
      scrollPos.y = mozilla::ToZeroIfNonfinite(aOptions.mTop.Value());
    }
    Scroll(scrollPos, aOptions);
  }
}

void
Element::ScrollTo(double aXScroll, double aYScroll)
{
  Scroll(aXScroll, aYScroll);
}

void
Element::ScrollTo(const ScrollToOptions& aOptions)
{
  Scroll(aOptions);
}

void
Element::ScrollBy(double aXScrollDif, double aYScrollDif)
{
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    CSSIntPoint scrollPos = sf->GetScrollPositionCSSPixels();
    scrollPos += CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScrollDif),
                                       mozilla::ToZeroIfNonfinite(aYScrollDif));
    Scroll(scrollPos, ScrollOptions());
  }
}

void
Element::ScrollBy(const ScrollToOptions& aOptions)
{
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    CSSIntPoint scrollPos = sf->GetScrollPositionCSSPixels();
    if (aOptions.mLeft.WasPassed()) {
      scrollPos.x += mozilla::ToZeroIfNonfinite(aOptions.mLeft.Value());
    }
    if (aOptions.mTop.WasPassed()) {
      scrollPos.y += mozilla::ToZeroIfNonfinite(aOptions.mTop.Value());
    }
    Scroll(scrollPos, aOptions);
  }
}

int32_t
Element::ScrollTop()
{
  nsIScrollableFrame* sf = GetScrollFrame();
  return sf ? sf->GetScrollPositionCSSPixels().y : 0;
}

void
Element::SetScrollTop(int32_t aScrollTop)
{
  // When aScrollTop is 0, we don't need to flush layout to scroll to that
  // point; we know 0 is always in range.  At least we think so...  But we do
  // need to flush frames so we ensure we find the right scrollable frame if
  // there is one.
  //
  // If aScrollTop is nonzero, we need to flush layout because we need to figure
  // out what our real scrollTopMax is.
  FlushType flushType = aScrollTop == 0 ? FlushType::Frames : FlushType::Layout;
  nsIScrollableFrame* sf = GetScrollFrame(nullptr, flushType);
  if (sf) {
    nsIScrollableFrame::ScrollMode scrollMode = nsIScrollableFrame::INSTANT;
    if (sf->GetScrollbarStyles().mScrollBehavior == NS_STYLE_SCROLL_BEHAVIOR_SMOOTH) {
      scrollMode = nsIScrollableFrame::SMOOTH_MSD;
    }
    sf->ScrollToCSSPixels(CSSIntPoint(sf->GetScrollPositionCSSPixels().x,
                                      aScrollTop),
                          scrollMode);
  }
}

int32_t
Element::ScrollLeft()
{
  nsIScrollableFrame* sf = GetScrollFrame();
  return sf ? sf->GetScrollPositionCSSPixels().x : 0;
}

void
Element::SetScrollLeft(int32_t aScrollLeft)
{
  // We can't assume things here based on the value of aScrollLeft, because
  // depending on our direction and layout 0 may or may not be in our scroll
  // range.  So we need to flush layout no matter what.
  nsIScrollableFrame* sf = GetScrollFrame();
  if (sf) {
    nsIScrollableFrame::ScrollMode scrollMode = nsIScrollableFrame::INSTANT;
    if (sf->GetScrollbarStyles().mScrollBehavior == NS_STYLE_SCROLL_BEHAVIOR_SMOOTH) {
      scrollMode = nsIScrollableFrame::SMOOTH_MSD;
    }

    sf->ScrollToCSSPixels(CSSIntPoint(aScrollLeft,
                                      sf->GetScrollPositionCSSPixels().y),
                          scrollMode);
  }
}


bool
Element::ScrollByNoFlush(int32_t aDx, int32_t aDy)
{
  nsIScrollableFrame* sf = GetScrollFrame(nullptr, FlushType::None);
  if (!sf) {
    return false;
  }

  AutoWeakFrame weakRef(sf->GetScrolledFrame());

  CSSIntPoint before = sf->GetScrollPositionCSSPixels();
  sf->ScrollToCSSPixelsApproximate(CSSIntPoint(before.x + aDx, before.y + aDy));

  // The frame was destroyed, can't keep on scrolling.
  if (!weakRef.IsAlive()) {
    return false;
  }

  CSSIntPoint after = sf->GetScrollPositionCSSPixels();
  return (before != after);
}

void
Element::MozScrollSnap()
{
  nsIScrollableFrame* sf = GetScrollFrame(nullptr, FlushType::None);
  if (sf) {
    sf->ScrollSnap();
  }
}

static nsSize GetScrollRectSizeForOverflowVisibleFrame(nsIFrame* aFrame)
{
  if (!aFrame) {
    return nsSize(0,0);
  }

  nsRect paddingRect = aFrame->GetPaddingRectRelativeToSelf();
  nsOverflowAreas overflowAreas(paddingRect, paddingRect);
  // Add the scrollable overflow areas of children (if any) to the paddingRect.
  // It's important to start with the paddingRect, otherwise if there are no
  // children the overflow rect will be 0,0,0,0 which will force the point 0,0
  // to be included in the final rect.
  nsLayoutUtils::UnionChildOverflow(aFrame, overflowAreas);
  // Make sure that an empty padding-rect's edges are included, by adding
  // the padding-rect in again with UnionEdges.
  nsRect overflowRect =
    overflowAreas.ScrollableOverflow().UnionEdges(paddingRect);
  return nsLayoutUtils::GetScrolledRect(aFrame,
      overflowRect, paddingRect.Size(),
      aFrame->StyleVisibility()->mDirection).Size();
}

int32_t
Element::ScrollHeight()
{
  if (IsSVGElement())
    return 0;

  nsIScrollableFrame* sf = GetScrollFrame();
  nscoord height;
  if (sf) {
    height = sf->GetScrollRange().Height() + sf->GetScrollPortRect().Height();
  } else {
    height = GetScrollRectSizeForOverflowVisibleFrame(GetStyledFrame()).height;
  }

  return nsPresContext::AppUnitsToIntCSSPixels(height);
}

int32_t
Element::ScrollWidth()
{
  if (IsSVGElement())
    return 0;

  nsIScrollableFrame* sf = GetScrollFrame();
  nscoord width;
  if (sf) {
    width = sf->GetScrollRange().Width() + sf->GetScrollPortRect().Width();
  } else {
    width = GetScrollRectSizeForOverflowVisibleFrame(GetStyledFrame()).width;
  }

  return nsPresContext::AppUnitsToIntCSSPixels(width);
}

nsRect
Element::GetClientAreaRect()
{
  nsIFrame* styledFrame;
  nsIScrollableFrame* sf = GetScrollFrame(&styledFrame);

  if (sf) {
    return sf->GetScrollPortRect();
  }

  if (styledFrame &&
      (styledFrame->StyleDisplay()->mDisplay != StyleDisplay::Inline ||
       styledFrame->IsFrameOfType(nsIFrame::eReplaced))) {
    // Special case code to make client area work even when there isn't
    // a scroll view, see bug 180552, bug 227567.
    return styledFrame->GetPaddingRect() - styledFrame->GetPositionIgnoringScrolling();
  }

  // SVG nodes reach here and just return 0
  return nsRect(0, 0, 0, 0);
}

already_AddRefed<DOMRect>
Element::GetBoundingClientRect()
{
  RefPtr<DOMRect> rect = new DOMRect(this);

  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);
  if (!frame) {
    // display:none, perhaps? Return the empty rect
    return rect.forget();
  }

  nsRect r = nsLayoutUtils::GetAllInFlowRectsUnion(frame,
          nsLayoutUtils::GetContainingBlockForClientRect(frame),
          nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
  rect->SetLayoutRect(r);
  return rect.forget();
}

already_AddRefed<DOMRectList>
Element::GetClientRects()
{
  RefPtr<DOMRectList> rectList = new DOMRectList(this);

  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);
  if (!frame) {
    // display:none, perhaps? Return an empty list
    return rectList.forget();
  }

  nsLayoutUtils::RectListBuilder builder(rectList);
  nsLayoutUtils::GetAllInFlowRects(frame,
          nsLayoutUtils::GetContainingBlockForClientRect(frame), &builder,
          nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
  return rectList.forget();
}

//----------------------------------------------------------------------

void
Element::AddToIdTable(nsAtom* aId)
{
  NS_ASSERTION(HasID(), "Node doesn't have an ID?");
  if (IsInShadowTree()) {
    ShadowRoot* containingShadow = GetContainingShadow();
    containingShadow->AddToIdTable(this, aId);
  } else {
    nsIDocument* doc = GetUncomposedDoc();
    if (doc && (!IsInAnonymousSubtree() || doc->IsXULDocument())) {
      doc->AddToIdTable(this, aId);
    }
  }
}

void
Element::RemoveFromIdTable()
{
  if (!HasID()) {
    return;
  }

  nsAtom* id = DoGetID();
  if (IsInShadowTree()) {
    ShadowRoot* containingShadow = GetContainingShadow();
    // Check for containingShadow because it may have
    // been deleted during unlinking.
    if (containingShadow) {
      containingShadow->RemoveFromIdTable(this, id);
    }
  } else {
    nsIDocument* doc = GetUncomposedDoc();
    if (doc && (!IsInAnonymousSubtree() || doc->IsXULDocument())) {
      doc->RemoveFromIdTable(this, id);
    }
  }
}

void
Element::SetSlot(const nsAString& aName, ErrorResult& aError)
{
  aError = SetAttr(kNameSpaceID_None, nsGkAtoms::slot, aName, true);
}

void
Element::GetSlot(nsAString& aName)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::slot, aName);
}

// https://dom.spec.whatwg.org/#dom-element-shadowroot
ShadowRoot*
Element::GetShadowRootByMode() const
{
  /**
   * 1. Let shadow be context object’s shadow root.
   * 2. If shadow is null or its mode is "closed", then return null.
   */
  ShadowRoot* shadowRoot = GetShadowRoot();
  if (!shadowRoot || shadowRoot->IsClosed()) {
    return nullptr;
  }

  /**
   * 3. Return shadow.
   */
  return shadowRoot;
}

// https://dom.spec.whatwg.org/#dom-element-attachshadow
already_AddRefed<ShadowRoot>
Element::AttachShadow(const ShadowRootInit& aInit, ErrorResult& aError)
{
  /**
   * 1. If context object’s namespace is not the HTML namespace,
   *    then throw a "NotSupportedError" DOMException.
   */
  if (!IsHTMLElement()) {
    aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  /**
   * 2. If context object’s local name is not
   *      a valid custom element name, "article", "aside", "blockquote",
   *      "body", "div", "footer", "h1", "h2", "h3", "h4", "h5", "h6",
   *      "header", "main" "nav", "p", "section", or "span",
   *    then throw a "NotSupportedError" DOMException.
   */
  nsAtom* nameAtom = NodeInfo()->NameAtom();
  if (!(nsContentUtils::IsCustomElementName(nameAtom, NodeInfo()->NamespaceID()) ||
        nameAtom == nsGkAtoms::article ||
        nameAtom == nsGkAtoms::aside ||
        nameAtom == nsGkAtoms::blockquote ||
        nameAtom == nsGkAtoms::body ||
        nameAtom == nsGkAtoms::div ||
        nameAtom == nsGkAtoms::footer ||
        nameAtom == nsGkAtoms::h1 ||
        nameAtom == nsGkAtoms::h2 ||
        nameAtom == nsGkAtoms::h3 ||
        nameAtom == nsGkAtoms::h4 ||
        nameAtom == nsGkAtoms::h5 ||
        nameAtom == nsGkAtoms::h6 ||
        nameAtom == nsGkAtoms::header ||
        nameAtom == nsGkAtoms::main ||
        nameAtom == nsGkAtoms::nav ||
        nameAtom == nsGkAtoms::p ||
        nameAtom == nsGkAtoms::section ||
        nameAtom == nsGkAtoms::span)) {
    aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  /**
   * 3. If context object is a shadow host, then throw
   *    an "InvalidStateError" DOMException.
   */
  if (GetShadowRoot() || GetXBLBinding()) {
    aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsAutoScriptBlocker scriptBlocker;

  RefPtr<mozilla::dom::NodeInfo> nodeInfo =
    mNodeInfo->NodeInfoManager()->GetNodeInfo(
      nsGkAtoms::documentFragmentNodeName, nullptr, kNameSpaceID_None,
      DOCUMENT_FRAGMENT_NODE);

  if (nsIDocument* doc = GetComposedDoc()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      shell->DestroyFramesForAndRestyle(this);
    }
  }
  MOZ_ASSERT(!GetPrimaryFrame());

  /**
   * 4. Let shadow be a new shadow root whose node document is
   *    context object’s node document, host is context object,
   *    and mode is init’s mode.
   */
  RefPtr<ShadowRoot> shadowRoot =
    new ShadowRoot(this, aInit.mMode, nodeInfo.forget());

  shadowRoot->SetIsComposedDocParticipant(IsInComposedDoc());

  /**
   * 5. Set context object’s shadow root to shadow.
   */
  SetShadowRoot(shadowRoot);

  /**
   * 6. Return shadow.
   */
  return shadowRoot.forget();
}

void
Element::GetAttribute(const nsAString& aName, DOMString& aReturn)
{
  const nsAttrValue* val =
    mAttrsAndChildren.GetAttr(aName,
                              IsHTMLElement() && IsInHTMLDocument() ?
                                eIgnoreCase : eCaseMatters);
  if (val) {
    val->ToString(aReturn);
  } else {
    if (IsXULElement()) {
      // XXX should be SetDOMStringToNull(aReturn);
      // See bug 232598
      // aReturn is already empty
    } else {
      aReturn.SetNull();
    }
  }
}

void
Element::SetAttribute(const nsAString& aName,
                      const nsAString& aValue,
                      nsIPrincipal* aTriggeringPrincipal,
                      ErrorResult& aError)
{
  aError = nsContentUtils::CheckQName(aName, false);
  if (aError.Failed()) {
    return;
  }

  nsAutoString nameToUse;
  const nsAttrName* name = InternalGetAttrNameFromQName(aName, &nameToUse);
  if (!name) {
    RefPtr<nsAtom> nameAtom = NS_AtomizeMainThread(nameToUse);
    if (!nameAtom) {
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    aError = SetAttr(kNameSpaceID_None, nameAtom, aValue, aTriggeringPrincipal, true);
    return;
  }

  aError = SetAttr(name->NamespaceID(), name->LocalName(), name->GetPrefix(),
                   aValue, aTriggeringPrincipal, true);
}

void
Element::RemoveAttribute(const nsAString& aName, ErrorResult& aError)
{
  const nsAttrName* name = InternalGetAttrNameFromQName(aName);

  if (!name) {
    // If there is no canonical nsAttrName for this attribute name, then the
    // attribute does not exist and we can't get its namespace ID and
    // local name below, so we return early.
    return;
  }

  // Hold a strong reference here so that the atom or nodeinfo doesn't go
  // away during UnsetAttr. If it did UnsetAttr would be left with a
  // dangling pointer as argument without knowing it.
  nsAttrName tmp(*name);

  aError = UnsetAttr(name->NamespaceID(), name->LocalName(), true);
}

Attr*
Element::GetAttributeNode(const nsAString& aName)
{
  return Attributes()->GetNamedItem(aName);
}

already_AddRefed<Attr>
Element::SetAttributeNode(Attr& aNewAttr, ErrorResult& aError)
{
  return Attributes()->SetNamedItemNS(aNewAttr, aError);
}

already_AddRefed<Attr>
Element::RemoveAttributeNode(Attr& aAttribute,
                             ErrorResult& aError)
{
  Element *elem = aAttribute.GetElement();
  if (elem != this) {
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  nsAutoString nameSpaceURI;
  aAttribute.NodeInfo()->GetNamespaceURI(nameSpaceURI);
  return Attributes()->RemoveNamedItemNS(nameSpaceURI, aAttribute.NodeInfo()->LocalName(), aError);
}

void
Element::GetAttributeNS(const nsAString& aNamespaceURI,
                        const nsAString& aLocalName,
                        nsAString& aReturn)
{
  int32_t nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI,
                                                       nsContentUtils::IsChromeDoc(OwnerDoc()));

  if (nsid == kNameSpaceID_Unknown) {
    // Unknown namespace means no attribute.
    SetDOMStringToNull(aReturn);
    return;
  }

  RefPtr<nsAtom> name = NS_AtomizeMainThread(aLocalName);
  bool hasAttr = GetAttr(nsid, name, aReturn);
  if (!hasAttr) {
    SetDOMStringToNull(aReturn);
  }
}

void
Element::SetAttributeNS(const nsAString& aNamespaceURI,
                        const nsAString& aQualifiedName,
                        const nsAString& aValue,
                        nsIPrincipal* aTriggeringPrincipal,
                        ErrorResult& aError)
{
  RefPtr<mozilla::dom::NodeInfo> ni;
  aError =
    nsContentUtils::GetNodeInfoFromQName(aNamespaceURI, aQualifiedName,
                                         mNodeInfo->NodeInfoManager(),
                                         ATTRIBUTE_NODE,
                                         getter_AddRefs(ni));
  if (aError.Failed()) {
    return;
  }

  aError = SetAttr(ni->NamespaceID(), ni->NameAtom(), ni->GetPrefixAtom(),
                   aValue, aTriggeringPrincipal, true);
}

void
Element::RemoveAttributeNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName,
                           ErrorResult& aError)
{
  RefPtr<nsAtom> name = NS_AtomizeMainThread(aLocalName);
  int32_t nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI,
                                                       nsContentUtils::IsChromeDoc(OwnerDoc()));

  if (nsid == kNameSpaceID_Unknown) {
    // If the namespace ID is unknown, it means there can't possibly be an
    // existing attribute. We would need a known namespace ID to pass into
    // UnsetAttr, so we return early if we don't have one.
    return;
  }

  aError = UnsetAttr(nsid, name, true);
}

Attr*
Element::GetAttributeNodeNS(const nsAString& aNamespaceURI,
                            const nsAString& aLocalName)
{
  return GetAttributeNodeNSInternal(aNamespaceURI, aLocalName);
}

Attr*
Element::GetAttributeNodeNSInternal(const nsAString& aNamespaceURI,
                                    const nsAString& aLocalName)
{
  return Attributes()->GetNamedItemNS(aNamespaceURI, aLocalName);
}

already_AddRefed<Attr>
Element::SetAttributeNodeNS(Attr& aNewAttr,
                            ErrorResult& aError)
{
  return Attributes()->SetNamedItemNS(aNewAttr, aError);
}

already_AddRefed<nsIHTMLCollection>
Element::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                const nsAString& aLocalName,
                                ErrorResult& aError)
{
  int32_t nameSpaceId = kNameSpaceID_Wildcard;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    aError =
      nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                            nameSpaceId);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  NS_ASSERTION(nameSpaceId != kNameSpaceID_Unknown, "Unexpected namespace ID!");

  return NS_GetContentList(this, nameSpaceId, aLocalName);
}

bool
Element::HasAttributeNS(const nsAString& aNamespaceURI,
                        const nsAString& aLocalName) const
{
  int32_t nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI,
                                                       nsContentUtils::IsChromeDoc(OwnerDoc()));

  if (nsid == kNameSpaceID_Unknown) {
    // Unknown namespace means no attr...
    return false;
  }

  RefPtr<nsAtom> name = NS_AtomizeMainThread(aLocalName);
  return HasAttr(nsid, name);
}

already_AddRefed<nsIHTMLCollection>
Element::GetElementsByClassName(const nsAString& aClassNames)
{
  return nsContentUtils::GetElementsByClassName(this, aClassNames);
}

void
Element::GetElementsWithGrid(nsTArray<RefPtr<Element>>& aElements)
{
  // This helper function is passed to GetElementsByMatching()
  // to identify elements with styling which will cause them to
  // generate a nsGridContainerFrame during layout.
  auto IsDisplayGrid = [](Element* aElement) -> bool
  {
    RefPtr<ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyle(aElement, nullptr);
    if (computedStyle) {
      const nsStyleDisplay* display = computedStyle->StyleDisplay();
      return (display->mDisplay == StyleDisplay::Grid ||
              display->mDisplay == StyleDisplay::InlineGrid);
    }
    return false;
  };

  GetElementsByMatching(IsDisplayGrid, aElements);
}

void
Element::GetElementsByMatching(nsElementMatchFunc aFunc,
                               nsTArray<RefPtr<Element>>& aElements)
{
  for (nsINode* cur = this; cur; cur = cur->GetNextNode(this)) {
    if (cur->IsElement() && aFunc(cur->AsElement())) {
      aElements.AppendElement(cur->AsElement());
    }
  }
}


/**
 * Returns the count of descendants (inclusive of aContent) in
 * the uncomposed document that are explicitly set as editable.
 */
static uint32_t
EditableInclusiveDescendantCount(nsIContent* aContent)
{
  auto htmlElem = nsGenericHTMLElement::FromNode(aContent);
  if (htmlElem) {
    return htmlElem->EditableInclusiveDescendantCount();
  }

  return aContent->EditableDescendantCount();
}

nsresult
Element::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                    nsIContent* aBindingParent,
                    bool aCompileEventHandlers)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION((NODE_FROM(aParent, aDocument)->OwnerDoc() == OwnerDoc()),
                  "Must have the same owner document");
  NS_PRECONDITION(!aParent || aDocument == aParent->GetUncomposedDoc(),
                  "aDocument must be current doc of aParent");
  NS_PRECONDITION(!GetUncomposedDoc(), "Already have a document.  Unbind first!");
  // Note that as we recurse into the kids, they'll have a non-null parent.  So
  // only assert if our parent is _changing_ while we have a parent.
  NS_PRECONDITION(!GetParent() || aParent == GetParent(),
                  "Already have a parent.  Unbind first!");
  NS_PRECONDITION(!GetBindingParent() ||
                  aBindingParent == GetBindingParent() ||
                  (!aBindingParent && aParent &&
                   aParent->GetBindingParent() == GetBindingParent()),
                  "Already have a binding parent.  Unbind first!");
  NS_PRECONDITION(aBindingParent != this,
                  "Content must not be its own binding parent");
  NS_PRECONDITION(!IsRootOfNativeAnonymousSubtree() ||
                  aBindingParent == aParent,
                  "Native anonymous content must have its parent as its "
                  "own binding parent");
  NS_PRECONDITION(aBindingParent || !aParent ||
                  aBindingParent == aParent->GetBindingParent(),
                  "We should be passed the right binding parent");

#ifdef MOZ_XUL
  // First set the binding parent
  nsXULElement* xulElem = nsXULElement::FromNode(this);
  if (xulElem) {
    xulElem->SetXULBindingParent(aBindingParent);
  }
  else
#endif
  {
    if (aBindingParent) {
      nsExtendedDOMSlots* slots = ExtendedDOMSlots();

      slots->mBindingParent = aBindingParent; // Weak, so no addref happens.
    }
  }
  NS_ASSERTION(!aBindingParent || IsRootOfNativeAnonymousSubtree() ||
               !HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE) ||
               (aParent && aParent->IsInNativeAnonymousSubtree()),
               "Trying to re-bind content from native anonymous subtree to "
               "non-native anonymous parent!");
  if (aParent) {
    if (aParent->IsInNativeAnonymousSubtree()) {
      SetFlags(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE);
    }
    if (aParent->HasFlag(NODE_CHROME_ONLY_ACCESS)) {
      SetFlags(NODE_CHROME_ONLY_ACCESS);
    }
    if (HasFlag(NODE_IS_ANONYMOUS_ROOT)) {
      aParent->SetMayHaveAnonymousChildren();
    }
    if (aParent->IsInShadowTree()) {
      ClearSubtreeRootPointer();
      SetFlags(NODE_IS_IN_SHADOW_TREE);
    }
    ShadowRoot* parentContainingShadow = aParent->GetContainingShadow();
    if (parentContainingShadow) {
      ExtendedDOMSlots()->mContainingShadow = parentContainingShadow;
    }
  }

  bool hadParent = !!GetParentNode();

  // Now set the parent.
  if (aParent) {
    if (!GetParent()) {
      NS_ADDREF(aParent);
    }
    mParent = aParent;
  }
  else {
    mParent = aDocument;
  }
  SetParentIsContent(aParent);

  // XXXbz sXBL/XBL2 issue!

  MOZ_ASSERT(!HasAnyOfFlags(Element::kAllServoDescendantBits));

  // Finally, set the document
  if (aDocument) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    // XXXbz ordering issues here?  Probably not, since ChangeDocumentFor is
    // just pretty broken anyway....  Need to get it working.
    // XXXbz XBL doesn't handle this (asserts), and we don't really want
    // to be doing this during parsing anyway... sort this out.
    //    aDocument->BindingManager()->ChangeDocumentFor(this, nullptr,
    //                                                   aDocument);

    // We no longer need to track the subtree pointer (and in fact we'll assert
    // if we do this any later).
    ClearSubtreeRootPointer();

    // Being added to a document.
    SetIsInDocument();

    // Clear the lazy frame construction bits.
    UnsetFlags(NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES);
  } else if (IsInShadowTree()) {
    // We're not in a document, but we did get inserted into a shadow tree.
    // Since we won't have any restyle data in the document's restyle trackers,
    // don't let us get inserted with restyle bits set incorrectly.
    //
    // Also clear all the other flags that are cleared above when we do get
    // inserted into a document.
    //
    // See the comment about the restyle bits above, it also applies.
    UnsetFlags(NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES);
  } else {
    // If we're not in the doc and not in a shadow tree,
    // update our subtree pointer.
    SetSubtreeRootPointer(aParent->SubtreeRoot());
  }

  if (CustomElementRegistry::IsCustomElementEnabled(OwnerDoc()) && IsInComposedDoc()) {
    // Connected callback must be enqueued whenever a custom element becomes
    // connected.
    CustomElementData* data = GetCustomElementData();
    if (data) {
      if (data->mState == CustomElementData::State::eCustom) {
        nsContentUtils::EnqueueLifecycleCallback(nsIDocument::eConnected, this);
      } else {
        // Step 7.7.2.2 https://dom.spec.whatwg.org/#concept-node-insert
        nsContentUtils::TryToUpgradeElement(this);
      }
    }
  }

  // Propagate scoped style sheet tracking bit.
  if (mParent->IsContent()) {
    nsIContent* parent = mParent->AsContent();
    if (ShadowRoot* shadowRootParent = ShadowRoot::FromNode(parent)) {
      parent = shadowRootParent->GetHost();
    }
  }

  // This has to be here, rather than in nsGenericHTMLElement::BindToTree,
  //  because it has to happen after updating the parent pointer, but before
  //  recursively binding the kids.
  if (IsHTMLElement()) {
    SetDirOnBind(this, aParent);
  }

  uint32_t editableDescendantCount = 0;

  UpdateEditableState(false);

  // If we had a pre-existing XBL binding, we might have anonymous children that
  // also need to be told that they are moving.
  if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsXBLBinding* binding =
      OwnerDoc()->BindingManager()->GetBindingWithContent(this);

    if (binding) {
      binding->BindAnonymousContent(
        binding->GetAnonymousContent(),
        this,
        binding->PrototypeBinding()->ChromeOnlyContent());
    }
  }

  // Now recurse into our kids
  nsresult rv;
  for (nsIContent* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    rv = child->BindToTree(aDocument, this, aBindingParent,
                           aCompileEventHandlers);
    NS_ENSURE_SUCCESS(rv, rv);

    editableDescendantCount += EditableInclusiveDescendantCount(child);
  }

  if (aDocument) {
    // Update our editable descendant count because we don't keep track of it
    // for content that is not in the uncomposed document.
    MOZ_ASSERT(EditableDescendantCount() == 0);
    ChangeEditableDescendantCount(editableDescendantCount);

    if (!hadParent) {
      uint32_t editableDescendantChange = EditableInclusiveDescendantCount(this);
      if (editableDescendantChange != 0) {
        // If we are binding a subtree root to the document, we need to update
        // the editable descendant count of all the ancestors.
        // But we don't cross Shadow DOM boundary.
        // (The expected behavior with Shadow DOM is unclear)
        nsIContent* parent = GetParent();
        while (parent && parent->IsElement()) {
          parent->ChangeEditableDescendantCount(editableDescendantChange);
          parent = parent->GetParent();
        }
      }
    }
  }

  nsNodeUtils::ParentChainChanged(this);
  if (!hadParent && IsRootOfNativeAnonymousSubtree()) {
    nsNodeUtils::NativeAnonymousChildListChange(this, false);
  }

  if (HasID()) {
    AddToIdTable(DoGetID());
  }

  if (MayHaveStyle() && !IsXULElement()) {
    // XXXbz if we already have a style attr parsed, this won't do
    // anything... need to fix that.
    // If MayHaveStyle() is true, we must be an nsStyledElement
    static_cast<nsStyledElement*>(this)->ReparseStyleAttribute(false, false);
  }

  // Call BindToTree on shadow root children.
  if (ShadowRoot* shadowRoot = GetShadowRoot()) {
    shadowRoot->SetIsComposedDocParticipant(IsInComposedDoc());
    for (nsIContent* child = shadowRoot->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      rv = child->BindToTree(nullptr, shadowRoot,
                             shadowRoot->GetBindingParent(),
                             aCompileEventHandlers);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Pseudo-elements implemented by JS must have the NODE_IS_NATIVE_ANONYMOUS
  // flag set on them. For C++-created pseudo-elements, this is done in
  // nsCSSFrameConstructor::GetAnonymousContent, but any JS that creates
  // pseudo-elements would run after that. So we set that flag here,
  // when the element implementing the pseudo is inserted into the document.
  // We maintain the invariant that any NAC-implemented pseudo-element's
  // anonymous ancestors are also flagged as NAC, which the style system relies on.
  if (aDocument) {
    CSSPseudoElementType pseudoType = GetPseudoElementType();
    if (pseudoType != CSSPseudoElementType::NotPseudo &&
        nsCSSPseudoElements::PseudoElementIsJSCreatedNAC(pseudoType)) {
      SetFlags(NODE_IS_NATIVE_ANONYMOUS);
      nsIContent* parent = aParent;
      while (parent && !parent->IsRootOfNativeAnonymousSubtree()) {
        MOZ_ASSERT(parent->IsInNativeAnonymousSubtree());
        parent->SetFlags(NODE_IS_NATIVE_ANONYMOUS);
        parent = parent->GetParent();
      }
      MOZ_ASSERT(parent);
    }

    if (MayHaveAnimations() &&
        (pseudoType == CSSPseudoElementType::NotPseudo ||
         pseudoType == CSSPseudoElementType::before ||
         pseudoType == CSSPseudoElementType::after) &&
        EffectSet::GetEffectSet(this, pseudoType)) {
      if (nsPresContext* presContext = aDocument->GetPresContext()) {
        presContext->EffectCompositor()->
          RequestRestyle(this, pseudoType,
                         EffectCompositor::RestyleType::Standard,
                         EffectCompositor::CascadeLevel::Animations);
      }
    }
  }

  // XXXbz script execution during binding can trigger some of these
  // postcondition asserts....  But we do want that, since things will
  // generally be quite broken when that happens.
  MOZ_ASSERT(aDocument == GetUncomposedDoc(), "Bound to wrong document");
  MOZ_ASSERT(aParent == GetParent(), "Bound to wrong parent");
  MOZ_ASSERT(aBindingParent == GetBindingParent(),
             "Bound to wrong binding parent");

  return NS_OK;
}

RemoveFromBindingManagerRunnable::RemoveFromBindingManagerRunnable(
  nsBindingManager* aManager,
  nsIContent* aContent,
  nsIDocument* aDoc)
  : mozilla::Runnable("dom::RemoveFromBindingManagerRunnable")
  , mManager(aManager)
  , mContent(aContent)
  , mDoc(aDoc)
{}

RemoveFromBindingManagerRunnable::~RemoveFromBindingManagerRunnable() {}

NS_IMETHODIMP
RemoveFromBindingManagerRunnable::Run()
{
  // It may be the case that the element was removed from the
  // DOM, causing this runnable to be created, then inserted back
  // into the document before the this runnable had a chance to
  // tear down the binding. Only tear down the binding if the element
  // is still no longer in the DOM. nsXBLService::LoadBinding tears
  // down the old binding if the element is inserted back into the
  // DOM and loads a different binding.
  if (!mContent->IsInComposedDoc()) {
    mManager->RemovedFromDocumentInternal(mContent, mDoc,
                                          nsBindingManager::eRunDtor);
  }

  return NS_OK;
}


void
Element::UnbindFromTree(bool aDeep, bool aNullParent)
{
  NS_PRECONDITION(aDeep || (!GetUncomposedDoc() && !GetBindingParent()),
                  "Shallow unbind won't clear document and binding parent on "
                  "kids!");

  RemoveFromIdTable();

  // Make sure to unbind this node before doing the kids
  nsIDocument* document = GetComposedDoc();

  if (HasPointerLock()) {
    nsIDocument::UnlockPointer();
  }
  if (mState.HasState(NS_EVENT_STATE_FULL_SCREEN)) {
    // The element being removed is an ancestor of the full-screen element,
    // exit full-screen state.
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("DOM"), OwnerDoc(),
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "RemovedFullscreenElement");
    // Fully exit full-screen.
    nsIDocument::ExitFullscreenInDocTree(OwnerDoc());
  }

  if (HasServoData()) {
    MOZ_ASSERT(document);
    MOZ_ASSERT(IsInAnonymousSubtree());
  }

  if (document) {
    ClearServoData(document);
  }

  if (aNullParent) {
    if (GetParent() && GetParent()->IsInUncomposedDoc()) {
      // Update the editable descendant count in the ancestors before we
      // lose the reference to the parent.
      int32_t editableDescendantChange = -1 * EditableInclusiveDescendantCount(this);
      if (editableDescendantChange != 0) {
        nsIContent* parent = GetParent();
        while (parent) {
          parent->ChangeEditableDescendantCount(editableDescendantChange);
          parent = parent->GetParent();
        }
      }
    }

    if (this->IsRootOfNativeAnonymousSubtree()) {
      nsNodeUtils::NativeAnonymousChildListChange(this, true);
    }

    if (GetParent()) {
      RefPtr<nsINode> p;
      p.swap(mParent);
    } else {
      mParent = nullptr;
    }
    SetParentIsContent(false);
  }

#ifdef DEBUG
  // If we can get access to the PresContext, then we sanity-check that
  // we're not leaving behind a pointer to ourselves as the PresContext's
  // cached provider of the viewport's scrollbar styles.
  if (document) {
    nsPresContext* presContext = document->GetPresContext();
    if (presContext) {
      MOZ_ASSERT(this !=
                 presContext->GetViewportScrollbarStylesOverrideElement(),
                 "Leaving behind a raw pointer to this element (as having "
                 "propagated scrollbar styles) - that's dangerous...");
    }
  }
#endif

  ClearInDocument();

  // Ensure that CSS transitions don't continue on an element at a
  // different place in the tree (even if reinserted before next
  // animation refresh).
  // We need to delete the properties while we're still in document
  // (if we were in document).
  // FIXME (Bug 522599): Need a test for this.
  if (MayHaveAnimations()) {
    DeleteProperty(nsGkAtoms::transitionsOfBeforeProperty);
    DeleteProperty(nsGkAtoms::transitionsOfAfterProperty);
    DeleteProperty(nsGkAtoms::transitionsProperty);
    DeleteProperty(nsGkAtoms::animationsOfBeforeProperty);
    DeleteProperty(nsGkAtoms::animationsOfAfterProperty);
    DeleteProperty(nsGkAtoms::animationsProperty);
    if (document) {
      if (nsPresContext* presContext = document->GetPresContext()) {
        // We have to clear all pending restyle requests for the animations on
        // this element to avoid unnecessary restyles when we re-attached this
        // element.
        presContext->EffectCompositor()->ClearRestyleRequestsFor(this);
      }
    }
  }

  // Editable descendant count only counts descendants that
  // are in the uncomposed document.
  ResetEditableDescendantCount();

  if (aNullParent || !mParent->IsInShadowTree()) {
    UnsetFlags(NODE_IS_IN_SHADOW_TREE);

    // Begin keeping track of our subtree root.
    SetSubtreeRootPointer(aNullParent ? this : mParent->SubtreeRoot());
  }

  bool clearBindingParent = true;

#ifdef MOZ_XUL
  if (nsXULElement* xulElem = nsXULElement::FromNode(this)) {;
    xulElem->SetXULBindingParent(nullptr);
    clearBindingParent = false;
  }
#endif

  if (nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots()) {
    if (clearBindingParent) {
      slots->mBindingParent = nullptr;
    }
    if (aNullParent || !mParent->IsInShadowTree()) {
      slots->mContainingShadow = nullptr;
    }
  }

  if (document) {
    if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
      // Notify XBL- & nsIAnonymousContentCreator-generated anonymous content
      // that the document is changing.
      nsContentUtils::AddScriptRunner(
        new RemoveFromBindingManagerRunnable(
          document->BindingManager(), this, document));
      nsXBLBinding* binding =
        document->BindingManager()->GetBindingWithContent(this);
      if (binding) {
        nsXBLBinding::UnbindAnonymousContent(
          document,
          binding->GetAnonymousContent(),
          /* aNullParent */ false);
      }
    }

    document->ClearBoxObjectFor(this);

     // Disconnected must be enqueued whenever a connected custom element becomes
     // disconnected.
    if (CustomElementRegistry::IsCustomElementEnabled(OwnerDoc())) {
      CustomElementData* data  = GetCustomElementData();
      if (data) {
        if (data->mState == CustomElementData::State::eCustom) {
          nsContentUtils::EnqueueLifecycleCallback(nsIDocument::eDisconnected,
                                                   this);
        } else {
          // Remove an unresolved custom element that is a candidate for upgrade
          // when a custom element is disconnected.
          nsContentUtils::UnregisterUnresolvedElement(this);
        }
      }
    }
  }

  // This has to be here, rather than in nsGenericHTMLElement::UnbindFromTree,
  //  because it has to happen after unsetting the parent pointer, but before
  //  recursively unbinding the kids.
  if (IsHTMLElement()) {
    ResetDir(this);
  }

  if (aDeep) {
    // Do the kids. Don't call GetChildCount() here since that'll force
    // XUL to generate template children, which there is no need for since
    // all we're going to do is unbind them anyway.
    uint32_t i, n = mAttrsAndChildren.ChildCount();

    for (i = 0; i < n; ++i) {
      // Note that we pass false for aNullParent here, since we don't want
      // the kids to forget us.  We _do_ want them to forget their binding
      // parent, though, since this only walks non-anonymous kids.
      mAttrsAndChildren.ChildAt(i)->UnbindFromTree(true, false);
    }
  }

  nsNodeUtils::ParentChainChanged(this);

  // Unbind children of shadow root.
  if (ShadowRoot* shadowRoot = GetShadowRoot()) {
    for (nsIContent* child = shadowRoot->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      child->UnbindFromTree(true, false);
    }

    shadowRoot->SetIsComposedDocParticipant(false);
  }

  MOZ_ASSERT(!HasAnyOfFlags(kAllServoDescendantBits));
  MOZ_ASSERT(!document || document->GetServoRestyleRoot() != this);
}

nsDOMCSSAttributeDeclaration*
Element::GetSMILOverrideStyle()
{
  Element::nsExtendedDOMSlots* slots = ExtendedDOMSlots();

  if (!slots->mSMILOverrideStyle) {
    slots->mSMILOverrideStyle = new nsDOMCSSAttributeDeclaration(this, true);
  }

  return slots->mSMILOverrideStyle;
}

DeclarationBlock*
Element::GetSMILOverrideStyleDeclaration()
{
  Element::nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
  return slots ? slots->mSMILOverrideStyleDeclaration.get() : nullptr;
}

nsresult
Element::SetSMILOverrideStyleDeclaration(DeclarationBlock* aDeclaration,
                                         bool aNotify)
{
  Element::nsExtendedDOMSlots* slots = ExtendedDOMSlots();

  slots->mSMILOverrideStyleDeclaration = aDeclaration;

  if (aNotify) {
    nsIDocument* doc = GetComposedDoc();
    // Only need to request a restyle if we're in a document.  (We might not
    // be in a document, if we're clearing animation effects on a target node
    // that's been detached since the previous animation sample.)
    if (doc) {
      nsCOMPtr<nsIPresShell> shell = doc->GetShell();
      if (shell) {
        shell->RestyleForAnimation(this, eRestyle_StyleAttribute_Animations);
      }
    }
  }

  return NS_OK;
}

bool
Element::IsLabelable() const
{
  return false;
}

bool
Element::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return false;
}

DeclarationBlock*
Element::GetInlineStyleDeclaration() const
{
  if (!MayHaveStyle()) {
    return nullptr;
  }
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);

  if (attrVal && attrVal->Type() == nsAttrValue::eCSSDeclaration) {
    return attrVal->GetCSSDeclarationValue();
  }

  return nullptr;
}

const nsMappedAttributes*
Element::GetMappedAttributes() const
{
  return mAttrsAndChildren.GetMapped();
}

nsresult
Element::SetInlineStyleDeclaration(DeclarationBlock* aDeclaration,
                                   const nsAString* aSerialized,
                                   bool aNotify)
{
  MOZ_ASSERT_UNREACHABLE("Element::SetInlineStyleDeclaration");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
Element::IsAttributeMapped(const nsAtom* aAttribute) const
{
  return false;
}

nsChangeHint
Element::GetAttributeChangeHint(const nsAtom* aAttribute,
                                int32_t aModType) const
{
  return nsChangeHint(0);
}

bool
Element::FindAttributeDependence(const nsAtom* aAttribute,
                                 const MappedAttributeEntry* const aMaps[],
                                 uint32_t aMapCount)
{
  for (uint32_t mapindex = 0; mapindex < aMapCount; ++mapindex) {
    for (const MappedAttributeEntry* map = aMaps[mapindex];
         map->attribute; ++map) {
      if (aAttribute == *map->attribute) {
        return true;
      }
    }
  }

  return false;
}

already_AddRefed<mozilla::dom::NodeInfo>
Element::GetExistingAttrNameFromQName(const nsAString& aStr) const
{
  const nsAttrName* name = InternalGetAttrNameFromQName(aStr);
  if (!name) {
    return nullptr;
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  if (name->IsAtom()) {
    nodeInfo = mNodeInfo->NodeInfoManager()->
      GetNodeInfo(name->Atom(), nullptr, kNameSpaceID_None, ATTRIBUTE_NODE);
  }
  else {
    nodeInfo = name->NodeInfo();
  }

  return nodeInfo.forget();
}

// static
bool
Element::ShouldBlur(nsIContent *aContent)
{
  // Determine if the current element is focused, if it is not focused
  // then we should not try to blur
  nsIDocument* document = aContent->GetComposedDoc();
  if (!document)
    return false;

  nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
  if (!window)
    return false;

  nsCOMPtr<nsPIDOMWindowOuter> focusedFrame;
  nsIContent* contentToBlur =
    nsFocusManager::GetFocusedDescendant(window,
                                         nsFocusManager::eOnlyCurrentWindow,
                                         getter_AddRefs(focusedFrame));
  if (contentToBlur == aContent)
    return true;

  // if focus on this element would get redirected, then check the redirected
  // content as well when blurring.
  return (contentToBlur && nsFocusManager::GetRedirectedFocus(aContent) == contentToBlur);
}

bool
Element::IsNodeOfType(uint32_t aFlags) const
{
  return false;
}

/* static */
nsresult
Element::DispatchEvent(nsPresContext* aPresContext,
                       WidgetEvent* aEvent,
                       nsIContent* aTarget,
                       bool aFullDispatch,
                       nsEventStatus* aStatus)
{
  NS_PRECONDITION(aTarget, "Must have target");
  NS_PRECONDITION(aEvent, "Must have source event");
  NS_PRECONDITION(aStatus, "Null out param?");

  if (!aPresContext) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> shell = aPresContext->GetPresShell();
  if (!shell) {
    return NS_OK;
  }

  if (aFullDispatch) {
    return shell->HandleEventWithTarget(aEvent, nullptr, aTarget, aStatus);
  }

  return shell->HandleDOMEventWithTarget(aTarget, aEvent, aStatus);
}

/* static */
nsresult
Element::DispatchClickEvent(nsPresContext* aPresContext,
                            WidgetInputEvent* aSourceEvent,
                            nsIContent* aTarget,
                            bool aFullDispatch,
                            const EventFlags* aExtraEventFlags,
                            nsEventStatus* aStatus)
{
  NS_PRECONDITION(aTarget, "Must have target");
  NS_PRECONDITION(aSourceEvent, "Must have source event");
  NS_PRECONDITION(aStatus, "Null out param?");

  WidgetMouseEvent event(aSourceEvent->IsTrusted(), eMouseClick,
                         aSourceEvent->mWidget, WidgetMouseEvent::eReal);
  event.mRefPoint = aSourceEvent->mRefPoint;
  uint32_t clickCount = 1;
  float pressure = 0;
  uint32_t pointerId = 0; // Use the default value here.
  uint16_t inputSource = 0;
  WidgetMouseEvent* sourceMouseEvent = aSourceEvent->AsMouseEvent();
  if (sourceMouseEvent) {
    clickCount = sourceMouseEvent->mClickCount;
    pressure = sourceMouseEvent->pressure;
    pointerId = sourceMouseEvent->pointerId;
    inputSource = sourceMouseEvent->inputSource;
  } else if (aSourceEvent->mClass == eKeyboardEventClass) {
    event.mFlags.mIsPositionless = true;
    inputSource = MouseEventBinding::MOZ_SOURCE_KEYBOARD;
  }
  event.pressure = pressure;
  event.mClickCount = clickCount;
  event.pointerId = pointerId;
  event.inputSource = inputSource;
  event.mModifiers = aSourceEvent->mModifiers;
  if (aExtraEventFlags) {
    // Be careful not to overwrite existing flags!
    event.mFlags.Union(*aExtraEventFlags);
  }

  return DispatchEvent(aPresContext, &event, aTarget, aFullDispatch, aStatus);
}

nsIFrame*
Element::GetPrimaryFrame(FlushType aType)
{
  nsIDocument* doc = GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  // Cause a flush, so we get up-to-date frame
  // information
  if (aType != FlushType::None) {
    doc->FlushPendingNotifications(aType);
  }

  return GetPrimaryFrame();
}

//----------------------------------------------------------------------
nsresult
Element::LeaveLink(nsPresContext* aPresContext)
{
  nsILinkHandler *handler = aPresContext->GetLinkHandler();
  if (!handler) {
    return NS_OK;
  }

  return handler->OnLeaveLink();
}

nsresult
Element::SetEventHandler(nsAtom* aEventName,
                         const nsAString& aValue,
                         bool aDefer)
{
  nsIDocument *ownerDoc = OwnerDoc();
  if (ownerDoc->IsLoadedAsData()) {
    // Make this a no-op rather than throwing an error to avoid
    // the error causing problems setting the attribute.
    return NS_OK;
  }

  NS_PRECONDITION(aEventName, "Must have event name!");
  bool defer = true;
  EventListenerManager* manager =
    GetEventListenerManagerForAttr(aEventName, &defer);
  if (!manager) {
    return NS_OK;
  }

  defer = defer && aDefer; // only defer if everyone agrees...
  manager->SetEventHandler(aEventName, aValue,
                           defer, !nsContentUtils::IsChromeDoc(ownerDoc),
                           this);
  return NS_OK;
}


//----------------------------------------------------------------------

const nsAttrName*
Element::InternalGetAttrNameFromQName(const nsAString& aStr,
                                      nsAutoString* aNameToUse) const
{
  MOZ_ASSERT(!aNameToUse || aNameToUse->IsEmpty());
  const nsAttrName* val = nullptr;
  if (IsHTMLElement() && IsInHTMLDocument()) {
    nsAutoString lower;
    nsAutoString& outStr = aNameToUse ? *aNameToUse : lower;
    nsContentUtils::ASCIIToLower(aStr, outStr);
    val = mAttrsAndChildren.GetExistingAttrNameFromQName(outStr);
    if (val) {
      outStr.Truncate();
    }
  } else {
    val = mAttrsAndChildren.GetExistingAttrNameFromQName(aStr);
    if (!val && aNameToUse) {
      *aNameToUse = aStr;
    }
  }

  return val;
}

bool
Element::MaybeCheckSameAttrVal(int32_t aNamespaceID,
                               nsAtom* aName,
                               nsAtom* aPrefix,
                               const nsAttrValueOrString& aValue,
                               bool aNotify,
                               nsAttrValue& aOldValue,
                               uint8_t* aModType,
                               bool* aHasListeners,
                               bool* aOldValueSet)
{
  bool modification = false;
  *aHasListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);
  *aOldValueSet = false;

  // If we have no listeners and aNotify is false, we are almost certainly
  // coming from the content sink and will almost certainly have no previous
  // value.  Even if we do, setting the value is cheap when we have no
  // listeners and don't plan to notify.  The check for aNotify here is an
  // optimization, the check for *aHasListeners is a correctness issue.
  if (*aHasListeners || aNotify) {
    BorrowedAttrInfo info(GetAttrInfo(aNamespaceID, aName));
    if (info.mValue) {
      // Check whether the old value is the same as the new one.  Note that we
      // only need to actually _get_ the old value if we have listeners or
      // if the element is a custom element (because it may have an
      // attribute changed callback).
      if (*aHasListeners || GetCustomElementData()) {
        // Need to store the old value.
        //
        // If the current attribute value contains a pointer to some other data
        // structure that gets updated in the process of setting the attribute
        // we'll no longer have the old value of the attribute. Therefore, we
        // should serialize the attribute value now to keep a snapshot.
        //
        // We have to serialize the value anyway in order to create the
        // mutation event so there's no cost in doing it now.
        aOldValue.SetToSerialized(*info.mValue);
        *aOldValueSet = true;
      }
      bool valueMatches = aValue.EqualsAsStrings(*info.mValue);
      if (valueMatches && aPrefix == info.mName->GetPrefix()) {
        return true;
      }
      modification = true;
    }
  }
  *aModType = modification ?
    static_cast<uint8_t>(MutationEventBinding::MODIFICATION) :
    static_cast<uint8_t>(MutationEventBinding::ADDITION);
  return false;
}

bool
Element::OnlyNotifySameValueSet(int32_t aNamespaceID, nsAtom* aName,
                                nsAtom* aPrefix,
                                const nsAttrValueOrString& aValue,
                                bool aNotify, nsAttrValue& aOldValue,
                                uint8_t* aModType, bool* aHasListeners,
                                bool* aOldValueSet)
{
  if (!MaybeCheckSameAttrVal(aNamespaceID, aName, aPrefix, aValue, aNotify,
                             aOldValue, aModType, aHasListeners,
                             aOldValueSet)) {
    return false;
  }

  nsAutoScriptBlocker scriptBlocker;
  nsNodeUtils::AttributeSetToCurrentValue(this, aNamespaceID, aName);
  return true;
}

nsresult
Element::SetSingleClassFromParser(nsAtom* aSingleClassName)
{
  // Keep this in sync with SetAttr and SetParsedAttr below.

  if (!mAttrsAndChildren.CanFitMoreAttrs()) {
    return NS_ERROR_FAILURE;
  }

  nsAttrValue value(aSingleClassName);

  nsIDocument* document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, false);

  // In principle, BeforeSetAttr should be called here if a node type
  // existed that wanted to do something special for class, but there
  // is no such node type, so calling SetMayHaveClass() directly.
  SetMayHaveClass();

  return SetAttrAndNotify(kNameSpaceID_None,
                          nsGkAtoms::_class,
                          nullptr, // prefix
                          nullptr, // old value
                          value,
                          nullptr,
                          static_cast<uint8_t>(MutationEventBinding::ADDITION),
                          false, // hasListeners
                          false, // notify
                          kCallAfterSetAttr,
                          document,
                          updateBatch);
}

nsresult
Element::SetAttr(int32_t aNamespaceID, nsAtom* aName,
                 nsAtom* aPrefix, const nsAString& aValue,
                 nsIPrincipal* aSubjectPrincipal,
                 bool aNotify)
{
  // Keep this in sync with SetParsedAttr below and SetSingleClassFromParser
  // above.

  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");

  if (!mAttrsAndChildren.CanFitMoreAttrs()) {
    return NS_ERROR_FAILURE;
  }

  uint8_t modType;
  bool hasListeners;
  // We don't want to spend time preparsing class attributes if the value is not
  // changing, so just init our nsAttrValueOrString with aValue for the
  // OnlyNotifySameValueSet call.
  nsAttrValueOrString value(aValue);
  nsAttrValue oldValue;
  bool oldValueSet;

  if (OnlyNotifySameValueSet(aNamespaceID, aName, aPrefix, value, aNotify,
                             oldValue, &modType, &hasListeners, &oldValueSet)) {
    return OnAttrSetButNotChanged(aNamespaceID, aName, value, aNotify);
  }

  nsAttrValue attrValue;
  nsAttrValue* preparsedAttrValue;
  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::_class) {
    attrValue.ParseAtomArray(aValue);
    value.ResetToAttrValue(attrValue);
    preparsedAttrValue = &attrValue;
  } else {
    preparsedAttrValue = nullptr;
  }

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNamespaceID, aName, modType,
                                     preparsedAttrValue);
  }

  // Hold a script blocker while calling ParseAttribute since that can call
  // out to id-observers
  nsIDocument* document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  nsresult rv = BeforeSetAttr(aNamespaceID, aName, &value, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!preparsedAttrValue &&
      !ParseAttribute(aNamespaceID, aName, aValue, aSubjectPrincipal,
                      attrValue)) {
    attrValue.SetTo(aValue);
  }

  PreIdMaybeChange(aNamespaceID, aName, &value);

  return SetAttrAndNotify(aNamespaceID, aName, aPrefix,
                          oldValueSet ? &oldValue : nullptr,
                          attrValue, aSubjectPrincipal, modType,
                          hasListeners, aNotify,
                          kCallAfterSetAttr, document, updateBatch);
}

nsresult
Element::SetParsedAttr(int32_t aNamespaceID, nsAtom* aName,
                       nsAtom* aPrefix, nsAttrValue& aParsedValue,
                       bool aNotify)
{
  // Keep this in sync with SetAttr and SetSingleClassFromParser above

  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");

  if (!mAttrsAndChildren.CanFitMoreAttrs()) {
    return NS_ERROR_FAILURE;
  }


  uint8_t modType;
  bool hasListeners;
  nsAttrValueOrString value(aParsedValue);
  nsAttrValue oldValue;
  bool oldValueSet;

  if (OnlyNotifySameValueSet(aNamespaceID, aName, aPrefix, value, aNotify,
                             oldValue, &modType, &hasListeners, &oldValueSet)) {
    return OnAttrSetButNotChanged(aNamespaceID, aName, value, aNotify);
  }

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNamespaceID, aName, modType,
                                     &aParsedValue);
  }

  nsresult rv = BeforeSetAttr(aNamespaceID, aName, &value, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  PreIdMaybeChange(aNamespaceID, aName, &value);

  nsIDocument* document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);
  return SetAttrAndNotify(aNamespaceID, aName, aPrefix,
                          oldValueSet ? &oldValue : nullptr,
                          aParsedValue, nullptr, modType, hasListeners, aNotify,
                          kCallAfterSetAttr, document, updateBatch);
}

nsresult
Element::SetAttrAndNotify(int32_t aNamespaceID,
                          nsAtom* aName,
                          nsAtom* aPrefix,
                          const nsAttrValue* aOldValue,
                          nsAttrValue& aParsedValue,
                          nsIPrincipal* aSubjectPrincipal,
                          uint8_t aModType,
                          bool aFireMutation,
                          bool aNotify,
                          bool aCallAfterSetAttr,
                          nsIDocument* aComposedDocument,
                          const mozAutoDocUpdate&)
{
  nsresult rv;
  nsMutationGuard::DidMutate();

  // Copy aParsedValue for later use since it will be lost when we call
  // SetAndSwapMappedAttr below
  nsAttrValue valueForAfterSetAttr;
  if (aCallAfterSetAttr || GetCustomElementData()) {
    valueForAfterSetAttr.SetTo(aParsedValue);
  }

  bool hadValidDir = false;
  bool hadDirAuto = false;
  bool oldValueSet;

  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::dir) {
      hadValidDir = HasValidDir() || IsHTMLElement(nsGkAtoms::bdi);
      hadDirAuto = HasDirAuto(); // already takes bdi into account
    }

    // XXXbz Perhaps we should push up the attribute mapping function
    // stuff to Element?
    if (!IsAttributeMapped(aName) ||
        !SetAndSwapMappedAttribute(aName, aParsedValue, &oldValueSet, &rv)) {
      rv = mAttrsAndChildren.SetAndSwapAttr(aName, aParsedValue, &oldValueSet);
    }
  }
  else {
    RefPtr<mozilla::dom::NodeInfo> ni;
    ni = mNodeInfo->NodeInfoManager()->GetNodeInfo(aName, aPrefix,
                                                   aNamespaceID,
                                                   ATTRIBUTE_NODE);

    rv = mAttrsAndChildren.SetAndSwapAttr(ni, aParsedValue, &oldValueSet);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  PostIdMaybeChange(aNamespaceID, aName, &valueForAfterSetAttr);

  // If the old value owns its own data, we know it is OK to keep using it.
  // oldValue will be null if there was no previously set value
  const nsAttrValue* oldValue;
  if (aParsedValue.StoresOwnData()) {
    if (oldValueSet) {
      oldValue = &aParsedValue;
    } else {
      oldValue = nullptr;
    }
  } else {
    // No need to conditionally assign null here. If there was no previously
    // set value for the attribute, aOldValue will already be null.
    oldValue = aOldValue;
  }

  if (aComposedDocument) {
    RefPtr<nsXBLBinding> binding = GetXBLBinding();
    if (binding) {
      binding->AttributeChanged(aName, aNamespaceID, false, aNotify);
    }
  }

  if (CustomElementRegistry::IsCustomElementEnabled(OwnerDoc())) {
    CustomElementDefinition* definition = GetCustomElementDefinition();
    // Only custom element which is in `custom` state could get the
    // CustomElementDefinition.
    if (definition && definition->IsInObservedAttributeList(aName)) {
      RefPtr<nsAtom> oldValueAtom;
      if (oldValue) {
        oldValueAtom = oldValue->GetAsAtom();
      } else {
        // If there is no old value, get the value of the uninitialized
        // attribute that was swapped with aParsedValue.
        oldValueAtom = aParsedValue.GetAsAtom();
      }
      RefPtr<nsAtom> newValueAtom = valueForAfterSetAttr.GetAsAtom();
      nsAutoString ns;
      nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNamespaceID, ns);

      LifecycleCallbackArgs args = {
        nsDependentAtomString(aName),
        aModType == MutationEventBinding::ADDITION ?
          VoidString() : nsDependentAtomString(oldValueAtom),
        nsDependentAtomString(newValueAtom),
        (ns.IsEmpty() ? VoidString() : ns)
      };

      nsContentUtils::EnqueueLifecycleCallback(nsIDocument::eAttributeChanged,
        this, &args, nullptr, definition);
    }
  }

  if (aCallAfterSetAttr) {
    rv = AfterSetAttr(aNamespaceID, aName, &valueForAfterSetAttr, oldValue,
                      aSubjectPrincipal, aNotify);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::dir) {
      OnSetDirAttr(this, &valueForAfterSetAttr,
                   hadValidDir, hadDirAuto, aNotify);
    }
  }

  UpdateState(aNotify);

  if (aNotify) {
    // Don't pass aOldValue to AttributeChanged since it may not be reliable.
    // Callers only compute aOldValue under certain conditions which may not
    // be triggered by all nsIMutationObservers.
    nsNodeUtils::AttributeChanged(this, aNamespaceID, aName, aModType,
        aParsedValue.StoresOwnData() ? &aParsedValue : nullptr);
  }

  if (aFireMutation) {
    InternalMutationEvent mutation(true, eLegacyAttrModified);

    nsAutoString ns;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNamespaceID, ns);
    Attr* attrNode =
      GetAttributeNodeNSInternal(ns, nsDependentAtomString(aName));
    mutation.mRelatedNode = attrNode;

    mutation.mAttrName = aName;
    nsAutoString newValue;
    GetAttr(aNamespaceID, aName, newValue);
    if (!newValue.IsEmpty()) {
      mutation.mNewAttrValue = NS_Atomize(newValue);
    }
    if (oldValue && !oldValue->IsEmptyString()) {
      mutation.mPrevAttrValue = oldValue->GetAsAtom();
    }
    mutation.mAttrChange = aModType;

    mozAutoSubtreeModified subtree(OwnerDoc(), this);
    (new AsyncEventDispatcher(this, mutation))->RunDOMEventWhenSafe();
  }

  return NS_OK;
}

bool
Element::ParseAttribute(int32_t aNamespaceID,
                        nsAtom* aAttribute,
                        const nsAString& aValue,
                        nsIPrincipal* aMaybeScriptedPrincipal,
                        nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::lang) {
    aResult.ParseAtom(aValue);
    return true;
  }

  if (aNamespaceID == kNameSpaceID_None) {
    MOZ_ASSERT(aAttribute != nsGkAtoms::_class,
               "The class attribute should be preparsed and therefore should "
               "never be passed to Element::ParseAttribute");
    if (aAttribute == nsGkAtoms::id) {
      // Store id as an atom.  id="" means that the element has no id,
      // not that it has an emptystring as the id.
      if (aValue.IsEmpty()) {
        return false;
      }
      aResult.ParseAtom(aValue);
      return true;
    }
  }

  return false;
}

bool
Element::SetAndSwapMappedAttribute(nsAtom* aName,
                                   nsAttrValue& aValue,
                                   bool* aValueWasSet,
                                   nsresult* aRetval)
{
  *aRetval = NS_OK;
  return false;
}

nsresult
Element::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                       const nsAttrValueOrString* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::_class) {
      if (aValue) {
        // Note: This flag is asymmetrical. It is never unset and isn't exact.
        // If it is ever made to be exact, we probably need to handle this
        // similarly to how ids are handled in PreIdMaybeChange and
        // PostIdMaybeChange.
        // Note that SetSingleClassFromParser inlines BeforeSetAttr and
        // calls SetMayHaveClass directly. Making a subclass take action
        // on the class attribute in a BeforeSetAttr override would
        // require revising SetSingleClassFromParser.
        SetMayHaveClass();
      }
    }
  }

  return NS_OK;
}

void
Element::PreIdMaybeChange(int32_t aNamespaceID, nsAtom* aName,
                          const nsAttrValueOrString* aValue)
{
  if (aNamespaceID != kNameSpaceID_None || aName != nsGkAtoms::id) {
    return;
  }
  RemoveFromIdTable();
}

void
Element::PostIdMaybeChange(int32_t aNamespaceID, nsAtom* aName,
                           const nsAttrValue* aValue)
{
  if (aNamespaceID != kNameSpaceID_None || aName != nsGkAtoms::id) {
    return;
  }

  // id="" means that the element has no id, not that it has an empty
  // string as the id.
  if (aValue && !aValue->IsEmptyString()) {
    SetHasID();
    AddToIdTable(aValue->GetAtomValue());
  } else {
    ClearHasID();
  }
}

nsresult
Element::OnAttrSetButNotChanged(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValueOrString& aValue,
                                bool aNotify)
{
  if (CustomElementRegistry::IsCustomElementEnabled(OwnerDoc())) {
    // Only custom element which is in `custom` state could get the
    // CustomElementDefinition.
    CustomElementDefinition* definition = GetCustomElementDefinition();
    if (definition && definition->IsInObservedAttributeList(aName)) {
      nsAutoString ns;
      nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNamespaceID, ns);

      nsAutoString value(aValue.String());
      LifecycleCallbackArgs args = {
        nsDependentAtomString(aName),
        value,
        value,
        (ns.IsEmpty() ? VoidString() : ns)
      };

      nsContentUtils::EnqueueLifecycleCallback(nsIDocument::eAttributeChanged,
        this, &args, nullptr, definition);
    }
  }

  return NS_OK;
}

EventListenerManager*
Element::GetEventListenerManagerForAttr(nsAtom* aAttrName,
                                        bool* aDefer)
{
  *aDefer = true;
  return GetOrCreateListenerManager();
}

bool
Element::GetAttr(int32_t aNameSpaceID, nsAtom* aName,
                 nsAString& aResult) const
{
  DOMString str;
  bool haveAttr = GetAttr(aNameSpaceID, aName, str);
  str.ToString(aResult);
  return haveAttr;
}

int32_t
Element::FindAttrValueIn(int32_t aNameSpaceID,
                         nsAtom* aName,
                         AttrValuesArray* aValues,
                         nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValues, "Null value array");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  if (val) {
    for (int32_t i = 0; aValues[i]; ++i) {
      if (val->Equals(*aValues[i], aCaseSensitive)) {
        return i;
      }
    }
    return ATTR_VALUE_NO_MATCH;
  }
  return ATTR_MISSING;
}

nsresult
Element::UnsetAttr(int32_t aNameSpaceID, nsAtom* aName,
                   bool aNotify)
{
  NS_ASSERTION(nullptr != aName, "must have attribute name");

  int32_t index = mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID);
  if (index < 0) {
    return NS_OK;
  }

  nsIDocument *document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNameSpaceID, aName,
                                     MutationEventBinding::REMOVAL,
                                     nullptr);
  }

  nsresult rv = BeforeSetAttr(aNameSpaceID, aName, nullptr, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMutationListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  PreIdMaybeChange(aNameSpaceID, aName, nullptr);

  // Grab the attr node if needed before we remove it from the attr map
  RefPtr<Attr> attrNode;
  if (hasMutationListeners) {
    nsAutoString ns;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, ns);
    attrNode = GetAttributeNodeNSInternal(ns, nsDependentAtomString(aName));
  }

  // Clear the attribute out from attribute map.
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots && slots->mAttributeMap) {
    slots->mAttributeMap->DropAttribute(aNameSpaceID, aName);
  }

  // The id-handling code, and in the future possibly other code, need to
  // react to unexpected attribute changes.
  nsMutationGuard::DidMutate();

  bool hadValidDir = false;
  bool hadDirAuto = false;

  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::dir) {
    hadValidDir = HasValidDir() || IsHTMLElement(nsGkAtoms::bdi);
    hadDirAuto = HasDirAuto(); // already takes bdi into account
  }

  nsAttrValue oldValue;
  rv = mAttrsAndChildren.RemoveAttrAt(index, oldValue);
  NS_ENSURE_SUCCESS(rv, rv);

  PostIdMaybeChange(aNameSpaceID, aName, nullptr);

  if (document) {
    RefPtr<nsXBLBinding> binding = GetXBLBinding();
    if (binding) {
      binding->AttributeChanged(aName, aNameSpaceID, true, aNotify);
    }
  }

  if (CustomElementRegistry::IsCustomElementEnabled(OwnerDoc())) {
    CustomElementDefinition* definition = GetCustomElementDefinition();
    // Only custom element which is in `custom` state could get the
    // CustomElementDefinition.
    if (definition && definition->IsInObservedAttributeList(aName)) {
      nsAutoString ns;
      nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, ns);

      RefPtr<nsAtom> oldValueAtom = oldValue.GetAsAtom();
      LifecycleCallbackArgs args = {
        nsDependentAtomString(aName),
        nsDependentAtomString(oldValueAtom),
        VoidString(),
        (ns.IsEmpty() ? VoidString() : ns)
      };

      nsContentUtils::EnqueueLifecycleCallback(nsIDocument::eAttributeChanged,
        this, &args, nullptr, definition);
    }
  }

  rv = AfterSetAttr(aNameSpaceID, aName, nullptr, &oldValue, nullptr, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateState(aNotify);

  if (aNotify) {
    // We can always pass oldValue here since there is no new value which could
    // have corrupted it.
    nsNodeUtils::AttributeChanged(this, aNameSpaceID, aName,
                                  MutationEventBinding::REMOVAL, &oldValue);
  }

  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::dir) {
    OnSetDirAttr(this, nullptr, hadValidDir, hadDirAuto, aNotify);
  }

  if (hasMutationListeners) {
    InternalMutationEvent mutation(true, eLegacyAttrModified);

    mutation.mRelatedNode = attrNode;
    mutation.mAttrName = aName;

    nsAutoString value;
    oldValue.ToString(value);
    if (!value.IsEmpty())
      mutation.mPrevAttrValue = NS_Atomize(value);
    mutation.mAttrChange = MutationEventBinding::REMOVAL;

    mozAutoSubtreeModified subtree(OwnerDoc(), this);
    (new AsyncEventDispatcher(this, mutation))->RunDOMEventWhenSafe();
  }

  return NS_OK;
}

void
Element::DescribeAttribute(uint32_t index, nsAString& aOutDescription) const
{
  // name
  mAttrsAndChildren.AttrNameAt(index)->GetQualifiedName(aOutDescription);

  // value
  aOutDescription.AppendLiteral("=\"");
  nsAutoString value;
  mAttrsAndChildren.AttrAt(index)->ToString(value);
  for (uint32_t i = value.Length(); i > 0; --i) {
    if (value[i - 1] == char16_t('"'))
      value.Insert(char16_t('\\'), i - 1);
  }
  aOutDescription.Append(value);
  aOutDescription.Append('"');
}

#ifdef DEBUG
void
Element::ListAttributes(FILE* out) const
{
  uint32_t index, count = mAttrsAndChildren.AttrCount();
  for (index = 0; index < count; index++) {
    nsAutoString attributeDescription;
    DescribeAttribute(index, attributeDescription);

    fputs(" ", out);
    fputs(NS_LossyConvertUTF16toASCII(attributeDescription).get(), out);
  }
}

void
Element::List(FILE* out, int32_t aIndent,
              const nsCString& aPrefix) const
{
  int32_t indent;
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs(aPrefix.get(), out);

  fputs(NS_LossyConvertUTF16toASCII(mNodeInfo->QualifiedName()).get(), out);

  fprintf(out, "@%p", (void *)this);

  ListAttributes(out);

  fprintf(out, " state=[%llx]",
          static_cast<unsigned long long>(State().GetInternalValue()));
  fprintf(out, " flags=[%08x]", static_cast<unsigned int>(GetFlags()));
  if (IsCommonAncestorForRangeInSelection()) {
    const LinkedList<nsRange>* ranges = GetExistingCommonAncestorRanges();
    int32_t count = 0;
    if (ranges) {
      // Can't use range-based iteration on a const LinkedList, unfortunately.
      for (const nsRange* r = ranges->getFirst(); r; r = r->getNext()) {
        ++count;
      }
    }
    fprintf(out, " ranges:%d", count);
  }
  fprintf(out, " primaryframe=%p", static_cast<void*>(GetPrimaryFrame()));
  fprintf(out, " refcount=%" PRIuPTR "<", mRefCnt.get());

  nsIContent* child = GetFirstChild();
  if (child) {
    fputs("\n", out);

    for (; child; child = child->GetNextSibling()) {
      child->List(out, aIndent + 1);
    }

    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
  }

  fputs(">\n", out);

  Element* nonConstThis = const_cast<Element*>(this);

  // XXX sXBL/XBL2 issue! Owner or current document?
  nsIDocument *document = OwnerDoc();

  // Note: not listing nsIAnonymousContentCreator-created content...

  nsBindingManager* bindingManager = document->BindingManager();
  nsINodeList* anonymousChildren =
    bindingManager->GetAnonymousNodesFor(nonConstThis);

  if (anonymousChildren) {
    uint32_t length = anonymousChildren->Length();

    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
    fputs("anonymous-children<\n", out);

    for (uint32_t i = 0; i < length; ++i) {
      nsIContent* child = anonymousChildren->Item(i);
      child->List(out, aIndent + 1);
    }

    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
    fputs(">\n", out);

    bool outHeader = false;
    ExplicitChildIterator iter(nonConstThis);
    for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
      if (!outHeader) {
        outHeader = true;

        for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
        fputs("content-list<\n", out);
      }

      child->List(out, aIndent + 1);
    }

    if (outHeader) {
      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }
  }
}

void
Element::DumpContent(FILE* out, int32_t aIndent,
                     bool aDumpAll) const
{
  int32_t indent;
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

  const nsString& buf = mNodeInfo->QualifiedName();
  fputs("<", out);
  fputs(NS_LossyConvertUTF16toASCII(buf).get(), out);

  if(aDumpAll) ListAttributes(out);

  fputs(">", out);

  if(aIndent) fputs("\n", out);

  for (nsIContent* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    int32_t indent = aIndent ? aIndent + 1 : 0;
    child->DumpContent(out, indent, aDumpAll);
  }
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
  fputs("</", out);
  fputs(NS_LossyConvertUTF16toASCII(buf).get(), out);
  fputs(">", out);

  if(aIndent) fputs("\n", out);
}
#endif

void
Element::Describe(nsAString& aOutDescription) const
{
  aOutDescription.Append(mNodeInfo->QualifiedName());
  aOutDescription.AppendPrintf("@%p", (void *)this);

  uint32_t index, count = mAttrsAndChildren.AttrCount();
  for (index = 0; index < count; index++) {
    aOutDescription.Append(' ');
    nsAutoString attributeDescription;
    DescribeAttribute(index, attributeDescription);
    aOutDescription.Append(attributeDescription);
  }
}

bool
Element::CheckHandleEventForLinksPrecondition(EventChainVisitor& aVisitor,
                                              nsIURI** aURI) const
{
  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault ||
      (!aVisitor.mEvent->IsTrusted() &&
       (aVisitor.mEvent->mMessage != eMouseClick) &&
       (aVisitor.mEvent->mMessage != eKeyPress) &&
       (aVisitor.mEvent->mMessage != eLegacyDOMActivate)) ||
      !aVisitor.mPresContext ||
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented) {
    return false;
  }

  // Make sure we actually are a link
  return IsLink(aURI);
}

void
Element::GetEventTargetParentForLinks(EventChainPreVisitor& aVisitor)
{
  // Optimisation: return early if this event doesn't interest us.
  // IMPORTANT: this switch and the switch below it must be kept in sync!
  switch (aVisitor.mEvent->mMessage) {
  case eMouseOver:
  case eFocus:
  case eMouseOut:
  case eBlur:
    break;
  default:
    return;
  }

  // Make sure we meet the preconditions before continuing
  nsCOMPtr<nsIURI> absURI;
  if (!CheckHandleEventForLinksPrecondition(aVisitor, getter_AddRefs(absURI))) {
    return;
  }

  // We do the status bar updates in GetEventTargetParent so that the status bar
  // gets updated even if the event is consumed before we have a chance to set
  // it.
  switch (aVisitor.mEvent->mMessage) {
  // Set the status bar similarly for mouseover and focus
  case eMouseOver:
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    MOZ_FALLTHROUGH;
  case eFocus: {
    InternalFocusEvent* focusEvent = aVisitor.mEvent->AsFocusEvent();
    if (!focusEvent || !focusEvent->mIsRefocus) {
      nsAutoString target;
      GetLinkTarget(target);
      nsContentUtils::TriggerLink(this, aVisitor.mPresContext, absURI, target,
                                  false, true, true);
      // Make sure any ancestor links don't also TriggerLink
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
    }
    break;
  }
  case eMouseOut:
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    MOZ_FALLTHROUGH;
  case eBlur:
    {
      nsresult rv = LeaveLink(aVisitor.mPresContext);
      if (NS_SUCCEEDED(rv)) {
        aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
      }
      break;
    }

  default:
    // switch not in sync with the optimization switch earlier in this function
    NS_NOTREACHED("switch statements not in sync");
  }
}

nsresult
Element::PostHandleEventForLinks(EventChainPostVisitor& aVisitor)
{
  // Optimisation: return early if this event doesn't interest us.
  // IMPORTANT: this switch and the switch below it must be kept in sync!
  switch (aVisitor.mEvent->mMessage) {
  case eMouseDown:
  case eMouseClick:
  case eLegacyDOMActivate:
  case eKeyPress:
    break;
  default:
    return NS_OK;
  }

  // Make sure we meet the preconditions before continuing
  nsCOMPtr<nsIURI> absURI;
  if (!CheckHandleEventForLinksPrecondition(aVisitor, getter_AddRefs(absURI))) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  switch (aVisitor.mEvent->mMessage) {
  case eMouseDown:
    {
      if (aVisitor.mEvent->AsMouseEvent()->button ==
            WidgetMouseEvent::eLeftButton) {
        // don't make the link grab the focus if there is no link handler
        nsILinkHandler *handler = aVisitor.mPresContext->GetLinkHandler();
        nsIDocument *document = GetComposedDoc();
        if (handler && document) {
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
            nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(this);
            fm->SetFocus(elem, nsIFocusManager::FLAG_BYMOUSE |
                               nsIFocusManager::FLAG_NOSCROLL);
          }

          EventStateManager::SetActiveManager(
            aVisitor.mPresContext->EventStateManager(), this);

          // OK, we're pretty sure we're going to load, so warm up a speculative
          // connection to be sure we have one ready when we open the channel.
          nsCOMPtr<nsISpeculativeConnect> sc =
            do_QueryInterface(nsContentUtils::GetIOService());
          nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(handler);
          sc->SpeculativeConnect2(absURI, NodePrincipal(), ir);
        }
      }
    }
    break;

  case eMouseClick: {
    WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
    if (mouseEvent->IsLeftClickEvent()) {
      if (mouseEvent->IsControl() || mouseEvent->IsMeta() ||
          mouseEvent->IsAlt() ||mouseEvent->IsShift()) {
        break;
      }

      // The default action is simply to dispatch DOMActivate
      nsCOMPtr<nsIPresShell> shell = aVisitor.mPresContext->GetPresShell();
      if (shell) {
        // single-click
        nsEventStatus status = nsEventStatus_eIgnore;
        // DOMActive event should be trusted since the activation is actually
        // occurred even if the cause is an untrusted click event.
        InternalUIEvent actEvent(true, eLegacyDOMActivate, mouseEvent);
        actEvent.mDetail = 1;

        rv = shell->HandleDOMEventWithTarget(this, &actEvent, &status);
        if (NS_SUCCEEDED(rv)) {
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;
  }
  case eLegacyDOMActivate:
    {
      if (aVisitor.mEvent->mOriginalTarget == this) {
        nsAutoString target;
        GetLinkTarget(target);
        const InternalUIEvent* activeEvent = aVisitor.mEvent->AsUIEvent();
        MOZ_ASSERT(activeEvent);
        nsContentUtils::TriggerLink(this, aVisitor.mPresContext, absURI, target,
                                    true, true, activeEvent->IsTrustable());
        aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
      }
    }
    break;

  case eKeyPress:
    {
      WidgetKeyboardEvent* keyEvent = aVisitor.mEvent->AsKeyboardEvent();
      if (keyEvent && keyEvent->mKeyCode == NS_VK_RETURN) {
        nsEventStatus status = nsEventStatus_eIgnore;
        rv = DispatchClickEvent(aVisitor.mPresContext, keyEvent, this,
                                false, nullptr, &status);
        if (NS_SUCCEEDED(rv)) {
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;

  default:
    // switch not in sync with the optimization switch earlier in this function
    NS_NOTREACHED("switch statements not in sync");
    return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

void
Element::GetLinkTarget(nsAString& aTarget)
{
  aTarget.Truncate();
}

static void
nsDOMTokenListPropertyDestructor(void *aObject, nsAtom *aProperty,
                                 void *aPropertyValue, void *aData)
{
  nsDOMTokenList* list =
    static_cast<nsDOMTokenList*>(aPropertyValue);
  NS_RELEASE(list);
}

static nsStaticAtom** sPropertiesToTraverseAndUnlink[] =
  {
    &nsGkAtoms::sandbox,
    &nsGkAtoms::sizes,
    &nsGkAtoms::dirAutoSetBy,
    nullptr
  };

// static
nsStaticAtom***
Element::HTMLSVGPropertiesToTraverseAndUnlink()
{
  return sPropertiesToTraverseAndUnlink;
}

nsDOMTokenList*
Element::GetTokenList(nsAtom* aAtom,
                      const DOMTokenListSupportedTokenArray aSupportedTokens)
{
#ifdef DEBUG
  nsStaticAtom*** props = HTMLSVGPropertiesToTraverseAndUnlink();
  bool found = false;
  for (uint32_t i = 0; props[i]; ++i) {
    if (*props[i] == aAtom) {
      found = true;
      break;
    }
  }
  MOZ_ASSERT(found, "Trying to use an unknown tokenlist!");
#endif

  nsDOMTokenList* list = nullptr;
  if (HasProperties()) {
    list = static_cast<nsDOMTokenList*>(GetProperty(aAtom));
  }
  if (!list) {
    list = new nsDOMTokenList(this, aAtom, aSupportedTokens);
    NS_ADDREF(list);
    SetProperty(aAtom, list, nsDOMTokenListPropertyDestructor);
  }
  return list;
}

Element*
Element::Closest(const nsAString& aSelector, ErrorResult& aResult)
{
  const RawServoSelectorList* list = ParseSelectorList(aSelector, aResult);
  if (!list) {
    return nullptr;
  }

  return const_cast<Element*>(Servo_SelectorList_Closest(this, list));
}

bool
Element::Matches(const nsAString& aSelector, ErrorResult& aResult)
{
  const RawServoSelectorList* list = ParseSelectorList(aSelector, aResult);
  if (!list) {
    return false;
  }
  return Servo_SelectorList_Matches(this, list);
}

static const nsAttrValue::EnumTable kCORSAttributeTable[] = {
  // Order matters here
  // See ParseCORSValue
  { "anonymous",       CORS_ANONYMOUS       },
  { "use-credentials", CORS_USE_CREDENTIALS },
  { nullptr,           0 }
};

/* static */ void
Element::ParseCORSValue(const nsAString& aValue,
                        nsAttrValue& aResult)
{
  DebugOnly<bool> success =
    aResult.ParseEnumValue(aValue, kCORSAttributeTable, false,
                           // default value is anonymous if aValue is
                           // not a value we understand
                           &kCORSAttributeTable[0]);
  MOZ_ASSERT(success);
}

/* static */ CORSMode
Element::StringToCORSMode(const nsAString& aValue)
{
  if (aValue.IsVoid()) {
    return CORS_NONE;
  }

  nsAttrValue val;
  Element::ParseCORSValue(aValue, val);
  return CORSMode(val.GetEnumValue());
}

/* static */ CORSMode
Element::AttrValueToCORSMode(const nsAttrValue* aValue)
{
  if (!aValue) {
    return CORS_NONE;
  }

  return CORSMode(aValue->GetEnumValue());
}

static const char*
GetFullScreenError(CallerType aCallerType)
{
  if (!nsContentUtils::IsRequestFullScreenAllowed(aCallerType)) {
    return "FullscreenDeniedNotInputDriven";
  }

  return nullptr;
}

void
Element::RequestFullscreen(CallerType aCallerType, ErrorResult& aError)
{
  // Only grant full-screen requests if this is called from inside a trusted
  // event handler (i.e. inside an event handler for a user initiated event).
  // This stops the full-screen from being abused similar to the popups of old,
  // and it also makes it harder for bad guys' script to go full-screen and
  // spoof the browser chrome/window and phish logins etc.
  // Note that requests for fullscreen inside a web app's origin are exempt
  // from this restriction.
  if (const char* error = GetFullScreenError(aCallerType)) {
    OwnerDoc()->DispatchFullscreenError(error);
    return;
  }

  auto request = MakeUnique<FullscreenRequest>(this);
  request->mIsCallerChrome = (aCallerType == CallerType::System);

  OwnerDoc()->AsyncRequestFullScreen(Move(request));
}

void
Element::RequestPointerLock(CallerType aCallerType)
{
  OwnerDoc()->RequestPointerLock(this, aCallerType);
}

already_AddRefed<Flex>
Element::GetAsFlexContainer()
{
  nsIFrame* frame = GetPrimaryFrame();

  // We need the flex frame to compute additional info, and use
  // that annotated version of the frame.
  nsFlexContainerFrame* flexFrame =
    nsFlexContainerFrame::GetFlexFrameWithComputedInfo(frame);

  if (flexFrame) {
    RefPtr<Flex> flex = new Flex(this, flexFrame);
    return flex.forget();
  }
  return nullptr;
}

void
Element::GetGridFragments(nsTArray<RefPtr<Grid>>& aResult)
{
  nsGridContainerFrame* frame =
    nsGridContainerFrame::GetGridFrameWithComputedInfo(GetPrimaryFrame());

  // If we get a nsGridContainerFrame from the prior call,
  // all the next-in-flow frames will also be nsGridContainerFrames.
  while (frame) {
    aResult.AppendElement(
      new Grid(this, frame)
    );
    frame = static_cast<nsGridContainerFrame*>(frame->GetNextInFlow());
  }
}

already_AddRefed<DOMMatrixReadOnly>
Element::GetTransformToAncestor(Element& aAncestor)
{
  nsIFrame* primaryFrame = GetPrimaryFrame();
  nsIFrame* ancestorFrame = aAncestor.GetPrimaryFrame();

  Matrix4x4 transform;
  if (primaryFrame) {
    // If aAncestor is not actually an ancestor of this (including nullptr),
    // then the call to GetTransformToAncestor will return the transform
    // all the way up through the parent chain.
    transform = nsLayoutUtils::GetTransformToAncestor(primaryFrame,
      ancestorFrame, nsIFrame::IN_CSS_UNITS).GetMatrix();
  }

  DOMMatrixReadOnly* matrix = new DOMMatrix(this, transform);
  RefPtr<DOMMatrixReadOnly> result(matrix);
  return result.forget();
}

already_AddRefed<DOMMatrixReadOnly>
Element::GetTransformToParent()
{
  nsIFrame* primaryFrame = GetPrimaryFrame();

  Matrix4x4 transform;
  if (primaryFrame) {
    nsIFrame* parentFrame = primaryFrame->GetParent();
    transform = nsLayoutUtils::GetTransformToAncestor(primaryFrame,
      parentFrame, nsIFrame::IN_CSS_UNITS).GetMatrix();
  }

  DOMMatrixReadOnly* matrix = new DOMMatrix(this, transform);
  RefPtr<DOMMatrixReadOnly> result(matrix);
  return result.forget();
}

already_AddRefed<DOMMatrixReadOnly>
Element::GetTransformToViewport()
{
  nsIFrame* primaryFrame = GetPrimaryFrame();
  Matrix4x4 transform;
  if (primaryFrame) {
    transform = nsLayoutUtils::GetTransformToAncestor(primaryFrame,
      nsLayoutUtils::GetDisplayRootFrame(primaryFrame), nsIFrame::IN_CSS_UNITS).GetMatrix();
  }

  DOMMatrixReadOnly* matrix = new DOMMatrix(this, transform);
  RefPtr<DOMMatrixReadOnly> result(matrix);
  return result.forget();
}

already_AddRefed<Animation>
Element::Animate(JSContext* aContext,
                 JS::Handle<JSObject*> aKeyframes,
                 const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
                 ErrorResult& aError)
{
  Nullable<ElementOrCSSPseudoElement> target;
  target.SetValue().SetAsElement() = this;
  return Animate(target, aContext, aKeyframes, aOptions, aError);
}

/* static */ already_AddRefed<Animation>
Element::Animate(const Nullable<ElementOrCSSPseudoElement>& aTarget,
                 JSContext* aContext,
                 JS::Handle<JSObject*> aKeyframes,
                 const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
                 ErrorResult& aError)
{
  MOZ_ASSERT(!aTarget.IsNull() &&
             (aTarget.Value().IsElement() ||
              aTarget.Value().IsCSSPseudoElement()),
             "aTarget should be initialized");

  RefPtr<Element> referenceElement;
  if (aTarget.Value().IsElement()) {
    referenceElement = &aTarget.Value().GetAsElement();
  } else {
    referenceElement = aTarget.Value().GetAsCSSPseudoElement().ParentElement();
  }

  nsCOMPtr<nsIGlobalObject> ownerGlobal = referenceElement->GetOwnerGlobal();
  if (!ownerGlobal) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  GlobalObject global(aContext, ownerGlobal->GetGlobalJSObject());
  MOZ_ASSERT(!global.Failed());

  // KeyframeEffect constructor doesn't follow the standard Xray calling
  // convention and needs to be called in caller's compartment.
  // This should match to RunConstructorInCallerCompartment attribute in
  // KeyframeEffect.webidl.
  RefPtr<KeyframeEffect> effect =
    KeyframeEffect::Constructor(global, aTarget, aKeyframes, aOptions,
                                aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Animation constructor follows the standard Xray calling convention and
  // needs to be called in the target element's compartment.
  Maybe<JSAutoCompartment> ac;
  if (js::GetContextCompartment(aContext) !=
      js::GetObjectCompartment(ownerGlobal->GetGlobalJSObject())) {
    ac.emplace(aContext, ownerGlobal->GetGlobalJSObject());
  }

  AnimationTimeline* timeline = referenceElement->OwnerDoc()->Timeline();
  RefPtr<Animation> animation =
    Animation::Constructor(global, effect,
                           Optional<AnimationTimeline*>(timeline), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (aOptions.IsKeyframeAnimationOptions()) {
    animation->SetId(aOptions.GetAsKeyframeAnimationOptions().mId);
  }

  animation->Play(aError, Animation::LimitBehavior::AutoRewind);
  if (aError.Failed()) {
    return nullptr;
  }

  return animation.forget();
}

void
Element::GetAnimations(const AnimationFilter& filter,
                       nsTArray<RefPtr<Animation>>& aAnimations)
{
  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    // We don't need to explicitly flush throttled animations here, since
    // updating the animation style of elements will never affect the set of
    // running animations and it's only the set of running animations that is
    // important here.
    doc->FlushPendingNotifications(
      ChangesToFlush(FlushType::Style, false /* flush animations */));
  }

  Element* elem = this;
  CSSPseudoElementType pseudoType = CSSPseudoElementType::NotPseudo;
  // For animations on generated-content elements, the animations are stored
  // on the parent element.
  if (IsGeneratedContentContainerForBefore()) {
    elem = GetParentElement();
    pseudoType = CSSPseudoElementType::before;
  } else if (IsGeneratedContentContainerForAfter()) {
    elem = GetParentElement();
    pseudoType = CSSPseudoElementType::after;
  }

  if (!elem) {
    return;
  }

  if (!filter.mSubtree ||
      pseudoType == CSSPseudoElementType::before ||
      pseudoType == CSSPseudoElementType::after) {
    GetAnimationsUnsorted(elem, pseudoType, aAnimations);
  } else {
    for (nsIContent* node = this;
         node;
         node = node->GetNextNode(this)) {
      if (!node->IsElement()) {
        continue;
      }
      Element* element = node->AsElement();
      Element::GetAnimationsUnsorted(element, CSSPseudoElementType::NotPseudo,
                                     aAnimations);
      Element::GetAnimationsUnsorted(element, CSSPseudoElementType::before,
                                     aAnimations);
      Element::GetAnimationsUnsorted(element, CSSPseudoElementType::after,
                                     aAnimations);
    }
  }
  aAnimations.Sort(AnimationPtrComparator<RefPtr<Animation>>());
}

/* static */ void
Element::GetAnimationsUnsorted(Element* aElement,
                               CSSPseudoElementType aPseudoType,
                               nsTArray<RefPtr<Animation>>& aAnimations)
{
  MOZ_ASSERT(aPseudoType == CSSPseudoElementType::NotPseudo ||
             aPseudoType == CSSPseudoElementType::after ||
             aPseudoType == CSSPseudoElementType::before,
             "Unsupported pseudo type");
  MOZ_ASSERT(aElement, "Null element");

  EffectSet* effects = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effects) {
    return;
  }

  for (KeyframeEffectReadOnly* effect : *effects) {
    MOZ_ASSERT(effect && effect->GetAnimation(),
               "Only effects associated with an animation should be "
               "added to an element's effect set");
    Animation* animation = effect->GetAnimation();

    MOZ_ASSERT(animation->IsRelevant(),
               "Only relevant animations should be added to an element's "
               "effect set");
    aAnimations.AppendElement(animation);
  }
}

void
Element::GetInnerHTML(nsAString& aInnerHTML, OOMReporter& aError)
{
  GetMarkup(false, aInnerHTML);
}

void
Element::SetInnerHTML(const nsAString& aInnerHTML, nsIPrincipal* aSubjectPrincipal, ErrorResult& aError)
{
  SetInnerHTMLInternal(aInnerHTML, aError);
}

void
Element::UnsafeSetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError)
{
  SetInnerHTMLInternal(aInnerHTML, aError, true);
}

void
Element::GetOuterHTML(nsAString& aOuterHTML)
{
  GetMarkup(true, aOuterHTML);
}

void
Element::SetOuterHTML(const nsAString& aOuterHTML, ErrorResult& aError)
{
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  if (parent->NodeType() == DOCUMENT_NODE) {
    aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (OwnerDoc()->IsHTMLDocument()) {
    nsAtom* localName;
    int32_t namespaceID;
    if (parent->IsElement()) {
      localName = parent->NodeInfo()->NameAtom();
      namespaceID = parent->NodeInfo()->NamespaceID();
    } else {
      NS_ASSERTION(parent->NodeType() == DOCUMENT_FRAGMENT_NODE,
        "How come the parent isn't a document, a fragment or an element?");
      localName = nsGkAtoms::body;
      namespaceID = kNameSpaceID_XHTML;
    }
    RefPtr<DocumentFragment> fragment =
      new DocumentFragment(OwnerDoc()->NodeInfoManager());
    nsContentUtils::ParseFragmentHTML(aOuterHTML,
                                      fragment,
                                      localName,
                                      namespaceID,
                                      OwnerDoc()->GetCompatibilityMode() ==
                                        eCompatibility_NavQuirks,
                                      true);
    parent->ReplaceChild(*fragment, *this, aError);
    return;
  }

  nsCOMPtr<nsINode> context;
  if (parent->IsElement()) {
    context = parent;
  } else {
    NS_ASSERTION(parent->NodeType() == DOCUMENT_FRAGMENT_NODE,
      "How come the parent isn't a document, a fragment or an element?");
    RefPtr<mozilla::dom::NodeInfo> info =
      OwnerDoc()->NodeInfoManager()->GetNodeInfo(nsGkAtoms::body,
                                                 nullptr,
                                                 kNameSpaceID_XHTML,
                                                 ELEMENT_NODE);
    context = NS_NewHTMLBodyElement(info.forget(), FROM_PARSER_FRAGMENT);
  }

  nsCOMPtr<nsIDOMDocumentFragment> df;
  aError = nsContentUtils::CreateContextualFragment(context,
                                                    aOuterHTML,
                                                    true,
                                                    getter_AddRefs(df));
  if (aError.Failed()) {
    return;
  }
  nsCOMPtr<nsINode> fragment = do_QueryInterface(df);
  parent->ReplaceChild(*fragment, *this, aError);
}

enum nsAdjacentPosition {
  eBeforeBegin,
  eAfterBegin,
  eBeforeEnd,
  eAfterEnd
};

void
Element::InsertAdjacentHTML(const nsAString& aPosition, const nsAString& aText,
                            ErrorResult& aError)
{
  nsAdjacentPosition position;
  if (aPosition.LowerCaseEqualsLiteral("beforebegin")) {
    position = eBeforeBegin;
  } else if (aPosition.LowerCaseEqualsLiteral("afterbegin")) {
    position = eAfterBegin;
  } else if (aPosition.LowerCaseEqualsLiteral("beforeend")) {
    position = eBeforeEnd;
  } else if (aPosition.LowerCaseEqualsLiteral("afterend")) {
    position = eAfterEnd;
  } else {
    aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nsCOMPtr<nsIContent> destination;
  if (position == eBeforeBegin || position == eAfterEnd) {
    destination = GetParent();
    if (!destination) {
      aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
      return;
    }
  } else {
    destination = this;
  }

  nsIDocument* doc = OwnerDoc();

  // Needed when insertAdjacentHTML is used in combination with contenteditable
  mozAutoDocUpdate updateBatch(doc, UPDATE_CONTENT_MODEL, true);
  nsAutoScriptLoaderDisabler sld(doc);

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(doc, nullptr);

  // Parse directly into destination if possible
  if (doc->IsHTMLDocument() && !OwnerDoc()->MayHaveDOMMutationObservers() &&
      (position == eBeforeEnd ||
       (position == eAfterEnd && !GetNextSibling()) ||
       (position == eAfterBegin && !GetFirstChild()))) {
    int32_t oldChildCount = destination->GetChildCount();
    int32_t contextNs = destination->GetNameSpaceID();
    nsAtom* contextLocal = destination->NodeInfo()->NameAtom();
    if (contextLocal == nsGkAtoms::html && contextNs == kNameSpaceID_XHTML) {
      // For compat with IE6 through IE9. Willful violation of HTML5 as of
      // 2011-04-06. CreateContextualFragment does the same already.
      // Spec bug: http://www.w3.org/Bugs/Public/show_bug.cgi?id=12434
      contextLocal = nsGkAtoms::body;
    }
    aError = nsContentUtils::ParseFragmentHTML(aText,
                                               destination,
                                               contextLocal,
                                               contextNs,
                                               doc->GetCompatibilityMode() ==
                                                 eCompatibility_NavQuirks,
                                               true);
    // HTML5 parser has notified, but not fired mutation events.
    nsContentUtils::FireMutationEventsForDirectParsing(doc, destination,
                                                       oldChildCount);
    return;
  }

  // couldn't parse directly
  nsCOMPtr<nsIDOMDocumentFragment> df;
  aError = nsContentUtils::CreateContextualFragment(destination,
                                                    aText,
                                                    true,
                                                    getter_AddRefs(df));
  if (aError.Failed()) {
    return;
  }

  nsCOMPtr<nsINode> fragment = do_QueryInterface(df);

  // Suppress assertion about node removal mutation events that can't have
  // listeners anyway, because no one has had the chance to register mutation
  // listeners on the fragment that comes from the parser.
  nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

  nsAutoMutationBatch mb(destination, true, false);
  switch (position) {
    case eBeforeBegin:
      destination->InsertBefore(*fragment, this, aError);
      break;
    case eAfterBegin:
      static_cast<nsINode*>(this)->InsertBefore(*fragment, GetFirstChild(),
                                                aError);
      break;
    case eBeforeEnd:
      static_cast<nsINode*>(this)->AppendChild(*fragment, aError);
      break;
    case eAfterEnd:
      destination->InsertBefore(*fragment, GetNextSibling(), aError);
      break;
  }
}

nsINode*
Element::InsertAdjacent(const nsAString& aWhere,
                        nsINode* aNode,
                        ErrorResult& aError)
{
  if (aWhere.LowerCaseEqualsLiteral("beforebegin")) {
    nsCOMPtr<nsINode> parent = GetParentNode();
    if (!parent) {
      return nullptr;
    }
    parent->InsertBefore(*aNode, this, aError);
  } else if (aWhere.LowerCaseEqualsLiteral("afterbegin")) {
    nsCOMPtr<nsINode> refNode = GetFirstChild();
    static_cast<nsINode*>(this)->InsertBefore(*aNode, refNode, aError);
  } else if (aWhere.LowerCaseEqualsLiteral("beforeend")) {
    static_cast<nsINode*>(this)->AppendChild(*aNode, aError);
  } else if (aWhere.LowerCaseEqualsLiteral("afterend")) {
    nsCOMPtr<nsINode> parent = GetParentNode();
    if (!parent) {
      return nullptr;
    }
    nsCOMPtr<nsINode> refNode = GetNextSibling();
    parent->InsertBefore(*aNode, refNode, aError);
  } else {
    aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  return aError.Failed() ? nullptr : aNode;
}

Element*
Element::InsertAdjacentElement(const nsAString& aWhere,
                               Element& aElement,
                               ErrorResult& aError) {
  nsINode* newNode = InsertAdjacent(aWhere, &aElement, aError);
  MOZ_ASSERT(!newNode || newNode->IsElement());

  return newNode ? newNode->AsElement() : nullptr;
}

void
Element::InsertAdjacentText(
  const nsAString& aWhere, const nsAString& aData, ErrorResult& aError)
{
  RefPtr<nsTextNode> textNode = OwnerDoc()->CreateTextNode(aData);
  InsertAdjacent(aWhere, textNode, aError);
}

TextEditor*
Element::GetTextEditorInternal()
{
  nsCOMPtr<nsITextControlElement> textCtrl = do_QueryInterface(this);
  return textCtrl ? textCtrl->GetTextEditor() : nullptr;
}

nsresult
Element::SetBoolAttr(nsAtom* aAttr, bool aValue)
{
  if (aValue) {
    return SetAttr(kNameSpaceID_None, aAttr, EmptyString(), true);
  }

  return UnsetAttr(kNameSpaceID_None, aAttr, true);
}

void
Element::GetEnumAttr(nsAtom* aAttr,
                     const char* aDefault,
                     nsAString& aResult) const
{
  GetEnumAttr(aAttr, aDefault, aDefault, aResult);
}

void
Element::GetEnumAttr(nsAtom* aAttr,
                     const char* aDefaultMissing,
                     const char* aDefaultInvalid,
                     nsAString& aResult) const
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(aAttr);

  aResult.Truncate();

  if (!attrVal) {
    if (aDefaultMissing) {
      AppendASCIItoUTF16(nsDependentCString(aDefaultMissing), aResult);
    } else {
      SetDOMStringToNull(aResult);
    }
  } else {
    if (attrVal->Type() == nsAttrValue::eEnum) {
      attrVal->GetEnumString(aResult, true);
    } else if (aDefaultInvalid) {
      AppendASCIItoUTF16(nsDependentCString(aDefaultInvalid), aResult);
    }
  }
}

void
Element::SetOrRemoveNullableStringAttr(nsAtom* aName, const nsAString& aValue,
                                       ErrorResult& aError)
{
  if (DOMStringIsNull(aValue)) {
    UnsetAttr(aName, aError);
  } else {
    SetAttr(aName, aValue, aError);
  }
}

Directionality
Element::GetComputedDirectionality() const
{
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    return frame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR
             ? eDir_LTR : eDir_RTL;
  }

  return GetDirectionality();
}

float
Element::FontSizeInflation()
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return -1.0;
  }

  if (nsLayoutUtils::FontSizeInflationEnabled(frame->PresContext())) {
    return nsLayoutUtils::FontSizeInflationFor(frame);
  }

  return 1.0;
}

net::ReferrerPolicy
Element::GetReferrerPolicyAsEnum()
{
  if (IsHTMLElement()) {
    const nsAttrValue* referrerValue = GetParsedAttr(nsGkAtoms::referrerpolicy);
    return ReferrerPolicyFromAttr(referrerValue);
  }
  return net::RP_Unset;
}

net::ReferrerPolicy
Element::ReferrerPolicyFromAttr(const nsAttrValue* aValue)
{
  if (aValue && aValue->Type() == nsAttrValue::eEnum) {
    return net::ReferrerPolicy(aValue->GetEnumValue());
  }
  return net::RP_Unset;
}

already_AddRefed<nsDOMStringMap>
Element::Dataset()
{
  nsDOMSlots *slots = DOMSlots();

  if (!slots->mDataset) {
    // mDataset is a weak reference so assignment will not AddRef.
    // AddRef is called before returning the pointer.
    slots->mDataset = new nsDOMStringMap(this);
  }

  RefPtr<nsDOMStringMap> ret = slots->mDataset;
  return ret.forget();
}

void
Element::ClearDataset()
{
  nsDOMSlots *slots = GetExistingDOMSlots();

  MOZ_ASSERT(slots && slots->mDataset,
             "Slots should exist and dataset should not be null.");
  slots->mDataset = nullptr;
}

enum nsPreviousIntersectionThreshold {
  eUninitialized = -2,
  eNonIntersecting = -1
};

static void
IntersectionObserverPropertyDtor(void* aObject, nsAtom* aPropertyName,
                                 void* aPropertyValue, void* aData)
{
  Element* element = static_cast<Element*>(aObject);
  IntersectionObserverList* observers =
    static_cast<IntersectionObserverList*>(aPropertyValue);
  for (auto iter = observers->Iter(); !iter.Done(); iter.Next()) {
    DOMIntersectionObserver* observer = iter.Key();
    observer->UnlinkTarget(*element);
  }
  delete observers;
}

void
Element::RegisterIntersectionObserver(DOMIntersectionObserver* aObserver)
{
  IntersectionObserverList* observers =
    static_cast<IntersectionObserverList*>(
      GetProperty(nsGkAtoms::intersectionobserverlist)
    );

  if (!observers) {
    observers = new IntersectionObserverList();
    observers->Put(aObserver, eUninitialized);
    SetProperty(nsGkAtoms::intersectionobserverlist, observers,
                IntersectionObserverPropertyDtor, true);
    return;
  }

  observers->LookupForAdd(aObserver).OrInsert([]() {
    // Value can be:
    //   -2:   Makes sure next calculated threshold always differs, leading to a
    //         notification task being scheduled.
    //   -1:   Non-intersecting.
    //   >= 0: Intersecting, valid index of aObserver->mThresholds.
    return eUninitialized;
  });
}

void
Element::UnregisterIntersectionObserver(DOMIntersectionObserver* aObserver)
{
  IntersectionObserverList* observers =
    static_cast<IntersectionObserverList*>(
      GetProperty(nsGkAtoms::intersectionobserverlist)
    );
  if (observers) {
    observers->Remove(aObserver);
  }
}

void
Element::UnlinkIntersectionObservers()
{
  IntersectionObserverList* observers =
    static_cast<IntersectionObserverList*>(
      GetProperty(nsGkAtoms::intersectionobserverlist)
    );
  if (!observers) {
    return;
  }
  for (auto iter = observers->Iter(); !iter.Done(); iter.Next()) {
    DOMIntersectionObserver* observer = iter.Key();
    observer->UnlinkTarget(*this);
  }
  observers->Clear();
}

bool
Element::UpdateIntersectionObservation(DOMIntersectionObserver* aObserver, int32_t aThreshold)
{
  IntersectionObserverList* observers =
    static_cast<IntersectionObserverList*>(
      GetProperty(nsGkAtoms::intersectionobserverlist)
    );
  if (!observers) {
    return false;
  }
  bool updated = false;
  if (auto entry = observers->Lookup(aObserver)) {
    updated = entry.Data() != aThreshold;
    entry.Data() = aThreshold;
  }
  return updated;
}

void
Element::ClearServoData(nsIDocument* aDoc) {
  MOZ_ASSERT(aDoc);
  if (HasServoData()) {
    Servo_Element_ClearData(this);
  } else {
    UnsetFlags(kAllServoDescendantBits | NODE_NEEDS_FRAME);
  }
  // Since this element is losing its servo data, nothing under it may have
  // servo data either, so we can forget restyles rooted at this element. This
  // is necessary for correctness, since we invoke ClearServoData in various
  // places where an element's flattened tree parent changes, and such a change
  // may also make an element invalid to be used as a restyle root.
  if (aDoc->GetServoRestyleRoot() == this) {
    aDoc->ClearServoRestyleRoot();
  }
}

void
Element::SetCustomElementData(CustomElementData* aData)
{
  nsExtendedDOMSlots *slots = ExtendedDOMSlots();
  MOZ_ASSERT(!slots->mCustomElementData, "Custom element data may not be changed once set.");
  #if DEBUG
    nsAtom* name = NodeInfo()->NameAtom();
    nsAtom* type = aData->GetCustomElementType();
    if (nsContentUtils::IsCustomElementName(name, NodeInfo()->NamespaceID())) {
      MOZ_ASSERT(type == name);
    } else {
      MOZ_ASSERT(type != name);
    }
  #endif
  slots->mCustomElementData = aData;
}

CustomElementDefinition*
Element::GetCustomElementDefinition() const
{
  CustomElementData* data = GetCustomElementData();
  if (!data) {
    return nullptr;
  }

  return data->GetCustomElementDefinition();
}

void
Element::SetCustomElementDefinition(CustomElementDefinition* aDefinition)
{
  CustomElementData* data = GetCustomElementData();
  MOZ_ASSERT(data);

  data->SetCustomElementDefinition(aDefinition);
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoElementMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoElementMallocEnclosingSizeOf)

void
Element::AddSizeOfExcludingThis(nsWindowSizes& aSizes, size_t* aNodeSize) const
{
  FragmentOrElement::AddSizeOfExcludingThis(aSizes, aNodeSize);

  if (HasServoData()) {
    // Measure the ElementData object itself.
    aSizes.mLayoutElementDataObjects +=
      aSizes.mState.mMallocSizeOf(mServoData.Get());

    // Measure mServoData, excluding the ComputedValues. This measurement
    // counts towards the element's size. We use ServoElementMallocSizeOf and
    // ServoElementMallocEnclosingSizeOf rather than |aState.mMallocSizeOf| to
    // better distinguish in DMD's output the memory measured within Servo
    // code.
    *aNodeSize +=
      Servo_Element_SizeOfExcludingThisAndCVs(ServoElementMallocSizeOf,
                                              ServoElementMallocEnclosingSizeOf,
                                              &aSizes.mState.mSeenPtrs, this);

    // Now measure just the ComputedValues (and style structs) under
    // mServoData. This counts towards the relevant fields in |aSizes|.
    RefPtr<ComputedStyle> sc;
    if (Servo_Element_HasPrimaryComputedValues(this)) {
      sc = Servo_Element_GetPrimaryComputedValues(this).Consume();
      if (!aSizes.mState.HaveSeenPtr(sc.get())) {
        sc->AddSizeOfIncludingThis(aSizes, &aSizes.mLayoutComputedValuesDom);
      }

      for (size_t i = 0; i < nsCSSPseudoElements::kEagerPseudoCount; i++) {
        if (Servo_Element_HasPseudoComputedValues(this, i)) {
          sc = Servo_Element_GetPseudoComputedValues(this, i).Consume();
          if (!aSizes.mState.HaveSeenPtr(sc.get())) {
            sc->AddSizeOfIncludingThis(aSizes,
                                       &aSizes.mLayoutComputedValuesDom);
          }
        }
      }
    }
  }
}

#ifdef DEBUG
static bool
BitsArePropagated(const Element* aElement, uint32_t aBits, nsINode* aRestyleRoot)
{
  const Element* curr = aElement;
  while (curr) {
    if (curr == aRestyleRoot) {
      return true;
    }
    if (!curr->HasAllFlags(aBits)) {
      return false;
    }
    nsINode* parentNode = curr->GetParentNode();
    curr = curr->GetFlattenedTreeParentElementForStyle();
    MOZ_ASSERT_IF(!curr,
                  parentNode == aElement->OwnerDoc() ||
                  parentNode == parentNode->OwnerDoc()->GetRootElement());
  }
  return true;
}
#endif

static inline void
AssertNoBitsPropagatedFrom(nsINode* aRoot)
{
#ifdef DEBUG
  if (!aRoot || !aRoot->IsElement()) {
    return;
  }

  auto* element = aRoot->GetFlattenedTreeParentElementForStyle();
  while (element) {
    MOZ_ASSERT(!element->HasAnyOfFlags(Element::kAllServoDescendantBits));
    element = element->GetFlattenedTreeParentElementForStyle();
  }
#endif
}

// Sets |aBits| on aElement and all of its flattened-tree ancestors up to and
// including aStopAt or the root element (whichever is encountered first).
static inline Element*
PropagateBits(Element* aElement, uint32_t aBits, nsINode* aStopAt)
{
  Element* curr = aElement;
  while (curr && !curr->HasAllFlags(aBits)) {
    curr->SetFlags(aBits);
    if (curr == aStopAt) {
      break;
    }
    curr = curr->GetFlattenedTreeParentElementForStyle();
  }

  return curr;
}

// Notes that a given element is "dirty" with respect to the given descendants
// bit (which may be one of dirty descendants, dirty animation descendants, or
// need frame construction for descendants).
//
// This function operates on the dirty element itself, despite the fact that the
// bits are generally used to describe descendants. This allows restyle roots
// to be scoped as tightly as possible. On the first call to NoteDirtyElement
// since the last restyle, we don't set any descendant bits at all, and just set
// the element as the restyle root.
//
// Because the style traversal handles multiple tasks (styling, animation-ticking,
// and lazy frame construction), there are potentially three separate kinds of
// dirtiness to track. Rather than maintaining three separate restyle roots, we
// use a single root, and always bubble it up to be the nearest common ancestor
// of all the dirty content in the tree. This means that we need to track the
// types of dirtiness that the restyle root corresponds to, so
// SetServoRestyleRoot accepts a bitfield along with an element.
//
// The overall algorithm is as follows:
// * When the first dirty element is noted, we just set as the restyle root.
// * When additional dirty elements are noted, we propagate the given bit up
//   the tree, until we either reach the restyle root or the document root.
// * If we reach the document root, we then propagate the bits associated with
//   the restyle root up the tree until we cross the path of the new root. Once
//   we find this common ancestor, we record it as the restyle root, and then
//   clear the bits between the new restyle root and the document root.
// * If we have dirty content beneath multiple "document style traversal roots"
//   (which are the main DOM + each piece of document-level native-anoymous
//   content), we set the restyle root to the nsINode of the document itself.
//   This is the bail-out case where we traverse everything.
//
// Note that, since we track a root, we try to optimize the case where an
// element under the current root is dirtied, that's why we don't trivially use
// `nsContentUtils::GetCommonFlattenedTreeAncestorForStyle`.
static void
NoteDirtyElement(Element* aElement, uint32_t aBits)
{
  MOZ_ASSERT(aElement->IsInComposedDoc());

  // Check the existing root early on, since it may allow us to short-circuit
  // before examining the parent chain.
  nsIDocument* doc = aElement->GetComposedDoc();
  nsINode* existingRoot = doc->GetServoRestyleRoot();
  if (existingRoot == aElement) {
    doc->SetServoRestyleRootDirtyBits(doc->GetServoRestyleRootDirtyBits() | aBits);
    return;
  }

  nsINode* parent = aElement->GetFlattenedTreeParentNodeForStyle();
  if (!parent) {
    // The element is not in the flattened tree, bail.
    return;
  }

  if (MOZ_LIKELY(parent->IsElement())) {
    // If our parent is unstyled, we can inductively assume that it will be
    // traversed when the time is right, and that the traversal will reach us
    // when it happens. Nothing left to do.
    if (!parent->AsElement()->HasServoData()) {
      return;
    }

    // Similarly, if our parent already has the bit we're propagating, we can
    // assume everything is already set up.
    if (parent->HasAllFlags(aBits)) {
      return;
    }

    // If the parent is styled but is display:none, we're done.
    //
    // We can't check for a frame here, since <frame> elements inside <frameset>
    // still need to generate a frame, even if they're display: none. :(
    //
    // The servo traversal doesn't keep style data under display: none subtrees, 
    // so in order for it to not need to cleanup each time anything happens in a
    // display: none subtree, we keep it clean.
    //
    // Also, we can't be much more smarter about using the parent's frame in
    // order to avoid work here, because since the style system keeps style data
    // in, e.g., subtrees under a leaf frame, missing restyles and such in there
    // has observable behavior via getComputedStyle, for example.
    if (Servo_Element_IsDisplayNone(parent->AsElement())) {
      return;
    }
  }

  if (nsIPresShell* shell = doc->GetShell()) {
    shell->EnsureStyleFlush();
  }

  MOZ_ASSERT(parent->IsElement() || parent == doc);

  // The bit checks below rely on this to arrive to useful conclusions about the
  // shape of the tree.
  AssertNoBitsPropagatedFrom(existingRoot);

  // If there's no existing restyle root, or if the root is already aElement,
  // just note the root+bits and return.
  if (!existingRoot) {
    doc->SetServoRestyleRoot(aElement, aBits);
    return;
  }

  // There is an existing restyle root - walk up the tree from our element,
  // propagating bits as we go.
  const bool reachedDocRoot =
    !parent->IsElement() ||
    !PropagateBits(parent->AsElement(), aBits, existingRoot);

  uint32_t existingBits = doc->GetServoRestyleRootDirtyBits();
  if (!reachedDocRoot || existingRoot == doc) {
      // We're a descendant of the existing root. All that's left to do is to
      // make sure the bit we propagated is also registered on the root.
      doc->SetServoRestyleRoot(existingRoot, existingBits | aBits);
  } else {
    // We reached the root without crossing the pre-existing restyle root. We
    // now need to find the nearest common ancestor, so climb up from the
    // existing root, extending bits along the way.
    Element* rootParent = existingRoot->GetFlattenedTreeParentElementForStyle();
    if (Element* commonAncestor = PropagateBits(rootParent, existingBits, aElement)) {
      MOZ_ASSERT(commonAncestor == aElement ||
                 commonAncestor == nsContentUtils::GetCommonFlattenedTreeAncestorForStyle(aElement, rootParent));

      // We found a common ancestor. Make that the new style root, and clear the
      // bits between the new style root and the document root.
      doc->SetServoRestyleRoot(commonAncestor, existingBits | aBits);
      Element* curr = commonAncestor;
      while ((curr = curr->GetFlattenedTreeParentElementForStyle())) {
        MOZ_ASSERT(curr->HasAllFlags(aBits));
        curr->UnsetFlags(aBits);
      }
    } else {
      // We didn't find a common ancestor element. That means we're descended
      // from two different document style roots, so the common ancestor is the
      // document.
      doc->SetServoRestyleRoot(doc, existingBits | aBits);
    }
  }

  // See the comment in nsIDocument::SetServoRestyleRoot about the !IsElement()
  // check there. Same justification here.
  MOZ_ASSERT(aElement == doc->GetServoRestyleRoot() ||
             !doc->GetServoRestyleRoot()->IsElement() ||
             nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(
               aElement, doc->GetServoRestyleRoot()));
  MOZ_ASSERT(aElement == doc->GetServoRestyleRoot() ||
             !doc->GetServoRestyleRoot()->IsElement() ||
             !parent->IsElement() ||
             BitsArePropagated(parent->AsElement(), aBits, doc->GetServoRestyleRoot()));
  MOZ_ASSERT(doc->GetServoRestyleRootDirtyBits() & aBits);
}

void
Element::NoteDirtySubtreeForServo()
{
  MOZ_ASSERT(IsInComposedDoc());
  MOZ_ASSERT(HasServoData());

  nsIDocument* doc = GetComposedDoc();
  nsINode* existingRoot = doc->GetServoRestyleRoot();
  uint32_t existingBits = existingRoot ? doc->GetServoRestyleRootDirtyBits() : 0;

  if (existingRoot &&
      existingRoot->IsElement() &&
      existingRoot != this &&
      nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(
        existingRoot->AsElement(), this)) {
    PropagateBits(existingRoot->AsElement()->GetFlattenedTreeParentElementForStyle(),
                  existingBits,
                  this);

    doc->ClearServoRestyleRoot();
  }

  NoteDirtyElement(this, existingBits | ELEMENT_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
}

void
Element::NoteDirtyForServo()
{
  NoteDirtyElement(this, ELEMENT_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
}

void
Element::NoteAnimationOnlyDirtyForServo()
{
  NoteDirtyElement(this, ELEMENT_HAS_ANIMATION_ONLY_DIRTY_DESCENDANTS_FOR_SERVO);
}

void
Element::NoteDescendantsNeedFramesForServo()
{
  // Since lazy frame construction can be required for non-element nodes, this
  // Note() method operates on the parent of the frame-requiring content, unlike
  // the other Note() methods above (which operate directly on the element that
  // needs processing).
  NoteDirtyElement(this, NODE_DESCENDANTS_NEED_FRAMES);
  SetFlags(NODE_DESCENDANTS_NEED_FRAMES);
}
