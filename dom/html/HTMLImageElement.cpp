/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/HTMLImageElementBinding.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsSize.h"
#include "mozilla/dom/Document.h"
#include "nsImageFrame.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"
#include "nsContainerFrame.h"
#include "nsNodeInfoManager.h"
#include "mozilla/MouseEvents.h"
#include "nsContentPolicyUtils.h"
#include "nsFocusManager.h"
#include "mozilla/dom/DOMIntersectionObserver.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/UserActivation.h"
#include "nsAttrValueOrString.h"
#include "imgLoader.h"
#include "Image.h"

// Responsive images!
#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/ResponsiveImageSelector.h"

#include "imgINotificationObserver.h"
#include "imgRequestProxy.h"

#include "mozilla/CycleCollectedJSContext.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/MappedDeclarationsBuilder.h"
#include "mozilla/Maybe.h"
#include "mozilla/RestyleManager.h"

#include "nsLayoutUtils.h"

using namespace mozilla::net;
using mozilla::Maybe;

NS_IMPL_NS_NEW_HTML_ELEMENT(Image)

#ifdef DEBUG
// Is aSubject a previous sibling of aNode.
static bool IsPreviousSibling(const nsINode* aSubject, const nsINode* aNode) {
  if (aSubject == aNode) {
    return false;
  }

  nsINode* parent = aSubject->GetParentNode();
  if (parent && parent == aNode->GetParentNode()) {
    const Maybe<uint32_t> indexOfSubject = parent->ComputeIndexOf(aSubject);
    const Maybe<uint32_t> indexOfNode = parent->ComputeIndexOf(aNode);
    if (MOZ_LIKELY(indexOfSubject.isSome() && indexOfNode.isSome())) {
      return *indexOfSubject < *indexOfNode;
    }
    // XXX Keep the odd traditional behavior for now.
    return indexOfSubject.isNothing() && indexOfNode.isSome();
  }

  return false;
}
#endif

namespace mozilla::dom {

// Calls LoadSelectedImage on host element unless it has been superseded or
// canceled -- this is the synchronous section of "update the image data".
// https://html.spec.whatwg.org/multipage/embedded-content.html#update-the-image-data
class ImageLoadTask final : public MicroTaskRunnable {
 public:
  ImageLoadTask(HTMLImageElement* aElement, bool aAlwaysLoad,
                bool aUseUrgentStartForChannel)
      : mElement(aElement),
        mAlwaysLoad(aAlwaysLoad),
        mUseUrgentStartForChannel(aUseUrgentStartForChannel) {
    mDocument = aElement->OwnerDoc();
    mDocument->BlockOnload();
  }

  void Run(AutoSlowOperation& aAso) override {
    if (mElement->mPendingImageLoadTask == this) {
      mElement->mPendingImageLoadTask = nullptr;
      mElement->mUseUrgentStartForChannel = mUseUrgentStartForChannel;
      mElement->LoadSelectedImage(true, true, mAlwaysLoad);
    }
    mDocument->UnblockOnload(false);
  }

  bool Suppressed() override {
    nsIGlobalObject* global = mElement->GetOwnerGlobal();
    return global && global->IsInSyncOperation();
  }

  bool AlwaysLoad() const { return mAlwaysLoad; }

 private:
  ~ImageLoadTask() = default;
  RefPtr<HTMLImageElement> mElement;
  nsCOMPtr<Document> mDocument;
  bool mAlwaysLoad;

  // True if we want to set nsIClassOfService::UrgentStart to the channel to
  // get the response ASAP for better user responsiveness.
  bool mUseUrgentStartForChannel;
};

HTMLImageElement::HTMLImageElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {
  // We start out broken
  AddStatesSilently(ElementState::BROKEN);
}

HTMLImageElement::~HTMLImageElement() { nsImageLoadingContent::Destroy(); }

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLImageElement, nsGenericHTMLElement,
                                   mResponsiveSelector)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLImageElement,
                                             nsGenericHTMLElement,
                                             nsIImageLoadingContent,
                                             imgINotificationObserver)

NS_IMPL_ELEMENT_CLONE(HTMLImageElement)

bool HTMLImageElement::IsInteractiveHTMLContent() const {
  return HasAttr(nsGkAtoms::usemap) ||
         nsGenericHTMLElement::IsInteractiveHTMLContent();
}

void HTMLImageElement::AsyncEventRunning(AsyncEventDispatcher* aEvent) {
  nsImageLoadingContent::AsyncEventRunning(aEvent);
}

void HTMLImageElement::GetCurrentSrc(nsAString& aValue) {
  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  if (currentURI) {
    nsAutoCString spec;
    currentURI->GetSpec(spec);
    CopyUTF8toUTF16(spec, aValue);
  } else {
    SetDOMStringToNull(aValue);
  }
}

bool HTMLImageElement::Draggable() const {
  // images may be dragged unless the draggable attribute is false
  return !AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                      nsGkAtoms::_false, eIgnoreCase);
}

bool HTMLImageElement::Complete() {
  // It is still not clear what value should img.complete return in various
  // cases, see https://github.com/whatwg/html/issues/4884

  if (!HasAttr(nsGkAtoms::srcset) && !HasNonEmptyAttr(nsGkAtoms::src)) {
    return true;
  }

  if (!mCurrentRequest || mPendingRequest) {
    return false;
  }

  uint32_t status;
  mCurrentRequest->GetImageStatus(&status);
  return (status &
          (imgIRequest::STATUS_LOAD_COMPLETE | imgIRequest::STATUS_ERROR)) != 0;
}

CSSIntPoint HTMLImageElement::GetXY() {
  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);
  if (!frame) {
    return CSSIntPoint(0, 0);
  }
  return CSSIntPoint::FromAppUnitsRounded(
      frame->GetOffsetTo(frame->PresShell()->GetRootFrame()));
}

int32_t HTMLImageElement::X() { return GetXY().x; }

int32_t HTMLImageElement::Y() { return GetXY().y; }

void HTMLImageElement::GetDecoding(nsAString& aValue) {
  GetEnumAttr(nsGkAtoms::decoding, kDecodingTableDefault->tag, aValue);
}

already_AddRefed<Promise> HTMLImageElement::Decode(ErrorResult& aRv) {
  return nsImageLoadingContent::QueueDecodeAsync(aRv);
}

bool HTMLImageElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsIPrincipal* aMaybeScriptedPrincipal,
                                      nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }
    if (aAttribute == nsGkAtoms::decoding) {
      return aResult.ParseEnumValue(aValue, kDecodingTable,
                                    /* aCaseSensitive = */ false,
                                    kDecodingTableDefault);
    }
    if (aAttribute == nsGkAtoms::loading) {
      return ParseLoadingAttribute(aValue, aResult);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLImageElement::MapAttributesIntoRule(
    MappedDeclarationsBuilder& aBuilder) {
  MapImageAlignAttributeInto(aBuilder);
  MapImageBorderAttributeInto(aBuilder);
  MapImageMarginAttributeInto(aBuilder);
  MapImageSizeAttributesInto(aBuilder, MapAspectRatio::Yes);
  MapCommonAttributesInto(aBuilder);
}

nsChangeHint HTMLImageElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                      int32_t aModType) const {
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::usemap || aAttribute == nsGkAtoms::ismap) {
    retval |= nsChangeHint_ReconstructFrame;
  } else if (aAttribute == nsGkAtoms::alt) {
    if (aModType == MutationEvent_Binding::ADDITION ||
        aModType == MutationEvent_Binding::REMOVAL) {
      retval |= nsChangeHint_ReconstructFrame;
    }
  }
  return retval;
}

NS_IMETHODIMP_(bool)
HTMLImageElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {
      sCommonAttributeMap, sImageMarginSizeAttributeMap,
      sImageBorderAttributeMap, sImageAlignAttributeMap};

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLImageElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

void HTMLImageElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                     const nsAttrValue* aValue, bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && mForm &&
      (aName == nsGkAtoms::name || aName == nsGkAtoms::id)) {
    // remove the image from the hashtable as needed
    if (const auto* old = GetParsedAttr(aName); old && !old->IsEmptyString()) {
      mForm->RemoveImageElementFromTable(
          this, nsDependentAtomString(old->GetAtomValue()));
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

void HTMLImageElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                    const nsAttrValue* aValue,
                                    const nsAttrValue* aOldValue,
                                    nsIPrincipal* aMaybeScriptedPrincipal,
                                    bool aNotify) {
  if (aNameSpaceID != kNameSpaceID_None) {
    return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                              aOldValue,
                                              aMaybeScriptedPrincipal, aNotify);
  }

  nsAttrValueOrString attrVal(aValue);
  if (aName == nsGkAtoms::src) {
    mSrcURI = nullptr;
    if (aValue && !aValue->IsEmptyString()) {
      StringToURI(attrVal.String(), OwnerDoc(), getter_AddRefs(mSrcURI));
    }
  }

  if (aValue) {
    AfterMaybeChangeAttr(aNameSpaceID, aName, attrVal, aOldValue,
                         aMaybeScriptedPrincipal, aNotify);
  }

  if (mForm && (aName == nsGkAtoms::name || aName == nsGkAtoms::id) && aValue &&
      !aValue->IsEmptyString()) {
    // add the image to the hashtable as needed
    MOZ_ASSERT(aValue->Type() == nsAttrValue::eAtom,
               "Expected atom value for name/id");
    mForm->AddImageElementToTable(
        this, nsDependentAtomString(aValue->GetAtomValue()));
  }

  bool forceReload = false;

  if (aName == nsGkAtoms::loading && !mLoading) {
    if (aValue && Loading(aValue->GetEnumValue()) == Loading::Lazy) {
      SetLazyLoading();
    } else if (aOldValue &&
               Loading(aOldValue->GetEnumValue()) == Loading::Lazy) {
      StopLazyLoading(StartLoading::Yes);
    }
  } else if (aName == nsGkAtoms::src && !aValue) {
    // NOTE: regular src value changes are handled in AfterMaybeChangeAttr, so
    // this only needs to handle unsetting the src attribute.
    // Mark channel as urgent-start before load image if the image load is
    // initaiated by a user interaction.
    mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();

    // AfterMaybeChangeAttr handles setting src since it needs to catch
    // img.src = img.src, so we only need to handle the unset case
    if (InResponsiveMode()) {
      if (mResponsiveSelector && mResponsiveSelector->Content() == this) {
        mResponsiveSelector->SetDefaultSource(VoidString());
      }
      UpdateSourceSyncAndQueueImageTask(true);
    } else {
      // Bug 1076583 - We still behave synchronously in the non-responsive case
      CancelImageRequests(aNotify);
    }
  } else if (aName == nsGkAtoms::srcset) {
    // Mark channel as urgent-start before load image if the image load is
    // initaiated by a user interaction.
    mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();

    mSrcsetTriggeringPrincipal = aMaybeScriptedPrincipal;

    PictureSourceSrcsetChanged(this, attrVal.String(), aNotify);
  } else if (aName == nsGkAtoms::sizes) {
    // Mark channel as urgent-start before load image if the image load is
    // initiated by a user interaction.
    mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();

    PictureSourceSizesChanged(this, attrVal.String(), aNotify);
  } else if (aName == nsGkAtoms::decoding) {
    // Request sync or async image decoding.
    SetSyncDecodingHint(
        aValue && static_cast<ImageDecodingType>(aValue->GetEnumValue()) ==
                      ImageDecodingType::Sync);
  } else if (aName == nsGkAtoms::referrerpolicy) {
    ReferrerPolicy referrerPolicy = GetReferrerPolicyAsEnum();
    // FIXME(emilio): Why only  when not in responsive mode? Also see below for
    // aNotify.
    forceReload = aNotify && !InResponsiveMode() &&
                  referrerPolicy != ReferrerPolicy::_empty &&
                  referrerPolicy != ReferrerPolicyFromAttr(aOldValue);
  } else if (aName == nsGkAtoms::crossorigin) {
    // FIXME(emilio): The aNotify bit seems a bit suspicious, but it is useful
    // to avoid extra sync loads, specially in non-responsive mode. Ideally we
    // can unify the responsive and non-responsive code paths (bug 1076583), and
    // simplify this a bit.
    forceReload = aNotify && GetCORSMode() != AttrValueToCORSMode(aOldValue);
  }

  if (forceReload) {
    // Because we load image synchronously in non-responsive-mode, we need to do
    // reload after the attribute has been set if the reload is triggered by
    // cross origin / referrer policy changing.
    //
    // Mark channel as urgent-start before load image if the image load is
    // initiated by a user interaction.
    mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();
    if (InResponsiveMode()) {
      // Per spec, full selection runs when this changes, even though
      // it doesn't directly affect the source selection
      UpdateSourceSyncAndQueueImageTask(true);
    } else if (ShouldLoadImage()) {
      // Bug 1076583 - We still use the older synchronous algorithm in
      // non-responsive mode. Force a new load of the image with the
      // new cross origin policy
      ForceReload(aNotify, IgnoreErrors());
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

void HTMLImageElement::OnAttrSetButNotChanged(int32_t aNamespaceID,
                                              nsAtom* aName,
                                              const nsAttrValueOrString& aValue,
                                              bool aNotify) {
  AfterMaybeChangeAttr(aNamespaceID, aName, aValue, nullptr, nullptr, aNotify);
  return nsGenericHTMLElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                      aValue, aNotify);
}

void HTMLImageElement::AfterMaybeChangeAttr(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString& aValue,
    const nsAttrValue* aOldValue, nsIPrincipal* aMaybeScriptedPrincipal,
    bool aNotify) {
  if (aNamespaceID != kNameSpaceID_None || aName != nsGkAtoms::src) {
    return;
  }

  // We need to force our image to reload.  This must be done here, not in
  // AfterSetAttr or BeforeSetAttr, because we want to do it even if the attr is
  // being set to its existing value, which is normally optimized away as a
  // no-op.
  //
  // If we are in responsive mode, we drop the forced reload behavior,
  // but still trigger a image load task for img.src = img.src per
  // spec.
  //
  // Both cases handle unsetting src in AfterSetAttr
  // Mark channel as urgent-start before load image if the image load is
  // initaiated by a user interaction.
  mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();

  mSrcTriggeringPrincipal = nsContentUtils::GetAttrTriggeringPrincipal(
      this, aValue.String(), aMaybeScriptedPrincipal);

  if (InResponsiveMode()) {
    if (mResponsiveSelector && mResponsiveSelector->Content() == this) {
      mResponsiveSelector->SetDefaultSource(mSrcURI, mSrcTriggeringPrincipal);
    }
    UpdateSourceSyncAndQueueImageTask(true);
  } else if (aNotify && ShouldLoadImage()) {
    // If aNotify is false, we are coming from the parser or some such place;
    // we'll get bound after all the attributes have been set, so we'll do the
    // sync image load from BindToTree. Skip the LoadImage call in that case.

    // Note that this sync behavior is partially removed from the spec, bug
    // 1076583

    // A hack to get animations to reset. See bug 594771.
    mNewRequestsWillNeedAnimationReset = true;

    // Force image loading here, so that we'll try to load the image from
    // network if it's set to be not cacheable.
    // Potentially, false could be passed here rather than aNotify since
    // UpdateState will be called by SetAttrAndNotify, but there are two
    // obstacles to this: 1) LoadImage will end up calling
    // UpdateState(aNotify), and we do not want it to call UpdateState(false)
    // when aNotify is true, and 2) When this function is called by
    // OnAttrSetButNotChanged, SetAttrAndNotify will not subsequently call
    // UpdateState.
    LoadSelectedImage(/* aForce = */ true, aNotify,
                      /* aAlwaysLoad = */ true);

    mNewRequestsWillNeedAnimationReset = false;
  }
}

void HTMLImageElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  // We handle image element with attribute ismap in its corresponding frame
  // element. Set mMultipleActionsPrevented here to prevent the click event
  // trigger the behaviors in Element::PostHandleEventForLinks
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  if (mouseEvent && mouseEvent->IsLeftClickEvent() && IsMap()) {
    mouseEvent->mFlags.mMultipleActionsPrevented = true;
  }
  nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

nsINode* HTMLImageElement::GetScopeChainParent() const {
  if (mForm) {
    return mForm;
  }
  return nsGenericHTMLElement::GetScopeChainParent();
}

bool HTMLImageElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                       int32_t* aTabIndex) {
  int32_t tabIndex = TabIndex();

  if (IsInComposedDoc() && FindImageMap()) {
    if (aTabIndex) {
      // Use tab index on individual map areas
      *aTabIndex = (sTabFocusModel & eTabFocus_linksMask) ? 0 : -1;
    }
    // Image map is not focusable itself, but flag as tabbable
    // so that image map areas get walked into.
    *aIsFocusable = false;

    return false;
  }

  if (aTabIndex) {
    // Can be in tab order if tabindex >=0 and form controls are tabbable.
    *aTabIndex = (sTabFocusModel & eTabFocus_formElementsMask) ? tabIndex : -1;
  }

  *aIsFocusable = IsFormControlDefaultFocusable(aWithMouse) &&
                  (tabIndex >= 0 || GetTabIndexAttrValue().isSome());

  return false;
}

nsresult HTMLImageElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  nsImageLoadingContent::BindToTree(aContext, aParent);

  UpdateFormOwner();

  if (HaveSrcsetOrInPicture()) {
    if (IsInComposedDoc() && !mInDocResponsiveContent) {
      aContext.OwnerDoc().AddResponsiveContent(this);
      mInDocResponsiveContent = true;
    }

    // Mark channel as urgent-start before load image if the image load is
    // initaiated by a user interaction.
    mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();

    // Run selection algorithm when an img element is inserted into a document
    // in order to react to changes in the environment. See note of
    // https://html.spec.whatwg.org/multipage/embedded-content.html#img-environment-changes
    //
    // We also do this in PictureSourceAdded() if it is in <picture>, so here
    // we only need to do if its parent is not <picture>, even if there is no
    // <source>.
    if (!IsInPicture()) {
      UpdateSourceSyncAndQueueImageTask(false);
    }
  } else if (!InResponsiveMode() && HasAttr(nsGkAtoms::src)) {
    // We skip loading when our attributes were set from parser land,
    // so trigger a aForce=false load now to check if things changed.
    // This isn't necessary for responsive mode, since creating the
    // image load task is asynchronous we don't need to take special
    // care to avoid doing so when being filled by the parser.

    // Mark channel as urgent-start before load image if the image load is
    // initaiated by a user interaction.
    mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();

    // We still act synchronously for the non-responsive case (Bug
    // 1076583), but still need to delay if it is unsafe to run
    // script.

    // If loading is temporarily disabled, don't even launch MaybeLoadImage.
    // Otherwise MaybeLoadImage may run later when someone has reenabled
    // loading.
    if (LoadingEnabled() && ShouldLoadImage()) {
      nsContentUtils::AddScriptRunner(
          NewRunnableMethod<bool>("dom::HTMLImageElement::MaybeLoadImage", this,
                                  &HTMLImageElement::MaybeLoadImage, false));
    }
  }

  return rv;
}

void HTMLImageElement::UnbindFromTree(bool aNullParent) {
  if (mForm) {
    if (aNullParent || !FindAncestorForm(mForm)) {
      ClearForm(true);
    } else {
      UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
    }
  }

  if (mInDocResponsiveContent) {
    OwnerDoc()->RemoveResponsiveContent(this);
    mInDocResponsiveContent = false;
  }

  nsImageLoadingContent::UnbindFromTree(aNullParent);
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLImageElement::UpdateFormOwner() {
  if (!mForm) {
    mForm = FindAncestorForm();
  }

  if (mForm && !HasFlag(ADDED_TO_FORM)) {
    // Now we need to add ourselves to the form
    nsAutoString nameVal, idVal;
    GetAttr(nsGkAtoms::name, nameVal);
    GetAttr(nsGkAtoms::id, idVal);

    SetFlags(ADDED_TO_FORM);

    mForm->AddImageElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->AddImageElementToTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->AddImageElementToTable(this, idVal);
    }
  }
}

void HTMLImageElement::MaybeLoadImage(bool aAlwaysForceLoad) {
  // Our base URI may have changed, or we may have had responsive parameters
  // change while not bound to the tree. However, at this moment, we should have
  // updated the responsive source in other places, so we don't have to re-parse
  // src/srcset here. Just need to LoadImage.

  // Note, check LoadingEnabled() after LoadImage call.

  LoadSelectedImage(aAlwaysForceLoad, /* aNotify */ true, aAlwaysForceLoad);

  if (!LoadingEnabled()) {
    CancelImageRequests(true);
  }
}

void HTMLImageElement::NodeInfoChanged(Document* aOldDoc) {
  nsGenericHTMLElement::NodeInfoChanged(aOldDoc);

  // Reparse the URI if needed. Note that we can't check whether we already have
  // a parsed URI, because it might be null even if we have a valid src
  // attribute, if we tried to parse with a different base.
  mSrcURI = nullptr;
  nsAutoString src;
  if (GetAttr(nsGkAtoms::src, src) && !src.IsEmpty()) {
    StringToURI(src, OwnerDoc(), getter_AddRefs(mSrcURI));
  }

  if (mLazyLoading) {
    aOldDoc->GetLazyLoadObserver()->Unobserve(*this);
    mLazyLoading = false;
    SetLazyLoading();
  }

  // Run selection algorithm synchronously when an img element's adopting steps
  // are run, in order to react to changes in the environment, per spec,
  // https://html.spec.whatwg.org/multipage/images.html#reacting-to-dom-mutations,
  // and
  // https://html.spec.whatwg.org/multipage/images.html#reacting-to-environment-changes.
  if (InResponsiveMode()) {
    UpdateResponsiveSource();
  }

  // Force reload image if adoption steps are run.
  // If loading is temporarily disabled, don't even launch script runner.
  // Otherwise script runner may run later when someone has reenabled loading.
  StartLoadingIfNeeded();
}

// static
already_AddRefed<HTMLImageElement> HTMLImageElement::Image(
    const GlobalObject& aGlobal, const Optional<uint32_t>& aWidth,
    const Optional<uint32_t>& aHeight, ErrorResult& aError) {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal.GetAsSupports());
  Document* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo = doc->NodeInfoManager()->GetNodeInfo(
      nsGkAtoms::img, nullptr, kNameSpaceID_XHTML, ELEMENT_NODE);

  auto* nim = nodeInfo->NodeInfoManager();
  RefPtr<HTMLImageElement> img = new (nim) HTMLImageElement(nodeInfo.forget());

  if (aWidth.WasPassed()) {
    img->SetWidth(aWidth.Value(), aError);
    if (aError.Failed()) {
      return nullptr;
    }

    if (aHeight.WasPassed()) {
      img->SetHeight(aHeight.Value(), aError);
      if (aError.Failed()) {
        return nullptr;
      }
    }
  }

  return img.forget();
}

uint32_t HTMLImageElement::Height() { return GetWidthHeightForImage().height; }

uint32_t HTMLImageElement::Width() { return GetWidthHeightForImage().width; }

nsIntSize HTMLImageElement::NaturalSize() {
  if (!mCurrentRequest) {
    return {};
  }

  nsCOMPtr<imgIContainer> image;
  mCurrentRequest->GetImage(getter_AddRefs(image));
  if (!image) {
    return {};
  }

  nsIntSize size;
  Unused << image->GetHeight(&size.height);
  Unused << image->GetWidth(&size.width);

  ImageResolution resolution = image->GetResolution();
  // NOTE(emilio): What we implement here matches the image-set() spec, but it's
  // unclear whether this is the right thing to do, see
  // https://github.com/whatwg/html/pull/5574#issuecomment-826335244.
  if (mResponsiveSelector) {
    float density = mResponsiveSelector->GetSelectedImageDensity();
    MOZ_ASSERT(density >= 0.0);
    resolution.ScaleBy(density);
  }

  resolution.ApplyTo(size.width, size.height);
  return size;
}

nsresult HTMLImageElement::CopyInnerTo(HTMLImageElement* aDest) {
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // In SetAttr (called from nsGenericHTMLElement::CopyInnerTo), aDest skipped
  // doing the image load because we passed in false for aNotify.  But we
  // really do want it to do the load, so set it up to happen once the cloning
  // reaches a stable state.
  if (!aDest->InResponsiveMode() && aDest->HasAttr(nsGkAtoms::src) &&
      aDest->ShouldLoadImage()) {
    // Mark channel as urgent-start before load image if the image load is
    // initaiated by a user interaction.
    mUseUrgentStartForChannel = UserActivation::IsHandlingUserInput();

    nsContentUtils::AddScriptRunner(
        NewRunnableMethod<bool>("dom::HTMLImageElement::MaybeLoadImage", aDest,
                                &HTMLImageElement::MaybeLoadImage, false));
  }

  return NS_OK;
}

CORSMode HTMLImageElement::GetCORSMode() {
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

JSObject* HTMLImageElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return HTMLImageElement_Binding::Wrap(aCx, this, aGivenProto);
}

#ifdef DEBUG
HTMLFormElement* HTMLImageElement::GetForm() const { return mForm; }
#endif

void HTMLImageElement::SetForm(HTMLFormElement* aForm) {
  MOZ_ASSERT(aForm, "Don't pass null here");
  NS_ASSERTION(!mForm,
               "We don't support switching from one non-null form to another.");

  mForm = aForm;
}

void HTMLImageElement::ClearForm(bool aRemoveFromForm) {
  NS_ASSERTION((mForm != nullptr) == HasFlag(ADDED_TO_FORM),
               "Form control should have had flag set correctly");

  if (!mForm) {
    return;
  }

  if (aRemoveFromForm) {
    nsAutoString nameVal, idVal;
    GetAttr(nsGkAtoms::name, nameVal);
    GetAttr(nsGkAtoms::id, idVal);

    mForm->RemoveImageElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, idVal);
    }
  }

  UnsetFlags(ADDED_TO_FORM);
  mForm = nullptr;
}

void HTMLImageElement::UpdateSourceSyncAndQueueImageTask(
    bool aAlwaysLoad, const HTMLSourceElement* aSkippedSource) {
  // Per spec, when updating the image data or reacting to environment
  // changes, we always run the full selection (including selecting the source
  // element and the best fit image from srcset) even if it doesn't directly
  // affect the source selection.
  //
  // However, in the spec of updating the image data, the selection of image
  // source URL is in the asynchronous part (i.e. in a microtask), and so this
  // doesn't guarantee that the image style is correct after we flush the style
  // synchornously. So here we update the responsive source synchronously always
  // to make sure the image source is always up-to-date after each DOM mutation.
  // Spec issue: https://github.com/whatwg/html/issues/8207.
  const bool changed = UpdateResponsiveSource(aSkippedSource);

  // If loading is temporarily disabled, we don't want to queue tasks
  // that may then run when loading is re-enabled.
  if (!LoadingEnabled() || !ShouldLoadImage()) {
    return;
  }

  // Ensure that we don't overwrite a previous load request that requires
  // a complete load to occur.
  bool alwaysLoad = aAlwaysLoad;
  if (mPendingImageLoadTask) {
    alwaysLoad = alwaysLoad || mPendingImageLoadTask->AlwaysLoad();
  }

  if (!changed && !alwaysLoad) {
    return;
  }

  QueueImageLoadTask(alwaysLoad);
}

bool HTMLImageElement::HaveSrcsetOrInPicture() {
  if (HasAttr(nsGkAtoms::srcset)) {
    return true;
  }

  return IsInPicture();
}

bool HTMLImageElement::InResponsiveMode() {
  // When we lose srcset or leave a <picture> element, the fallback to img.src
  // will happen from the microtask, and we should behave responsively in the
  // interim
  return mResponsiveSelector || mPendingImageLoadTask ||
         HaveSrcsetOrInPicture();
}

bool HTMLImageElement::SelectedSourceMatchesLast(nsIURI* aSelectedSource) {
  // If there was no selected source previously, we don't want to short-circuit
  // the load. Similarly for if there is no newly selected source.
  if (!mLastSelectedSource || !aSelectedSource) {
    return false;
  }
  bool equal = false;
  return NS_SUCCEEDED(mLastSelectedSource->Equals(aSelectedSource, &equal)) &&
         equal;
}

nsresult HTMLImageElement::LoadSelectedImage(bool aForce, bool aNotify,
                                             bool aAlwaysLoad) {
  // In responsive mode, we have to make sure we ran the full selection algrithm
  // before loading the selected image.
  // Use this assertion to catch any cases we missed.
  MOZ_ASSERT(!UpdateResponsiveSource(),
             "The image source should be the same because we update the "
             "responsive source synchronously");

  // The density is default to 1.0 for the src attribute case.
  double currentDensity = mResponsiveSelector
                              ? mResponsiveSelector->GetSelectedImageDensity()
                              : 1.0;

  nsCOMPtr<nsIURI> selectedSource;
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  ImageLoadType type = eImageLoadType_Normal;
  bool hasSrc = false;
  if (mResponsiveSelector) {
    selectedSource = mResponsiveSelector->GetSelectedImageURL();
    triggeringPrincipal =
        mResponsiveSelector->GetSelectedImageTriggeringPrincipal();
    type = eImageLoadType_Imageset;
  } else if (mSrcURI || HasAttr(nsGkAtoms::src)) {
    hasSrc = true;
    if (mSrcURI) {
      selectedSource = mSrcURI;
      if (HaveSrcsetOrInPicture()) {
        // If we have a srcset attribute or are in a <picture> element, we
        // always use the Imageset load type, even if we parsed no valid
        // responsive sources from either, per spec.
        type = eImageLoadType_Imageset;
      }
      triggeringPrincipal = mSrcTriggeringPrincipal;
    }
  }

  if (!aAlwaysLoad && SelectedSourceMatchesLast(selectedSource)) {
    // Update state when only density may have changed (i.e., the source to load
    // hasn't changed, and we don't do any request at all). We need (apart from
    // updating our internal state) to tell the image frame because its
    // intrinsic size may have changed.
    //
    // In the case we actually trigger a new load, that load will trigger a call
    // to nsImageFrame::NotifyNewCurrentRequest, which takes care of that for
    // us.
    SetDensity(currentDensity);
    return NS_OK;
  }

  // Before we actually defer the lazy-loading
  if (mLazyLoading) {
    if (!selectedSource ||
        !nsContentUtils::IsImageAvailable(this, selectedSource,
                                          triggeringPrincipal, GetCORSMode())) {
      return NS_OK;
    }
    StopLazyLoading(StartLoading::No);
  }

  nsresult rv = NS_ERROR_FAILURE;

  // src triggers an error event on invalid URI, unlike other loads.
  if (selectedSource || hasSrc) {
    rv = LoadImage(selectedSource, aForce, aNotify, type, triggeringPrincipal);
  }

  mLastSelectedSource = selectedSource;
  mCurrentDensity = currentDensity;

  if (NS_FAILED(rv)) {
    CancelImageRequests(aNotify);
  }
  return rv;
}

void HTMLImageElement::PictureSourceSrcsetChanged(nsIContent* aSourceNode,
                                                  const nsAString& aNewValue,
                                                  bool aNotify) {
  MOZ_ASSERT(aSourceNode == this || IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  nsIContent* currentSrc =
      mResponsiveSelector ? mResponsiveSelector->Content() : nullptr;

  if (aSourceNode == currentSrc) {
    // We're currently using this node as our responsive selector
    // source.
    nsCOMPtr<nsIPrincipal> principal;
    if (aSourceNode == this) {
      principal = mSrcsetTriggeringPrincipal;
    } else if (auto* source = HTMLSourceElement::FromNode(aSourceNode)) {
      principal = source->GetSrcsetTriggeringPrincipal();
    }
    mResponsiveSelector->SetCandidatesFromSourceSet(aNewValue, principal);
  }

  if (!mInDocResponsiveContent && IsInComposedDoc()) {
    OwnerDoc()->AddResponsiveContent(this);
    mInDocResponsiveContent = true;
  }

  // This always triggers the image update steps per the spec, even if
  // we are not using this source.
  UpdateSourceSyncAndQueueImageTask(true);
}

void HTMLImageElement::PictureSourceSizesChanged(nsIContent* aSourceNode,
                                                 const nsAString& aNewValue,
                                                 bool aNotify) {
  MOZ_ASSERT(aSourceNode == this || IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  nsIContent* currentSrc =
      mResponsiveSelector ? mResponsiveSelector->Content() : nullptr;

  if (aSourceNode == currentSrc) {
    // We're currently using this node as our responsive selector
    // source.
    mResponsiveSelector->SetSizesFromDescriptor(aNewValue);
  }

  // This always triggers the image update steps per the spec, even if
  // we are not using this source.
  UpdateSourceSyncAndQueueImageTask(true);
}

void HTMLImageElement::PictureSourceMediaOrTypeChanged(nsIContent* aSourceNode,
                                                       bool aNotify) {
  MOZ_ASSERT(IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  // This always triggers the image update steps per the spec, even if
  // we are not switching to/from this source
  UpdateSourceSyncAndQueueImageTask(true);
}

void HTMLImageElement::PictureSourceDimensionChanged(
    HTMLSourceElement* aSourceNode, bool aNotify) {
  MOZ_ASSERT(IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  // "width" and "height" affect the dimension of images, but they don't have
  // impact on the selection of <source> elements. In other words,
  // UpdateResponsiveSource doesn't change the source, so all we need to do is
  // just request restyle.
  if (mResponsiveSelector && mResponsiveSelector->Content() == aSourceNode) {
    InvalidateAttributeMapping();
  }
}

void HTMLImageElement::PictureSourceAdded(HTMLSourceElement* aSourceNode) {
  MOZ_ASSERT(!aSourceNode || IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  UpdateSourceSyncAndQueueImageTask(true);
}

void HTMLImageElement::PictureSourceRemoved(HTMLSourceElement* aSourceNode) {
  MOZ_ASSERT(!aSourceNode || IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  UpdateSourceSyncAndQueueImageTask(true, aSourceNode);
}

bool HTMLImageElement::UpdateResponsiveSource(
    const HTMLSourceElement* aSkippedSource) {
  bool hadSelector = !!mResponsiveSelector;

  nsIContent* currentSource =
      mResponsiveSelector ? mResponsiveSelector->Content() : nullptr;

  // Walk source nodes previous to ourselves if IsInPicture().
  nsINode* candidateSource =
      IsInPicture() ? GetParentElement()->GetFirstChild() : this;

  // Initialize this as nullptr so we don't have to nullify it when runing out
  // of siblings without finding ourself, e.g. XBL magic.
  RefPtr<ResponsiveImageSelector> newResponsiveSelector = nullptr;

  for (; candidateSource; candidateSource = candidateSource->GetNextSibling()) {
    if (aSkippedSource == candidateSource) {
      continue;
    }

    if (candidateSource == currentSource) {
      // found no better source before current, re-run selection on
      // that and keep it if it's still usable.
      bool changed = mResponsiveSelector->SelectImage(true);
      if (mResponsiveSelector->NumCandidates()) {
        bool isUsableCandidate = true;

        // an otherwise-usable source element may still have a media query that
        // may not match any more.
        if (candidateSource->IsHTMLElement(nsGkAtoms::source) &&
            !SourceElementMatches(candidateSource->AsElement())) {
          isUsableCandidate = false;
        }

        if (isUsableCandidate) {
          // We are still using the current source, but the selected image may
          // be changed, so always set the density from the selected image.
          SetDensity(mResponsiveSelector->GetSelectedImageDensity());
          return changed;
        }
      }

      // no longer valid
      newResponsiveSelector = nullptr;
      if (candidateSource == this) {
        // No further possibilities
        break;
      }
    } else if (candidateSource == this) {
      // We are the last possible source
      newResponsiveSelector =
          TryCreateResponsiveSelector(candidateSource->AsElement());
      break;
    } else if (auto* source = HTMLSourceElement::FromNode(candidateSource)) {
      if (RefPtr<ResponsiveImageSelector> selector =
              TryCreateResponsiveSelector(source)) {
        newResponsiveSelector = selector.forget();
        // This led to a valid source, stop
        break;
      }
    }
  }

  // If we reach this point, either:
  // - there was no selector originally, and there is not one now
  // - there was no selector originally, and there is one now
  // - there was a selector, and there is a different one now
  // - there was a selector, and there is not one now
  SetResponsiveSelector(std::move(newResponsiveSelector));
  return hadSelector || mResponsiveSelector;
}

/*static */
bool HTMLImageElement::SupportedPictureSourceType(const nsAString& aType) {
  nsAutoString type;
  nsAutoString params;

  nsContentUtils::SplitMimeType(aType, type, params);
  if (type.IsEmpty()) {
    return true;
  }

  return imgLoader::SupportImageWithMimeType(
      NS_ConvertUTF16toUTF8(type), AcceptedMimeTypes::IMAGES_AND_DOCUMENTS);
}

bool HTMLImageElement::SourceElementMatches(Element* aSourceElement) {
  MOZ_ASSERT(aSourceElement->IsHTMLElement(nsGkAtoms::source));

  MOZ_ASSERT(IsInPicture());
  MOZ_ASSERT(IsPreviousSibling(aSourceElement, this));

  // Check media and type
  auto* src = static_cast<HTMLSourceElement*>(aSourceElement);
  if (!src->MatchesCurrentMedia()) {
    return false;
  }

  nsAutoString type;
  return !src->GetAttr(nsGkAtoms::type, type) ||
         SupportedPictureSourceType(type);
}

already_AddRefed<ResponsiveImageSelector>
HTMLImageElement::TryCreateResponsiveSelector(Element* aSourceElement) {
  nsCOMPtr<nsIPrincipal> principal;

  // Skip if this is not a <source> with matching media query
  bool isSourceTag = aSourceElement->IsHTMLElement(nsGkAtoms::source);
  if (isSourceTag) {
    if (!SourceElementMatches(aSourceElement)) {
      return nullptr;
    }
    auto* source = HTMLSourceElement::FromNode(aSourceElement);
    principal = source->GetSrcsetTriggeringPrincipal();
  } else if (aSourceElement->IsHTMLElement(nsGkAtoms::img)) {
    // Otherwise this is the <img> tag itself
    MOZ_ASSERT(aSourceElement == this);
    principal = mSrcsetTriggeringPrincipal;
  }

  // Skip if has no srcset or an empty srcset
  nsString srcset;
  if (!aSourceElement->GetAttr(nsGkAtoms::srcset, srcset)) {
    return nullptr;
  }

  if (srcset.IsEmpty()) {
    return nullptr;
  }

  // Try to parse
  RefPtr<ResponsiveImageSelector> sel =
      new ResponsiveImageSelector(aSourceElement);
  if (!sel->SetCandidatesFromSourceSet(srcset, principal)) {
    // No possible candidates, don't need to bother parsing sizes
    return nullptr;
  }

  nsAutoString sizes;
  aSourceElement->GetAttr(nsGkAtoms::sizes, sizes);
  sel->SetSizesFromDescriptor(sizes);

  // If this is the <img> tag, also pull in src as the default source
  if (!isSourceTag) {
    MOZ_ASSERT(aSourceElement == this);
    if (mSrcURI) {
      sel->SetDefaultSource(mSrcURI, mSrcTriggeringPrincipal);
    }
  }

  return sel.forget();
}

/* static */
bool HTMLImageElement::SelectSourceForTagWithAttrs(
    Document* aDocument, bool aIsSourceTag, const nsAString& aSrcAttr,
    const nsAString& aSrcsetAttr, const nsAString& aSizesAttr,
    const nsAString& aTypeAttr, const nsAString& aMediaAttr,
    nsAString& aResult) {
  MOZ_ASSERT(aIsSourceTag || (aTypeAttr.IsEmpty() && aMediaAttr.IsEmpty()),
             "Passing type or media attrs makes no sense without aIsSourceTag");
  MOZ_ASSERT(!aIsSourceTag || aSrcAttr.IsEmpty(),
             "Passing aSrcAttr makes no sense with aIsSourceTag set");

  if (aSrcsetAttr.IsEmpty()) {
    if (!aIsSourceTag) {
      // For an <img> with no srcset, we would always select the src attr.
      aResult.Assign(aSrcAttr);
      return true;
    }
    // Otherwise, a <source> without srcset is never selected
    return false;
  }

  // Would not consider source tags with unsupported media or type
  if (aIsSourceTag &&
      ((!aMediaAttr.IsVoid() && !HTMLSourceElement::WouldMatchMediaForDocument(
                                    aMediaAttr, aDocument)) ||
       (!aTypeAttr.IsVoid() && !SupportedPictureSourceType(aTypeAttr)))) {
    return false;
  }

  // Using srcset or picture <source>, build a responsive selector for this tag.
  RefPtr<ResponsiveImageSelector> sel = new ResponsiveImageSelector(aDocument);

  sel->SetCandidatesFromSourceSet(aSrcsetAttr);
  if (!aSizesAttr.IsEmpty()) {
    sel->SetSizesFromDescriptor(aSizesAttr);
  }
  if (!aIsSourceTag) {
    sel->SetDefaultSource(aSrcAttr);
  }

  if (sel->GetSelectedImageURLSpec(aResult)) {
    return true;
  }

  if (!aIsSourceTag) {
    // <img> tag with no match would definitively load nothing.
    aResult.Truncate();
    return true;
  }

  // <source> tags with no match would leave source yet-undetermined.
  return false;
}

void HTMLImageElement::DestroyContent() {
  // Clear mPendingImageLoadTask to avoid running LoadSelectedImage() after
  // getting destroyed.
  mPendingImageLoadTask = nullptr;

  mResponsiveSelector = nullptr;

  nsImageLoadingContent::Destroy();
  nsGenericHTMLElement::DestroyContent();
}

void HTMLImageElement::MediaFeatureValuesChanged() {
  UpdateSourceSyncAndQueueImageTask(false);
}

bool HTMLImageElement::ShouldLoadImage() const {
  return OwnerDoc()->ShouldLoadImages();
}

void HTMLImageElement::SetLazyLoading() {
  if (mLazyLoading) {
    return;
  }

  // If scripting is disabled don't do lazy load.
  // https://whatpr.org/html/3752/images.html#updating-the-image-data
  //
  // Same for printing.
  Document* doc = OwnerDoc();
  if (!doc->IsScriptEnabled() || doc->IsStaticDocument()) {
    return;
  }

  doc->EnsureLazyLoadObserver().Observe(*this);
  mLazyLoading = true;
  UpdateImageState(true);
}

void HTMLImageElement::StartLoadingIfNeeded() {
  if (!LoadingEnabled() || !ShouldLoadImage()) {
    return;
  }

  // Use script runner for the case the adopt is from appendChild.
  // Bug 1076583 - We still behave synchronously in the non-responsive case
  nsContentUtils::AddScriptRunner(
      InResponsiveMode()
          ? NewRunnableMethod<bool>("dom::HTMLImageElement::QueueImageLoadTask",
                                    this, &HTMLImageElement::QueueImageLoadTask,
                                    true)
          : NewRunnableMethod<bool>("dom::HTMLImageElement::MaybeLoadImage",
                                    this, &HTMLImageElement::MaybeLoadImage,
                                    true));
}

void HTMLImageElement::StopLazyLoading(StartLoading aStartLoading) {
  if (!mLazyLoading) {
    return;
  }
  mLazyLoading = false;
  Document* doc = OwnerDoc();
  if (auto* obs = doc->GetLazyLoadObserver()) {
    obs->Unobserve(*this);
  }

  if (aStartLoading == StartLoading::Yes) {
    StartLoadingIfNeeded();
  }
}

const StyleLockedDeclarationBlock*
HTMLImageElement::GetMappedAttributesFromSource() const {
  if (!IsInPicture() || !mResponsiveSelector) {
    return nullptr;
  }

  const auto* source =
      HTMLSourceElement::FromNodeOrNull(mResponsiveSelector->Content());
  if (!source) {
    return nullptr;
  }

  MOZ_ASSERT(IsPreviousSibling(source, this),
             "Incorrect or out-of-date source");
  return source->GetAttributesMappedForImage();
}

void HTMLImageElement::InvalidateAttributeMapping() {
  if (!IsInPicture()) {
    return;
  }

  nsPresContext* presContext = nsContentUtils::GetContextForContent(this);
  if (!presContext) {
    return;
  }

  // Note: Unfortunately, we have to use RESTYLE_SELF, instead of using
  // RESTYLE_STYLE_ATTRIBUTE or other ways, to avoid re-selector-match because
  // we are using Gecko_GetExtraContentStyleDeclarations() to retrieve the
  // extra declaration block from |this|'s width and height attributes, and
  // other restyle hints seems not enough.
  // FIXME: We may refine this together with the restyle for presentation
  // attributes in RestyleManger::AttributeChagned()
  presContext->RestyleManager()->PostRestyleEvent(
      this, RestyleHint::RESTYLE_SELF, nsChangeHint(0));
}

void HTMLImageElement::SetResponsiveSelector(
    RefPtr<ResponsiveImageSelector>&& aSource) {
  if (mResponsiveSelector == aSource) {
    return;
  }

  mResponsiveSelector = std::move(aSource);

  // Invalidate the style if needed.
  InvalidateAttributeMapping();

  // Update density.
  SetDensity(mResponsiveSelector
                 ? mResponsiveSelector->GetSelectedImageDensity()
                 : 1.0);
}

void HTMLImageElement::SetDensity(double aDensity) {
  if (mCurrentDensity == aDensity) {
    return;
  }

  mCurrentDensity = aDensity;

  // Invalidate the reflow.
  if (nsImageFrame* f = do_QueryFrame(GetPrimaryFrame())) {
    f->ResponsiveContentDensityChanged();
  }
}

void HTMLImageElement::QueueImageLoadTask(bool aAlwaysLoad) {
  RefPtr<ImageLoadTask> task =
      new ImageLoadTask(this, aAlwaysLoad, mUseUrgentStartForChannel);
  // The task checks this to determine if it was the last
  // queued event, and so earlier tasks are implicitly canceled.
  mPendingImageLoadTask = task;
  CycleCollectedJSContext::Get()->DispatchToMicroTask(task.forget());
}

}  // namespace mozilla::dom
