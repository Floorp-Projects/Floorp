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
  explicit nsFormControlFrame(nsStyleContext*);

  virtual nsIAtom* GetType() const override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsLeafFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  NS_DECL_QUERYFRAME
  NS_DECL_ABSTRACT_FRAME(nsFormControlFrame)

  /** 
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  virtual nsresult HandleEvent(nsPresContext* aPresContext, 
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode)
    const override;

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  virtual void Reflow(nsPresContext*      aCX,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&      aStatus) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  // new behavior

  virtual void SetFocus(bool aOn = true, bool aRepaint = false) override;

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) override;

  // AccessKey Helper function
  static nsresult RegUnRegAccessKey(nsIFrame * aFrame, bool aDoReg);

  /**
   * Returns the usable screen rect in app units, eg the rect where we can
   * draw dropdowns.
   */
  static nsRect GetUsableScreenRect(nsPresContext* aPresContext);

protected:

  virtual ~nsFormControlFrame();

  virtual nscoord GetIntrinsicISize() override;
  virtual nscoord GetIntrinsicBSize() override;

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

