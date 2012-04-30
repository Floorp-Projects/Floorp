/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aaronl@netscape.com)
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
 * ***** END LICENSE BLOCK ***** */

#include "nsHTMLImageAccessible.h"

#include "nsAccUtils.h"
#include "Role.h"
#include "AccIterator.h"
#include "States.h"

#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsGenericHTMLElement.h"
#include "nsIDocument.h"
#include "nsIImageLoadingContent.h"
#include "nsILink.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLImageAccessible::
  nsHTMLImageAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsLinkableAccessible(aContent, aDoc)
{
  mFlags |= eImageAccessible;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLImageAccessible, nsAccessible,
                             nsIAccessibleImage)

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public

PRUint64
nsHTMLImageAccessible::NativeState()
{
  // The state is a bitfield, get our inherited state, then logically OR it with
  // states::ANIMATED if this is an animated image.

  PRUint64 state = nsLinkableAccessible::NativeState();

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

nsresult
nsHTMLImageAccessible::GetNameInternal(nsAString& aName)
{
  bool hasAltAttrib =
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName);
  if (!aName.IsEmpty())
    return NS_OK;

  nsresult rv = nsAccessible::GetNameInternal(aName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aName.IsEmpty() && hasAltAttrib) {
    // No accessible name but empty 'alt' attribute is present. If further name
    // computation algorithm doesn't provide non empty name then it means
    // an empty 'alt' attribute was used to indicate a decorative image (see
    // nsIAccessible::name attribute for details).
    return NS_OK_EMPTY_NAME;
  }

  return NS_OK;
}

role
nsHTMLImageAccessible::NativeRole()
{
  return roles::GRAPHIC;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

PRUint8
nsHTMLImageAccessible::ActionCount()
{
  PRUint8 actionCount = nsLinkableAccessible::ActionCount();
  return HasLongDesc() ? actionCount + 1 : actionCount;
}

NS_IMETHODIMP
nsHTMLImageAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (IsLongDescIndex(aIndex) && HasLongDesc()) {
    aName.AssignLiteral("showlongdesc"); 
    return NS_OK;
  }
  return nsLinkableAccessible::GetActionName(aIndex, aName);
}

NS_IMETHODIMP
nsHTMLImageAccessible::DoAction(PRUint8 aIndex)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Get the long description uri and open in a new window.
  if (!IsLongDescIndex(aIndex))
    return nsLinkableAccessible::DoAction(aIndex);

  nsCOMPtr<nsIURI> uri = GetLongDescURI();
  if (!uri)
    return NS_ERROR_INVALID_ARG;

  nsCAutoString utf8spec;
  uri->GetSpec(utf8spec);
  NS_ConvertUTF8toUTF16 spec(utf8spec);

  nsIDocument* document = mContent->OwnerDoc();
  nsCOMPtr<nsPIDOMWindow> piWindow = document->GetWindow();
  nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(piWindow);
  NS_ENSURE_STATE(win);

  nsCOMPtr<nsIDOMWindow> tmp;
  return win->Open(spec, EmptyString(), EmptyString(),
                   getter_AddRefs(tmp));

}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleImage

NS_IMETHODIMP
nsHTMLImageAccessible::GetImagePosition(PRUint32 aCoordType,
                                        PRInt32 *aX, PRInt32 *aY)
{
  PRInt32 width, height;
  nsresult rv = GetBounds(aX, aY, &width, &height);
  if (NS_FAILED(rv))
    return rv;

  return nsAccUtils::ConvertScreenCoordsTo(aX, aY, aCoordType, this);
}

NS_IMETHODIMP
nsHTMLImageAccessible::GetImageSize(PRInt32 *aWidth, PRInt32 *aHeight)
{
  PRInt32 x, y;
  return GetBounds(&x, &y, aWidth, aHeight);
}

// nsAccessible
nsresult
nsHTMLImageAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;
  
  nsresult rv = nsLinkableAccessible::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString src;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
  if (!src.IsEmpty())
    nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::src, src);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

already_AddRefed<nsIURI>
nsHTMLImageAccessible::GetLongDescURI() const
{
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::longdesc)) {
    nsGenericHTMLElement* element = 
      nsGenericHTMLElement::FromContent(mContent);
    if (element) {
      nsCOMPtr<nsIURI> uri;
      element->GetURIAttr(nsGkAtoms::longdesc, nsnull, getter_AddRefs(uri));
      return uri.forget();
    }
  }

  nsDocAccessible* document = Document();
  if (document) {
    IDRefsIterator iter(document, mContent, nsGkAtoms::aria_describedby);
    while (nsIContent* target = iter.NextElem()) {
      if ((target->IsHTML(nsGkAtoms::a) || target->IsHTML(nsGkAtoms::area)) &&
          target->HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
        nsGenericHTMLElement* element =
          nsGenericHTMLElement::FromContent(target);

        nsCOMPtr<nsIURI> uri;
        element->GetURIAttr(nsGkAtoms::href, nsnull, getter_AddRefs(uri));
        return uri.forget();
      }
    }
  }

  return nsnull;
}

bool
nsHTMLImageAccessible::IsLongDescIndex(PRUint8 aIndex)
{
  return aIndex == nsLinkableAccessible::ActionCount();
}

