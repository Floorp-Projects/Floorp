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

#ifndef __NS_SVGCOORDCTXPROVIDER_H__
#define __NS_SVGCOORDCTXPROVIDER_H__

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsSVGCoordCtx.h"
#include "nsIDOMSVGRect.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"

////////////////////////////////////////////////////////////////////////
// nsSVGCoordCtxHolder: This is a private inner helper class for
// nsSVGCoordCtxProvider. It holds the actual coordinate contexts; the
// accessors (GetContextX,etc.) on nsSVGCoordCtxProvider are delegated
// to it. The sole purpose for having this separate inner class is to
// hide the nsISVGValueObserver interface which we require to listen
// in on viewbox (mCtxRect) size changes.

class nsSVGCoordCtxHolder : nsISVGValueObserver,
                            nsSupportsWeakReference
{
private:
  friend class nsSVGCoordCtxProvider;
  nsSVGCoordCtxHolder(); // addrefs
  ~nsSVGCoordCtxHolder();
  
  // nsISupports interface:
  NS_DECL_ISUPPORTS
  
  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  void SetContextRect(nsIDOMSVGRect* ctxRect);
  void SetMMPerPx(float mmPerPxX, float mmPerPxY);

  void Update();
  
  already_AddRefed<nsSVGCoordCtx> GetContextX();
  already_AddRefed<nsSVGCoordCtx> GetContextY();
  already_AddRefed<nsSVGCoordCtx> GetContextUnspecified();
  
  nsCOMPtr<nsIDOMSVGRect> mCtxRect;
  nsRefPtr<nsSVGCoordCtx> mCtxX;
  nsRefPtr<nsSVGCoordCtx> mCtxY;
  nsRefPtr<nsSVGCoordCtx> mCtxUnspec;
};

////////////////////////////////////////////////////////////////////////
// pseudo interface nsSVGCoordCtxProvider

// {0C083184-4462-4B5D-B925-0BEC5A7E6779}
#define NS_SVGCOORDCTXPROVIDER_IID \
{ 0x0c083184, 0x4462, 0x4b5d, { 0xb9, 0x25, 0x0b, 0xec, 0x5a, 0x7e, 0x67, 0x79 } }

class nsSVGCoordCtxProvider : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_SVGCOORDCTXPROVIDER_IID)

  nsSVGCoordCtxProvider() :
      mInner(dont_AddRef(new nsSVGCoordCtxHolder)) {}
    
  already_AddRefed<nsSVGCoordCtx> GetContextX() { return mInner ? mInner->GetContextX() : nsnull; }
  already_AddRefed<nsSVGCoordCtx> GetContextY() { return mInner ? mInner->GetContextY() : nsnull; }
  already_AddRefed<nsSVGCoordCtx> GetContextUnspecified() { return mInner ? mInner->GetContextUnspecified() : nsnull; }

protected:
  void SetCoordCtxRect(nsIDOMSVGRect* ctxRect) { if(mInner) mInner->SetContextRect(ctxRect); }
  void SetCoordCtxMMPerPx(float mmPerPxX, float mmPerPxY) {
    if(mInner)
      mInner->SetMMPerPx(mmPerPxX, mmPerPxY);
  }
  
private:
  nsRefPtr<nsSVGCoordCtxHolder> mInner;
};

#endif // __NS_SVGCOORDCTXPROVIDER_H__
