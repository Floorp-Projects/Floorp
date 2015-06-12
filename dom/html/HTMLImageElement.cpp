/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLImageElementBinding.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsSize.h"
#include "nsIDocument.h"
#include "nsIDOMMutationEvent.h"
#include "nsIScriptContext.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsContainerFrame.h"
#include "nsNodeInfoManager.h"
#include "mozilla/MouseEvents.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsFocusManager.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "nsAttrValueOrString.h"
#include "imgLoader.h"

// Responsive images!
#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/ResponsiveImageSelector.h"

#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgINotificationObserver.h"
#include "imgRequestProxy.h"

#include "nsILoadGroup.h"

#include "nsRuleData.h"

#include "nsIDOMHTMLMapElement.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"

#include "nsLayoutUtils.h"

#include "mozilla/Preferences.h"
static const char *kPrefSrcsetEnabled = "dom.image.srcset.enabled";

NS_IMPL_NS_NEW_HTML_ELEMENT(Image)

#ifdef DEBUG
// Is aSubject a previous sibling of aNode.
static bool IsPreviousSibling(nsINode *aSubject, nsINode *aNode)
{
  if (aSubject == aNode) {
    return false;
  }

  nsINode *parent = aSubject->GetParentNode();
  if (parent && parent == aNode->GetParentNode()) {
    return parent->IndexOf(aSubject) < parent->IndexOf(aNode);
  }

  return false;
}
#endif

namespace mozilla {
namespace dom {

// Calls LoadSelectedImage on host element unless it has been superseded or
// canceled -- this is the synchronous section of "update the image data".
// https://html.spec.whatwg.org/multipage/embedded-content.html#update-the-image-data
class ImageLoadTask : public nsRunnable
{
public:
  explicit ImageLoadTask(HTMLImageElement *aElement) :
    mElement(aElement)
  {
    mDocument = aElement->OwnerDoc();
    mDocument->BlockOnload();
  }

  NS_IMETHOD Run()
  {
    if (mElement->mPendingImageLoadTask == this) {
      mElement->mPendingImageLoadTask = nullptr;
      mElement->LoadSelectedImage(true, true);
    }
    mDocument->UnblockOnload(false);
    return NS_OK;
  }

private:
  ~ImageLoadTask() {}
  nsRefPtr<HTMLImageElement> mElement;
  nsCOMPtr<nsIDocument> mDocument;
};

HTMLImageElement::HTMLImageElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , mForm(nullptr)
{
  // We start out broken
  AddStatesSilently(NS_EVENT_STATE_BROKEN);
}

HTMLImageElement::~HTMLImageElement()
{
  DestroyImageLoadingContent();
}


NS_IMPL_ADDREF_INHERITED(HTMLImageElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLImageElement, Element)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLImageElement,
                                   nsGenericHTMLElement,
                                   mResponsiveSelector)

// QueryInterface implementation for HTMLImageElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLImageElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLImageElement,
                               nsIDOMHTMLImageElement,
                               nsIImageLoadingContent,
                               imgIOnloadBlocker,
                               imgINotificationObserver)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)


NS_IMPL_ELEMENT_CLONE(HTMLImageElement)


NS_IMPL_STRING_ATTR(HTMLImageElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLImageElement, Align, align)
NS_IMPL_STRING_ATTR(HTMLImageElement, Alt, alt)
NS_IMPL_STRING_ATTR(HTMLImageElement, Border, border)
NS_IMPL_INT_ATTR(HTMLImageElement, Hspace, hspace)
NS_IMPL_BOOL_ATTR(HTMLImageElement, IsMap, ismap)
NS_IMPL_URI_ATTR(HTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(HTMLImageElement, Sizes, sizes)
NS_IMPL_STRING_ATTR(HTMLImageElement, Lowsrc, lowsrc)
NS_IMPL_URI_ATTR(HTMLImageElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLImageElement, Srcset, srcset)
NS_IMPL_STRING_ATTR(HTMLImageElement, UseMap, usemap)
NS_IMPL_INT_ATTR(HTMLImageElement, Vspace, vspace)

bool
HTMLImageElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::usemap) ||
          nsGenericHTMLElement::IsInteractiveHTMLContent(aIgnoreTabindex);
}

bool
HTMLImageElement::IsSrcsetEnabled()
{
  return Preferences::GetBool(kPrefSrcsetEnabled, false);
}

nsresult
HTMLImageElement::GetCurrentSrc(nsAString& aValue)
{
  if (!IsSrcsetEnabled()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  if (currentURI) {
    nsAutoCString spec;
    currentURI->GetSpec(spec);
    CopyUTF8toUTF16(spec, aValue);
  } else {
    SetDOMStringToNull(aValue);
  }

  return NS_OK;
}

void
HTMLImageElement::GetItemValueText(DOMString& aValue)
{
  GetSrc(aValue);
}

void
HTMLImageElement::SetItemValueText(const nsAString& aValue)
{
  SetSrc(aValue);
}

bool
HTMLImageElement::Draggable() const
{
  // images may be dragged unless the draggable attribute is false
  return !AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                      nsGkAtoms::_false, eIgnoreCase);
}

bool
HTMLImageElement::Complete()
{
  if (!mCurrentRequest) {
    return true;
  }

  if (mPendingRequest) {
    return false;
  }

  uint32_t status;
  mCurrentRequest->GetImageStatus(&status);
  return
    (status &
     (imgIRequest::STATUS_LOAD_COMPLETE | imgIRequest::STATUS_ERROR)) != 0;
}

NS_IMETHODIMP
HTMLImageElement::GetComplete(bool* aComplete)
{
  NS_PRECONDITION(aComplete, "Null out param!");

  *aComplete = Complete();

  return NS_OK;
}

CSSIntPoint
HTMLImageElement::GetXY()
{
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
  if (!frame) {
    return CSSIntPoint(0, 0);
  }

  nsIFrame* layer = nsLayoutUtils::GetClosestLayer(frame->GetParent());
  return CSSIntPoint::FromAppUnitsRounded(frame->GetOffsetTo(layer));
}

int32_t
HTMLImageElement::X()
{
  return GetXY().x;
}

int32_t
HTMLImageElement::Y()
{
  return GetXY().y;
}

NS_IMETHODIMP
HTMLImageElement::GetX(int32_t* aX)
{
  *aX = X();
  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::GetY(int32_t* aY)
{
  *aY = Y();
  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::GetHeight(uint32_t* aHeight)
{
  *aHeight = Height();

  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::SetHeight(uint32_t aHeight)
{
  ErrorResult rv;
  SetHeight(aHeight, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLImageElement::GetWidth(uint32_t* aWidth)
{
  *aWidth = Width();

  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::SetWidth(uint32_t aWidth)
{
  ErrorResult rv;
  SetWidth(aWidth, rv);
  return rv.StealNSResult();
}

bool
HTMLImageElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLImageElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                        nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
HTMLImageElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                         int32_t aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::usemap ||
      aAttribute == nsGkAtoms::ismap) {
    NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
  } else if (aAttribute == nsGkAtoms::alt) {
    if (aModType == nsIDOMMutationEvent::ADDITION ||
        aModType == nsIDOMMutationEvent::REMOVAL) {
      NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
    }
  }
  return retval;
}

NS_IMETHODIMP_(bool)
HTMLImageElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
    sImageAlignAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLImageElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}


nsresult
HTMLImageElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValueOrString* aValue,
                                bool aNotify)
{

  if (aNameSpaceID == kNameSpaceID_None && mForm &&
      (aName == nsGkAtoms::name || aName == nsGkAtoms::id)) {
    // remove the image from the hashtable as needed
    nsAutoString tmp;
    GetAttr(kNameSpaceID_None, aName, tmp);

    if (!tmp.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, tmp,
                                         HTMLFormElement::AttributeUpdated);
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName,
                                             aValue, aNotify);
}

nsresult
HTMLImageElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                               const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && mForm &&
      (aName == nsGkAtoms::name || aName == nsGkAtoms::id) &&
      aValue && !aValue->IsEmptyString()) {
    // add the image to the hashtable as needed
    MOZ_ASSERT(aValue->Type() == nsAttrValue::eAtom,
               "Expected atom value for name/id");
    mForm->AddImageElementToTable(this,
      nsDependentAtomString(aValue->GetAtomValue()));
  }

  // Handle src/srcset/crossorigin updates. If aNotify is false, we are coming
  // from the parser or some such place; we'll get bound after all the
  // attributes have been set, so we'll do the image load from BindToTree.

  nsAttrValueOrString attrVal(aValue);

  if (aName == nsGkAtoms::src &&
      aNameSpaceID == kNameSpaceID_None &&
      !aValue) {
    // SetAttr handles setting src since it needs to catch img.src =
    // img.src, so we only need to handle the unset case
    if (InResponsiveMode()) {
      if (mResponsiveSelector &&
          mResponsiveSelector->Content() == this) {
        mResponsiveSelector->SetDefaultSource(NullString());
      }
      QueueImageLoadTask();
    } else {
      // Bug 1076583 - We still behave synchronously in the non-responsive case
      CancelImageRequests(aNotify);
    }
  } else if (aName == nsGkAtoms::srcset &&
             aNameSpaceID == kNameSpaceID_None &&
             IsSrcsetEnabled()) {
    PictureSourceSrcsetChanged(this, attrVal.String(), aNotify);
  } else if (aName == nsGkAtoms::sizes &&
             aNameSpaceID == kNameSpaceID_None &&
             HTMLPictureElement::IsPictureEnabled()) {
    PictureSourceSizesChanged(this, attrVal.String(), aNotify);
  } else if (aName == nsGkAtoms::crossorigin &&
             aNameSpaceID == kNameSpaceID_None &&
             aNotify) {
    // Force a new load of the image with the new cross origin policy.
    if (InResponsiveMode()) {
      // per spec, full selection runs when this changes, even though
      // it doesn't directly affect the source selection
      QueueImageLoadTask();
    } else {
      // Bug 1076583 - We still use the older synchronous algorithm in
      // non-responsive mode.  Force a new load of the image with the
      // new cross origin policy.
      ForceReload(aNotify);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}


nsresult
HTMLImageElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  // If we are a map and get a mouse click, don't let it be handled by
  // the Generic Element as this could cause a click event to fire
  // twice, once by the image frame for the map and once by the Anchor
  // element. (bug 39723)
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  if (mouseEvent && mouseEvent->IsLeftClickEvent()) {
    bool isMap = false;
    GetIsMap(&isMap);
    if (isMap) {
      aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

bool
HTMLImageElement::IsHTMLFocusable(bool aWithMouse,
                                  bool *aIsFocusable, int32_t *aTabIndex)
{
  int32_t tabIndex = TabIndex();

  if (IsInDoc()) {
    nsAutoString usemap;
    GetUseMap(usemap);
    // XXXbz which document should this be using?  sXBL/XBL2 issue!  I
    // think that OwnerDoc() is right, since we don't want to
    // assume stuff about the document we're bound to.
    if (OwnerDoc()->FindImageMap(usemap)) {
      if (aTabIndex) {
        // Use tab index on individual map areas
        *aTabIndex = (sTabFocusModel & eTabFocus_linksMask)? 0 : -1;
      }
      // Image map is not focusable itself, but flag as tabbable
      // so that image map areas get walked into.
      *aIsFocusable = false;

      return false;
    }
  }

  if (aTabIndex) {
    // Can be in tab order if tabindex >=0 and form controls are tabbable.
    *aTabIndex = (sTabFocusModel & eTabFocus_formElementsMask)? tabIndex : -1;
  }

  *aIsFocusable =
#ifdef XP_MACOSX
    (!aWithMouse || nsFocusManager::sMouseFocusesFormControl) &&
#endif
    (tabIndex >= 0 || HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex));

  return false;
}

nsresult
HTMLImageElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                          nsIAtom* aPrefix, const nsAString& aValue,
                          bool aNotify)
{
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
  if (aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::src) {

    // This is for dom.disable_image_src_set, which predates "srcset"
    // as an attribute. See Bug 773429
    if (nsContentUtils::IsImageSrcSetDisabled()) {
      return NS_OK;
    }

    if (InResponsiveMode()) {
      if (mResponsiveSelector &&
          mResponsiveSelector->Content() == this) {
        mResponsiveSelector->SetDefaultSource(aValue);
      }
      QueueImageLoadTask();
    } else if (aNotify) {
      // If aNotify is false, we are coming from the parser or some such place;
      // we'll get bound after all the attributes have been set, so we'll do the
      // sync image load from BindToTree. Skip the LoadImage call in that case.

      // Note that this sync behavior is partially removed from the spec, bug 1076583

      // A hack to get animations to reset. See bug 594771.
      mNewRequestsWillNeedAnimationReset = true;

      // Force image loading here, so that we'll try to load the image from
      // network if it's set to be not cacheable...  If we change things so that
      // the state gets in Element's attr-setting happen around this
      // LoadImage call, we could start passing false instead of aNotify
      // here.
      LoadImage(aValue, true, aNotify, eImageLoadType_Normal);

      mNewRequestsWillNeedAnimationReset = false;
    }
  }

  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                       aNotify);
}

nsresult
HTMLImageElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsImageLoadingContent::BindToTree(aDocument, aParent, aBindingParent,
                                    aCompileEventHandlers);

  if (aParent) {
    UpdateFormOwner();
  }

  bool addedToPicture = aParent && aParent->IsHTMLElement(nsGkAtoms::picture) &&
                        HTMLPictureElement::IsPictureEnabled();
  if (addedToPicture) {
    if (aDocument) {
      aDocument->AddResponsiveContent(this);
    }
    QueueImageLoadTask();
  } else if (!InResponsiveMode() &&
             HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
    // We skip loading when our attributes were set from parser land,
    // so trigger a aForce=false load now to check if things changed.
    // This isn't necessary for responsive mode, since creating the
    // image load task is asynchronous we don't need to take special
    // care to avoid doing so when being filled by the parser.

    // FIXME: Bug 660963 it would be nice if we could just have
    // ClearBrokenState update our state and do it fast...
    ClearBrokenState();
    RemoveStatesSilently(NS_EVENT_STATE_BROKEN);

    // We still act synchronously for the non-responsive case (Bug
    // 1076583), but still need to delay if it is unsafe to run
    // script.

    // If loading is temporarily disabled, don't even launch MaybeLoadImage.
    // Otherwise MaybeLoadImage may run later when someone has reenabled
    // loading.
    if (LoadingEnabled()) {
      nsContentUtils::AddScriptRunner(
          NS_NewRunnableMethod(this, &HTMLImageElement::MaybeLoadImage));
    }
  }

  return rv;
}

void
HTMLImageElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mForm) {
    if (aNullParent || !FindAncestorForm(mForm)) {
      ClearForm(true);
    } else {
      UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
    }
  }

  if (GetParent() &&
      GetParent()->IsHTMLElement(nsGkAtoms::picture) &&
      HTMLPictureElement::IsPictureEnabled()) {
    nsIDocument* doc = GetOurOwnerDoc();
    if (doc) {
      doc->RemoveResponsiveContent(this);
    }
    // Being removed from picture re-triggers selection, even if we
    // weren't using a <source> peer
    if (aNullParent) {
      QueueImageLoadTask();
    }
  }

  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLImageElement::UpdateFormOwner()
{
  if (!mForm) {
    mForm = FindAncestorForm();
  }

  if (mForm && !HasFlag(ADDED_TO_FORM)) {
    // Now we need to add ourselves to the form
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

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

void
HTMLImageElement::MaybeLoadImage()
{
  // Our base URI may have changed, or we may have had responsive parameters
  // change while not bound to the tree. Re-parse src/srcset and call LoadImage,
  // which is a no-op if it resolves to the same effective URI without aForce.

  // Note, check LoadingEnabled() after LoadImage call.

  LoadSelectedImage(false, true);

  if (!LoadingEnabled()) {
    CancelImageRequests(true);
  }
}

EventStates
HTMLImageElement::IntrinsicState() const
{
  return nsGenericHTMLElement::IntrinsicState() |
    nsImageLoadingContent::ImageState();
}

// static
already_AddRefed<HTMLImageElement>
HTMLImageElement::Image(const GlobalObject& aGlobal,
                        const Optional<uint32_t>& aWidth,
                        const Optional<uint32_t>& aHeight,
                        ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  nsIDocument* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  already_AddRefed<mozilla::dom::NodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::img, nullptr,
                                        kNameSpaceID_XHTML,
                                        nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<HTMLImageElement> img = new HTMLImageElement(nodeInfo);

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

uint32_t
HTMLImageElement::NaturalHeight()
{
  uint32_t height;
  nsresult rv = nsImageLoadingContent::GetNaturalHeight(&height);

  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "GetNaturalHeight should not fail");
    return 0;
  }

  if (mResponsiveSelector) {
    double density = mResponsiveSelector->GetSelectedImageDensity();
    MOZ_ASSERT(IsFinite(density) && density > 0.0);
    height = NSToIntRound(double(height) / density);
    height = std::max(height, 0u);
  }

  return height;
}

NS_IMETHODIMP
HTMLImageElement::GetNaturalHeight(uint32_t* aNaturalHeight)
{
  *aNaturalHeight = NaturalHeight();
  return NS_OK;
}

uint32_t
HTMLImageElement::NaturalWidth()
{
  uint32_t width;
  nsresult rv = nsImageLoadingContent::GetNaturalWidth(&width);

  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "GetNaturalWidth should not fail");
    return 0;
  }

  if (mResponsiveSelector) {
    double density = mResponsiveSelector->GetSelectedImageDensity();
    MOZ_ASSERT(IsFinite(density) && density > 0.0);
    width = NSToIntRound(double(width) / density);
    width = std::max(width, 0u);
  }

  return width;
}

NS_IMETHODIMP
HTMLImageElement::GetNaturalWidth(uint32_t* aNaturalWidth)
{
  *aNaturalWidth = NaturalWidth();
  return NS_OK;
}

nsresult
HTMLImageElement::CopyInnerTo(Element* aDest)
{
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticImageClone(static_cast<HTMLImageElement*>(aDest));
  }
  return nsGenericHTMLElement::CopyInnerTo(aDest);
}

CORSMode
HTMLImageElement::GetCORSMode()
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

JSObject*
HTMLImageElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLImageElementBinding::Wrap(aCx, this, aGivenProto);
}

#ifdef DEBUG
nsIDOMHTMLFormElement*
HTMLImageElement::GetForm() const
{
  return mForm;
}
#endif

void
HTMLImageElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  NS_PRECONDITION(aForm, "Don't pass null here");
  NS_ASSERTION(!mForm,
               "We don't support switching from one non-null form to another.");

  mForm = static_cast<HTMLFormElement*>(aForm);
}

void
HTMLImageElement::ClearForm(bool aRemoveFromForm)
{
  NS_ASSERTION((mForm != nullptr) == HasFlag(ADDED_TO_FORM),
               "Form control should have had flag set correctly");

  if (!mForm) {
    return;
  }

  if (aRemoveFromForm) {
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    mForm->RemoveImageElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, nameVal,
                                         HTMLFormElement::ElementRemoved);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, idVal,
                                         HTMLFormElement::ElementRemoved);
    }
  }

  UnsetFlags(ADDED_TO_FORM);
  mForm = nullptr;
}

void
HTMLImageElement::QueueImageLoadTask()
{
  // If loading is temporarily disabled, we don't want to queue tasks
  // that may then run when loading is re-enabled.
  if (!LoadingEnabled() || !this->OwnerDoc()->IsCurrentActiveDocument()) {
    return;
  }

  nsCOMPtr<nsIRunnable> task = new ImageLoadTask(this);
  // The task checks this to determine if it was the last
  // queued event, and so earlier tasks are implicitly canceled.
  mPendingImageLoadTask = task;
  nsContentUtils::RunInStableState(task.forget());
}

bool
HTMLImageElement::HaveSrcsetOrInPicture()
{
  if (IsSrcsetEnabled() && HasAttr(kNameSpaceID_None, nsGkAtoms::srcset)) {
    return true;
  }

  if (!HTMLPictureElement::IsPictureEnabled()) {
    return false;
  }

  Element *parent = nsINode::GetParentElement();
  return (parent && parent->IsHTMLElement(nsGkAtoms::picture));
}

bool
HTMLImageElement::InResponsiveMode()
{
  // When we lose srcset or leave a <picture> element, the fallback to img.src
  // will happen from the microtask, and we should behave responsively in the
  // interim
  return mResponsiveSelector ||
         mPendingImageLoadTask ||
         HaveSrcsetOrInPicture();
}

nsresult
HTMLImageElement::LoadSelectedImage(bool aForce, bool aNotify)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (aForce) {
    // In responsive mode we generally want to re-run the full
    // selection algorithm whenever starting a new load, per
    // spec. This also causes us to re-resolve the URI as appropriate.
    UpdateResponsiveSource();
  }

  if (mResponsiveSelector) {
    nsCOMPtr<nsIURI> url = mResponsiveSelector->GetSelectedImageURL();
    if (url) {
      rv = LoadImage(url, aForce, aNotify, eImageLoadType_Imageset);
    }
  } else {
    nsAutoString src;
    if (!GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
      CancelImageRequests(aNotify);
      rv = NS_OK;
    } else {
      // If we have a srcset attribute or are in a <picture> element,
      // we always use the Imageset load type, even if we parsed no
      // valid responsive sources from either, per spec.
      rv = LoadImage(src, aForce, aNotify,
                     HaveSrcsetOrInPicture() ? eImageLoadType_Imageset
                                             : eImageLoadType_Normal);
    }
  }

  if (NS_FAILED(rv)) {
    CancelImageRequests(aNotify);
  }
  return rv;
}

void
HTMLImageElement::PictureSourceSrcsetChanged(nsIContent *aSourceNode,
                                             const nsAString& aNewValue,
                                             bool aNotify)
{
  bool isSelf = aSourceNode == this;

  if (!IsSrcsetEnabled() ||
      (!isSelf && !HTMLPictureElement::IsPictureEnabled())) {
    return;
  }

  MOZ_ASSERT(isSelf || IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  nsIContent *currentSrc =
    mResponsiveSelector ? mResponsiveSelector->Content() : nullptr;

  if (aSourceNode == currentSrc) {
    // We're currently using this node as our responsive selector
    // source.
    mResponsiveSelector->SetCandidatesFromSourceSet(aNewValue);
  }

  // This always triggers the image update steps per the spec, even if
  // we are not using this source.
  QueueImageLoadTask();
}

void
HTMLImageElement::PictureSourceSizesChanged(nsIContent *aSourceNode,
                                            const nsAString& aNewValue,
                                            bool aNotify)
{
  if (!HTMLPictureElement::IsPictureEnabled()) {
    return;
  }

  MOZ_ASSERT(aSourceNode == this ||
             IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  nsIContent *currentSrc =
    mResponsiveSelector ? mResponsiveSelector->Content() : nullptr;

  if (aSourceNode == currentSrc) {
    // We're currently using this node as our responsive selector
    // source.
    mResponsiveSelector->SetSizesFromDescriptor(aNewValue);
  }

  // This always triggers the image update steps per the spec, even if
  // we are not using this source.
  QueueImageLoadTask();
}

void
HTMLImageElement::PictureSourceMediaOrTypeChanged(nsIContent *aSourceNode,
                                                  bool aNotify)
{
  if (!HTMLPictureElement::IsPictureEnabled()) {
    return;
  }

  MOZ_ASSERT(IsPreviousSibling(aSourceNode, this),
             "Should not be getting notifications for non-previous-siblings");

  // This always triggers the image update steps per the spec, even if
  // we are not switching to/from this source
  QueueImageLoadTask();
}

void
HTMLImageElement::PictureSourceAdded(nsIContent *aSourceNode)
{
  if (!HTMLPictureElement::IsPictureEnabled()) {
    return;
  }

  QueueImageLoadTask();
}

void
HTMLImageElement::PictureSourceRemoved(nsIContent *aSourceNode)
{
  if (!HTMLPictureElement::IsPictureEnabled()) {
    return;
  }

  QueueImageLoadTask();
}

void
HTMLImageElement::UpdateResponsiveSource()
{
  if (!IsSrcsetEnabled()) {
    mResponsiveSelector = nullptr;
    return;
  }

  nsIContent *currentSource =
    mResponsiveSelector ? mResponsiveSelector->Content() : nullptr;
  bool pictureEnabled = HTMLPictureElement::IsPictureEnabled();
  Element *parent = pictureEnabled ? nsINode::GetParentElement() : nullptr;

  nsINode *candidateSource = nullptr;
  if (parent && parent->IsHTMLElement(nsGkAtoms::picture)) {
    // Walk source nodes previous to ourselves
    candidateSource = parent->GetFirstChild();
  } else {
    candidateSource = this;
  }

  while (candidateSource) {
    if (candidateSource == currentSource) {
      // found no better source before current, re-run selection on
      // that and keep it if it's still usable.
      mResponsiveSelector->SelectImage(true);
      if (mResponsiveSelector->NumCandidates()) {
        bool isUsableCandidate = true;

        // an otherwise-usable source element may still have a media query that may not
        // match any more.
        if (candidateSource->IsHTMLElement(nsGkAtoms::source) &&
            !SourceElementMatches(candidateSource->AsContent())) {
          isUsableCandidate = false;
        }

        if (isUsableCandidate) {
          break;
        }
      }

      // no longer valid
      mResponsiveSelector = nullptr;
      if (candidateSource == this) {
        // No further possibilities
        break;
      }
    } else if (candidateSource == this) {
      // We are the last possible source
      if (!TryCreateResponsiveSelector(candidateSource->AsContent())) {
        // Failed to find any source
        mResponsiveSelector = nullptr;
      }
      break;
    } else if (candidateSource->IsHTMLElement(nsGkAtoms::source) &&
               TryCreateResponsiveSelector(candidateSource->AsContent())) {
      // This led to a valid source, stop
      break;
    }
    candidateSource = candidateSource->GetNextSibling();
  }

  if (!candidateSource) {
    // Ran out of siblings without finding ourself, e.g. XBL magic.
    mResponsiveSelector = nullptr;
  }
}

/*static */ bool
HTMLImageElement::SupportedPictureSourceType(const nsAString& aType)
{
  return
    imgLoader::SupportImageWithMimeType(NS_ConvertUTF16toUTF8(aType).get(),
                                        AcceptedMimeTypes::IMAGES_AND_DOCUMENTS);
}

bool
HTMLImageElement::SourceElementMatches(nsIContent* aSourceNode)
{
  MOZ_ASSERT(aSourceNode->IsHTMLElement(nsGkAtoms::source));

  DebugOnly<Element *> parent(nsINode::GetParentElement());
  MOZ_ASSERT(parent && parent->IsHTMLElement(nsGkAtoms::picture));
  MOZ_ASSERT(IsPreviousSibling(aSourceNode, this));
  MOZ_ASSERT(HTMLPictureElement::IsPictureEnabled());

  // Check media and type
  HTMLSourceElement *src = static_cast<HTMLSourceElement*>(aSourceNode);
  if (!src->MatchesCurrentMedia()) {
    return false;
  }

  nsAutoString type;
  if (aSourceNode->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type) &&
      !SupportedPictureSourceType(type)) {
    return false;
  }

  return true;
}

bool
HTMLImageElement::TryCreateResponsiveSelector(nsIContent *aSourceNode,
                                              const nsAString *aSrcset,
                                              const nsAString *aSizes)
{
  if (!IsSrcsetEnabled()) {
    return false;
  }

  bool pictureEnabled = HTMLPictureElement::IsPictureEnabled();
  // Skip if this is not a <source> with matching media query
  bool isSourceTag = aSourceNode->IsHTMLElement(nsGkAtoms::source);
  if (isSourceTag) {
    if (!SourceElementMatches(aSourceNode)) {
      return false;
    }
  } else if (aSourceNode->IsHTMLElement(nsGkAtoms::img)) {
    // Otherwise this is the <img> tag itself
    MOZ_ASSERT(aSourceNode == this);
  }

  // Skip if has no srcset or an empty srcset
  nsString srcset;
  if (aSrcset) {
    srcset = *aSrcset;
  } else if (!aSourceNode->GetAttr(kNameSpaceID_None, nsGkAtoms::srcset,
                                   srcset)) {
    return false;
  }

  if (srcset.IsEmpty()) {
    return false;
  }


  // Try to parse
  nsRefPtr<ResponsiveImageSelector> sel = new ResponsiveImageSelector(aSourceNode);
  if (!sel->SetCandidatesFromSourceSet(srcset)) {
    // No possible candidates, don't need to bother parsing sizes
    return false;
  }

  if (pictureEnabled && aSizes) {
    sel->SetSizesFromDescriptor(*aSizes);
  } else if (pictureEnabled) {
    nsAutoString sizes;
    aSourceNode->GetAttr(kNameSpaceID_None, nsGkAtoms::sizes, sizes);
    sel->SetSizesFromDescriptor(sizes);
  }

  // If this is the <img> tag, also pull in src as the default source
  if (!isSourceTag) {
    MOZ_ASSERT(aSourceNode == this);
    nsAutoString src;
    if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src) && !src.IsEmpty()) {
      sel->SetDefaultSource(src);
    }
  }

  mResponsiveSelector = sel;
  return true;
}

/* static */ bool
HTMLImageElement::SelectSourceForTagWithAttrs(nsIDocument *aDocument,
                                              bool aIsSourceTag,
                                              const nsAString& aSrcAttr,
                                              const nsAString& aSrcsetAttr,
                                              const nsAString& aSizesAttr,
                                              const nsAString& aTypeAttr,
                                              const nsAString& aMediaAttr,
                                              nsAString& aResult)
{
  MOZ_ASSERT(aIsSourceTag || (aTypeAttr.IsEmpty() && aMediaAttr.IsEmpty()),
             "Passing type or media attrs makes no sense without aIsSourceTag");
  MOZ_ASSERT(!aIsSourceTag || aSrcAttr.IsEmpty(),
             "Passing aSrcAttr makes no sense with aIsSourceTag set");

  bool pictureEnabled = HTMLPictureElement::IsPictureEnabled();
  if (aIsSourceTag && !pictureEnabled) {
    return false;
  }

  if (!IsSrcsetEnabled() || aSrcsetAttr.IsEmpty()) {
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
      ((!aMediaAttr.IsVoid() &&
       !HTMLSourceElement::WouldMatchMediaForDocument(aMediaAttr, aDocument)) ||
      (!aTypeAttr.IsVoid() &&
       !SupportedPictureSourceType(aTypeAttr)))) {
    return false;
  }

  // Using srcset or picture <source>, build a responsive selector for this tag.
  nsRefPtr<ResponsiveImageSelector> sel =
    new ResponsiveImageSelector(aDocument);

  sel->SetCandidatesFromSourceSet(aSrcsetAttr);
  if (pictureEnabled && !aSizesAttr.IsEmpty()) {
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

void
HTMLImageElement::DestroyContent()
{
  mResponsiveSelector = nullptr;
}

void
HTMLImageElement::MediaFeatureValuesChanged()
{
  QueueImageLoadTask();
}

} // namespace dom
} // namespace mozilla

