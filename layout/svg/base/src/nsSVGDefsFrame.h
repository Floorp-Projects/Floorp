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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef NS_SVGDEFSFRAME_H
#define NS_SVGDEFSFRAME_H

#include "nsContainerFrame.h"
#include "nsISVGChildFrame.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"

typedef nsContainerFrame nsSVGDefsFrameBase;

class nsSVGDefsFrame : public nsSVGDefsFrameBase,
                       public nsISVGChildFrame,
                       public nsISVGContainerFrame,
                       public nsISVGValueObserver,
                       public nsSupportsWeakReference
{
  friend nsresult
  NS_NewSVGDefsFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
protected:
  nsSVGDefsFrame();
  virtual ~nsSVGDefsFrame();
  nsresult Init();
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:

  NS_IMETHOD  AppendFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD  ReplaceFrame(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);


  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGChildFrame interface:
  NS_IMETHOD Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips);
  NS_IMETHOD GetFrameForPoint(float x, float y, nsIFrame** hit);  
  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>) GetCoveredRegion();
  NS_IMETHOD InitialUpdate();
  NS_IMETHOD NotifyCanvasTMChanged();
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISVGContainerFrame interface:
  nsISVGOuterSVGFrame *GetOuterSVGFrame();
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();

protected:
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
};

#endif
