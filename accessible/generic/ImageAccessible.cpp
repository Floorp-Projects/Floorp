/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageAccessible.h"

#include "DocAccessible-inl.h"
#include "LocalAccessible-inl.h"
#include "nsAccUtils.h"
#include "mozilla/a11y/Role.h"
#include "AccAttributes.h"
#include "AccIterator.h"
#include "CacheConstants.h"
#include "States.h"

#include "imgIRequest.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsContentUtils.h"
#include "nsIImageLoadingContent.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"

namespace mozilla::a11y {

NS_IMPL_ISUPPORTS_INHERITED(ImageAccessible, LinkableAccessible,
                            imgINotificationObserver)

////////////////////////////////////////////////////////////////////////////////
// ImageAccessible
////////////////////////////////////////////////////////////////////////////////

ImageAccessible::ImageAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : LinkableAccessible(aContent, aDoc),
      mImageRequestStatus(imgIRequest::STATUS_NONE) {
  mType = eImageType;
  nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(mContent));
  if (content) {
    content->AddNativeObserver(this);
    nsCOMPtr<imgIRequest> imageRequest;
    content->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                        getter_AddRefs(imageRequest));
    if (imageRequest) {
      imageRequest->GetImageStatus(&mImageRequestStatus);
    }
  }
}

ImageAccessible::~ImageAccessible() {}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible public

void ImageAccessible::Shutdown() {
  nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(mContent));
  if (content) {
    content->RemoveNativeObserver(this);
  }

  LinkableAccessible::Shutdown();
}

uint64_t ImageAccessible::NativeState() const {
  // The state is a bitfield, get our inherited state, then logically OR it with
  // states::ANIMATED if this is an animated image.

  uint64_t state = LinkableAccessible::NativeState();

  if (mImageRequestStatus & imgIRequest::STATUS_IS_ANIMATED) {
    state |= states::ANIMATED;
  }

  if (!(mImageRequestStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
    nsIFrame* frame = GetFrame();
    MOZ_ASSERT(!frame || frame->AccessibleType() == eImageType ||
               frame->AccessibleType() == a11y::eHTMLImageMapType);
    if (frame && !frame->HasAnyStateBits(IMAGE_SIZECONSTRAINED)) {
      // The size of this image hasn't been constrained and we haven't loaded
      // enough of the image to know its size yet. This means it currently
      // has 0 width and height.
      state |= states::INVISIBLE;
    }
  }

  return state;
}

ENameValueFlag ImageAccessible::NativeName(nsString& aName) const {
  mContent->AsElement()->GetAttr(nsGkAtoms::alt, aName);
  if (!aName.IsEmpty()) return eNameOK;

  ENameValueFlag nameFlag = LocalAccessible::NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  return eNameOK;
}

role ImageAccessible::NativeRole() const { return roles::GRAPHIC; }

void ImageAccessible::DOMAttributeChanged(int32_t aNameSpaceID,
                                          nsAtom* aAttribute, int32_t aModType,
                                          const nsAttrValue* aOldValue,
                                          uint64_t aOldState) {
  LinkableAccessible::DOMAttributeChanged(aNameSpaceID, aAttribute, aModType,
                                          aOldValue, aOldState);

  if (aAttribute == nsGkAtoms::longdesc &&
      (aModType == dom::MutationEvent_Binding::ADDITION ||
       aModType == dom::MutationEvent_Binding::REMOVAL)) {
    mDoc->QueueCacheUpdate(this, CacheDomain::Actions);
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

uint8_t ImageAccessible::ActionCount() const {
  uint8_t actionCount = LinkableAccessible::ActionCount();
  return HasLongDesc() ? actionCount + 1 : actionCount;
}

void ImageAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();
  if (IsLongDescIndex(aIndex) && HasLongDesc()) {
    aName.AssignLiteral("showlongdesc");
  } else {
    LinkableAccessible::ActionNameAt(aIndex, aName);
  }
}

bool ImageAccessible::DoAction(uint8_t aIndex) const {
  // Get the long description uri and open in a new window.
  if (!IsLongDescIndex(aIndex)) return LinkableAccessible::DoAction(aIndex);

  nsCOMPtr<nsIURI> uri = GetLongDescURI();
  if (!uri) return false;

  nsAutoCString utf8spec;
  uri->GetSpec(utf8spec);
  NS_ConvertUTF8toUTF16 spec(utf8spec);

  dom::Document* document = mContent->OwnerDoc();
  nsCOMPtr<nsPIDOMWindowOuter> piWindow = document->GetWindow();
  if (!piWindow) return false;

  RefPtr<dom::BrowsingContext> tmp;
  return NS_SUCCEEDED(piWindow->Open(spec, u""_ns, u""_ns,
                                     /* aLoadInfo = */ nullptr,
                                     /* aForceNoOpener = */ false,
                                     getter_AddRefs(tmp)));
}

////////////////////////////////////////////////////////////////////////////////
// ImageAccessible

LayoutDeviceIntPoint ImageAccessible::Position(uint32_t aCoordType) {
  LayoutDeviceIntPoint point = Bounds().TopLeft();
  nsAccUtils::ConvertScreenCoordsTo(&point.x.value, &point.y.value, aCoordType,
                                    this);
  return point;
}

LayoutDeviceIntSize ImageAccessible::Size() { return Bounds().Size(); }

// LocalAccessible
already_AddRefed<AccAttributes> ImageAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes = LinkableAccessible::NativeAttributes();

  nsString src;
  mContent->AsElement()->GetAttr(nsGkAtoms::src, src);
  if (!src.IsEmpty()) attributes->SetAttribute(nsGkAtoms::src, std::move(src));

  return attributes.forget();
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

already_AddRefed<nsIURI> ImageAccessible::GetLongDescURI() const {
  if (mContent->AsElement()->HasAttr(nsGkAtoms::longdesc)) {
    // To check if longdesc contains an invalid url.
    nsAutoString longdesc;
    mContent->AsElement()->GetAttr(nsGkAtoms::longdesc, longdesc);
    if (longdesc.FindChar(' ') != -1 || longdesc.FindChar('\t') != -1 ||
        longdesc.FindChar('\r') != -1 || longdesc.FindChar('\n') != -1) {
      return nullptr;
    }
    nsCOMPtr<nsIURI> uri;
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri), longdesc,
                                              mContent->OwnerDoc(),
                                              mContent->GetBaseURI());
    return uri.forget();
  }

  DocAccessible* document = Document();
  if (document) {
    IDRefsIterator iter(document, mContent, nsGkAtoms::aria_describedby);
    while (nsIContent* target = iter.NextElem()) {
      if ((target->IsHTMLElement(nsGkAtoms::a) ||
           target->IsHTMLElement(nsGkAtoms::area)) &&
          target->AsElement()->HasAttr(nsGkAtoms::href)) {
        nsGenericHTMLElement* element = nsGenericHTMLElement::FromNode(target);

        nsCOMPtr<nsIURI> uri;
        element->GetURIAttr(nsGkAtoms::href, nullptr, getter_AddRefs(uri));
        return uri.forget();
      }
    }
  }

  return nullptr;
}

bool ImageAccessible::IsLongDescIndex(uint8_t aIndex) const {
  return aIndex == LinkableAccessible::ActionCount();
}

////////////////////////////////////////////////////////////////////////////////
// imgINotificationObserver

void ImageAccessible::Notify(imgIRequest* aRequest, int32_t aType,
                             const nsIntRect* aData) {
  if (aType != imgINotificationObserver::FRAME_COMPLETE &&
      aType != imgINotificationObserver::LOAD_COMPLETE &&
      aType != imgINotificationObserver::DECODE_COMPLETE) {
    // We should update our state if the whole image was decoded,
    // or the first frame in the case of a gif.
    return;
  }

  if (IsDefunct() || !mParent) {
    return;
  }

  uint32_t status = imgIRequest::STATUS_NONE;
  aRequest->GetImageStatus(&status);

  if ((status ^ mImageRequestStatus) & imgIRequest::STATUS_SIZE_AVAILABLE) {
    nsIFrame* frame = GetFrame();
    if (frame && !frame->HasAnyStateBits(IMAGE_SIZECONSTRAINED)) {
      RefPtr<AccEvent> event = new AccStateChangeEvent(
          this, states::INVISIBLE,
          !(status & imgIRequest::STATUS_SIZE_AVAILABLE));
      mDoc->FireDelayedEvent(event);
    }
  }

  if ((status ^ mImageRequestStatus) & imgIRequest::STATUS_IS_ANIMATED) {
    RefPtr<AccEvent> event = new AccStateChangeEvent(
        this, states::ANIMATED, (status & imgIRequest::STATUS_IS_ANIMATED));
    mDoc->FireDelayedEvent(event);
  }

  mImageRequestStatus = status;
}

}  // namespace mozilla::a11y
