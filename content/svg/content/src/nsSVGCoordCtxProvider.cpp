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
 *   Alex Fritze <alex@croczilla.com> (original author)
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

#include "nsSVGCoordCtxProvider.h"
#include "nsISVGValue.h"
#include "nsISVGValueUtils.h"
#include <math.h>

////////////////////////////////////////////////////////////////////////
// nsSVGCoordCtxHolder implementation

nsSVGCoordCtxHolder::nsSVGCoordCtxHolder()
    : mRefCnt(1),
      mCtxX(dont_AddRef(new nsSVGCoordCtx)),
      mCtxY(dont_AddRef(new nsSVGCoordCtx)),
      mCtxUnspec(dont_AddRef(new nsSVGCoordCtx))
{}

nsSVGCoordCtxHolder::~nsSVGCoordCtxHolder()
{
  if (mCtxRect)
    NS_REMOVE_SVGVALUE_OBSERVER(mCtxRect);  
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCoordCtxHolder)
NS_IMPL_RELEASE(nsSVGCoordCtxHolder)

NS_INTERFACE_MAP_BEGIN(nsSVGCoordCtxHolder)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValueObserver)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGCoordCtxHolder::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCoordCtxHolder::DidModifySVGObservable(nsISVGValue* observable)
{
  Update();
  return NS_OK;
}

//----------------------------------------------------------------------

void
nsSVGCoordCtxHolder::SetContextRect(nsIDOMSVGRect* ctxRect)
{
  if (mCtxRect)
    NS_REMOVE_SVGVALUE_OBSERVER(mCtxRect);
  mCtxRect = ctxRect;
  if (mCtxRect) {
    NS_ADD_SVGVALUE_OBSERVER(mCtxRect);
    Update();
  }
}

void
nsSVGCoordCtxHolder::SetMMPerPx(float mmPerPxX, float mmPerPxY)
{
  mCtxX->mmPerPx = mmPerPxX;
  mCtxY->mmPerPx = mmPerPxY;
  mCtxUnspec->mmPerPx = (float)sqrt((mmPerPxX*mmPerPxX + mmPerPxY*mmPerPxY)/2.0);
}

void
nsSVGCoordCtxHolder::Update()
{
  float w,h;
  mCtxRect->GetWidth(&w);
  mCtxRect->GetHeight(&h);
  mCtxX->mLength->SetValue(w);
  mCtxY->mLength->SetValue(h);
  mCtxUnspec->mLength->SetValue((float)sqrt((w*w+h*h)/2.0));
}

already_AddRefed<nsSVGCoordCtx>
nsSVGCoordCtxHolder::GetContextX()
{
  nsSVGCoordCtx *rv = mCtxX.get();
  NS_IF_ADDREF(rv);
  return rv;
}

already_AddRefed<nsSVGCoordCtx>
nsSVGCoordCtxHolder::GetContextY()
{
  nsSVGCoordCtx *rv = mCtxY.get();
  NS_IF_ADDREF(rv);
  return rv;
}

already_AddRefed<nsSVGCoordCtx>
nsSVGCoordCtxHolder::GetContextUnspecified()
{
  nsSVGCoordCtx *rv = mCtxUnspec.get();
  NS_IF_ADDREF(rv);
  return rv;
}
