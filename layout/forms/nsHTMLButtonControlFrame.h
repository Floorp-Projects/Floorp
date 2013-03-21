/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLButtonControlFrame_h___
#define nsHTMLButtonControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsHTMLParts.h"

#include "nsStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsISupports.h"
#include "nsStyleConsts.h"
#include "nsIComponentManager.h"
#include "nsButtonFrameRenderer.h"

class nsRenderingContext;
class nsPresContext;

class nsHTMLButtonControlFrame : public nsContainerFrame,
                                 public nsIFormControlFrame 
{
public:
  nsHTMLButtonControlFrame(nsStyleContext* aContext);
  ~nsHTMLButtonControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        asPrevInFlow) MOZ_OVERRIDE;

  virtual nsStyleContext* GetAdditionalStyleContext(int32_t aIndex) const;
  virtual void SetAdditionalStyleContext(int32_t aIndex, 
                                         nsStyleContext* aStyleContext) MOZ_OVERRIDE;
 
  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame) MOZ_OVERRIDE;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("HTMLButtonControl"), aResult);
  }
#endif

  virtual bool HonorPrintBackgroundSettings() { return false; }

  // nsIFormControlFrame
  void SetFocus(bool aOn, bool aRepaint);
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) MOZ_OVERRIDE;
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const MOZ_OVERRIDE; 

  // Inserted child content gets its frames parented by our child block
  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

protected:
  virtual bool IsInput() { return false; }
  void ReflowButtonContents(nsPresContext* aPresContext,
                            nsHTMLReflowMetrics& aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsIFrame* aFirstKid,
                            nsMargin aFocusPadding,
                            nsReflowStatus& aStatus);

  nsButtonFrameRenderer mRenderer;
};

#endif




