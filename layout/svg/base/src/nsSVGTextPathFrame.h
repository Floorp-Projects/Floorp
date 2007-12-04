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

#ifndef NSSVGTEXTPATHFRAME_H
#define NSSVGTEXTPATHFRAME_H

#include "nsSVGTSpanFrame.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsSVGLengthList.h"
#include "nsIDOMSVGLength.h"
#include "gfxPath.h"
#include "nsStubMutationObserver.h"

class nsSVGTextPathFrame;

class nsSVGPathListener : public nsStubMutationObserver {
public:
  nsSVGPathListener(nsIContent *aPathElement,
                    nsSVGTextPathFrame *aTextPathFrame);
  ~nsSVGPathListener();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

private:
  nsWeakPtr mObservedPath;
  nsSVGTextPathFrame *mTextPathFrame;
};

typedef nsSVGTSpanFrame nsSVGTextPathFrameBase;

class nsSVGTextPathFrame : public nsSVGTextPathFrameBase
{
  friend nsIFrame*
  NS_NewSVGTextPathFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                         nsIFrame* parentFrame, nsStyleContext* aContext);
protected:
  nsSVGTextPathFrame(nsStyleContext* aContext) : nsSVGTextPathFrameBase(aContext) {}

public:
  // nsIFrame:
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);
  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgGFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGTextPath"), aResult);
  }
#endif

  // nsSVGTextPathFrame methods:
  already_AddRefed<gfxFlattenedPath> GetFlattenedPath();
  nsIFrame *GetPathFrame();

  gfxFloat GetStartOffset();
  gfxFloat GetPathScale();
protected:

  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetX();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetY();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDx();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDy();

private:
  already_AddRefed<gfxFlattenedPath> GetFlattenedPath(nsIFrame *path);

  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
  nsRefPtr<nsSVGPathListener> mPathListener;

  friend class nsSVGPathListener;
};

#endif
