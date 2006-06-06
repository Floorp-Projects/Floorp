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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#include "nsSVGAtoms.h"
#include "nsSVGPathSegList.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsSVGPathSeg.h"
#include "nsCOMPtr.h"
#include "nsISVGPathFlatten.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsSVGPathDataParser.h"
#include "nsSVGPathElement.h"
#include "nsISVGValueUtils.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Path)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPathElement,nsSVGPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPathElement,nsSVGPathElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPathElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPathElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPathData)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPathElement::nsSVGPathElement(nsINodeInfo* aNodeInfo)
  : nsSVGPathElementBase(aNodeInfo)
{
}

nsSVGPathElement::~nsSVGPathElement()
{
  if (mSegments)
    NS_REMOVE_SVGVALUE_OBSERVER(mSegments);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGPathElement)

//----------------------------------------------------------------------
// nsIDOMSVGPathElement methods:

/* readonly attribute nsIDOMSVGAnimatedNumber pathLength; */
NS_IMETHODIMP
nsSVGPathElement::GetPathLength(nsIDOMSVGAnimatedNumber * *aPathLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* float getTotalLength (); */
NS_IMETHODIMP
nsSVGPathElement::GetTotalLength(float *_retval)
{
  *_retval = 0;
  nsCOMPtr<nsISVGPathFlatten> flattener = GetPathFlatten();
  if (!flattener)
    return NS_ERROR_FAILURE;

  nsSVGPathData *data;
  flattener->GetFlattenedPath(&data);
  *_retval = data->Length();

  delete data;

  return NS_OK;
}

/* nsIDOMSVGPoint getPointAtLength (in float distance); */
NS_IMETHODIMP
nsSVGPathElement::GetPointAtLength(float distance, nsIDOMSVGPoint **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getPathSegAtLength (in float distance); */
NS_IMETHODIMP
nsSVGPathElement::GetPathSegAtLength(float distance, PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGPathSegClosePath createSVGPathSegClosePath (); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegClosePath(nsIDOMSVGPathSegClosePath **_retval)
{
  return NS_NewSVGPathSegClosePath(_retval);
}

/* nsIDOMSVGPathSegMovetoAbs createSVGPathSegMovetoAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegMovetoAbs(float x, float y, nsIDOMSVGPathSegMovetoAbs **_retval)
{
  return NS_NewSVGPathSegMovetoAbs(_retval, x, y);
}

/* nsIDOMSVGPathSegMovetoRel createSVGPathSegMovetoRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegMovetoRel(float x, float y, nsIDOMSVGPathSegMovetoRel **_retval)
{
  return NS_NewSVGPathSegMovetoRel(_retval, x, y);
}

/* nsIDOMSVGPathSegLinetoAbs createSVGPathSegLinetoAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y, nsIDOMSVGPathSegLinetoAbs **_retval)
{
  return NS_NewSVGPathSegLinetoAbs(_retval, x, y);
}

/* nsIDOMSVGPathSegLinetoRel createSVGPathSegLinetoRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoRel(float x, float y, nsIDOMSVGPathSegLinetoRel **_retval)
{
  return NS_NewSVGPathSegLinetoRel(_retval, x, y);
}

/* nsIDOMSVGPathSegCurvetoCubicAbs createSVGPathSegCurvetoCubicAbs (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicAbs **_retval)
{
  return NS_NewSVGPathSegCurvetoCubicAbs(_retval, x, y, x1, y1, x2, y2);
}

/* nsIDOMSVGPathSegCurvetoCubicRel createSVGPathSegCurvetoCubicRel (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicRel **_retval)
{
  return NS_NewSVGPathSegCurvetoCubicRel(_retval, x, y, x1, y1, x2, y2);
}

/* nsIDOMSVGPathSegCurvetoQuadraticAbs createSVGPathSegCurvetoQuadraticAbs (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1, nsIDOMSVGPathSegCurvetoQuadraticAbs **_retval)
{
  return NS_NewSVGPathSegCurvetoQuadraticAbs(_retval, x, y, x1, y1);
}

/* nsIDOMSVGPathSegCurvetoQuadraticRel createSVGPathSegCurvetoQuadraticRel (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1, nsIDOMSVGPathSegCurvetoQuadraticRel **_retval)
{
  return NS_NewSVGPathSegCurvetoQuadraticRel(_retval, x, y, x1, y1);
}

/* nsIDOMSVGPathSegArcAbs createSVGPathSegArcAbs (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, PRBool largeArcFlag, PRBool sweepFlag, nsIDOMSVGPathSegArcAbs **_retval)
{
  return NS_NewSVGPathSegArcAbs(_retval, x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

/* nsIDOMSVGPathSegArcRel createSVGPathSegArcRel (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, PRBool largeArcFlag, PRBool sweepFlag, nsIDOMSVGPathSegArcRel **_retval)
{
  return NS_NewSVGPathSegArcRel(_retval, x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

/* nsIDOMSVGPathSegLinetoHorizontalAbs createSVGPathSegLinetoHorizontalAbs (in float x); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x, nsIDOMSVGPathSegLinetoHorizontalAbs **_retval)
{
  return NS_NewSVGPathSegLinetoHorizontalAbs(_retval, x);
}

/* nsIDOMSVGPathSegLinetoHorizontalRel createSVGPathSegLinetoHorizontalRel (in float x); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x, nsIDOMSVGPathSegLinetoHorizontalRel **_retval)
{
  return NS_NewSVGPathSegLinetoHorizontalRel(_retval, x);
}

/* nsIDOMSVGPathSegLinetoVerticalAbs createSVGPathSegLinetoVerticalAbs (in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y, nsIDOMSVGPathSegLinetoVerticalAbs **_retval)
{
  return NS_NewSVGPathSegLinetoVerticalAbs(_retval, y);
}

/* nsIDOMSVGPathSegLinetoVerticalRel createSVGPathSegLinetoVerticalRel (in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y, nsIDOMSVGPathSegLinetoVerticalRel **_retval)
{
  return NS_NewSVGPathSegLinetoVerticalRel(_retval, y);
}

/* nsIDOMSVGPathSegCurvetoCubicSmoothAbs createSVGPathSegCurvetoCubicSmoothAbs (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicSmoothAbs **_retval)
{
  return NS_NewSVGPathSegCurvetoCubicSmoothAbs(_retval, x, y, x2, y2);
}

/* nsIDOMSVGPathSegCurvetoCubicSmoothRel createSVGPathSegCurvetoCubicSmoothRel (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicSmoothRel **_retval)
{
  return NS_NewSVGPathSegCurvetoCubicSmoothRel(_retval, x, y, x2, y2);
}

/* nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs createSVGPathSegCurvetoQuadraticSmoothAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y, nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs **_retval)
{
  return NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(_retval, x, y);
}

/* nsIDOMSVGPathSegCurvetoQuadraticSmoothRel createSVGPathSegCurvetoQuadraticSmoothRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y, nsIDOMSVGPathSegCurvetoQuadraticSmoothRel **_retval)
{
  return NS_NewSVGPathSegCurvetoQuadraticSmoothRel(_retval, x, y);
}


nsresult
nsSVGPathElement::CreatePathSegList()
{
  if (mSegments)
    return NS_OK;

  nsresult rv = NS_NewSVGPathSegList(getter_AddRefs(mSegments));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mSegments);

  nsAutoString d;
  if (NS_SUCCEEDED(GetAttr(kNameSpaceID_None, nsGkAtoms::d, d)))
    value->SetValueString(d);

  NS_ADD_SVGVALUE_OBSERVER(mSegments);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedPathData methods:

/* readonly attribute nsIDOMSVGPathSegList pathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetPathSegList(nsIDOMSVGPathSegList * *aPathSegList)
{
  nsresult rv = CreatePathSegList();
  NS_ENSURE_SUCCESS(rv, rv);

  *aPathSegList = mSegments;
  NS_ADDREF(*aPathSegList);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPathSegList normalizedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetNormalizedPathSegList(nsIDOMSVGPathSegList * *aNormalizedPathSegList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGPathSegList animatedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetAnimatedPathSegList(nsIDOMSVGPathSegList * *aAnimatedPathSegList)
{
  nsresult rv = CreatePathSegList();
  NS_ENSURE_SUCCESS(rv, rv);

  *aAnimatedPathSegList = mSegments;
  NS_ADDREF(*aAnimatedPathSegList);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPathSegList animatedNormalizedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetAnimatedNormalizedPathSegList(nsIDOMSVGPathSegList * *aAnimatedNormalizedPathSegList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGPathElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap,
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGPathElementBase::IsAttributeMapped(name);
}

nsresult
nsSVGPathElement::BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                               const nsAString* aValue, PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::d) {
    if (mSegments) {
      NS_REMOVE_SVGVALUE_OBSERVER(mSegments);
      mSegments = nsnull;
    }
    
    nsSVGPathDataParserToInternal parser(&mPathData);
    parser.Parse(*aValue);
  }

  return nsSVGPathElementBase::BeforeSetAttr(aNamespaceID, aName,
                                             aValue, aNotify);
}

NS_IMETHODIMP
nsSVGPathElement::DidModifySVGObservable(nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGPathSegList> list = do_QueryInterface(observable);

  if (list && mSegments == list) {
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mSegments);
    nsAutoString d;
    nsresult rv = value->GetValueString(d);
    NS_ENSURE_SUCCESS(rv, rv);

    // Want to keep the seglist alive - SetAttr normally invalidates it
    nsCOMPtr<nsIDOMSVGPathSegList> deathGrip = mSegments;
    mSegments = nsnull;

    rv = SetAttr(kNameSpaceID_None, nsGkAtoms::d, d, PR_TRUE);

    // Restore seglist
    mSegments = deathGrip;
    
    return rv;
  }

  return nsSVGPathElementBase::DidModifySVGObservable(observable, aModType);
}

//----------------------------------------------------------------------
// implementation helpers:

already_AddRefed<nsISVGPathFlatten>
nsSVGPathElement::GetPathFlatten()
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return nsnull;
  }
  
  // Flush all pending notifications so that our frames are up to date.  Make
  // sure to do this first thing, since it may end up destroying our document's
  // presshell.
  doc->FlushPendingNotifications(Flush_Layout);

  nsIPresShell* presShell = doc->GetShellAt(0);
  if (!presShell) {
    return nsnull;
  }

  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);

  if (!frame) {
    return nsnull;
  }
  
  nsISVGPathFlatten* flattener;
  CallQueryInterface(frame, &flattener);
  return flattener;
}

//==================================================================
// nsSVGPathList

void
nsSVGPathList::Clear()
{
  if (mCommands) {
    free(mCommands);
    mCommands = nsnull;
  }
  if (mArguments) {
    free(mArguments);
    mArguments = nsnull;
  }
  mNumCommands = 0;
}

void
nsSVGPathList::Playback(cairo_t *aCtx)
{
  float *args = mArguments;
  for (PRUint32 i = 0; i < mNumCommands; i++) {
    PRUint8 command = (mCommands[i / 4] >> (2 * (i % 4))) & 0x3;
    switch (command) {
    case MOVETO:
      cairo_move_to(aCtx, args[0], args[1]);
      args += 2;
      break;
    case LINETO:
      cairo_line_to(aCtx, args[0], args[1]);
      args += 2;
      break;
    case CURVETO:
      cairo_curve_to(aCtx,
                     args[0], args[1], args[2], args[3], args[4], args[5]);
      args += 6;
      break;
    case CLOSEPATH:
      cairo_close_path(aCtx);
      break;
    default:
      break;
    }
  }
}
