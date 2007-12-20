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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#ifndef __NS_ISVGCHILDFRAME_H__
#define __NS_ISVGCHILDFRAME_H__


#include "nsISupports.h"
#include "nsCOMPtr.h"

class gfxContext;
class nsPresContext;
class nsIDOMSVGRect;
class nsIDOMSVGMatrix;
class nsSVGRenderState;
struct nsRect;

#define NS_ISVGCHILDFRAME_IID \
{ 0x93560e72, 0x6818, 0x4218, \
 { 0xa1, 0xe9, 0xf3, 0xb9, 0x63, 0x6a, 0xff, 0xc2 } }

class nsISVGChildFrame : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISVGCHILDFRAME_IID)

  // Paint this frame - aDirtyRect is the area being redrawn, in frame
  // offset pixel coordinates
  NS_IMETHOD PaintSVG(nsSVGRenderState* aContext, nsRect *aDirtyRect)=0;

  // Check if this frame or children contain the given point,
  // specified in device pixels relative to the origin of the outer
  // svg frame (origin ill-defined in the case of borders - bug
  // 290770).  Return value unspecified (usually NS_OK for hit, error
  // no hit, but not always [ex: nsSVGPathGeometryFrame.cpp]) and no
  // code trusts the return value - this should be fixed (bug 290765).
  // *hit set to topmost frame in the children (or 'this' if leaf
  // frame) which is accepting pointer events, null if no frame hit.
  // See bug 290852 for foreignObject complications.
  NS_IMETHOD GetFrameForPointSVG(float x, float y, nsIFrame** hit)=0;

  // Get bounds in our gfxContext's coordinates space (in device pixels)
  NS_IMETHOD_(nsRect) GetCoveredRegion()=0;
  NS_IMETHOD UpdateCoveredRegion()=0;

  // Called once on SVG child frames except descendants of <defs>, either
  // when their nsSVGOuterSVGFrame receives its initial reflow (i.e. once
  // the SVG viewport dimensions are known), or else when they're inserted
  // into the frame tree (if they're inserted after the initial reflow).
  NS_IMETHOD InitialUpdate()=0;

  NS_IMETHOD NotifyCanvasTMChanged(PRBool suppressInvalidation)=0;
  NS_IMETHOD NotifyRedrawSuspended()=0;
  NS_IMETHOD NotifyRedrawUnsuspended()=0;

  // Set whether we should stop multiplying matrices when building up
  // the current transformation matrix at this frame.
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate)=0;

  // Set the current transformation matrix to a particular matrix.
  // Value is only used if matrix propagation is prevented
  // (SetMatrixPropagation()).  nsnull aCTM means identity transform.
  NS_IMETHOD SetOverrideCTM(nsIDOMSVGMatrix *aCTM)=0;
  virtual already_AddRefed<nsIDOMSVGMatrix> GetOverrideCTM()=0;

  // XXX move this function into interface nsISVGLocatableMetrics
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval)=0; // bbox in local coords

  // Are we a container frame?
  NS_IMETHOD_(PRBool) IsDisplayContainer()=0;

  // Does this frame have an current covered region in mRect (aka GetRect())?
  NS_IMETHOD_(PRBool) HasValidCoveredRect()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISVGChildFrame, NS_ISVGCHILDFRAME_IID)

#endif // __NS_ISVGCHILDFRAME_H__

