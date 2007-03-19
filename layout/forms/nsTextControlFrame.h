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

#ifndef nsTextControlFrame_h___
#define nsTextControlFrame_h___

#include "nsStackFrame.h"
#include "nsAreaFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIEditor.h"
#include "nsITextControlFrame.h"
#include "nsIFontMetrics.h"
#include "nsWeakReference.h" //for service and presshell pointers
#include "nsIScrollableViewProvider.h"
#include "nsIPhonetic.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"

class nsIEditor;
class nsISelectionController;
class nsTextInputSelectionImpl;
class nsTextInputListener;
class nsIDOMCharacterData;
class nsIScrollableView;
#ifdef ACCESSIBILITY
class nsIAccessible;
#endif


class nsTextControlFrame : public nsStackFrame,
                           public nsIAnonymousContentCreator,
                           public nsITextControlFrame,
                           public nsIScrollableViewProvider,
                           public nsIPhonetic

{
public:
  nsTextControlFrame(nsIPresShell* aShell, nsStyleContext* aContext);
  virtual ~nsTextControlFrame();

  virtual void RemovedAsPrimaryFrame(); 

  virtual void Destroy();

  virtual nscoord GetMinWidth(nsIRenderingContext* aRenderingContext);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);
  virtual PRBool IsCollapsed(nsBoxLayoutState& aBoxLayoutState);

  DECL_DO_GLOBAL_REFLOW_COUNT_DSP(nsTextControlFrame, nsStackFrame)

  virtual PRBool IsLeaf() const;
  
#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    aResult.AssignLiteral("nsTextControlFrame");
    return NS_OK;
  }
#endif

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    // nsStackFrame is already both of these, but that's somewhat bogus,
    // and we really mean it.
    return nsStackFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements);
  virtual nsIFrame* CreateFrameFor(nsIContent* aContent);
  virtual void PostCreateFrames();

  // Utility methods to set current widget state
  void SetValue(const nsAString& aValue);
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

//==== BEGIN NSIFORMCONTROLFRAME
  virtual void SetFocus(PRBool aOn , PRBool aRepaint); 
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 


//==== END NSIFORMCONTROLFRAME

//==== NSIGFXTEXTCONTROLFRAME2

  NS_IMETHOD    GetEditor(nsIEditor **aEditor);
  NS_IMETHOD    OwnsValue(PRBool* aOwnsValue);
  NS_IMETHOD    GetValue(nsAString& aValue, PRBool aIgnoreWrap) const;
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength);
  NS_IMETHOD    CheckFireOnChange();
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart);
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd);
  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd);
  NS_IMETHOD    GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);
  virtual nsISelectionController* GetOwnedSelectionController()
    { return mSelCon; }
  virtual nsFrameSelection* GetOwnedFrameSelection()
    { return mFrameSel; }

  // nsIPhonetic
  NS_DECL_NSIPHONETIC

//==== END NSIGFXTEXTCONTROLFRAME2
//==== OVERLOAD of nsIFrame
  virtual nsIAtom* GetType() const;

  /** handler for attribute changes to mContent */
  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  NS_IMETHOD GetText(nsString* aText);

  NS_DECL_ISUPPORTS_INHERITED

public: //for methods who access nsTextControlFrame directly
  /**
   * Find out whether this is a single line text control.  (text or password)
   * @return whether this is a single line text control
   */
  PRBool IsSingleLineTextControl() const;
  /**
   * Find out whether this control is a textarea.
   * @return whether this is a textarea text control
   */
  PRBool IsTextArea() const;
  /**
   * Find out whether this control edits plain text.  (Currently always true.)
   * @return whether this is a plain text control
   */
  PRBool IsPlainTextControl() const;
  /**
   * Find out whether this is a password control (input type=password)
   * @return whether this is a password ontrol
   */
  PRBool IsPasswordTextControl() const;
  void FireOnInput();
  void SetValueChanged(PRBool aValueChanged);
  /** Called when the frame is focused, to remember the value for onChange. */
  nsresult InitFocusedValue();
  nsresult DOMPointToOffset(nsIDOMNode* aNode, PRInt32 aNodeOffset, PRInt32 *aResult);
  nsresult OffsetToDOMPoint(PRInt32 aOffset, nsIDOMNode** aResult, PRInt32* aPosition);

  void SetFireChangeEventState(PRBool aNewState)
  {
    mFireChangeEventState = aNewState;
  };

  PRBool GetFireChangeEventState() const
  {
    return mFireChangeEventState;
  }    

  /* called to free up native keybinding services */
  static NS_HIDDEN_(void) ShutDown();
  
protected:
  /**
   * Find out whether this control is scrollable (i.e. if it is not a single
   * line text control)
   * @return whether this control is scrollable
   */
  PRBool IsScrollable() const;
  /**
   * Initialize mEditor with the proper flags and the default value.
   * @throws NS_ERROR_NOT_INITIALIZED if mEditor has not been created
   * @throws various and sundry other things
   */
  nsresult InitEditor();
  /**
   * Strip all \n, \r and nulls from the given string
   * @param aString the string to remove newlines from [in/out]
   */
  void RemoveNewlines(nsString &aString);
  /**
   * Get the maxlength attribute
   * @param aMaxLength the value of the max length attr
   * @returns PR_FALSE if attr not defined
   */
  PRBool GetMaxLength(PRInt32* aMaxLength);
  /**
   * Find out whether an attribute exists on the content or not.
   * @param aAtt the attribute to determine the existence of
   * @returns PR_FALSE if it does not exist
   */
  PRBool AttributeExists(nsIAtom *aAtt) const
  { return mContent && mContent->HasAttr(kNameSpaceID_None, aAtt); }

  /**
   * We call this when we are being destroyed or removed from the PFM.
   * @param aPresContext the current pres context
   */
  void PreDestroy();
  /**
   * Fire the onChange event.
   */

  // Helper methods
  /**
   * Get the cols attribute (if textarea) or a default
   * @return the number of columns to use
   */
  PRInt32 GetCols();
  /**
   * Get the rows attribute (if textarea) or a default
   * @return the number of rows to use
   */
  PRInt32 GetRows();

  // Compute our intrinsic size.  This does not include any borders, paddings,
  // etc.  Just the size of our actual area for the text (and the scrollbars,
  // for <textarea>).
  nsresult CalcIntrinsicSize(nsIRenderingContext* aRenderingContext,
                             nsSize&              aIntrinsicSize);

  // nsIScrollableViewProvider
  virtual nsIScrollableView* GetScrollableView();

private:
  //helper methods
  nsresult SetSelectionInternal(nsIDOMNode *aStartNode, PRInt32 aStartOffset,
                                nsIDOMNode *aEndNode, PRInt32 aEndOffset);
  nsresult SelectAllContents();
  nsresult SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd);
  
private:
  nsCOMPtr<nsIContent> mAnonymousDiv;

  nsCOMPtr<nsIEditor> mEditor;

  // these packed bools could instead use the high order bits on mState, saving 4 bytes 
  PRPackedBool mUseEditor;
  PRPackedBool mIsProcessing;
  PRPackedBool mNotifyOnInput;//default this to off to stop any notifications until setup is complete
  PRPackedBool mDidPreDestroy; // has PreDestroy been called
  // Calls to SetValue will be treated as user values (i.e. trigger onChange
  // eventually) when mFireChangeEventState==true, this is used by nsFileControlFrame.
  PRPackedBool mFireChangeEventState;

  nsCOMPtr<nsISelectionController> mSelCon;
  nsCOMPtr<nsFrameSelection> mFrameSel;
  nsTextInputListener* mTextListener;
  // XXX This seems unsafe; what's keeping it around?
  nsIScrollableView *mScrollableView;
  nsString mFocusedValue;

#ifdef DEBUG
  PRBool mCreateFrameForCalled;
#endif
};

#endif


