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
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
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

#ifndef __NS_SVGSVGELEMENT_H__
#define __NS_SVGSVGELEMENT_H__

#include "nsSVGStylableElement.h"
#include "nsISVGSVGElement.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGLocatable.h"
#include "nsIDOMSVGZoomAndPan.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGLength2.h"

#define QI_TO_NSSVGSVGELEMENT(base)                                           \
  NS_STATIC_CAST(nsSVGSVGElement*,                                            \
    NS_STATIC_CAST(nsISVGSVGElement*,                                         \
      nsCOMPtr<nsISVGSVGElement>(do_QueryInterface(base))));

typedef nsSVGStylableElement nsSVGSVGElementBase;

class nsSVGSVGElement : public nsSVGSVGElementBase,
                        public nsISVGSVGElement, // : nsIDOMSVGSVGElement
                        public nsIDOMSVGFitToViewBox,
                        public nsIDOMSVGLocatable,
                        public nsIDOMSVGZoomAndPan
{
  friend class nsSVGOuterSVGFrame;
  friend class nsSVGInnerSVGFrame;

protected:
  friend nsresult NS_NewSVGSVGElement(nsIContent **aResult,
                                      nsINodeInfo *aNodeInfo);
  nsSVGSVGElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGSVGElement();
  nsresult Init();
  
  // nsSVGSVGElement methods:
  void SetCoordCtxRect(nsIDOMSVGRect* aCtxRect);

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISVGSVGELEMENT_IID)

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSVGELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX
  NS_DECL_NSIDOMSVGLOCATABLE
  NS_DECL_NSIDOMSVGZOOMANDPAN
  
  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGSVGElementBase::)

  // nsISVGSVGElement interface:
  NS_IMETHOD GetCurrentScaleNumber(nsIDOMSVGNumber **aResult);
  NS_IMETHOD GetZoomAndPanEnum(nsISVGEnum **aResult);
  NS_IMETHOD SetCurrentScaleTranslate(float s, float x, float y);
  NS_IMETHOD SetCurrentTranslate(float x, float y);
  NS_IMETHOD_(void) RecordCurrentScaleTranslate();
  NS_IMETHOD_(float) GetPreviousTranslate_x();
  NS_IMETHOD_(float) GetPreviousTranslate_y();
  NS_IMETHOD_(float) GetPreviousScale();

  // nsIContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsSVGElement specializations:
  virtual void DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr);

  // nsSVGSVGElement methods:
  float GetLength(PRUint8 mCtxType);
  float GetMMPerPx(PRUint8 mCtxType = 0);
  already_AddRefed<nsIDOMSVGRect> GetCtxRect();

  // public helpers:
  nsresult GetViewboxToViewportTransform(nsIDOMSVGMatrix **_retval);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  // nsSVGElement overrides
  PRBool IsEventName(nsIAtom* aName);

  // implementation helpers:
  void GetOffsetToAncestor(nsIContent* ancestor, float &x, float &y);

  // invalidate viewbox -> viewport xform & inform frames
  void InvalidateTransformNotifyFrame();

  virtual LengthAttributesInfo GetLengthInfo();

  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  nsSVGSVGElement                  *mCoordCtx;
  nsCOMPtr<nsIDOMSVGAnimatedRect>   mViewBox;
  nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> mPreserveAspectRatio;

  float mViewportWidth, mViewportHeight;  // valid only for outersvg
  float mCoordCtxMmPerPx;

  // zoom and pan
  // IMPORTANT: only RecordCurrentScaleTranslate should change the "mPreviousX"
  // members below - see the comment in RecordCurrentScaleTranslate
  nsCOMPtr<nsISVGEnum>              mZoomAndPan;
  nsCOMPtr<nsIDOMSVGPoint>          mCurrentTranslate;
  nsCOMPtr<nsIDOMSVGNumber>         mCurrentScale;
  float                             mPreviousTranslate_x;
  float                             mPreviousTranslate_y;
  float                             mPreviousScale;
  PRInt32                           mRedrawSuspendCount;
  PRPackedBool                      mDispatchEvent;
};

#endif
