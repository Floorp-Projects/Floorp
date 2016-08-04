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
#include "mozilla/dom/Grid.h"
#include "nsDOMAttributeMap.h"
#include "nsIAtom.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsIDocumentInlines.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIContentIterator.h"
#include "nsFocusManager.h"
#include "nsFrameManager.h"
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
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsNameSpaceManager.h"
#include "nsContentList.h"
#include "nsVariant.h"
#include "nsDOMTokenList.h"
#include "nsXBLPrototypeBinding.h"
#include "nsError.h"
#include "nsDOMString.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMMutationEvent.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/AnimationComparator.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "nsNodeUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsDocument.h"
#include "nsAttrValueOrString.h"
#include "nsAttrValueInlines.h"
#include "nsCSSPseudoElements.h"
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
#include "nsIDOMDocumentType.h"
#include "nsGenericHTMLElement.h"
#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsContentCreatorFunctions.h"
#include "nsIControllers.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIScrollableFrame.h"
#include "mozilla/css/StyleRule.h" /* For nsCSSSelectorList */
#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include "nsTextNode.h"

#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif /* MOZ_XUL */

#include "nsCycleCollectionParticipant.h"
#include "nsCCUncollectableMarker.h"

#include "mozAutoDocUpdate.h"

#include "nsCSSParser.h"
#include "prprf.h"
#include "nsDOMMutationObserver.h"
#include "nsSVGFeatures.h"
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
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/VRDevice.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "nsComputedDOMStyle.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIAtom*
nsIContent::DoGetID() const
{
  MOZ_ASSERT(HasID(), "Unexpected call");
  MOZ_ASSERT(IsElement(), "Only elements can have IDs");

  return AsElement()->GetParsedAttr(nsGkAtoms::id)->GetAtomValue();
}

const nsAttrValue*
nsIContent::DoGetClasses() const
{
  MOZ_ASSERT(HasFlag(NODE_MAY_HAVE_CLASS), "Unexpected call");
  MOZ_ASSERT(IsElement(), "Only elements can have classes");

  if (IsSVGElement()) {
    const nsAttrValue* animClass =
      static_cast<const nsSVGElement*>(this)->GetAnimatedClassName();
    if (animClass) {
      return animClass;
    }
  }

  return AsElement()->GetParsedAttr(nsGkAtoms::_class);
}

NS_IMETHODIMP
Element::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
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
  mState = IntrinsicState() | (oldState & ESM_MANAGED_STATES);
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

EventStates
Element::StyleStateFromLocks() const
{
  EventStates locks = LockedStyleStates();
  EventStates state = mState | locks;

  if (locks.HasState(NS_EVENT_STATE_VISITED)) {
    return state & ~NS_EVENT_STATE_UNVISITED;
  }
  if (locks.HasState(NS_EVENT_STATE_UNVISITED)) {
    return state & ~NS_EVENT_STATE_VISITED;
  }
  return state;
}

EventStates
Element::LockedStyleStates() const
{
  EventStates* locks =
    static_cast<EventStates*>(GetProperty(nsGkAtoms::lockedStyleStates));
  if (locks) {
    return *locks;
  }
  return EventStates();
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
Element::LockStyleStates(EventStates aStates)
{
  EventStates* locks = new EventStates(LockedStyleStates());

  *locks |= aStates;

  if (aStates.HasState(NS_EVENT_STATE_VISITED)) {
    *locks &= ~NS_EVENT_STATE_UNVISITED;
  }
  if (aStates.HasState(NS_EVENT_STATE_UNVISITED)) {
    *locks &= ~NS_EVENT_STATE_VISITED;
  }

  SetProperty(nsGkAtoms::lockedStyleStates, locks,
              nsINode::DeleteProperty<EventStates>);
  SetHasLockedStyleStates();

  NotifyStyleStateChange(aStates);
}

void
Element::UnlockStyleStates(EventStates aStates)
{
  EventStates* locks = new EventStates(LockedStyleStates());

  *locks &= ~aStates;

  if (locks->IsEmpty()) {
    DeleteProperty(nsGkAtoms::lockedStyleStates);
    ClearHasLockedStyleStates();
    delete locks;
  }
  else {
    SetProperty(nsGkAtoms::lockedStyleStates, locks,
                nsINode::DeleteProperty<EventStates>);
  }

  NotifyStyleStateChange(aStates);
}

void
Element::ClearStyleStateLocks()
{
  EventStates locks = LockedStyleStates();

  DeleteProperty(nsGkAtoms::lockedStyleStates);
  ClearHasLockedStyleStates();

  NotifyStyleStateChange(locks);
}

bool
Element::GetBindingURL(nsIDocument *aDocument, css::URLValue **aResult)
{
  // If we have a frame the frame has already loaded the binding.  And
  // otherwise, don't do anything else here unless we're dealing with
  // XUL or an HTML element that may have a plugin-related overlay
  // (i.e. object, embed, or applet).
  bool isXULorPluginElement = (IsXULElement() ||
                               IsHTMLElement(nsGkAtoms::object) ||
                               IsHTMLElement(nsGkAtoms::embed) ||
                               IsHTMLElement(nsGkAtoms::applet));
  nsCOMPtr<nsIPresShell> shell = aDocument->GetShell();
  if (!shell || GetPrimaryFrame() || !isXULorPluginElement) {
    *aResult = nullptr;

    return true;
  }

  // Get the computed -moz-binding directly from the style context
  RefPtr<nsStyleContext> sc =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(this, nullptr, shell);
  NS_ENSURE_TRUE(sc, false);

  *aResult = sc->StyleDisplay()->mBinding;

  return true;
}

JSObject*
Element::WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  JS::Rooted<JSObject*> givenProto(aCx, aGivenProto);
  JS::Rooted<JSObject*> customProto(aCx);

  if (!givenProto) {
    // Custom element prototype swizzling.
    CustomElementData* data = GetCustomElementData();
    if (data) {
      // If this is a registered custom element then fix the prototype.
      nsDocument* document = static_cast<nsDocument*>(OwnerDoc());
      document->GetCustomPrototype(NodeInfo()->NamespaceID(), data->mType, &customProto);
      if (customProto &&
          NodePrincipal()->SubsumesConsideringDomain(nsContentUtils::ObjectPrincipal(customProto))) {
        // Just go ahead and create with the right proto up front.  Set
        // customProto to null to flag that we don't need to do any post-facto
        // proto fixups here.
        givenProto = customProto;
        customProto = nullptr;
      }
    }
  }

  JS::Rooted<JSObject*> obj(aCx, nsINode::WrapObject(aCx, givenProto));
  if (!obj) {
    return nullptr;
  }

  if (customProto) {
    // We want to set the custom prototype in the compartment where it was
    // registered. In the case that |obj| and |prototype| are in different
    // compartments, this will set the prototype on the |obj|'s wrapper and
    // thus only visible in the wrapper's compartment, since we know obj's
    // principal does not subsume customProto's in this case.
    JSAutoCompartment ac(aCx, customProto);
    JS::Rooted<JSObject*> wrappedObj(aCx, obj);
    if (!JS_WrapObject(aCx, &wrappedObj) ||
        !JS_SetPrototype(aCx, wrappedObj, customProto)) {
      return nullptr;
    }
  }

  nsIDocument* doc;
  if (HasFlag(NODE_FORCE_XBL_BINDINGS)) {
    doc = OwnerDoc();
  }
  else {
    doc = GetComposedDoc();
  }

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

  // Make sure the style context goes away _before_ we load the binding
  // since that can destroy the relevant presshell.
  mozilla::css::URLValue *bindingURL;
  bool ok = GetBindingURL(doc, &bindingURL);
  if (!ok) {
    dom::Throw(aCx, NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!bindingURL) {
    // No binding, nothing left to do here.
    return obj;
  }

  nsCOMPtr<nsIURI> uri = bindingURL->GetURI();
  nsCOMPtr<nsIPrincipal> principal = bindingURL->mOriginPrincipal.get();

  // We have a binding that must be installed.
  bool dummy;

  nsXBLService* xblService = nsXBLService::GetInstance();
  if (!xblService) {
    dom::Throw(aCx, NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  {
    // Make a scope so that ~nsRefPtr can GC before returning obj.
    RefPtr<nsXBLBinding> binding;
    xblService->LoadBindings(this, uri, principal, getter_AddRefs(binding), &dummy);

    if (binding) {
      if (nsContentUtils::IsSafeToRunScript()) {
        binding->ExecuteAttachedHandler();
      }
      else {
        nsContentUtils::AddScriptRunner(
          NewRunnableMethod(binding, &nsXBLBinding::ExecuteAttachedHandler));
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
Element::GetClassList(nsISupports** aClassList)
{
  NS_ADDREF(*aClassList = ClassList());
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

void
Element::GetElementsByTagName(const nsAString& aLocalName,
                              nsIDOMHTMLCollection** aResult)
{
  *aResult = GetElementsByTagName(aLocalName).take();
}

nsIFrame*
Element::GetStyledFrame()
{
  nsIFrame *frame = GetPrimaryFrame(Flush_Layout);
  return frame ? nsLayoutUtils::GetStyleFrame(frame) : nullptr;
}

nsIScrollableFrame*
Element::GetScrollFrame(nsIFrame **aStyledFrame, bool aFlushLayout)
{
  // it isn't clear what to return for SVG nodes, so just return nothing
  if (IsSVGElement()) {
    if (aStyledFrame) {
      *aStyledFrame = nullptr;
    }
    return nullptr;
  }

  // Inline version of GetStyledFrame to use Flush_None if needed.
  nsIFrame* frame = GetPrimaryFrame(aFlushLayout ? Flush_Layout : Flush_None);
  if (frame) {
    frame = nsLayoutUtils::GetStyleFrame(frame);
  }

  if (aStyledFrame) {
    *aStyledFrame = frame;
  }
  if (!frame) {
    return nullptr;
  }

  // menu frames implement GetScrollTargetFrame but we don't want
  // to use it here.  Similar for comboboxes.
  nsIAtom* type = frame->GetType();
  if (type != nsGkAtoms::menuFrame && type != nsGkAtoms::comboboxControlFrame) {
    nsIScrollableFrame *scrollFrame = frame->GetScrollTargetFrame();
    if (scrollFrame)
      return scrollFrame;
  }

  nsIDocument* doc = OwnerDoc();
  bool quirksMode = doc->GetCompatibilityMode() == eCompatibility_NavQuirks;
  Element* elementWithRootScrollInfo =
    quirksMode ? doc->GetBodyElement() : doc->GetRootElement();
  if (this == elementWithRootScrollInfo) {
    // In quirks mode, the scroll info for the body element should map to the
    // root scrollable frame.
    // In strict mode, the scroll info for the root element should map to the
    // the root scrollable frame.
    return frame->PresContext()->PresShell()->GetRootScrollFrameAsScrollable();
  }

  return nullptr;
}

void
Element::ScrollIntoView()
{
  ScrollIntoView(ScrollIntoViewOptions());
}

void
Element::ScrollIntoView(bool aTop)
{
  ScrollIntoViewOptions options;
  if (!aTop) {
    options.mBlock = ScrollLogicalPosition::End;
  }
  ScrollIntoView(options);
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

  int16_t vpercent = (aOptions.mBlock == ScrollLogicalPosition::Start)
                       ? nsIPresShell::SCROLL_TOP
                       : nsIPresShell::SCROLL_BOTTOM;

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
                                   nsIPresShell::ScrollAxis(),
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
  nsIScrollableFrame* sf = GetScrollFrame();
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
  nsIScrollableFrame* sf = GetScrollFrame(nullptr, false);
  if (!sf) {
    return false;
  }

  nsWeakFrame weakRef(sf->GetScrolledFrame());

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
  nsIScrollableFrame* sf = GetScrollFrame(nullptr, false);
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
    height = sf->GetScrollRange().height + sf->GetScrollPortRect().height;
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
    width = sf->GetScrollRange().width + sf->GetScrollPortRect().width;
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
      (styledFrame->StyleDisplay()->mDisplay != NS_STYLE_DISPLAY_INLINE ||
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

  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
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

  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
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
Element::AddToIdTable(nsIAtom* aId)
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

  nsIAtom* id = DoGetID();
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

already_AddRefed<ShadowRoot>
Element::CreateShadowRoot(ErrorResult& aError)
{
  nsAutoScriptBlocker scriptBlocker;

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfo->NodeInfoManager()->GetNodeInfo(
    nsGkAtoms::documentFragmentNodeName, nullptr, kNameSpaceID_None,
    nsIDOMNode::DOCUMENT_FRAGMENT_NODE);

  RefPtr<nsXBLDocumentInfo> docInfo = new nsXBLDocumentInfo(OwnerDoc());

  nsXBLPrototypeBinding* protoBinding = new nsXBLPrototypeBinding();
  aError = protoBinding->Init(NS_LITERAL_CSTRING("shadowroot"),
                              docInfo, nullptr, true);
  if (aError.Failed()) {
    delete protoBinding;
    return nullptr;
  }

  nsIDocument* doc = GetComposedDoc();
  nsIContent* destroyedFramesFor = nullptr;
  if (doc) {
    nsIPresShell* shell = doc->GetShell();
    if (shell) {
      shell->DestroyFramesFor(this, &destroyedFramesFor);
      MOZ_ASSERT(!shell->FrameManager()->GetDisplayContentsStyleFor(this));
    }
  }
  MOZ_ASSERT(!GetPrimaryFrame());

  // Unlike for XBL, false is the default for inheriting style.
  protoBinding->SetInheritsStyle(false);

  // Calling SetPrototypeBinding takes ownership of protoBinding.
  docInfo->SetPrototypeBinding(NS_LITERAL_CSTRING("shadowroot"), protoBinding);

  RefPtr<ShadowRoot> shadowRoot = new ShadowRoot(this, nodeInfo.forget(),
                                                   protoBinding);

  shadowRoot->SetIsComposedDocParticipant(IsInComposedDoc());

  // Replace the old ShadowRoot with the new one and let the old
  // ShadowRoot know about the younger ShadowRoot because the old
  // ShadowRoot is projected into the younger ShadowRoot's shadow
  // insertion point (if it exists).
  ShadowRoot* olderShadow = GetShadowRoot();
  SetShadowRoot(shadowRoot);
  if (olderShadow) {
    olderShadow->SetYoungerShadow(shadowRoot);

    // Unbind children of older shadow root because they
    // are no longer in the composed tree.
    for (nsIContent* child = olderShadow->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      child->UnbindFromTree(true, false);
    }

    olderShadow->SetIsComposedDocParticipant(false);
  }

  // xblBinding takes ownership of docInfo.
  RefPtr<nsXBLBinding> xblBinding = new nsXBLBinding(shadowRoot, protoBinding);
  shadowRoot->SetAssociatedBinding(xblBinding);
  xblBinding->SetBoundElement(this);

  SetXBLBinding(xblBinding);

  // Recreate the frame for the bound content because binding a ShadowRoot
  // changes how things are rendered.
  if (doc) {
    MOZ_ASSERT(doc == GetComposedDoc());
    nsIPresShell* shell = doc->GetShell();
    if (shell) {
      shell->CreateFramesFor(destroyedFramesFor);
    }
  }

  return shadowRoot.forget();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DestinationInsertionPointList, mParent,
                                      mDestinationPoints)

NS_INTERFACE_TABLE_HEAD(DestinationInsertionPointList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(DestinationInsertionPointList, nsINodeList, nsIDOMNodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(DestinationInsertionPointList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DestinationInsertionPointList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DestinationInsertionPointList)

DestinationInsertionPointList::DestinationInsertionPointList(Element* aElement)
  : mParent(aElement)
{
  nsTArray<nsIContent*>* destPoints = aElement->GetExistingDestInsertionPoints();
  if (destPoints) {
    for (uint32_t i = 0; i < destPoints->Length(); i++) {
      mDestinationPoints.AppendElement(destPoints->ElementAt(i));
    }
  }
}

DestinationInsertionPointList::~DestinationInsertionPointList()
{
}

nsIContent*
DestinationInsertionPointList::Item(uint32_t aIndex)
{
  return mDestinationPoints.SafeElementAt(aIndex);
}

NS_IMETHODIMP
DestinationInsertionPointList::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsIContent* item = Item(aIndex);
  if (!item) {
    return NS_ERROR_FAILURE;
  }

  return CallQueryInterface(item, aReturn);
}

uint32_t
DestinationInsertionPointList::Length() const
{
  return mDestinationPoints.Length();
}

NS_IMETHODIMP
DestinationInsertionPointList::GetLength(uint32_t* aLength)
{
  *aLength = Length();
  return NS_OK;
}

int32_t
DestinationInsertionPointList::IndexOf(nsIContent* aContent)
{
  return mDestinationPoints.IndexOf(aContent);
}

JSObject*
DestinationInsertionPointList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return NodeListBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DestinationInsertionPointList>
Element::GetDestinationInsertionPoints()
{
  RefPtr<DestinationInsertionPointList> list =
    new DestinationInsertionPointList(this);
  return list.forget();
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
                      ErrorResult& aError)
{
  aError = nsContentUtils::CheckQName(aName, false);
  if (aError.Failed()) {
    return;
  }

  nsAutoString nameToUse;
  const nsAttrName* name = InternalGetAttrNameFromQName(aName, &nameToUse);
  if (!name) {
    nsCOMPtr<nsIAtom> nameAtom = NS_Atomize(nameToUse);
    if (!nameAtom) {
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    aError = SetAttr(kNameSpaceID_None, nameAtom, aValue, true);
    return;
  }

  aError = SetAttr(name->NamespaceID(), name->LocalName(), name->GetPrefix(),
                   aValue, true);
  return;
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
  OwnerDoc()->WarnOnceAbout(nsIDocument::eGetAttributeNode);
  return Attributes()->GetNamedItem(aName);
}

already_AddRefed<Attr>
Element::SetAttributeNode(Attr& aNewAttr, ErrorResult& aError)
{
  // XXXbz can we just remove this warning and the one in setAttributeNodeNS and
  // alias setAttributeNode to setAttributeNodeNS?
  OwnerDoc()->WarnOnceAbout(nsIDocument::eSetAttributeNode);

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

  OwnerDoc()->WarnOnceAbout(nsIDocument::eRemoveAttributeNode);
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

  nsCOMPtr<nsIAtom> name = NS_Atomize(aLocalName);
  bool hasAttr = GetAttr(nsid, name, aReturn);
  if (!hasAttr) {
    SetDOMStringToNull(aReturn);
  }
}

void
Element::SetAttributeNS(const nsAString& aNamespaceURI,
                        const nsAString& aQualifiedName,
                        const nsAString& aValue,
                        ErrorResult& aError)
{
  RefPtr<mozilla::dom::NodeInfo> ni;
  aError =
    nsContentUtils::GetNodeInfoFromQName(aNamespaceURI, aQualifiedName,
                                         mNodeInfo->NodeInfoManager(),
                                         nsIDOMNode::ATTRIBUTE_NODE,
                                         getter_AddRefs(ni));
  if (aError.Failed()) {
    return;
  }

  aError = SetAttr(ni->NamespaceID(), ni->NameAtom(), ni->GetPrefixAtom(),
                   aValue, true);
}

void
Element::RemoveAttributeNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName,
                           ErrorResult& aError)
{
  nsCOMPtr<nsIAtom> name = NS_Atomize(aLocalName);
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
  OwnerDoc()->WarnOnceAbout(nsIDocument::eGetAttributeNodeNS);

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
  OwnerDoc()->WarnOnceAbout(nsIDocument::eSetAttributeNodeNS);
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

nsresult
Element::GetElementsByTagNameNS(const nsAString& namespaceURI,
                                const nsAString& localName,
                                nsIDOMHTMLCollection** aResult)
{
  mozilla::ErrorResult rv;
  nsCOMPtr<nsIHTMLCollection> list =
    GetElementsByTagNameNS(namespaceURI, localName, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }
  list.forget(aResult);
  return NS_OK;
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

  nsCOMPtr<nsIAtom> name = NS_Atomize(aLocalName);
  return HasAttr(nsid, name);
}

already_AddRefed<nsIHTMLCollection>
Element::GetElementsByClassName(const nsAString& aClassNames)
{
  return nsContentUtils::GetElementsByClassName(this, aClassNames);
}

nsresult
Element::GetElementsByClassName(const nsAString& aClassNames,
                                nsIDOMHTMLCollection** aResult)
{
  *aResult =
    nsContentUtils::GetElementsByClassName(this, aClassNames).take();
  return NS_OK;
}

/**
 * Returns the count of descendants (inclusive of aContent) in
 * the uncomposed document that are explicitly set as editable.
 */
static uint32_t
EditableInclusiveDescendantCount(nsIContent* aContent)
{
  auto htmlElem = nsGenericHTMLElement::FromContent(aContent);
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
  NS_PRECONDITION(!aParent || !aDocument ||
                  !aParent->HasFlag(NODE_FORCE_XBL_BINDINGS),
                  "Parent in document but flagged as forcing XBL");
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
  nsXULElement* xulElem = nsXULElement::FromContent(this);
  if (xulElem) {
    xulElem->SetXULBindingParent(aBindingParent);
  }
  else
#endif
  {
    if (aBindingParent) {
      nsDOMSlots *slots = DOMSlots();

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
    if (aParent->IsInShadowTree()) {
      ClearSubtreeRootPointer();
      SetFlags(NODE_IS_IN_SHADOW_TREE);
    }
    ShadowRoot* parentContainingShadow = aParent->GetContainingShadow();
    if (parentContainingShadow) {
      DOMSlots()->mContainingShadow = parentContainingShadow;
    }
  }

  bool hadForceXBL = HasFlag(NODE_FORCE_XBL_BINDINGS);

  bool hadParent = !!GetParentNode();

  // Now set the parent and set the "Force attach xbl" flag if needed.
  if (aParent) {
    if (!GetParent()) {
      NS_ADDREF(aParent);
    }
    mParent = aParent;

    if (aParent->HasFlag(NODE_FORCE_XBL_BINDINGS)) {
      SetFlags(NODE_FORCE_XBL_BINDINGS);
    }
  }
  else {
    mParent = aDocument;
  }
  SetParentIsContent(aParent);

  // XXXbz sXBL/XBL2 issue!

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

    // Unset this flag since we now really are in a document.
    UnsetFlags(NODE_FORCE_XBL_BINDINGS |
               // And clear the lazy frame construction bits.
               NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES);
    // And the restyle bits
    UnsetRestyleFlagsIfGecko();
  } else if (IsInShadowTree()) {
    // We're not in a document, but we did get inserted into a shadow tree.
    // Since we won't have any restyle data in the document's restyle trackers,
    // don't let us get inserted with restyle bits set incorrectly.
    //
    // Also clear all the other flags that are cleared above when we do get
    // inserted into a document.
    UnsetFlags(NODE_FORCE_XBL_BINDINGS |
               NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES);
    UnsetRestyleFlagsIfGecko();
  } else {
    // If we're not in the doc and not in a shadow tree,
    // update our subtree pointer.
    SetSubtreeRootPointer(aParent->SubtreeRoot());
  }

  nsIDocument* composedDoc = GetComposedDoc();
  if (composedDoc) {
    // Attached callback must be enqueued whenever custom element is inserted into a
    // document and this document has a browsing context.
    if (GetCustomElementData() && composedDoc->GetDocShell()) {
      // Enqueue an attached callback for the custom element.
      composedDoc->EnqueueLifecycleCallback(nsIDocument::eAttached, this);
    }
  }

  // Propagate scoped style sheet tracking bit.
  if (mParent->IsContent()) {
    nsIContent* parent;
    ShadowRoot* shadowRootParent = ShadowRoot::FromNode(mParent);
    if (shadowRootParent) {
      parent = shadowRootParent->GetHost();
    } else {
      parent = mParent->AsContent();
    }

    bool inStyleScope = parent->IsElementInStyleScope();

    SetIsElementInStyleScope(inStyleScope);
    SetIsElementInStyleScopeFlagOnShadowTree(inStyleScope);
  }

  // This has to be here, rather than in nsGenericHTMLElement::BindToTree,
  //  because it has to happen after updating the parent pointer, but before
  //  recursively binding the kids.
  if (IsHTMLElement()) {
    SetDirOnBind(this, aParent);
  }

  uint32_t editableDescendantCount = 0;

  // If NODE_FORCE_XBL_BINDINGS was set we might have anonymous children
  // that also need to be told that they are moving.
  nsresult rv;
  if (hadForceXBL) {
    nsBindingManager* bmgr = OwnerDoc()->BindingManager();

    nsXBLBinding* contBinding = bmgr->GetBindingWithContent(this);
    // First check if we have a binding...
    if (contBinding) {
      nsCOMPtr<nsIContent> anonRoot = contBinding->GetAnonymousContent();
      bool allowScripts = contBinding->AllowScripts();
      for (nsCOMPtr<nsIContent> child = anonRoot->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        rv = child->BindToTree(aDocument, this, this, allowScripts);
        NS_ENSURE_SUCCESS(rv, rv);

        editableDescendantCount += EditableInclusiveDescendantCount(child);
      }
    }
  }

  UpdateEditableState(false);

  // Now recurse into our kids
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
        nsIContent* parent = GetParent();
        while (parent) {
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
    static_cast<nsStyledElement*>(this)->ReparseStyleAttribute(false);
  }

  if (aDocument) {
    // If we're in a document now, let our mapped attrs know what their new
    // sheet is.  This is safe to run for non-mapped-attribute elements too;
    // it'll just do a small bit of unnecessary work.  But most elements in
    // practice are mapped-attribute elements.
    nsHTMLStyleSheet* sheet = aDocument->GetAttributeStyleSheet();
    if (sheet) {
      mAttrsAndChildren.SetMappedAttrStyleSheet(sheet);
    }
  }

  // Call BindToTree on shadow root children.
  ShadowRoot* shadowRoot = GetShadowRoot();
  if (shadowRoot) {
    shadowRoot->SetIsComposedDocParticipant(IsInComposedDoc());
    for (nsIContent* child = shadowRoot->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      rv = child->BindToTree(nullptr, shadowRoot,
                             shadowRoot->GetBindingParent(),
                             aCompileEventHandlers);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // XXXbz script execution during binding can trigger some of these
  // postcondition asserts....  But we do want that, since things will
  // generally be quite broken when that happens.
  NS_POSTCONDITION(aDocument == GetUncomposedDoc(), "Bound to wrong document");
  NS_POSTCONDITION(aParent == GetParent(), "Bound to wrong parent");
  NS_POSTCONDITION(aBindingParent == GetBindingParent(),
                   "Bound to wrong binding parent");

  return NS_OK;
}

RemoveFromBindingManagerRunnable::RemoveFromBindingManagerRunnable(nsBindingManager* aManager,
                                                                   nsIContent* aContent,
                                                                   nsIDocument* aDoc):
  mManager(aManager), mContent(aContent), mDoc(aDoc)
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
  nsIDocument* document =
    HasFlag(NODE_FORCE_XBL_BINDINGS) ? OwnerDoc() : GetComposedDoc();

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
  }

  ClearInDocument();

#ifdef MOZ_STYLO
  // Drop any servo node data, since it will generally need to be recomputed on
  // re-insertion anyway.
  ServoData().reset();
#endif

  // Editable descendant count only counts descendants that
  // are in the uncomposed document.
  ResetEditableDescendantCount();

  if (aNullParent || !mParent->IsInShadowTree()) {
    UnsetFlags(NODE_IS_IN_SHADOW_TREE);

    // Begin keeping track of our subtree root.
    SetSubtreeRootPointer(aNullParent ? this : mParent->SubtreeRoot());
  }

  if (document) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    // Unlike XBL, bindings for web components shadow DOM
    // do not get uninstalled.
    if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR) && !GetShadowRoot()) {
      nsContentUtils::AddScriptRunner(
        new RemoveFromBindingManagerRunnable(document->BindingManager(), this,
                                             document));
    }

    document->ClearBoxObjectFor(this);

    // Detached must be enqueued whenever custom element is removed from
    // the document and this document has a browsing context.
    if (GetCustomElementData() && document->GetDocShell()) {
      // Enqueue a detached callback for the custom element.
      document->EnqueueLifecycleCallback(nsIDocument::eDetached, this);
    }
  }

  // Unset this since that's what the old code effectively did.
  UnsetFlags(NODE_FORCE_XBL_BINDINGS);
  bool clearBindingParent = true;

#ifdef MOZ_XUL
  nsXULElement* xulElem = nsXULElement::FromContent(this);
  if (xulElem) {
    xulElem->SetXULBindingParent(nullptr);
    clearBindingParent = false;
  }
#endif

  nsDOMSlots* slots = GetExistingDOMSlots();
  if (slots) {
    if (clearBindingParent) {
      slots->mBindingParent = nullptr;
    }
    if (aNullParent || !mParent->IsInShadowTree()) {
      slots->mContainingShadow = nullptr;
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
  ShadowRoot* shadowRoot = GetShadowRoot();
  if (shadowRoot) {
    for (nsIContent* child = shadowRoot->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      child->UnbindFromTree(true, false);
    }

    shadowRoot->SetIsComposedDocParticipant(false);
  }
}

nsICSSDeclaration*
Element::GetSMILOverrideStyle()
{
  Element::nsDOMSlots *slots = DOMSlots();

  if (!slots->mSMILOverrideStyle) {
    slots->mSMILOverrideStyle = new nsDOMCSSAttributeDeclaration(this, true);
  }

  return slots->mSMILOverrideStyle;
}

css::Declaration*
Element::GetSMILOverrideStyleDeclaration()
{
  Element::nsDOMSlots *slots = GetExistingDOMSlots();
  return slots ? slots->mSMILOverrideStyleDeclaration.get() : nullptr;
}

nsresult
Element::SetSMILOverrideStyleDeclaration(css::Declaration* aDeclaration,
                                         bool aNotify)
{
  Element::nsDOMSlots *slots = DOMSlots();

  slots->mSMILOverrideStyleDeclaration = aDeclaration;

  if (aNotify) {
    nsIDocument* doc = GetComposedDoc();
    // Only need to request a restyle if we're in a document.  (We might not
    // be in a document, if we're clearing animation effects on a target node
    // that's been detached since the previous animation sample.)
    if (doc) {
      nsCOMPtr<nsIPresShell> shell = doc->GetShell();
      if (shell) {
        // Pass both eRestyle_StyleAttribute and
        // eRestyle_StyleAttribute_Animations since we don't know if
        // this style represents only the ticking of an existing
        // animation or whether it's a new or changed animation.
        shell->RestyleForAnimation(this, eRestyle_StyleAttribute |
                                         eRestyle_StyleAttribute_Animations);
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

css::Declaration*
Element::GetInlineStyleDeclaration()
{
  return nullptr;
}

nsresult
Element::SetInlineStyleDeclaration(css::Declaration* aDeclaration,
                                   const nsAString* aSerialized,
                                   bool aNotify)
{
  NS_NOTYETIMPLEMENTED("Element::SetInlineStyleDeclaration");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
Element::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  return false;
}

nsChangeHint
Element::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                int32_t aModType) const
{
  return nsChangeHint(0);
}

bool
Element::FindAttributeDependence(const nsIAtom* aAttribute,
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
      GetNodeInfo(name->Atom(), nullptr, kNameSpaceID_None,
                  nsIDOMNode::ATTRIBUTE_NODE);
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
    nsFocusManager::GetFocusedDescendant(window, false, getter_AddRefs(focusedFrame));
  if (contentToBlur == aContent)
    return true;

  // if focus on this element would get redirected, then check the redirected
  // content as well when blurring.
  return (contentToBlur && nsFocusManager::GetRedirectedFocus(aContent) == contentToBlur);
}

bool
Element::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~eCONTENT);
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
  uint16_t inputSource = 0;
  WidgetMouseEvent* sourceMouseEvent = aSourceEvent->AsMouseEvent();
  if (sourceMouseEvent) {
    clickCount = sourceMouseEvent->mClickCount;
    pressure = sourceMouseEvent->pressure;
    inputSource = sourceMouseEvent->inputSource;
  } else if (aSourceEvent->mClass == eKeyboardEventClass) {
    inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
  }
  event.pressure = pressure;
  event.mClickCount = clickCount;
  event.inputSource = inputSource;
  event.mModifiers = aSourceEvent->mModifiers;
  if (aExtraEventFlags) {
    // Be careful not to overwrite existing flags!
    event.mFlags.Union(*aExtraEventFlags);
  }

  return DispatchEvent(aPresContext, &event, aTarget, aFullDispatch, aStatus);
}

nsIFrame*
Element::GetPrimaryFrame(mozFlushType aType)
{
  nsIDocument* doc = GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  // Cause a flush, so we get up-to-date frame
  // information
  if (aType != Flush_None) {
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
Element::SetEventHandler(nsIAtom* aEventName,
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
                               nsIAtom* aName,
                               nsIAtom* aPrefix,
                               const nsAttrValueOrString& aValue,
                               bool aNotify,
                               nsAttrValue& aOldValue,
                               uint8_t* aModType,
                               bool* aHasListeners)
{
  bool modification = false;
  *aHasListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

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
      }
      bool valueMatches = aValue.EqualsAsStrings(*info.mValue);
      if (valueMatches && aPrefix == info.mName->GetPrefix()) {
        return true;
      }
      modification = true;
    }
  }
  *aModType = modification ?
    static_cast<uint8_t>(nsIDOMMutationEvent::MODIFICATION) :
    static_cast<uint8_t>(nsIDOMMutationEvent::ADDITION);
  return false;
}

bool
Element::OnlyNotifySameValueSet(int32_t aNamespaceID, nsIAtom* aName,
                                nsIAtom* aPrefix,
                                const nsAttrValueOrString& aValue,
                                bool aNotify, nsAttrValue& aOldValue,
                                uint8_t* aModType, bool* aHasListeners)
{
  if (!MaybeCheckSameAttrVal(aNamespaceID, aName, aPrefix, aValue, aNotify,
                             aOldValue, aModType, aHasListeners)) {
    return false;
  }

  nsAutoScriptBlocker scriptBlocker;
  nsNodeUtils::AttributeSetToCurrentValue(this, aNamespaceID, aName);
  return true;
}

nsresult
Element::SetAttr(int32_t aNamespaceID, nsIAtom* aName,
                 nsIAtom* aPrefix, const nsAString& aValue,
                 bool aNotify)
{
  // Keep this in sync with SetParsedAttr below

  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");

  if (!mAttrsAndChildren.CanFitMoreAttrs()) {
    return NS_ERROR_FAILURE;
  }

  uint8_t modType;
  bool hasListeners;
  nsAttrValueOrString value(aValue);
  nsAttrValue oldValue;

  if (OnlyNotifySameValueSet(aNamespaceID, aName, aPrefix, value, aNotify,
                             oldValue, &modType, &hasListeners)) {
    return NS_OK;
  }

  nsresult rv = BeforeSetAttr(aNamespaceID, aName, &value, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAttrValue* preparsedAttrValue = value.GetStoredAttrValue();

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNamespaceID, aName, modType,
                                     preparsedAttrValue);
  }

  // Hold a script blocker while calling ParseAttribute since that can call
  // out to id-observers
  nsAutoScriptBlocker scriptBlocker;

  nsAttrValue attrValue;
  if (preparsedAttrValue) {
    attrValue.SwapValueWith(*preparsedAttrValue);
  }
  // Even the value was pre-parsed in BeforeSetAttr, we still need to call
  // ParseAttribute because it can have side effects.
  if (!ParseAttribute(aNamespaceID, aName, aValue, attrValue)) {
    attrValue.SetTo(aValue);
  }

  return SetAttrAndNotify(aNamespaceID, aName, aPrefix, oldValue,
                          attrValue, modType, hasListeners, aNotify,
                          kCallAfterSetAttr);
}

nsresult
Element::SetParsedAttr(int32_t aNamespaceID, nsIAtom* aName,
                       nsIAtom* aPrefix, nsAttrValue& aParsedValue,
                       bool aNotify)
{
  // Keep this in sync with SetAttr above

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

  if (OnlyNotifySameValueSet(aNamespaceID, aName, aPrefix, value, aNotify,
                             oldValue, &modType, &hasListeners)) {
    return NS_OK;
  }

  nsresult rv = BeforeSetAttr(aNamespaceID, aName, &value, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNamespaceID, aName, modType,
                                     &aParsedValue);
  }

  return SetAttrAndNotify(aNamespaceID, aName, aPrefix, oldValue,
                          aParsedValue, modType, hasListeners, aNotify,
                          kCallAfterSetAttr);
}

nsresult
Element::SetAttrAndNotify(int32_t aNamespaceID,
                          nsIAtom* aName,
                          nsIAtom* aPrefix,
                          const nsAttrValue& aOldValue,
                          nsAttrValue& aParsedValue,
                          uint8_t aModType,
                          bool aFireMutation,
                          bool aNotify,
                          bool aCallAfterSetAttr)
{
  nsresult rv;

  nsIDocument* document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  nsMutationGuard::DidMutate();

  // Copy aParsedValue for later use since it will be lost when we call
  // SetAndSwapMappedAttr below
  nsAttrValue valueForAfterSetAttr;
  if (aCallAfterSetAttr) {
    valueForAfterSetAttr.SetTo(aParsedValue);
  }

  bool hadValidDir = false;
  bool hadDirAuto = false;

  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::dir) {
      hadValidDir = HasValidDir() || IsHTMLElement(nsGkAtoms::bdi);
      hadDirAuto = HasDirAuto(); // already takes bdi into account
    }

    // XXXbz Perhaps we should push up the attribute mapping function
    // stuff to Element?
    if (!IsAttributeMapped(aName) ||
        !SetMappedAttribute(document, aName, aParsedValue, &rv)) {
      rv = mAttrsAndChildren.SetAndSwapAttr(aName, aParsedValue);
    }
  }
  else {
    RefPtr<mozilla::dom::NodeInfo> ni;
    ni = mNodeInfo->NodeInfoManager()->GetNodeInfo(aName, aPrefix,
                                                   aNamespaceID,
                                                   nsIDOMNode::ATTRIBUTE_NODE);

    rv = mAttrsAndChildren.SetAndSwapAttr(ni, aParsedValue);
  }

  // If the old value owns its own data, we know it is OK to keep using it.
  const nsAttrValue* oldValue =
      aParsedValue.StoresOwnData() ? &aParsedValue : &aOldValue;

  NS_ENSURE_SUCCESS(rv, rv);

  if (document || HasFlag(NODE_FORCE_XBL_BINDINGS)) {
    RefPtr<nsXBLBinding> binding = GetXBLBinding();
    if (binding) {
      binding->AttributeChanged(aName, aNamespaceID, false, aNotify);
    }
  }

  UpdateState(aNotify);

  nsIDocument* ownerDoc = OwnerDoc();
  if (ownerDoc && GetCustomElementData()) {
    nsCOMPtr<nsIAtom> oldValueAtom = oldValue->GetAsAtom();
    nsCOMPtr<nsIAtom> newValueAtom = valueForAfterSetAttr.GetAsAtom();
    LifecycleCallbackArgs args = {
      nsDependentAtomString(aName),
      aModType == nsIDOMMutationEvent::ADDITION ?
        NullString() : nsDependentAtomString(oldValueAtom),
      nsDependentAtomString(newValueAtom)
    };

    ownerDoc->EnqueueLifecycleCallback(nsIDocument::eAttributeChanged, this, &args);
  }

  if (aCallAfterSetAttr) {
    rv = AfterSetAttr(aNamespaceID, aName, &valueForAfterSetAttr, aNotify);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::dir) {
      OnSetDirAttr(this, &valueForAfterSetAttr,
                   hadValidDir, hadDirAuto, aNotify);
    }
  }

  if (aNotify) {
    // Don't pass aOldValue to AttributeChanged since it may not be reliable.
    // Callers only compute aOldValue under certain conditions which may not
    // be triggered by all nsIMutationObservers.
    nsNodeUtils::AttributeChanged(this, aNamespaceID, aName, aModType,
        oldValue == &aParsedValue ? &aParsedValue : nullptr);
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
    if (!oldValue->IsEmptyString()) {
      mutation.mPrevAttrValue = oldValue->GetAsAtom();
    }
    mutation.mAttrChange = aModType;

    mozAutoSubtreeModified subtree(OwnerDoc(), this);
    (new AsyncEventDispatcher(this, mutation))->RunDOMEventWhenSafe();
  }

  return NS_OK;
}

nsresult
Element::BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                       nsAttrValueOrString* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::_class) {
      // aValue->GetAttrValue will only be non-null here when this is called
      // via Element::SetParsedAttr. This shouldn't happen for "class", but
      // this will handle it.
      if (aValue && !aValue->GetAttrValue()) {
        nsAttrValue attr;
        attr.ParseAtomArray(aValue->String());
        aValue->TakeParsedValue(attr);
      }
    }
  }
  return NS_OK;
}

bool
Element::ParseAttribute(int32_t aNamespaceID,
                        nsIAtom* aAttribute,
                        const nsAString& aValue,
                        nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::_class) {
      SetFlags(NODE_MAY_HAVE_CLASS);
      // Result should have been preparsed above.
      return true;
    }
    if (aAttribute == nsGkAtoms::id) {
      // Store id as an atom.  id="" means that the element has no id,
      // not that it has an emptystring as the id.
      RemoveFromIdTable();
      if (aValue.IsEmpty()) {
        ClearHasID();
        return false;
      }
      aResult.ParseAtom(aValue);
      SetHasID();
      AddToIdTable(aResult.GetAtomValue());
      return true;
    }
  }

  return false;
}

bool
Element::SetMappedAttribute(nsIDocument* aDocument,
                            nsIAtom* aName,
                            nsAttrValue& aValue,
                            nsresult* aRetval)
{
  *aRetval = NS_OK;
  return false;
}

EventListenerManager*
Element::GetEventListenerManagerForAttr(nsIAtom* aAttrName,
                                        bool* aDefer)
{
  *aDefer = true;
  return GetOrCreateListenerManager();
}

BorrowedAttrInfo
Element::GetAttrInfo(int32_t aNamespaceID, nsIAtom* aName) const
{
  NS_ASSERTION(nullptr != aName, "must have attribute name");
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  int32_t index = mAttrsAndChildren.IndexOfAttr(aName, aNamespaceID);
  if (index < 0) {
    return BorrowedAttrInfo(nullptr, nullptr);
  }

  return mAttrsAndChildren.AttrInfoAt(index);
}

BorrowedAttrInfo
Element::GetAttrInfoAt(uint32_t aIndex) const
{
  if (aIndex >= mAttrsAndChildren.AttrCount()) {
    return BorrowedAttrInfo(nullptr, nullptr);
  }

  return mAttrsAndChildren.AttrInfoAt(aIndex);
}

bool
Element::GetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                 nsAString& aResult) const
{
  DOMString str;
  bool haveAttr = GetAttr(aNameSpaceID, aName, str);
  str.ToString(aResult);
  return haveAttr;
}

int32_t
Element::FindAttrValueIn(int32_t aNameSpaceID,
                         nsIAtom* aName,
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
Element::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   bool aNotify)
{
  NS_ASSERTION(nullptr != aName, "must have attribute name");

  int32_t index = mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID);
  if (index < 0) {
    return NS_OK;
  }

  nsresult rv = BeforeSetAttr(aNameSpaceID, aName, nullptr, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument *document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNameSpaceID, aName,
                                     nsIDOMMutationEvent::REMOVAL,
                                     nullptr);
  }

  bool hasMutationListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  // Grab the attr node if needed before we remove it from the attr map
  RefPtr<Attr> attrNode;
  if (hasMutationListeners) {
    nsAutoString ns;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, ns);
    attrNode = GetAttributeNodeNSInternal(ns, nsDependentAtomString(aName));
  }

  // Clear binding to nsIDOMMozNamedAttrMap
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots && slots->mAttributeMap) {
    slots->mAttributeMap->DropAttribute(aNameSpaceID, aName);
  }

  // The id-handling code, and in the future possibly other code, need to
  // react to unexpected attribute changes.
  nsMutationGuard::DidMutate();

  if (aName == nsGkAtoms::id && aNameSpaceID == kNameSpaceID_None) {
    // Have to do this before clearing flag. See RemoveFromIdTable
    RemoveFromIdTable();
    ClearHasID();
  }

  bool hadValidDir = false;
  bool hadDirAuto = false;

  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::dir) {
    hadValidDir = HasValidDir() || IsHTMLElement(nsGkAtoms::bdi);
    hadDirAuto = HasDirAuto(); // already takes bdi into account
  }

  nsAttrValue oldValue;
  rv = mAttrsAndChildren.RemoveAttrAt(index, oldValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (document || HasFlag(NODE_FORCE_XBL_BINDINGS)) {
    RefPtr<nsXBLBinding> binding = GetXBLBinding();
    if (binding) {
      binding->AttributeChanged(aName, aNameSpaceID, true, aNotify);
    }
  }

  UpdateState(aNotify);

  nsIDocument* ownerDoc = OwnerDoc();
  if (ownerDoc && GetCustomElementData()) {
    nsCOMPtr<nsIAtom> oldValueAtom = oldValue.GetAsAtom();
    LifecycleCallbackArgs args = {
      nsDependentAtomString(aName),
      nsDependentAtomString(oldValueAtom),
      NullString()
    };

    ownerDoc->EnqueueLifecycleCallback(nsIDocument::eAttributeChanged, this, &args);
  }

  if (aNotify) {
    // We can always pass oldValue here since there is no new value which could
    // have corrupted it.
    nsNodeUtils::AttributeChanged(this, aNameSpaceID, aName,
                                  nsIDOMMutationEvent::REMOVAL, &oldValue);
  }

  rv = AfterSetAttr(aNameSpaceID, aName, nullptr, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

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
    mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

    mozAutoSubtreeModified subtree(OwnerDoc(), this);
    (new AsyncEventDispatcher(this, mutation))->RunDOMEventWhenSafe();
  }

  return NS_OK;
}

const nsAttrName*
Element::GetAttrNameAt(uint32_t aIndex) const
{
  return mAttrsAndChildren.GetSafeAttrNameAt(aIndex);
}

uint32_t
Element::GetAttrCount() const
{
  return mAttrsAndChildren.AttrCount();
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
  for (int i = value.Length(); i >= 0; --i) {
    if (value[i] == char16_t('"'))
      value.Insert(char16_t('\\'), uint32_t(i));
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
    nsRange::RangeHashTable* ranges =
      static_cast<nsRange::RangeHashTable*>(GetProperty(nsGkAtoms::range));
    fprintf(out, " ranges:%d", ranges ? ranges->Count() : 0);
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
  nsCOMPtr<nsIDOMNodeList> anonymousChildren;
  bindingManager->GetAnonymousNodesFor(nonConstThis,
                                       getter_AddRefs(anonymousChildren));

  if (anonymousChildren) {
    uint32_t length = 0;
    anonymousChildren->GetLength(&length);

    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
    fputs("anonymous-children<\n", out);

    for (uint32_t i = 0; i < length; ++i) {
      nsCOMPtr<nsIDOMNode> node;
      anonymousChildren->Item(i, getter_AddRefs(node));
      nsCOMPtr<nsIContent> child = do_QueryInterface(node);
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

nsresult
Element::PreHandleEventForLinks(EventChainPreVisitor& aVisitor)
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
    return NS_OK;
  }

  // Make sure we meet the preconditions before continuing
  nsCOMPtr<nsIURI> absURI;
  if (!CheckHandleEventForLinksPrecondition(aVisitor, getter_AddRefs(absURI))) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  // We do the status bar updates in PreHandleEvent so that the status bar gets
  // updated even if the event is consumed before we have a chance to set it.
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
    rv = LeaveLink(aVisitor.mPresContext);
    if (NS_SUCCEEDED(rv)) {
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
    }
    break;

  default:
    // switch not in sync with the optimization switch earlier in this function
    NS_NOTREACHED("switch statements not in sync");
    return NS_ERROR_UNEXPECTED;
  }

  return rv;
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
nsDOMTokenListPropertyDestructor(void *aObject, nsIAtom *aProperty,
                                 void *aPropertyValue, void *aData)
{
  nsDOMTokenList* list =
    static_cast<nsDOMTokenList*>(aPropertyValue);
  NS_RELEASE(list);
}

static nsIAtom** sPropertiesToTraverseAndUnlink[] =
  {
    &nsGkAtoms::sandbox,
    &nsGkAtoms::sizes,
    nullptr
  };

// static
nsIAtom***
Element::HTMLSVGPropertiesToTraverseAndUnlink()
{
  return sPropertiesToTraverseAndUnlink;
}

nsDOMTokenList*
Element::GetTokenList(nsIAtom* aAtom,
                      const DOMTokenListSupportedTokenArray aSupportedTokens)
{
#ifdef DEBUG
  nsIAtom*** props =
    HTMLSVGPropertiesToTraverseAndUnlink();
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
  nsCSSSelectorList* selectorList = ParseSelectorList(aSelector, aResult);
  if (!selectorList) {
    // Either we failed (and aResult already has the exception), or this
    // is a pseudo-element-only selector that matches nothing.
    return nullptr;
  }
  OwnerDoc()->FlushPendingLinkUpdates();
  TreeMatchContext matchingContext(false,
                                   nsRuleWalker::eRelevantLinkUnvisited,
                                   OwnerDoc(),
                                   TreeMatchContext::eNeverMatchVisited);
  matchingContext.SetHasSpecifiedScope();
  matchingContext.AddScopeElement(this);
  for (nsINode* node = this; node; node = node->GetParentNode()) {
    if (node->IsElement() &&
        nsCSSRuleProcessor::SelectorListMatches(node->AsElement(),
                                                matchingContext,
                                                selectorList)) {
      return node->AsElement();
    }
  }
  return nullptr;
}

bool
Element::Matches(const nsAString& aSelector, ErrorResult& aError)
{
  nsCSSSelectorList* selectorList = ParseSelectorList(aSelector, aError);
  if (!selectorList) {
    // Either we failed (and aError already has the exception), or this
    // is a pseudo-element-only selector that matches nothing.
    return false;
  }

  OwnerDoc()->FlushPendingLinkUpdates();
  TreeMatchContext matchingContext(false,
                                   nsRuleWalker::eRelevantLinkUnvisited,
                                   OwnerDoc(),
                                   TreeMatchContext::eNeverMatchVisited);
  matchingContext.SetHasSpecifiedScope();
  matchingContext.AddScopeElement(this);
  return nsCSSRuleProcessor::SelectorListMatches(this, matchingContext,
                                                 selectorList);
}

static const nsAttrValue::EnumTable kCORSAttributeTable[] = {
  // Order matters here
  // See ParseCORSValue
  { "anonymous",       CORS_ANONYMOUS       },
  { "use-credentials", CORS_USE_CREDENTIALS },
  { 0 }
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
GetFullScreenError(nsIDocument* aDoc)
{
  if (aDoc->NodePrincipal()->GetAppStatus() >= nsIPrincipal::APP_STATUS_INSTALLED) {
    // Request is in a web app and in the same origin as the web app.
    // Don't enforce as strict security checks for web apps, the user
    // is supposed to have trust in them. However documents cross-origin
    // to the web app must still confirm to the normal security checks.
    return nullptr;
  }

  if (!nsContentUtils::IsRequestFullScreenAllowed()) {
    return "FullscreenDeniedNotInputDriven";
  }

  return nullptr;
}

void
Element::RequestFullscreen(JSContext* aCx, JS::Handle<JS::Value> aOptions,
                           ErrorResult& aError)
{
  MOZ_ASSERT_IF(!aCx, aOptions.isNullOrUndefined());
  // Only grant full-screen requests if this is called from inside a trusted
  // event handler (i.e. inside an event handler for a user initiated event).
  // This stops the full-screen from being abused similar to the popups of old,
  // and it also makes it harder for bad guys' script to go full-screen and
  // spoof the browser chrome/window and phish logins etc.
  // Note that requests for fullscreen inside a web app's origin are exempt
  // from this restriction.
  if (const char* error = GetFullScreenError(OwnerDoc())) {
    OwnerDoc()->DispatchFullscreenError(error);
    return;
  }

  auto request = MakeUnique<FullscreenRequest>(this);
  request->mIsCallerChrome = nsContentUtils::IsCallerChrome();

  RequestFullscreenOptions fsOptions;
  // We need to check if options is convertible to a dict first before
  // trying to init fsOptions; otherwise Init() would throw, and we want to
  // silently ignore non-dictionary values
  if (aCx) {
    bool convertible;
    if (!IsConvertibleToDictionary(aCx, aOptions, &convertible)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    if (convertible) {
      if (!fsOptions.Init(aCx, aOptions)) {
        aError.Throw(NS_ERROR_FAILURE);
        return;
      }

      if (fsOptions.mVrDisplay) {
        request->mVRHMDDevice = fsOptions.mVrDisplay->GetHMD();
      }
    }
  }

  OwnerDoc()->AsyncRequestFullScreen(Move(request));
}

void
Element::RequestPointerLock()
{
  OwnerDoc()->RequestPointerLock(this);
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

  // Wrap the aKeyframes object for the cross-compartment case.
  JS::Rooted<JSObject*> keyframes(aContext);
  keyframes = aKeyframes;
  Maybe<JSAutoCompartment> ac;
  if (js::GetContextCompartment(aContext) !=
      js::GetObjectCompartment(ownerGlobal->GetGlobalJSObject())) {
    ac.emplace(aContext, ownerGlobal->GetGlobalJSObject());
    if (!JS_WrapObject(aContext, &keyframes)) {
      return nullptr;
    }
  }

  RefPtr<KeyframeEffect> effect =
    KeyframeEffect::Constructor(global, aTarget, keyframes, aOptions, aError);
  if (aError.Failed()) {
    return nullptr;
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
    doc->FlushPendingNotifications(Flush_Style);
  }

  Element* elem = this;
  CSSPseudoElementType pseudoType = CSSPseudoElementType::NotPseudo;
  // For animations on generated-content elements, the animations are stored
  // on the parent element.
  nsIAtom* name = NodeInfo()->NameAtom();
  if (name == nsGkAtoms::mozgeneratedcontentbefore) {
    elem = GetParentElement();
    pseudoType = CSSPseudoElementType::before;
  } else if (name == nsGkAtoms::mozgeneratedcontentafter) {
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

NS_IMETHODIMP
Element::GetInnerHTML(nsAString& aInnerHTML)
{
  GetMarkup(false, aInnerHTML);
  return NS_OK;
}

void
Element::SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError)
{
  SetInnerHTMLInternal(aInnerHTML, aError);
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

  if (parent->NodeType() == nsIDOMNode::DOCUMENT_NODE) {
    aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (OwnerDoc()->IsHTMLDocument()) {
    nsIAtom* localName;
    int32_t namespaceID;
    if (parent->IsElement()) {
      localName = parent->NodeInfo()->NameAtom();
      namespaceID = parent->NodeInfo()->NamespaceID();
    } else {
      NS_ASSERTION(parent->NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE,
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
    NS_ASSERTION(parent->NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE,
      "How come the parent isn't a document, a fragment or an element?");
    RefPtr<mozilla::dom::NodeInfo> info =
      OwnerDoc()->NodeInfoManager()->GetNodeInfo(nsGkAtoms::body,
                                                 nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
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
    nsIAtom* contextLocal = destination->NodeInfo()->NameAtom();
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
    static_cast<nsINode*>(this)->InsertBefore(*aNode, GetFirstChild(), aError);
  } else if (aWhere.LowerCaseEqualsLiteral("beforeend")) {
    static_cast<nsINode*>(this)->AppendChild(*aNode, aError);
  } else if (aWhere.LowerCaseEqualsLiteral("afterend")) {
    nsCOMPtr<nsINode> parent = GetParentNode();
    if (!parent) {
      return nullptr;
    }
    parent->InsertBefore(*aNode, GetNextSibling(), aError);
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

nsIEditor*
Element::GetEditorInternal()
{
  nsCOMPtr<nsITextControlElement> textCtrl = do_QueryInterface(this);
  return textCtrl ? textCtrl->GetTextEditor() : nullptr;
}

nsresult
Element::SetBoolAttr(nsIAtom* aAttr, bool aValue)
{
  if (aValue) {
    return SetAttr(kNameSpaceID_None, aAttr, EmptyString(), true);
  }

  return UnsetAttr(kNameSpaceID_None, aAttr, true);
}

void
Element::GetEnumAttr(nsIAtom* aAttr,
                     const char* aDefault,
                     nsAString& aResult) const
{
  GetEnumAttr(aAttr, aDefault, aDefault, aResult);
}

void
Element::GetEnumAttr(nsIAtom* aAttr,
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
Element::SetOrRemoveNullableStringAttr(nsIAtom* aName, const nsAString& aValue,
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
  if (Preferences::GetBool("network.http.enablePerElementReferrer", true) &&
      IsHTMLElement()) {
    const nsAttrValue* referrerValue = GetParsedAttr(nsGkAtoms::referrerpolicy);
    if (referrerValue && referrerValue->Type() == nsAttrValue::eEnum) {
      return net::ReferrerPolicy(referrerValue->GetEnumValue());
    }
  }
  return net::RP_Unset;
}
