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
#ifndef nsFormFrame_h___
#define nsFormFrame_h___

#include "nsBlockFrame.h"
#include "nsVoidArray.h"
#include "nsIFileSpec.h"
#include "nsIFormProcessor.h"

class  nsString;
class  nsIContent;
class  nsIFrame;
class  nsIPresContext;
struct nsHTMLReflowState;
class  nsFormControlFrame;
class  nsGfxRadioControlFrame;
class  nsIFormControlFrame;
class  nsIDOMHTMLFormElement;
class nsIDocument;
class nsIPresContext;
class nsFormFrame;
class nsIUnicodeEncoder;
class nsRadioControlGroup;

class nsFormFrame : public nsBlockFrame
{
public:
  nsFormFrame();
  virtual ~nsFormFrame();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  // other methods 

  //--------------------------------------------------------
  // returns NS_ERROR_FAILURE if the radiobtn doesn't have a group
  // returns NS_OK is it did have a radio group 
  //--------------------------------------------------------
  nsresult OnRadioChecked(nsIPresContext* aPresContext, nsGfxRadioControlFrame& aRadio, PRBool aChecked = PR_TRUE); 
    
  void AddFormControlFrame(nsIPresContext* aPresContext, nsIFormControlFrame& aFrame);
  static void AddFormControlFrame(nsIPresContext* aPresContext, nsIFrame& aFrame);
  void RemoveFormControlFrame(nsIFormControlFrame& aFrame);
  void RemoveRadioControlFrame(nsIFormControlFrame * aFrame);
  nsresult GetRadioInfo(nsIFormControlFrame* aFrame, nsString& aName, nsRadioControlGroup *& aGroup);

  // static helper functions for nsIFormControls
  
  static PRBool GetDisabled(nsIFrame* aChildFrame, nsIContent* aContent = 0);
  static PRBool GetReadonly(nsIFrame* aChildFrame, nsIContent* aContent = 0);
  static nsresult GetName(nsIFrame* aChildFrame,
                          nsAString& aName,
                          nsIContent* aContent = 0);
  static nsresult GetValue(nsIFrame* aChildFrame,
                           nsAString& aValue,
                           nsIContent* aContent = 0);
  static void StyleChangeReflow(nsIPresContext* aPresContext,
                                nsIFrame* aFrame);
  static nsresult GetRadioGroupSelectedContent(nsGfxRadioControlFrame* aControl,
                                               nsIContent **           aRadiobtn);
  void SetFlags(PRUint32 aFlags) {
    mState &= ~NS_BLOCK_FLAGS_MASK;
    mState |= aFlags;
  }

  // helper function
  nsIFrame* GetFirstSubmitButtonAndTxtCnt(PRInt32& aInputTxtCnt);

protected:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  void RemoveRadioGroups();

  void DoDefaultSelection(nsIPresContext*          aPresContext, 
                          nsRadioControlGroup *    aGroup,
                          nsGfxRadioControlFrame * aRadioToIgnore = nsnull);

  nsVoidArray          mFormControls;
  nsVoidArray          mRadioGroups;

  static PRBool gInitPasswordManager;
};

#endif // nsFormFrame_h___
