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

#ifndef nsTextEditRules_h__
#define nsTextEditRules_h__

#include "nsCOMPtr.h"

#include "nsHTMLEditor.h"
#include "nsIDOMNode.h"

#include "nsEditRules.h"
#include "TypeInState.h"

class PlaceholderTxn;
class nsTextEditor;

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
class nsTextEditRules : public nsEditRules
{
public:

              nsTextEditRules();
  virtual     ~nsTextEditRules();

  // nsEditRules methods
  NS_IMETHOD Init(nsHTMLEditor *aEditor, PRUint32 aFlags);
  NS_IMETHOD WillDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, PRBool *aCancel);
  NS_IMETHOD DidDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  NS_IMETHOD GetFlags(PRUint32 *aFlags);
  NS_IMETHOD SetFlags(PRUint32 aFlags);
  NS_IMETHOD DocumentIsEmpty(PRBool *aDocumentIsEmpty);

  // nsTextEditRules action id's
  enum 
  {
    // any editor that has a txn mgr
    kUndo            = 1000,
    kRedo            = 1001,
    // text actions
    kInsertText      = 2000,
    kDeleteSelection = 2001,
    kSetTextProperty = 2003,
    kRemoveTextProperty = 2004,
    kOutputText      = 2005,
    // html only action
    kInsertBreak     = 3000,
    kMakeList        = 3001,
    kIndent          = 3002,
    kOutdent         = 3003,
    kAlign           = 3004,
    kMakeBasicBlock  = 3005,
    kRemoveList      = 3006,
    kInsertElement   = 3008
  };
  
protected:

  // nsTextEditRules implementation methods
  nsresult WillInsertText(nsIDOMSelection  *aSelection, 
                            PRBool         *aCancel,
                            PlaceholderTxn **aTxn,
                            const nsString *inString,
                            nsString       *outString,
                            TypeInState    typeInState,
                            PRInt32         aMaxLength);
  nsresult DidInsertText(nsIDOMSelection *aSelection, nsresult aResult);
  nsresult CreateStyleForInsertText(nsIDOMSelection *aSelection, TypeInState &aTypeInState);

  nsresult WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidInsert(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillDeleteSelection(nsIDOMSelection *aSelection, 
                               nsIEditor::ESelectionCollapseDirection aCollapsedAction, 
                               PRBool *aCancel);
  nsresult DidDeleteSelection(nsIDOMSelection *aSelection, 
                              nsIEditor::ESelectionCollapseDirection aCollapsedAction, 
                              nsresult aResult);

  nsresult WillSetTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidSetTextProperty(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillRemoveTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidRemoveTextProperty(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillUndo(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidUndo(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillRedo(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidRedo(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillOutputText(nsIDOMSelection *aSelection, nsString *aOutText, PRBool *aCancel);
  nsresult DidOutputText(nsIDOMSelection *aSelection, nsresult aResult);


  // helper functions
  
  /** insert aNode into a new style node of type aTag.
    * aSelection is optional.  If provided, aSelection is set to (aNode, 0)
    * if aNode was successfully placed in a new style node
    * @param aNewStyleNode   [OUT] The newly created style node, if result is successful
    *                              undefined if result is a failure.
    */
  nsresult InsertStyleNode(nsIDOMNode      *aNode, 
                           nsIAtom         *aTag, 
                           nsIDOMSelection *aSelection,
                           nsIDOMNode     **aNewStyleNode);

  /** inserts a new <FONT> node and sets the aAttr attribute to aValue */
  nsresult CreateFontStyleForInsertText(nsIDOMNode      *aNewTextNode,
                                        const nsString  &aAttr, 
                                        const nsString  &aValue, 
                                        nsIDOMSelection *aSelection);

  /** create a new style node of type aTag in aParentNode, and put a new text node 
    * in the new style node.  
    * If aSelection is provided and points to a text node, just call InsertStyleNode instead.
    * aSelection is optional.  If provided, aSelection is set to (newTextNode, 0)
    * if newTextNode was successfully created.
    */
  nsresult InsertStyleAndNewTextNode(nsIDOMNode *aParentNode, 
                                       nsIAtom    *aTag, 
                                       nsIDOMSelection *aSelection);

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
  nsresult EchoInsertionToPWBuff(nsIDOMSelection *aSelection, nsString *aOutString);

  /** do the actual text insertion */
  nsresult DoTextInsertion(nsIDOMSelection *aSelection, 
                           PRBool          *aCancel,
                           PlaceholderTxn **aTxn,
                           const nsString  *aInString,
                           TypeInState      aTypeInState);
  
  // data
  nsHTMLEditor *mEditor;  // note that we do not refcount the editor
  nsString      mPasswordText;  // a buffer we use to store the real value of password editors
  nsCOMPtr<nsIDOMNode> mBogusNode;  // magic node acts as placeholder in empty doc
  PRUint32 mFlags;
};



class nsTextRulesInfo : public nsRulesInfo
{
 public:
 
  nsTextRulesInfo(int aAction) : 
    nsRulesInfo(aAction),
    placeTxn(0),
    inString(0),
    outString(0),
    typeInState(),
    maxLength(-1),
    collapsedAction(nsIEditor::eDeleteNext),
    bOrdered(PR_FALSE),
    alignType(0),
    blockType(0),
    insertElement(0)
    {};

  virtual ~nsTextRulesInfo() {};
  
  // kInsertText
  PlaceholderTxn **placeTxn;
  const nsString *inString;
  nsString *outString;
  TypeInState typeInState;
  PRInt32 maxLength;
  
  // kDeleteSelection
  nsIEditor::ESelectionCollapseDirection collapsedAction;
  
  // kMakeList
  PRBool bOrdered;
  
  // kAlign
  const nsString *alignType;
  
  // kMakeBasicBlock
  const nsString *blockType;
  
  // kInsertElement
  const nsIDOMElement* insertElement;
};

#endif //nsTextEditRules_h__

