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

#ifndef nsTextEditRules_h__
#define nsTextEditRules_h__

#include "nsCOMPtr.h"

#include "nsHTMLEditor.h"
#include "nsIDOMNode.h"

#include "nsEditRules.h"

/** Object that encapsulates HTML text-specific editing rules.
  *  
  * To be a good citizen, edit rules must live by these restrictions:
  * 1. All data manipulation is through the editor.  
  *    Content nodes in the document tree must <B>not</B> be manipulated directly.
  *    Content nodes in document fragments that are not part of the document itself
  *    may be manipulated at will.  Operations on document fragments must <B>not</B>
  *    go through the editor.
  * 2. Selection must not be explicitly set by the rule method.  
  *    Any manipulation of Selection must be done by the editor.
  */
class nsTextEditRules : public nsIEditRules
{
public:
  NS_DECL_ISUPPORTS
  
              nsTextEditRules();
  virtual     ~nsTextEditRules();

  // nsIEditRules methods
  NS_IMETHOD Init(nsHTMLEditor *aEditor, PRUint32 aFlags);
  NS_IMETHOD BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD WillDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, PRBool *aCancel, PRBool *aHandled);
  NS_IMETHOD DidDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  NS_IMETHOD GetFlags(PRUint32 *aFlags);
  NS_IMETHOD SetFlags(PRUint32 aFlags);
  NS_IMETHOD DocumentIsEmpty(PRBool *aDocumentIsEmpty);

  // nsTextEditRules action id's
  enum 
  {
    kDefault             = 0,
    // any editor that has a txn mgr
    kUndo                = 1000,
    kRedo                = 1001,
    // text actions
    kInsertText          = 2000,
    kInsertTextIME       = 2001,
    kDeleteSelection     = 2002,
    kSetTextProperty     = 2003,
    kRemoveTextProperty  = 2004,
    kOutputText          = 2005,
    // html only action
    kInsertBreak         = 3000,
    kMakeList            = 3001,
    kIndent              = 3002,
    kOutdent             = 3003,
    kAlign               = 3004,
    kMakeBasicBlock      = 3005,
    kRemoveList          = 3006,
    kMakeDefListItem     = 3007,
    kInsertElement       = 3008
  };
  
protected:

  // nsTextEditRules implementation methods
  nsresult WillInsertText(  PRInt32          aAction,
                            nsIDOMSelection *aSelection, 
                            PRBool          *aCancel,
                            PRBool          *aHandled,
                            const nsString  *inString,
                            nsString        *outString,
                            PRInt32          aMaxLength);
  nsresult DidInsertText(nsIDOMSelection *aSelection, nsresult aResult);
  nsresult GetTopEnclosingPre(nsIDOMNode *aNode, nsIDOMNode** aOutPreNode);

  nsresult WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidInsert(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillDeleteSelection(nsIDOMSelection *aSelection, 
                               nsIEditor::EDirection aCollapsedAction, 
                               PRBool *aCancel,
                               PRBool *aHandled);
  nsresult DidDeleteSelection(nsIDOMSelection *aSelection, 
                              nsIEditor::EDirection aCollapsedAction, 
                              nsresult aResult);

  nsresult WillSetTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidSetTextProperty(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillRemoveTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidRemoveTextProperty(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillUndo(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidUndo(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillRedo(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidRedo(nsIDOMSelection *aSelection, nsresult aResult);

  /** called prior to nsIEditor::OutputToString
    * @param aSelection
    * @param aInFormat  the format requested for the output, a MIME type
    * @param aOutText   the string to use for output, if aCancel is set to true
    * @param aOutCancel if set to PR_TRUE, the caller should cancel the operation
    *                   and use aOutText as the result.
    */
  nsresult WillOutputText(nsIDOMSelection *aSelection,
                          const nsString  *aInFormat,
                          nsString *aOutText, 
                          PRBool   *aOutCancel, 
                          PRBool *aHandled);

  nsresult DidOutputText(nsIDOMSelection *aSelection, nsresult aResult);


  // helper functions
  
  /** replaces newllines with breaks, if needed.  acts on doc portion in aRange */
  nsresult ReplaceNewlines(nsIDOMRange *aRange);
  
  /** creates a bogus text node if the document has no editable content */
  nsresult CreateBogusNodeIfNeeded(nsIDOMSelection *aSelection);

  /** returns a truncated insertion string if insertion would place us
      over aMaxLength */
  nsresult TruncateInsertionIfNeeded(nsIDOMSelection *aSelection, 
                                           const nsString  *aInString,
                                           nsString        *aOutString,
                                           PRInt32          aMaxLength);
  
  /** Echo's the insertion text into the password buffer, and converts
      insertion text to '*'s */                                        
  nsresult EchoInsertionToPWBuff(PRInt32 aStart, PRInt32 aEnd, nsString *aOutString);

  nsresult CreateMozBR(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outBRNode);

  PRBool DeleteEmptyTextNode(nsIDOMNode *aNode);

  nsresult AdjustSelection(nsIDOMSelection *aSelection, nsIEditor::EDirection aDirection);
  
  // data members
  nsHTMLEditor *mEditor;  // note that we do not refcount the editor
  nsString      mPasswordText;  // a buffer we use to store the real value of password editors
  nsCOMPtr<nsIDOMNode> mBogusNode;  // magic node acts as placeholder in empty doc
  nsCOMPtr<nsIDOMNode> mBody;    // cached root node
  PRUint32 mFlags;
  PRUint32 mActionNesting;
  PRBool   mLockRulesSniffing;
  PRInt32  mTheAction;    // the top level editor action
  // friends
  friend class nsAutoLockRulesSniffing;

};



class nsTextRulesInfo : public nsRulesInfo
{
 public:
 
  nsTextRulesInfo(int aAction) : 
    nsRulesInfo(aAction),
    inString(0),
    outString(0),
    outputFormat(0),
    maxLength(-1),
    collapsedAction(nsIEditor::eNext),
    bOrdered(PR_FALSE),
    entireList(PR_FALSE),
    alignType(0),
    blockType(0),
    insertElement(0)
    {};

  virtual ~nsTextRulesInfo() {};
  
  // kInsertText
  const nsString *inString;
  nsString *outString;
  const nsString *outputFormat;
  PRInt32 maxLength;
  
  // kDeleteSelection
  nsIEditor::EDirection collapsedAction;
  
  // kMakeList
  PRBool bOrdered;
  PRBool entireList;

  // kAlign
  const nsString *alignType;
  
  // kMakeBasicBlock
  const nsString *blockType;
  
  // kInsertElement
  const nsIDOMElement* insertElement;
};


/***************************************************************************
 * stack based helper class for StartOperation()/EndOperation() sandwich.
 * this class sets a bool letting us know to ignore any rules sniffing
 * that tries to occur reentrantly. 
 */
class nsAutoLockRulesSniffing
{
  public:
  
  nsAutoLockRulesSniffing(nsTextEditRules *rules) : mRules(rules) 
                 {if (mRules) mRules->mLockRulesSniffing = PR_TRUE;}
  ~nsAutoLockRulesSniffing() 
                 {if (mRules) mRules->mLockRulesSniffing = PR_FALSE;}
  
  protected:
  nsTextEditRules *mRules;
};



/***************************************************************************
 * stack based helper class for turning on/off the edit listener.
 */
class nsAutoLockListener
{
  public:
  
  nsAutoLockListener(PRBool *enabled) : mEnabled(enabled)
                 {if (mEnabled) { mOldState=*mEnabled; *mEnabled = PR_FALSE;}}
  ~nsAutoLockListener() 
                 {if (mEnabled) *mEnabled = mOldState;}
  
  protected:
  PRBool *mEnabled;
  PRBool mOldState;
};


nsresult NS_NewTextEditRules(nsIEditRules** aInstancePtrResult);

#endif //nsTextEditRules_h__

