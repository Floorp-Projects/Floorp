/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsFormFrame_h___
#define nsFormFrame_h___

#include "nsIFormManager.h"
#include "nsLeafFrame.h"
#include "nsVoidArray.h"

class  nsString;
class  nsIContent;
class  nsIFrame;
class  nsIPresContext;
struct nsHTMLReflowState;
class  nsFormControlFrame;
class  nsRadioControlFrame;
class  nsIFormControlFrame;
class  nsIDOMHTMLFormElement;
class nsIDocument;
class nsIPresContext;
class nsFormFrame;

// XXX these structs and gFormFrameTable below provide a faster way to get from a form content to
// the appropriate frame. Before replacing this mechanism with FindFrameWithContent, please test
// a page with thousands of frames and hundreds of form controls.
struct nsFormFrameTableEntry 
{
  nsIPresContext*        mPresContext;
  nsIDOMHTMLFormElement* mFormElement;
  nsFormFrame*           mFormFrame;
  nsFormFrameTableEntry(nsIPresContext&        aPresContext, 
                        nsIDOMHTMLFormElement& aFormElement,
                        nsFormFrame&           aFormFrame);
  ~nsFormFrameTableEntry();
};
struct nsFormFrameTable
{
  nsVoidArray mEntries;
  nsFormFrameTable() {}
  ~nsFormFrameTable();
  void Put(nsIPresContext& aPresContext, nsIDOMHTMLFormElement& aFormElem, nsFormFrame& aFormFrame);  
  nsFormFrame* Get(nsIPresContext& aPresContext, nsIDOMHTMLFormElement& aFormElem);
  void Remove(nsFormFrame& aFormFrame);
};

class nsFormFrame : public nsLeafFrame, 
                    public nsIFormManager
{
public:
  nsFormFrame();

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);
  virtual ~nsFormFrame();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIFormManager

  NS_IMETHOD OnReset();

  NS_IMETHOD OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame);

  // other methods 

  void OnRadioChecked(nsRadioControlFrame& aRadio, PRBool aChecked = PR_TRUE); 
    
  void AddFormControlFrame(nsIFormControlFrame& aFrame);
  static void AddFormControlFrame(nsIPresContext& aPresContext, nsIFrame& aFrame);

  PRBool CanSubmit(nsFormControlFrame& aFrame);

  NS_IMETHOD GetMethod(PRInt32* aMethod);
  NS_IMETHOD GetEnctype(PRInt32* aEnctype);
  NS_IMETHOD GetTarget(nsString* aTarget);
  NS_IMETHOD GetAction(nsString* aAction);

  static nsFormFrame* GetFormFrame(nsIPresContext& aPresContext,
                                   nsIDOMHTMLFormElement& aFormElem);

  static void PutFormFrame(nsIPresContext& aPresContext,
                           nsIDOMHTMLFormElement& aFormElem, 
                           nsFormFrame& aFrame);

  static void RemoveFormFrame(nsFormFrame& aFrame);

  // static helper functions for nsIFormControls
  
  static PRBool GetDisabled(nsIFrame* aChildFrame, nsIContent* aContent = 0);
  static PRBool GetReadonly(nsIFrame* aChildFrame, nsIContent* aContent = 0);
  static nsresult GetName(nsIFrame* aChildFrame, nsString& aName, nsIContent* aContent = 0);
  static nsresult GetValue(nsIFrame* aChildFrame, nsString& aValue, nsIContent* aContent = 0);
  static void StyleChangeReflow(nsIPresContext* aPresContext,
                                nsIFrame* aFrame);

protected:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);
  void RemoveRadioGroups();
  void ProcessAsURLEncoded(PRBool aIsPost, nsString& aData, nsIFormControlFrame* aFrame);
  void ProcessAsMultipart(nsString& aData, nsIFormControlFrame* aFrame);
  static const char* GetFileNameWithinPath(char* aPathName);

  // the following are temporary until nspr and/or netlib provide them
  static PRBool Temp_GetTempDir(char* aTempDirName);
  static char* Temp_GenerateTempFileName(PRInt32 aMaxSize, char* aBuffer);
  static void  Temp_GetContentType(char* aPathName, char* aContentType);

  // XXX Hack to get document from parent html frame.  Should Document do this?
  nsIDocument* GetParentHTMLFrameDocument(nsIDocument* doc);

  nsVoidArray          mFormControls;
  nsVoidArray          mRadioGroups;
  nsIFormControlFrame* mTextSubmitter;
};

#endif // nsFormFrame_h___
