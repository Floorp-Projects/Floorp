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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#define NS_EDITORCONTROLLER_CID \
{ 0x26fb965c, 0x9de6, 0x11d3, { 0xbc, 0xcc, 0x0, 0x60, 0xb0, 0xfc, 0x76, 0xbd } }

#include "nsIController.h"
#include "nsIEditorController.h"

#include "nsString2.h"
#include "nsCOMPtr.h"

class nsIHTMLContent;
class nsIGfxTextControlFrame;
class nsIEditor;
class nsISelectionController;

class nsEditorController : public nsIController ,public nsIEditorController
{
public:
  nsEditorController();
  virtual ~nsEditorController();

  // nsISupports
  NS_DECL_ISUPPORTS
    
  // nsIController
  NS_DECL_NSICONTROLLER

  /** set the content for this controller instance */
  NS_IMETHOD SetContent(nsIHTMLContent *aContent);

  /** set the editor for this controller. Mutually exclusive with setContent*/
  NS_IMETHOD SetEditor(nsIEditor *aEditor);


protected:

  /** fetch the primary frame associated with mContent */
  NS_IMETHOD GetFrame(nsIGfxTextControlFrame **aFrame);

  /** fetch the editor associated with mContent */
  NS_IMETHOD GetEditor(nsIEditor ** aEditor);

  NS_IMETHOD GetSelectionController(nsISelectionController ** aSelCon);
  /** return PR_TRUE if the editor associated with mContent is enabled */
  PRBool IsEnabled();

    // XXX: should be static
	nsString mUndoString;
  nsString mRedoString;
  nsString mCutString;
  nsString mCopyString;
  nsString mPasteString;
  nsString mDeleteString;
  nsString mSelectAllString;

  nsString mBeginLineString;
  nsString mEndLineString  ;
  nsString mSelectBeginLineString;
  nsString mSelectEndLineString  ;

  nsString mScrollTopString;
  nsString mScrollBottomString;

  nsString mMoveTopString ;
  nsString mMoveBottomString;
  nsString mSelectMoveTopString;
  nsString mSelectMoveBottomString;

  nsString mLineNextString;
  nsString mLinePreviousString;
  nsString mSelectLineNextString;
  nsString mSelectLinePreviousString;

  nsString mLeftString;
  nsString mRightString;
  nsString mSelectLeftString;
  nsString mSelectRightString;

  nsString mWordLeftString;
  nsString mWordRightString;
  nsString mSelectWordLeftString;
  nsString mSelectWordRightString;

  nsString mScrollPageUp;
  nsString mScrollPageDown;
  nsString mScrollLineUp;
  nsString mScrollLineDown;

  nsString mMovePageUp;
  nsString mMovePageDown;
  nsString mSelectMovePageUp;
  nsString mSelectMovePageDown;

  nsString mDeleteCharBackward;
  nsString mDeleteCharForward;
  nsString mDeleteWordBackward;
  nsString mDeleteWordForward;
  nsString mDeleteToBeginningOfLine;
  nsString mDeleteToEndOfLine;

protected:
   nsIHTMLContent* mContent;    // weak reference, the content object owns this object

   //if editor is null then look to mContent. this is for dual use of window and content
   //attached controller.
   nsIEditor *mEditor;
};
