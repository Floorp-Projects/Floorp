/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageAccessible.h"

#include "nsAccUtils.h"
#include "Role.h"
#include "AccIterator.h"
#include "States.h"

#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsGenericHTMLElement.h"
#include "nsIDocument.h"
#include "nsIImageLoadingContent.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIPersistentProperties2.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ImageAccessible
////////////////////////////////////////////////////////////////////////////////

ImageAccessible::
  ImageAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LinkableAccessible(aContent, aDoc)
{
  mType = eImageType;
}

ImageAccessible::~ImageAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// Accessible public

uint64_t
ImageAccessible::NativeState()
{
  // The state is a bitfield, get our inherited state, then logically OR it with
  // states::ANIMATED if this is an animated image.

  uint64_t state = LinkableAccessible::NativeState();

  nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(mContent));
  nsCOMPtr<imgIRequest> imageRequest;

  if (content)
    content->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                        getter_AddRefs(imageRequest));

  nsCOMPtr<imgIContainer> imgContainer;
  if (imageRequest)
    imageRequest->GetImage(getter_AddRefs(imgContainer));

  if (imgContainer) {
    bool animated;
    imgContainer->GetAnimated(&animated);
    if (animated)
      state |= states::ANIMATED;
  }

  return state;
}

ENameValueFlag
ImageAccessible::NativeName(nsString& aName)
{
  bool hasAltAttrib =
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName);
  if (!aName.IsEmpty())
    return eNameOK;

  ENameValueFlag nameFlag = Accessible::NativeName(aName);
  if (!aName.IsEmpty())
    return nameFlag;

  // No accessible name but empty 'alt' attribute is present. If further name
  // computation algorithm doesn't provide non empty name then it means
  // an empty 'alt' attribute was used to indicate a decorative image (see
  // Accessible::Name() method for details).
  return hasAltAttrib ? eNoNameOnPurpose : eNameOK;
}

role
ImageAccessible::NativeRole()
{
  return roles::GRAPHIC;
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

uint8_t
ImageAccessible::ActionCount()
{
  uint8_t actionCount = LinkableAccessible::ActionCount();
  return HasLongDesc() ? actionCount + 1 : actionCount;
}

void
ImageAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();
  if (IsLongDescIndex(aIndex) && HasLongDesc())
    aName.AssignLiteral("showlongdesc");
  else
    LinkableAccessible::ActionNameAt(aIndex, aName);
}

bool
ImageAccessible::DoAction(uint8_t aIndex)
{
  // Get the long description uri and open in a new window.
  if (!IsLongDescIndex(aIndex))
    return LinkableAccessible::DoAction(aIndex);

  nsCOMPtr<nsIURI> uri = GetLongDescURI();
  if (!uri)
    return false;

  nsAutoCString utf8spec;
  uri->GetSpec(utf8spec);
  NS_ConvertUTF8toUTF16 spec(utf8spec);

  nsIDocument* document = mContent->OwnerDoc();
  nsCOMPtr<nsPIDOMWindowOuter> piWindow = document->GetWindow();
  if (!piWindow)
    return false;

  nsCOMPtr<nsPIDOMWindowOuter> tmp;
  return NS_SUCCEEDED(piWindow->Open(spec, EmptyString(), EmptyString(),
                                     /* aLoadInfo = */ nullptr,
                                     /* aForceNoOpener = */ false,
                                     getter_AddRefs(tmp)));
}

////////////////////////////////////////////////////////////////////////////////
// ImageAccessible

nsIntPoint
ImageAccessible::Position(uint32_t aCoordType)
{
  nsIntPoint point = Bounds().TopLeft();
  nsAccUtils::ConvertScreenCoordsTo(&point.x, &point.y, aCoordType, this);
  return point;
}

nsIntSize
ImageAccessible::Size()
{
  return Bounds().Size();
}

// Accessible
already_AddRefed<nsIPersistentProperties>
ImageAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    LinkableAccessible::NativeAttributes();

  nsAutoString src;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
  if (!src.IsEmpty())
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::src, src);

  return attributes.forget();
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

already_AddRefed<nsIURI>
ImageAccessible::GetLongDescURI() const
{
  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::longdesc)) {
    // To check if longdesc contains an invalid url.
    nsAutoString longdesc;
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::longdesc, longdesc);
    if (longdesc.FindChar(' ') != -1 || longdesc.FindChar('\t') != -1 ||
        longdesc.FindChar('\r') != -1 || longdesc.FindChar('\n') != -1) {
      return nullptr;
    }
    nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
    nsCOMPtr<nsIURI> uri;
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri), longdesc,
                                              mContent->OwnerDoc(), baseURI);
    return uri.forget();
  }

  DocAccessible* document = Document();
  if (document) {
    IDRefsIterator iter(document, mContent, nsGkAtoms::aria_describedby);
    while (nsIContent* target = iter.NextElem()) {
      if ((target->IsHTMLElement(nsGkAtoms::a) ||
           target->IsHTMLElement(nsGkAtoms::area)) &&
          target->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
        nsGenericHTMLElement* element =
          nsGenericHTMLElement::FromContent(target);

        nsCOMPtr<nsIURI> uri;
        element->GetURIAttr(nsGkAtoms::href, nullptr, getter_AddRefs(uri));
        return uri.forget();
      }
    }
  }

  return nullptr;
}

bool
ImageAccessible::IsLongDescIndex(uint8_t aIndex)
{
  return aIndex == LinkableAccessible::ActionCount();
}

