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


#ifndef nsEditorUtils_h__
#define nsEditorUtils_h__


#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsISelection.h"
#include "nsIEditor.h"
#include "nsIAtom.h"
#include "nsEditor.h"
#include "nsIContentIterator.h"
#include "nsCOMArray.h"

class nsPlaintextEditor;

/***************************************************************************
 * stack based helper class for batching a collection of txns inside a 
 * placeholder txn.
 */
class NS_STACK_CLASS nsAutoPlaceHolderBatch
{
  private:
    nsCOMPtr<nsIEditor> mEd;
  public:
    nsAutoPlaceHolderBatch( nsIEditor *aEd, nsIAtom *atom) : mEd(do_QueryInterface(aEd)) 
                   { if (mEd) mEd->BeginPlaceHolderTransaction(atom); }
    ~nsAutoPlaceHolderBatch() { if (mEd) mEd->EndPlaceHolderTransaction(); }
};

/***************************************************************************
 * stack based helper class for batching a collection of txns.  
 * Note: I changed this to use placeholder batching so that we get
 * proper selection save/restore across undo/redo.
 */
class nsAutoEditBatch : public nsAutoPlaceHolderBatch
{
  public:
    nsAutoEditBatch( nsIEditor *aEd) : nsAutoPlaceHolderBatch(aEd,nsnull)  {}
    ~nsAutoEditBatch() {}
};

/***************************************************************************
 * stack based helper class for saving/restoring selection.  Note that this
 * assumes that the nodes involved are still around afterwards!
 */
class NS_STACK_CLASS nsAutoSelectionReset
{
  private:
    /** ref-counted reference to the selection that we are supposed to restore */
    nsCOMPtr<nsISelection> mSel;
    nsEditor *mEd;  // non-owning ref to nsEditor

  public:
    /** constructor responsible for remembering all state needed to restore aSel */
    nsAutoSelectionReset(nsISelection *aSel, nsEditor *aEd);
    
    /** destructor restores mSel to its former state */
    ~nsAutoSelectionReset();

    /** Abort: cancel selection saver */
    void Abort();
};

/***************************************************************************
 * stack based helper class for StartOperation()/EndOperation() sandwich
 */
class NS_STACK_CLASS nsAutoRules
{
  public:
  
  nsAutoRules(nsEditor *ed, PRInt32 action, nsIEditor::EDirection aDirection) : 
         mEd(ed), mDoNothing(PR_FALSE)
  { 
    if (mEd && !mEd->mAction) // mAction will already be set if this is nested call
    {
      mEd->StartOperation(action, aDirection);
    }
    else mDoNothing = PR_TRUE; // nested calls will end up here
  }
  ~nsAutoRules() 
  {
    if (mEd && !mDoNothing) 
    {
      mEd->EndOperation();
    }
  }
  
  protected:
  nsEditor *mEd;
  bool mDoNothing;
};


/***************************************************************************
 * stack based helper class for turning off active selection adjustment
 * by low level transactions
 */
class NS_STACK_CLASS nsAutoTxnsConserveSelection
{
  public:
  
  nsAutoTxnsConserveSelection(nsEditor *ed) : mEd(ed), mOldState(PR_TRUE)
  {
    if (mEd) 
    {
      mOldState = mEd->GetShouldTxnSetSelection();
      mEd->SetShouldTxnSetSelection(PR_FALSE);
    }
  }
  
  ~nsAutoTxnsConserveSelection() 
  {
    if (mEd) 
    {
      mEd->SetShouldTxnSetSelection(mOldState);
    }
  }
  
  protected:
  nsEditor *mEd;
  bool mOldState;
};

/***************************************************************************
 * stack based helper class for batching reflow and paint requests.
 */
class NS_STACK_CLASS nsAutoUpdateViewBatch
{
  public:
  
  nsAutoUpdateViewBatch(nsEditor *ed) : mEd(ed)
  {
    NS_ASSERTION(mEd, "null mEd pointer!");

    if (mEd) 
      mEd->BeginUpdateViewBatch();
  }
  
  ~nsAutoUpdateViewBatch() 
  {
    if (mEd) 
      mEd->EndUpdateViewBatch();
  }
  
  protected:
  nsEditor *mEd;
};

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

class nsDomIterFunctor 
{
  public:
    virtual void* operator()(nsIDOMNode* aNode)=0;
};

class nsBoolDomIterFunctor 
{
  public:
    virtual bool operator()(nsIDOMNode* aNode)=0;
};

class NS_STACK_CLASS nsDOMIterator
{
  public:
    nsDOMIterator();
    virtual ~nsDOMIterator();
    
    nsresult Init(nsIDOMRange* aRange);
    nsresult Init(nsIDOMNode* aNode);
    void ForEach(nsDomIterFunctor& functor) const;
    nsresult AppendList(nsBoolDomIterFunctor& functor,
                        nsCOMArray<nsIDOMNode>& arrayOfNodes) const;
  protected:
    nsCOMPtr<nsIContentIterator> mIter;
};

class nsDOMSubtreeIterator : public nsDOMIterator
{
  public:
    nsDOMSubtreeIterator();
    virtual ~nsDOMSubtreeIterator();

    nsresult Init(nsIDOMRange* aRange);
    nsresult Init(nsIDOMNode* aNode);
};

class nsTrivialFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual bool operator()(nsIDOMNode* aNode)  // used to build list of all nodes iterator covers
    {
      return PR_TRUE;
    }
};


/******************************************************************************
 * general dom point utility struct
 *****************************************************************************/
struct NS_STACK_CLASS DOMPoint
{
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  DOMPoint() : node(0),offset(0) {}
  DOMPoint(nsIDOMNode *aNode, PRInt32 aOffset) : 
                 node(aNode),offset(aOffset) {}
  void SetPoint(nsIDOMNode *aNode, PRInt32 aOffset)
  {
    node = aNode; offset = aOffset;
  }
  void GetPoint(nsCOMPtr<nsIDOMNode> &aNode, PRInt32 &aOffset)
  {
    aNode = node; aOffset = offset;
  }
};


class nsEditorUtils
{
  public:
    static bool IsDescendantOf(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 *aOffset = 0);
    static bool IsLeafNode(nsIDOMNode *aNode);
};


class nsIDragSession;
class nsITransferable;
class nsIDOMEvent;
class nsISimpleEnumerator;

class nsEditorHookUtils
{
  public:
    static bool     DoInsertionHook(nsIDOMDocument *aDoc, nsIDOMEvent *aEvent,
                                    nsITransferable *aTrans);
  private:
    static nsresult GetHookEnumeratorFromDocument(nsIDOMDocument *aDoc,
                                                  nsISimpleEnumerator **aEnumerator);
};

#endif // nsEditorUtils_h__
