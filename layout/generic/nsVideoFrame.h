/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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

/* rendering object for the HTML <video> element */

#ifndef nsVideoFrame_h___
#define nsVideoFrame_h___

#include "nsContainerFrame.h"
#include "nsString.h"
#include "nsAString.h"
#include "nsIIOService.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsIAnonymousContentCreator.h"
#include "Layers.h"
#include "ImageLayers.h"

class nsPresContext;
class nsDisplayItem;

nsIFrame* NS_NewVideoFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsVideoFrame : public nsContainerFrame, public nsIAnonymousContentCreator
{
public:
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;

  nsVideoFrame(nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  /* get the size of the video's display */
  nsSize GetVideoIntrinsicSize(nsRenderingContext *aRenderingContext);
  virtual nsSize GetIntrinsicRatio();
  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             bool aShrinkWrap);
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  virtual bool IsLeaf() const;

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsSplittableFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }
  
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilters);

  nsIContent* GetPosterImage() { return mPosterImage; }

  // Returns PR_TRUE if we should display the poster. Note that once we show
  // a video frame, the poster will never be displayed again.
  bool ShouldDisplayPoster();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     nsDisplayItem* aItem);

protected:

  // Returns PR_TRUE if we're rendering for a video element. We still create
  // nsVideoFrame to render controls for an audio element.
  bool HasVideoElement();

  // Returns PR_TRUE if there is video data to render. Can return false
  // when we're the frame for an audio element, or we've created a video
  // element for a media which is audio-only.
  bool HasVideoData();

  // Sets the mPosterImage's src attribute to be the video's poster attribute,
  // if we're the frame for a video element. Only call on frames for video
  // elements, not for frames for audio elements.
  nsresult UpdatePosterSource(bool aNotify);

  virtual ~nsVideoFrame();

  nsMargin mBorderPadding;
  
  // Anonymous child which is bound via XBL to the video controls.
  nsCOMPtr<nsIContent> mVideoControls;
  
  // Anonymous child which is the image element of the poster frame.
  nsCOMPtr<nsIContent> mPosterImage;
};

#endif /* nsVideoFrame_h___ */
