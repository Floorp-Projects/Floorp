/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

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
#include "nsIFrame.h"
#include "nsNodeInfoManager.h"
#include "mozilla/MouseEvents.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsFocusManager.h"
#include "mozilla/dom/HTMLFormElement.h"

#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgINotificationObserver.h"
#include "imgRequestProxy.h"

#include "nsILoadGroup.h"

#include "nsRuleData.h"

#include "nsIDOMHTMLMapElement.h"
#include "nsEventDispatcher.h"

#include "nsLayoutUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Image)

namespace mozilla {
namespace dom {

HTMLImageElement::HTMLImageElement(already_AddRefed<nsINodeInfo> aNodeInfo)
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


// QueryInterface implementation for HTMLImageElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLImageElement)
  NS_INTERFACE_TABLE_INHERITED4(HTMLImageElement,
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
NS_IMPL_STRING_ATTR(HTMLImageElement, Lowsrc, lowsrc)
NS_IMPL_URI_ATTR(HTMLImageElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLImageElement, UseMap, usemap)
NS_IMPL_INT_ATTR(HTMLImageElement, Vspace, vspace)

void
HTMLImageElement::GetItemValueText(nsAString& aValue)
{
  GetSrc(aValue);
}

void
HTMLImageElement::SetItemValueText(const nsAString& aValue)
{
  SetSrc(aValue);
}

// crossorigin is not "limited to only known values" per spec, so it's
// just a string attr purposes of the DOM crossOrigin property.
// TODO: It is now (bug 880997).
NS_IMPL_STRING_ATTR(HTMLImageElement, CrossOrigin, crossorigin)

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
  return rv.ErrorCode();
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
  return rv.ErrorCode();
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
    NS_ABORT_IF_FALSE(aValue->Type() == nsAttrValue::eAtom,
      "Expected atom value for name/id");
    mForm->AddImageElementToTable(this,
      nsDependentAtomString(aValue->GetAtomValue()));
  }

  if (aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::src &&
      !aValue) {
    CancelImageRequests(aNotify);
  }

  // If we plan to call LoadImage, we want to do it first so that the image load
  // kicks off. But if aNotify is false, we are coming from the parser or some
  // such place; we'll get bound after all the attributes have been set, so
  // we'll do the image load from BindToTree. Skip the LoadImage call in that case.
  if (aNotify &&
      aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::crossorigin) {
    // We want aForce == true in this LoadImage call, because we want to force
    // a new load of the image with the new cross origin policy.
    nsAutoString uri;
    GetAttr(kNameSpaceID_None, nsGkAtoms::src, uri);
    LoadImage(uri, true, aNotify);
  }

  if (aNotify &&
      aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::src &&
      aValue) {

    // Prevent setting image.src by exiting early
    if (nsContentUtils::IsImageSrcSetDisabled()) {
      return NS_OK;
    }

    // A hack to get animations to reset. See bug 594771.
    mNewRequestsWillNeedAnimationReset = true;

    // Force image loading here, so that we'll try to load the image from
    // network if it's set to be not cacheable...  If we change things so that
    // the state gets in Element's attr-setting happen around this
    // LoadImage call, we could start passing false instead of aNotify
    // here.
    LoadImage(aValue->GetStringValue(), true, aNotify);

    mNewRequestsWillNeedAnimationReset = false;
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}


nsresult
HTMLImageElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
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
  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                       aNotify);
}

nsresult
HTMLImageElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                            bool aNotify)
{
  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
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

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
    // FIXME: Bug 660963 it would be nice if we could just have
    // ClearBrokenState update our state and do it fast...
    ClearBrokenState();
    RemoveStatesSilently(NS_EVENT_STATE_BROKEN);
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
  // Our base URI may have changed; claim that our URI changed, and the
  // nsImageLoadingContent will decide whether a new image load is warranted.
  // Note, check LoadingEnabled() after LoadImage call.
  nsAutoString uri;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, uri) &&
      (NS_FAILED(LoadImage(uri, false, true)) ||
       !LoadingEnabled())) {
    CancelImageRequests(true);
  }
}

nsEventStates
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

  nsCOMPtr<nsINodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::img, nullptr,
                                        kNameSpaceID_XHTML,
                                        nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<HTMLImageElement> img = new HTMLImageElement(nodeInfo.forget());

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
  if (!mCurrentRequest) {
    return 0;
  }

  nsCOMPtr<imgIContainer> image;
  mCurrentRequest->GetImage(getter_AddRefs(image));
  if (!image) {
    return 0;
  }

  int32_t height;
  if (NS_SUCCEEDED(image->GetHeight(&height))) {
    return height;
  }
  return 0;
}

NS_IMETHODIMP
HTMLImageElement::GetNaturalHeight(uint32_t* aNaturalHeight)
{
  NS_ENSURE_ARG_POINTER(aNaturalHeight);

  *aNaturalHeight = NaturalHeight();

  return NS_OK;
}

uint32_t
HTMLImageElement::NaturalWidth()
{
  if (!mCurrentRequest) {
    return 0;
  }

  nsCOMPtr<imgIContainer> image;
  mCurrentRequest->GetImage(getter_AddRefs(image));
  if (!image) {
    return 0;
  }

  int32_t width;
  if (NS_SUCCEEDED(image->GetWidth(&width))) {
    return width;
  }
  return 0;
}

NS_IMETHODIMP
HTMLImageElement::GetNaturalWidth(uint32_t* aNaturalWidth)
{
  NS_ENSURE_ARG_POINTER(aNaturalWidth);

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
HTMLImageElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLImageElementBinding::Wrap(aCx, aScope, this);
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

} // namespace dom
} // namespace mozilla
