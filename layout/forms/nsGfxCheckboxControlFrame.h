/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsGfxCheckboxControlFrame_h___
#define nsGfxCheckboxControlFrame_h___

#include "nsFormControlFrame.h"
#include "nsIStatefulFrame.h"
#include "nsICheckboxControlFrame.h"

#ifdef ACCESSIBILITY
class nsIAccessible;
#endif


#define NS_GFX_CHECKBOX_CONTROL_FRAME_FACE_CONTEXT_INDEX   0 // for additional style contexts
#define NS_GFX_CHECKBOX_CONTROL_FRAME_LAST_CONTEXT_INDEX   0

class nsGfxCheckboxControlFrame : public nsFormControlFrame,
                                  public nsIStatefulFrame,
                                  public nsICheckboxControlFrame//,
                                  //public nsIAccessible
{
public:

  //NS_DECL_NSIACCESSIBLE

  nsGfxCheckboxControlFrame();
  virtual ~nsGfxCheckboxControlFrame();
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("CheckboxControl", aResult);
  }
#endif
   // this should be protected, but VC6 is lame.
  enum CheckState { eOff, eOn, eMixed } ;

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow) ;

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType, 
                              PRInt32         aHint) ;

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif


   //nsICheckboxControlFrame methods
  NS_IMETHOD SetCheckboxFaceStyleContext(nsIStyleContext *aCheckboxFaceStyleContext);

  void InitializeControl(nsIPresContext* aPresContext);

  NS_IMETHOD GetAdditionalStyleContext(PRInt32 aIndex, 
                                       nsIStyleContext** aStyleContext) const;
  NS_IMETHOD SetAdditionalStyleContext(PRInt32 aIndex, 
                                       nsIStyleContext* aStyleContext);

    // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 
  virtual void Reset(nsIPresContext* aPresContext);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

   // nsIStatefulFrame
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);


#ifdef DEBUG_rodsXXX
  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
#endif

  NS_IMETHOD GetChecked(PRBool* aIsChecked) { *aIsChecked = (GetCheckboxState() == eOn); return NS_OK; }
  NS_IMETHOD SetChecked(nsIPresContext* aPresContext, PRBool aIsChecked) { SetCheckboxState(aPresContext, aIsChecked ? eOn : eOff); return NS_OK; }

protected:

    // native/gfx implementations need to implement needs.
  CheckState GetCheckboxState();
  void SetCheckboxState(nsIPresContext* aPresContext, CheckState aValue);

   // Utility methods for implementing SetProperty/GetProperty
  void SetCheckboxControlFrameState(nsIPresContext* aPresContext,
                                    const nsAReadableString& aValue);
  void GetCheckboxControlFrameState(nsAWritableString& aValue);  

    // utility routine for converting from DOM values to internal enum
  void CheckStateToString ( CheckState inState, nsAWritableString& outStateAsString ) ;
  CheckState StringToCheckState ( const nsAReadableString & aStateAsString ) ;

    // figure out if we're a tri-state checkbox.
  PRBool IsTristateCheckbox ( ) const { return mIsTristate; }

    // we just became a tri-state, or we just lost tri-state status. fix up
    // the attributes for the new mode.
  void SwitchModesWithEmergencyBrake ( nsIPresContext* aPresContext,
                                       PRBool inIsNowTristate ) ;
  
    // for tri-state checkbox. meaningless for normal HTML
  PRBool mIsTristate;
  
  static nsIAtom* GetTristateAtom() ;
  static nsIAtom* GetTristateValueAtom() ;

protected:

  virtual void PaintCheckBox(nsIPresContext* aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer);
  virtual void PaintMixedMark(nsIRenderingContext& aRenderingContext,
                               float aPixelsToTwips, const nsRect& aRect) ;

  //GFX-rendered state variables
  PRBool           mInClickEvent;
  CheckState       mChecked;
  nsIStyleContext* mCheckButtonFaceStyle;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
 
};

#endif
