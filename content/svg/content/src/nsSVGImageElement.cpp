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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsGkAtoms.h"
#include "nsSVGLength.h"
#include "nsSVGAnimatedString.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsSVGAnimatedPreserveAspectRatio.h"
#include "nsSVGPreserveAspectRatio.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "nsSVGPathGeometryElement.h"
#include "nsIDOMSVGImageElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsImageLoadingContent.h"
#include "nsSVGLength2.h"
#include "gfxContext.h"

class nsIDOMSVGAnimatedString;
class nsIDOMSVGAnimatedPreserveAspectRatio;

typedef nsSVGPathGeometryElement nsSVGImageElementBase;

class nsSVGImageElement : public nsSVGImageElementBase,
                          public nsIDOMSVGImageElement,
                          public nsIDOMSVGURIReference,
                          public nsImageLoadingContent
{
protected:
  friend nsresult NS_NewSVGImageElement(nsIContent **aResult,
                                        nsINodeInfo *aNodeInfo);
  nsSVGImageElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGImageElement();
  nsresult Init();

public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGIMAGEELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGImageElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGImageElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGImageElementBase::)

  // nsISVGValueObserver specializations:
  NS_IMETHOD DidModifySVGObservable(nsISVGValue *observable,
                                    nsISVGValue::modificationType aModType);

  // nsIContent interface
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);

  virtual PRInt32 IntrinsicState() const;

  NS_IMETHODIMP_(PRBool) IsAttributeMapped(const nsIAtom* name) const;

  // nsSVGPathGeometryElement methods:
  virtual void ConstructPath(gfxContext *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  virtual LengthAttributesInfo GetLengthInfo();

  void GetSrc(nsAString& src);

  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
  
  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
  nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> mPreserveAspectRatio;
};

nsSVGElement::LengthInfo nsSVGImageElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Image)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGImageElement,nsSVGImageElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGImageElement,nsSVGImageElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIImageLoadingContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGImageElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGImageElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGImageElement::nsSVGImageElement(nsINodeInfo *aNodeInfo)
  : nsSVGImageElementBase(aNodeInfo)
{
}

nsSVGImageElement::~nsSVGImageElement()
{
  DestroyImageLoadingContent();
}

nsresult
nsSVGImageElement::Init()
{
  nsresult rv = nsSVGImageElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // nsIDOMSVGURIReference properties

  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: preserveAspectRatio , #IMPLIED attrib: preserveAspectRatio
  {
    nsCOMPtr<nsIDOMSVGPreserveAspectRatio> preserveAspectRatio;
    rv = NS_NewSVGPreserveAspectRatio(getter_AddRefs(preserveAspectRatio));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedPreserveAspectRatio(
                                          getter_AddRefs(mPreserveAspectRatio),
                                          preserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::preserveAspectRatio,
                           mPreserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGImageElement)


//----------------------------------------------------------------------
// nsIDOMSVGImageElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGImageElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGImageElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGImageElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGImageElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGImageElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio
                                          **aPreserveAspectRatio)
{
  *aPreserveAspectRatio = mPreserveAspectRatio;
  NS_IF_ADDREF(*aPreserveAspectRatio);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGImageElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGImageElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

//----------------------------------------------------------------------

void nsSVGImageElement::GetSrc(nsAString& src)
{
  // resolve href attribute

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoString relURIStr;
  mHref->GetAnimVal(relURIStr);
  relURIStr.Trim(" \t\n\r");

  if (baseURI && !relURIStr.IsEmpty()) 
    NS_MakeAbsoluteURI(src, relURIStr, baseURI);
  else
    src = relURIStr;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGImageElement::DidModifySVGObservable(nsISVGValue* aObservable,
                                          nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGAnimatedString> s = do_QueryInterface(aObservable);

  if (s && mHref == s) {
    nsAutoString href;
    GetSrc(href);

#ifdef DEBUG_tor
    fprintf(stderr, "nsSVGImageElement - URI <%s>\n", ToNewCString(href));
#endif

    // If caller is not chrome and dom.disable_image_src_set is true,
    // prevent setting image.src by exiting early
    if (nsContentUtils::GetBoolPref("dom.disable_image_src_set") &&
        !nsContentUtils::IsCallerChrome()) {
      return NS_OK;
    }

    LoadImage(href, PR_TRUE, PR_TRUE);
  }

  return nsSVGImageElementBase::DidModifySVGObservable(aObservable, aModType);
}

//----------------------------------------------------------------------
// nsIContent methods:

nsresult
nsSVGImageElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers)
{
  nsresult rv = nsSVGImageElementBase::BindToTree(aDocument, aParent,
                                                  aBindingParent,
                                                  aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Our base URI may have changed; claim that our URI changed, and the
  // nsImageLoadingContent will decide whether a new image load is warranted.
  nsAutoString href;
  if (GetAttr(kNameSpaceID_XLink, nsGkAtoms::href, href)) {
    // Note: no need to notify here; since we're just now being bound
    // we don't have any frames or anything yet.
    LoadImage(href, PR_FALSE, PR_FALSE);
  }

  return rv;
}

PRInt32
nsSVGImageElement::IntrinsicState() const
{
  return nsSVGImageElementBase::IntrinsicState() |
    nsImageLoadingContent::ImageState();
}

NS_IMETHODIMP_(PRBool)
nsSVGImageElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sViewportsMap,
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGImageElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

/* For the purposes of the update/invalidation logic pretend to
   be a rectangle. */
void
nsSVGImageElement::ConstructPath(gfxContext *aCtx)
{
  float x, y, width, height;

  GetAnimatedLengthValues(&x, &y, &width, &height, nsnull);

  if (width == 0 || height == 0)
    return;

  aCtx->Rectangle(gfxRect(x, y, width, height));
}
