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

#ifndef nsTextEditRules_h__
#define nsTextEditRules_h__

#include "nsCOMPtr.h"

#include "nsPlaintextEditor.h"
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
  NS_IMETHOD Init(nsPlaintextEditor *aEditor, PRUint32 aFlags);
  NS_IMETHOD BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD WillDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, PRBool *aCancel, PRBool *aHandled);
  NS_IMETHOD DidDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
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
  
public:
  nsresult ResetIMETextPWBuf();

protected:

  // nsTextEditRules implementation methods
  nsresult WillInsertText(  PRInt32          aAction,
                            nsISelection *aSelection, 
                            PRBool          *aCancel,
                            PRBool          *aHandled,
                            const nsAReadableString  *inString,
                            nsAWritableString        *outString,
                            PRInt32          aMaxLength);
  nsresult DidInsertText(nsISelection *aSelection, nsresult aResult);
  nsresult GetTopEnclosingPre(nsIDOMNode *aNode, nsIDOMNode** aOutPreNode);

  nsresult WillInsertBreak(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidInsertBreak(nsISelection *aSelection, nsresult aResult);

  nsresult WillInsert(nsISelection *aSelection, PRBool *aCancel);
  nsresult DidInsert(nsISelection *aSelection, nsresult aResult);

  nsresult WillDeleteSelection(nsISelection *aSelection, 
                               nsIEditor::EDirection aCollapsedAction, 
                               PRBool *aCancel,
                               PRBool *aHandled);
  nsresult DidDeleteSelection(nsISelection *aSelection, 
                              nsIEditor::EDirection aCollapsedAction, 
                              nsresult aResult);

  nsresult WillSetTextProperty(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidSetTextProperty(nsISelection *aSelection, nsresult aResult);

  nsresult WillRemoveTextProperty(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidRemoveTextProperty(nsISelection *aSelection, nsresult aResult);

  nsresult WillUndo(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidUndo(nsISelection *aSelection, nsresult aResult);

  nsresult WillRedo(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidRedo(nsISelection *aSelection, nsresult aResult);

  /** called prior to nsIEditor::OutputToString
    * @param aSelection
    * @param aInFormat  the format requested for the output, a MIME type
    * @param aOutText   the string to use for output, if aCancel is set to true
    * @param aOutCancel if set to PR_TRUE, the caller should cancel the operation
    *                   and use aOutText as the result.
    */
  nsresult WillOutputText(nsISelection *aSelection,
                          const nsAReadableString  *aInFormat,
                          nsAWritableString *aOutText, 
                          PRBool   *aOutCancel, 
                          PRBool *aHandled);

  nsresult DidOutputText(nsISelection *aSelection, nsresult aResult);


  // helper functions
  
  /** replaces newllines with breaks, if needed.  acts on doc portion in aRange */
  nsresult ReplaceNewlines(nsIDOMRange *aRange);
  
  /** creates a bogus text node if the document has no editable content */
  nsresult CreateBogusNodeIfNeeded(nsISelection *aSelection);

  /** returns a truncated insertion string if insertion would place us
      over aMaxLength */
  nsresult TruncateInsertionIfNeeded(nsISelection             *aSelection, 
                                     const nsAReadableString  *aInString,
                                     nsAWritableString        *aOutString,
                                     PRInt32                   aMaxLength);
  
  /** Echo's the insertion text into the password buffer, and converts
      insertion text to '*'s */                                        
  nsresult EchoInsertionToPWBuff(PRInt32 aStart, PRInt32 aEnd, nsAWritableString *aOutString);

  /** Remove IME composition text from password buffer */
  nsresult RemoveIMETextFromPWBuf(PRInt32 &aStart, nsAWritableString *aIMEString);

  nsresult CreateMozBR(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outBRNode);

  PRBool DeleteEmptyTextNode(nsIDOMNode *aNode);

#ifdef IBMBIDI
  nsresult CheckBidiLevelForDeletion(nsIDOMNode           *aSelNode, 
                                     PRInt32               aSelOffset, 
                                     nsIEditor::EDirection aAction,
                                     PRBool               *aCancel);
#endif // IBMBIDI

  // data members
  nsPlaintextEditor   *mEditor;        // note that we do not refcount the editor
  nsString             mPasswordText;  // a buffer we use to store the real value of password editors
  nsString             mPasswordIMEText;  // a buffer we use to track the IME composition string
  PRInt32              mPasswordIMEIndex;
  nsCOMPtr<nsIDOMNode> mBogusNode;     // magic node acts as placeholder in empty doc
  nsCOMPtr<nsIDOMNode> mBody;          // cached root node
  PRUint32             mFlags;
  PRUint32             mActionNesting;
  PRBool               mLockRulesSniffing;
  PRInt32              mTheAction;     // the top level editor action
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
  const nsAReadableString *inString;
  nsAWritableString *outString;
  const nsAReadableString *outputFormat;
  PRInt32 maxLength;
  
  // kDeleteSelection
  nsIEditor::EDirection collapsedAction;
  
  // kMakeList
  PRBool bOrdered;
  PRBool entireList;

  // kAlign
  const nsAReadableString *alignType;
  
  // kMakeBasicBlock
  const nsAReadableString *blockType;
  
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

