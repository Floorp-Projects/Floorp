/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsFormFrame_h___
#define nsFormFrame_h___

#include "nsIFormManager.h"
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

class nsFormFrame : public nsBlockFrame, 
                    public nsIFormManager
{
public:
  nsFormFrame();
  virtual ~nsFormFrame();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  // nsIFormManager

  NS_IMETHOD OnReset(nsIPresContext* aPresContext);

  NS_IMETHOD OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame);

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

  NS_IMETHOD GetMethod(PRInt32* aMethod);
  NS_IMETHOD GetEnctype(PRInt32* aEnctype);
  NS_IMETHOD GetTarget(nsString* aTarget);
  NS_IMETHOD GetAction(nsString* aAction);

  // Detection of first form to notify observers
  static PRBool gFirstFormSubmitted;
  // Detection of first password field to notify any password manager
  // style modules
  static PRBool gInitPasswordManager;

  // static helper functions for nsIFormControls
  
  static PRBool GetDisabled(nsIFrame* aChildFrame, nsIContent* aContent = 0);
  static PRBool GetReadonly(nsIFrame* aChildFrame, nsIContent* aContent = 0);
  static nsresult GetName(nsIFrame* aChildFrame, nsString& aName, nsIContent* aContent = 0);
  static nsresult GetValue(nsIFrame* aChildFrame, nsString& aValue, nsIContent* aContent = 0);
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

  nsresult ProcessValue(nsIFormProcessor& aFormProcessor, nsIFormControlFrame* aFrameControl, const nsString& aName, nsString& aNewValue);
  nsresult ProcessAsURLEncoded(nsIFormProcessor* aFormProcessor, PRBool aIsPost, nsString& aData, nsIFormControlFrame* aFrame);
  nsresult ProcessAsMultipart(nsIFormProcessor* aFormProcessor,
                              nsIFile** aMultipartDataFile,
                              nsIFormControlFrame* aFrame);
  PRUint32 GetFileNameWithinPath(nsString aPathName);
  nsresult GetContentType(char* aPathName, char** aContentType);
  
  // i18n helper routines
  nsString* URLEncode(const nsString& aString, nsIUnicodeEncoder* encoder);
  char* UnicodeToNewBytes(const PRUnichar* aSrc, PRUint32 aLen, nsIUnicodeEncoder* encoder);
  
  NS_IMETHOD GetEncoder(nsIUnicodeEncoder** encoder);
  NS_IMETHOD GetPlatformEncoder(nsIUnicodeEncoder** encoder);
  void GetSubmitCharset(nsString& oCharset);

  nsVoidArray          mFormControls;
  nsVoidArray          mRadioGroups;
#ifdef IBMBIDI
//ahmed
  nsAutoString         mCharset;            // The charset in use
  PRUint8              mTextDir;            // The direction of the text 
  PRUint8              mCtrlsModAtSubmit;   // The text mode of the control (logical/visual)
                                            // Direction and text mode are set by Bidi Options in Preferences
#endif
};

#endif // nsFormFrame_h___
