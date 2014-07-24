/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSUBDOCUMENTFRAME_H_
#define NSSUBDOCUMENTFRAME_H_

#include "mozilla/Attributes.h"
#include "nsLeafFrame.h"
#include "nsIReflowCallback.h"
#include "nsFrameLoader.h"

/******************************************************************************
 * nsSubDocumentFrame
 *****************************************************************************/
class nsSubDocumentFrame : public nsLeafFrame,
                           public nsIReflowCallback
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsSubDocumentFrame)
  NS_DECL_FRAMEARENA_HELPERS

  nsSubDocumentFrame(nsStyleContext* aContext);

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "", uint32_t aFlags = 0) const MOZ_OVERRIDE;
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  NS_DECL_QUERYFRAME

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    // nsLeafFrame is already eReplacedContainsBlock, but that's somewhat bogus
    return nsLeafFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  virtual mozilla::IntrinsicSize GetIntrinsicSize() MOZ_OVERRIDE;
  virtual nsSize  GetIntrinsicRatio() MOZ_OVERRIDE;

  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap) MOZ_OVERRIDE;

  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             uint32_t aFlags) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) MOZ_OVERRIDE;

  // if the content is "visibility:hidden", then just hide the view
  // and all our contents. We don't extend "visibility:hidden" to
  // the child content ourselves, since it belongs to a different
  // document and CSS doesn't inherit in there.
  virtual bool SupportsVisibilityHidden() MOZ_OVERRIDE { return false; }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  nsresult GetDocShell(nsIDocShell **aDocShell);
  nsresult BeginSwapDocShells(nsIFrame* aOther);
  void EndSwapDocShells(nsIFrame* aOther);
  nsView* EnsureInnerView();
  nsIFrame* GetSubdocumentRootFrame();
  enum {
    IGNORE_PAINT_SUPPRESSION = 0x1
  };
  nsIPresShell* GetSubdocumentPresShellForPainting(uint32_t aFlags);
  nsIntSize GetSubdocumentSize();

  // nsIReflowCallback
  virtual bool ReflowFinished() MOZ_OVERRIDE;
  virtual void ReflowCallbackCanceled() MOZ_OVERRIDE;

  bool ShouldClipSubdocument()
  {
    nsFrameLoader* frameLoader = FrameLoader();
    return !frameLoader || frameLoader->ShouldClipSubdocument();
  }

  bool ShouldClampScrollPosition()
  {
    nsFrameLoader* frameLoader = FrameLoader();
    return !frameLoader || frameLoader->ShouldClampScrollPosition();
  }

  /**
   * Return true if pointer event hit-testing should be allowed to target
   * content in the subdocument.
   */
  bool PassPointerEventsToChildren();

protected:
  friend class AsyncFrameInit;

  // Helper method to look up the HTML marginwidth & marginheight attributes
  nsIntSize GetMarginAttributes();

  nsFrameLoader* FrameLoader();

  bool IsInline() { return mIsInline; }

  virtual nscoord GetIntrinsicISize() MOZ_OVERRIDE;
  virtual nscoord GetIntrinsicBSize() MOZ_OVERRIDE;

  // Show our document viewer. The document viewer is hidden via a script
  // runner, so that we can save and restore the presentation if we're
  // being reframed.
  void ShowViewer();

  /* Obtains the frame we should use for intrinsic size information if we are
   * an HTML <object>, <embed> or <applet> (a replaced element - not <iframe>)
   * and our sub-document has an intrinsic size. The frame returned is the
   * frame for the document element of the document we're embedding.
   *
   * Called "Obtain*" and not "Get*" because of comment on GetDocShell that
   * says it should be called ObtainDocShell because of it's side effects.
   */
  nsIFrame* ObtainIntrinsicSizeFrame();

  nsRefPtr<nsFrameLoader> mFrameLoader;
  nsView* mInnerView;
  bool mIsInline;
  bool mPostedReflowCallback;
  bool mDidCreateDoc;
  bool mCallingShow;
};

#endif /* NSSUBDOCUMENTFRAME_H_ */
