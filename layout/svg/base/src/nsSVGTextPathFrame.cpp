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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSVGTSpanFrame.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsSVGLengthList.h"
#include "nsISVGPathFlatten.h"
#include "nsSVGLength.h"
#include "nsSVGUtils.h"
#include "nsNetUtil.h"

#include "nsIDOMSVGAnimatedPathData.h"
#include "nsIDOMSVGPathSegList.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"

typedef nsSVGTSpanFrame nsSVGTextPathFrameBase;

class nsSVGTextPathFrame : public nsSVGTextPathFrameBase,
                           public nsISVGPathFlatten
{
public:

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgGFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGTextPath"), aResult);
  }
#endif

  // nsISVGValueObserver interface:
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable,
                                    nsISVGValue::modificationType aModType);

  // nsISVGPathFlatten interface
  NS_IMETHOD GetFlattenedPath(nsSVGPathData **data, nsIFrame *parent);

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  

protected:
  virtual ~nsSVGTextPathFrame();
  virtual nsresult InitSVG();

  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetX();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetY();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDx();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDy();

private:

  nsCOMPtr<nsIDOMSVGLength> mStartOffset;
  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
  nsCOMPtr<nsIDOMSVGPathSegList> mSegments;

  nsCOMPtr<nsISVGLengthList> mX;
};

NS_INTERFACE_MAP_BEGIN(nsSVGTextPathFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGPathFlatten)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTSpanFrame)


//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGTextPathFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                       nsIFrame* parentFrame, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextFrame *text_container;
  parentFrame->QueryInterface(NS_GET_IID(nsISVGTextFrame), (void**)&text_container);
  if (!text_container) {
    NS_ERROR("trying to construct an SVGTextPathFrame for an invalid container");
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIDOMSVGTextPathElement> tspan_elem = do_QueryInterface(aContent);
  if (!tspan_elem) {
    NS_ERROR("Trying to construct an SVGTextPathFrame for a "
             "content element that doesn't support the right interfaces");
    return NS_ERROR_FAILURE;
  }
  
  nsSVGTextPathFrame* it = new (aPresShell) nsSVGTextPathFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  
  return NS_OK;
}

nsSVGTextPathFrame::~nsSVGTextPathFrame()
{
  NS_REMOVE_SVGVALUE_OBSERVER(mStartOffset);
  NS_REMOVE_SVGVALUE_OBSERVER(mHref);
  NS_REMOVE_SVGVALUE_OBSERVER(mSegments);
}

nsresult
nsSVGTextPathFrame::InitSVG()
{
  nsCOMPtr<nsIDOMSVGTextPathElement> tpath = do_QueryInterface(mContent);

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    tpath->GetStartOffset(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mStartOffset));
    NS_ASSERTION(mStartOffset, "no startOffset");
    if (!mStartOffset)
      return NS_ERROR_FAILURE;

    NS_NewSVGLengthList(getter_AddRefs(mX));
    if (mX) {
      nsCOMPtr<nsIDOMSVGLength> length;
      mX->AppendItem(mStartOffset, getter_AddRefs(length));
    }

    NS_ADD_SVGVALUE_OBSERVER(mStartOffset);
  }

  {
    nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(mContent);
    if (aRef)
      aRef->GetHref(getter_AddRefs(mHref));
    if (!mHref)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mHref);
  }

  return NS_OK;
}

nsIAtom *
nsSVGTextPathFrame::GetType() const
{
  return nsLayoutAtoms::svgTextPathFrame;
}


NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextPathFrame::GetX()
{
  nsISVGLengthList *retval = mX;
  NS_IF_ADDREF(retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextPathFrame::GetY()
{
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextPathFrame::GetDx()
{
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextPathFrame::GetDy()
{
  return nsnull;
}

//----------------------------------------------------------------------
// nsISVGPathFlatten methods:

NS_IMETHODIMP
nsSVGTextPathFrame::GetFlattenedPath(nsSVGPathData **data, nsIFrame *parent) {
  *data = nsnull;
  nsIFrame *path;

  nsAutoString str;
  mHref->GetAnimVal(str);

  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI),
   str, mContent->GetCurrentDoc(), base);

  nsSVGUtils::GetReferencedFrame(&path, targetURI, mContent,
                                 GetPresContext()->PresShell());
  if (!path)
    return NS_ERROR_FAILURE;

  if (!mSegments) {
    nsCOMPtr<nsIDOMSVGAnimatedPathData> data =
      do_QueryInterface(path->GetContent());
    if (data) {
      data->GetAnimatedPathSegList(getter_AddRefs(mSegments));
      NS_ADD_SVGVALUE_OBSERVER(mSegments);
    }
  }

  nsISVGPathFlatten *flatten;
  CallQueryInterface(path, &flatten);

  if (!flatten)
    return NS_ERROR_FAILURE;

  return flatten->GetFlattenedPath(data, this);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGTextPathFrame::DidModifySVGObservable(nsISVGValue* observable,
                                           nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGAnimatedString> s = do_QueryInterface(observable);
  if (s && mHref==s) {
    NS_REMOVE_SVGVALUE_OBSERVER(mSegments);
    mSegments = nsnull;
  }

  return nsSVGTextPathFrameBase::DidModifySVGObservable(observable, aModType);
}
