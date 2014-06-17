/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFormControlFrame_h___
#define nsFormControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsIFormControlFrame.h"
#include "nsLeafFrame.h"

/** 
 * nsFormControlFrame is the base class for radio buttons and
 * checkboxes.  It also has two static methods (RegUnRegAccessKey and
 * GetScreenHeight) that are used by other form controls.
 */
class nsFormControlFrame : public nsLeafFrame,
                           public nsIFormControlFrame
{
public:
  /**
    * Main constructor
    * @param aContent the content representing this frame
    * @param aParentFrame the parent frame
    */
  nsFormControlFrame(nsStyleContext*);

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsLeafFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  /** 
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  virtual nsresult HandleEvent(nsPresContext* aPresContext, 
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode)
    const MOZ_OVERRIDE;

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  virtual void Reflow(nsPresContext*      aCX,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&      aStatus) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  // new behavior

  virtual void SetFocus(bool aOn = true, bool aRepaint = false) MOZ_OVERRIDE;

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) MOZ_OVERRIDE;

  // AccessKey Helper function
  static nsresult RegUnRegAccessKey(nsIFrame * aFrame, bool aDoReg);

  /**
   * Returns the usable screen rect in app units, eg the rect where we can
   * draw dropdowns.
   */
  static nsRect GetUsableScreenRect(nsPresContext* aPresContext);

protected:

  virtual ~nsFormControlFrame();

  virtual nscoord GetIntrinsicWidth() MOZ_OVERRIDE;
  virtual nscoord GetIntrinsicHeight() MOZ_OVERRIDE;

//
//-------------------------------------------------------------------------------------
//  Utility methods for managing checkboxes and radiobuttons
//-------------------------------------------------------------------------------------
//   
   /**
    * Get the state of the checked attribute.
    * @param aState set to true if the checked attribute is set,
    * false if the checked attribute has been removed
    */

  void GetCurrentCheckState(bool* aState);
};

#endif

