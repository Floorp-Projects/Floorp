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

/*
 * nsIRangeList: the implementation of selection.
 */

#include "nsIFactory.h"
#include "nsIEnumerator.h"
#include "nsIDOMRange.h"
#include "nsIFrameSelection.h"
#include "nsIDOMSelection.h"
#include "nsIDOMSelectionListener.h"
#include "nsIFocusTracker.h"
#include "nsIComponentManager.h"
#include "nsLayoutCID.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsRange.h"
#include "nsISupportsArray.h"
#include "nsIDOMUIEvent.h"

#include "nsIDOMSelectionListener.h"
#include "nsIContentIterator.h"

//included for desired x position;
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsICaret.h"

#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"

// included for view scrolling
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"

#define STATUS_CHECK_RETURN_MACRO() {if (!mTracker) return NS_ERROR_FAILURE;}

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kCSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);

//PROTOTYPES
static void printRange(nsIDOMRange *aDomRange);
static nsCOMPtr<nsIAtom> GetTag(nsIDOMNode *aNode);
static nsresult ParentOffset(nsIDOMNode *aNode, nsIDOMNode **aParent, PRInt32 *aChildOffset);



#if 0
#define DEBUG_OUT_RANGE(x)  printRange(x)
#else
#define DEBUG_OUT_RANGE(x)  
#endif //MOZ_DEBUG

//#define DEBUG_SELECTION // uncomment for printf describing every collapse and extend.
//#define DEBUG_NAVIGATION

class nsRangeListIterator;
class nsRangeList;

class nsDOMSelection : public nsIDOMSelection , public nsIScriptObjectOwner
{
public:
  nsDOMSelection(nsRangeList *aList);
  virtual ~nsDOMSelection();
  
  NS_DECL_ISUPPORTS

  /*BEGIN nsIDOMSelection interface implementations*/
  NS_IMETHOD    GetAnchorNode(nsIDOMNode** aAnchorNode);
  NS_IMETHOD    GetAnchorOffset(PRInt32* aAnchorOffset);
  NS_IMETHOD    GetFocusNode(nsIDOMNode** aFocusNode);
  NS_IMETHOD    GetFocusOffset(PRInt32* aFocusOffset);
  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed);
  NS_IMETHOD    GetRangeCount(PRInt32* aRangeCount);
  NS_IMETHOD    GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn);
  NS_IMETHOD    ClearSelection();
  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset);
  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset);
  NS_IMETHOD    ContainsNode(nsIDOMNode* aNode, PRBool aRecursive, PRBool* aAYes);
  NS_IMETHOD    DeleteFromDocument();
  NS_IMETHOD    AddRange(nsIDOMRange* aRange);

  NS_IMETHOD    StartBatchChanges();
  NS_IMETHOD    EndBatchChanges();

  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener);
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove);
  NS_IMETHOD    GetEnumerator(nsIEnumerator **aIterator);
/*END nsIDOMSelection interface implementations*/

/*BEGIN nsIScriptObjectOwner interface implementations*/
  NS_IMETHOD 		GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD 		SetScriptObject(void *aScriptObject);
/*END nsIScriptObjectOwner interface implementations*/

  // utility methods for scrolling the selection into view
  nsresult      GetPresShell(nsIPresShell **aPresShell);
  nsresult      GetRootScrollableView(nsIScrollableView **aScrollableView);
  nsresult      GetFrameToRootViewOffset(nsIFrame *aFrame, nscoord *aXOffset, nscoord *aYOffset);
  nsresult      GetPointFromOffset(nsIFrame *aFrame, PRInt32 aContentOffset, nsPoint *aPoint);
  nsresult      GetFocusNodeRect(nsRect *aRect);
  nsresult      ScrollRectIntoView(nsRect& aRect, PRIntn  aVPercent, PRIntn  aHPercent);

  NS_IMETHOD    ScrollIntoView(SelectionRegion aRegion=SELECTION_FOCUS_REGION);
  nsresult      AddItem(nsIDOMRange *aRange);
  nsresult      RemoveItem(nsIDOMRange *aRange);

  nsresult      Clear();
	// methods for convenience. Note, these don't addref
  nsIDOMNode*  FetchAnchorNode();  //where did the selection begin
  PRInt32      FetchAnchorOffset();

  nsIDOMNode*  FetchOriginalAnchorNode();  //where did the ORIGINAL selection begin
  PRInt32      FetchOriginalAnchorOffset();

  nsIDOMNode*  FetchFocusNode();   //where is the carret
  PRInt32      FetchFocusOffset();

  nsIDOMNode*  FetchStartParent(nsIDOMRange *aRange);   //skip all the com stuff and give me the start/end
  PRInt32      FetchStartOffset(nsIDOMRange *aRange);
  nsIDOMNode*  FetchEndParent(nsIDOMRange *aRange);     //skip all the com stuff and give me the start/end
  PRInt32      FetchEndOffset(nsIDOMRange *aRange);

  nsDirection  GetDirection(){return mDirection;}
  void         SetDirection(nsDirection aDir){mDirection = aDir;}
  NS_IMETHOD   GetPrimaryFrameForFocusNode(nsIFrame **aResultFrame);
  NS_IMETHOD   SetOriginalAnchorPoint(nsIDOMNode *aNode, PRInt32 aOffset);
  NS_IMETHOD   GetOriginalAnchorPoint(nsIDOMNode **aNode, PRInt32 *aOffset);
  NS_IMETHOD   LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails, SelectionType aType);
  NS_IMETHOD   Repaint();

private:
  friend class nsRangeListIterator;

  

  void         setAnchorFocusRange(PRInt32 aIndex); //pass in index into rangelist
  NS_IMETHOD   selectFrames(nsIDOMRange *aRange, PRBool aSelect);
  
  NS_IMETHOD   FixupSelectionPoints(nsIDOMRange *aRange, nsDirection *aDir, PRBool *aFixupState);


  nsCOMPtr<nsISupportsArray> mRangeArray;

  nsCOMPtr<nsIDOMRange> mAnchorFocusRange;
  nsCOMPtr<nsIDOMRange> mOriginalAnchorRange; //used as a point with range gravity for security
  nsDirection mDirection; //FALSE = focus, anchor;  TRUE = anchor,focus
  PRBool mFixupState; //was there a fixup?

  nsRangeList *mRangeList;

  // for nsIScriptContextOwner
  void*		mScriptObject;
};


class nsRangeList : public nsIFrameSelection
                    
{
public:
  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_ISUPPORTS

/*BEGIN nsIFrameSelection interfaces*/
  NS_IMETHOD Init(nsIFocusTracker *aTracker);
  NS_IMETHOD ShutDown();
  NS_IMETHOD HandleTextEvent(nsGUIEvent *aGUIEvent);
  NS_IMETHOD HandleKeyEvent(nsGUIEvent *aGuiEvent);
  NS_IMETHOD TakeFocus(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset, 
                       PRBool aContinueSelection, PRBool aMultipleSelection);
  NS_IMETHOD EnableFrameNotification(PRBool aEnable){mNotifyFrames = aEnable; return NS_OK;}
  NS_IMETHOD LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails);
  NS_IMETHOD SetMouseDownState(PRBool aState);
  NS_IMETHOD GetMouseDownState(PRBool *aState);
  NS_IMETHOD GetSelection(SelectionType aType, nsIDOMSelection **aDomSelection);
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion);
  NS_IMETHOD RepaintSelection(SelectionType aType);
/*END nsIFrameSelection interfacse*/



  nsRangeList();
  virtual ~nsRangeList();

  //methods called from nsDomSelection to allow ONE list of listeners for all types of selections
  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener);
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove);
  NS_IMETHOD    StartBatchChanges();
  NS_IMETHOD    EndBatchChanges();
  NS_IMETHOD    DeleteFromDocument();
  nsIFocusTracker *GetTracker(){return mTracker;}
private:

  friend class nsDOMSelection;
#ifdef DEBUG
  void printSelection();       // for debugging
#endif /* DEBUG */

  void ResizeBuffer(PRUint32 aNewBufSize);

  nscoord      FetchDesiredX(); //the x position requested by the Key Handling for up down
  void         InvalidateDesiredX(); //do not listen to mDesiredX you must get another.
  void         SetDesiredX(nscoord aX); //set the mDesiredX

  
  PRUint32     GetBatching(){return mBatching;}
  PRBool       GetNotifyFrames(){return mNotifyFrames;}
  void         SetDirty(PRBool aDirty=PR_TRUE){if (mBatching) mChangesDuringBatching = aDirty;}

  nsresult     NotifySelectionListeners();			// add parameters to say collapsed etc?

  nsDOMSelection *mDomSelections[NUM_SELECTIONTYPES];

  //batching
  PRInt32 mBatching;
  PRBool mChangesDuringBatching;
  PRBool mNotifyFrames;
  
  nsCOMPtr<nsISupportsArray> mSelectionListeners;
  
  nsIFocusTracker *mTracker;
  PRBool mMouseDownState; //for drag purposes
  PRInt32 mDesiredX;
  PRBool mDesiredXSet;
public:
  static nsIAtom *sTableAtom;
  static nsIAtom *sCellAtom;
  static nsIAtom *sTbodyAtom;
  static PRInt32 sInstanceCount;
};

class nsRangeListIterator : public nsIBidirectionalEnumerator
{
public:
/*BEGIN nsIEnumerator interfaces
see the nsIEnumerator for more details*/

  NS_IMETHOD_(nsrefcnt) AddRef();

  NS_IMETHOD_(nsrefcnt) Release();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_NSIENUMERATOR

  NS_DECL_NSIBIDIRECTIONALENUMERATOR

/*END nsIEnumerator interfaces*/
/*BEGIN Helper Methods*/
  NS_IMETHOD CurrentItem(nsIDOMRange **aRange);
/*END Helper Methods*/
private:
  friend class nsDOMSelection;

  //lame lame lame if delete from document goes away then get rid of this unless its debug
  friend class nsRangeList; 

  nsRangeListIterator(nsDOMSelection *);
  virtual ~nsRangeListIterator();
  PRInt32     mIndex;
  nsDOMSelection *mDomSelection;
  SelectionType mType;
};




nsresult NS_NewRangeList(nsIDOMSelection **);

nsresult NS_NewRangeList(nsIFrameSelection **aRangeList)
{
  nsRangeList *rlist = new nsRangeList;
  if (!rlist)
    return NS_ERROR_OUT_OF_MEMORY;
  *aRangeList = (nsIFrameSelection *)rlist;
  nsresult result = rlist->AddRef();
  if (!NS_SUCCEEDED(result))
  {
    delete rlist;
  }
  return result;
}



//Horrible statics but no choice
nsIAtom *nsRangeList::sTableAtom = 0;
nsIAtom *nsRangeList::sCellAtom = 0;
nsIAtom *nsRangeList::sTbodyAtom = 0;
PRInt32 nsRangeList::sInstanceCount = 0;
///////////BEGIN nsRangeListIterator methods

nsRangeListIterator::nsRangeListIterator(nsDOMSelection *aList)
:mIndex(0)
{
  if (!aList)
  {
    NS_NOTREACHED("nsRangeList");
    return;
  }
  mDomSelection = aList;
}



nsRangeListIterator::~nsRangeListIterator()
{
}



////////////END nsRangeListIterator methods

////////////BEGIN nsIRangeListIterator methods



NS_IMETHODIMP
nsRangeListIterator::Next()
{
  mIndex++;
  PRUint32 cnt;
  nsresult rv = mDomSelection->mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (mIndex < (PRInt32)cnt)
    return NS_OK;
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsRangeListIterator::Prev()
{
  mIndex--;
  if (mIndex >= 0 )
    return NS_OK;
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsRangeListIterator::First()
{
  if (!mDomSelection)
    return NS_ERROR_NULL_POINTER;
  mIndex = 0;
  return NS_OK;
}



NS_IMETHODIMP
nsRangeListIterator::Last()
{
  if (!mDomSelection)
    return NS_ERROR_NULL_POINTER;
  PRUint32 cnt;
  nsresult rv = mDomSelection->mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  mIndex = (PRInt32)cnt-1;
  return NS_OK;
}



NS_IMETHODIMP 
nsRangeListIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  PRUint32 cnt;
  nsresult rv = mDomSelection->mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (mIndex >=0 && mIndex < (PRInt32)cnt){
    *aItem = mDomSelection->mRangeArray->ElementAt(mIndex);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP 
nsRangeListIterator::CurrentItem(nsIDOMRange **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  PRUint32 cnt;
  nsresult rv = mDomSelection->mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (mIndex >=0 && mIndex < (PRInt32)cnt){
    nsCOMPtr<nsISupports> indexIsupports = dont_AddRef(mDomSelection->mRangeArray->ElementAt(mIndex));
    return indexIsupports->QueryInterface(nsIDOMRange::GetIID(),(void **)aItem);
  }
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsRangeListIterator::IsDone()
{
  PRUint32 cnt;
  nsresult rv = mDomSelection->mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (mIndex >= 0 && mIndex < (PRInt32)cnt ) { 
    return NS_COMFALSE;
  }
  return NS_OK;
}



NS_IMETHODIMP
nsRangeListIterator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  return mDomSelection->QueryInterface(aIID, aInstancePtr);
}



NS_IMETHODIMP_(nsrefcnt)
nsRangeListIterator::AddRef()
{
  return mDomSelection->AddRef();
}



NS_IMETHODIMP_(nsrefcnt)
nsRangeListIterator::Release()
{
  return mDomSelection->Release();
}



////////////END nsIRangeListIterator methods

#ifdef XP_MAC
#pragma mark -
#endif

////////////BEGIN nsRangeList methods

nsRangeList::nsRangeList()
{
  NS_INIT_REFCNT();
  PRInt32 i;
  for (i = 0;i<NUM_SELECTIONTYPES;i++){
    mDomSelections[i] = nsnull;
  }
  for (i = 0;i<NUM_SELECTIONTYPES;i++){
    mDomSelections[i] = new nsDOMSelection(this);
    if (!mDomSelections[i])
      return;
    mDomSelections[i]->AddRef();
  }
  NS_NewISupportsArray(getter_AddRefs(mSelectionListeners));
  mBatching = 0;
  mChangesDuringBatching = PR_FALSE;
  mNotifyFrames = PR_TRUE;
    
  if (sInstanceCount <= 0)
  {
    sTableAtom = NS_NewAtom("table");
    sCellAtom = NS_NewAtom("td");
    sTbodyAtom = NS_NewAtom("tbody");
  }
  sInstanceCount ++;
}



nsRangeList::~nsRangeList()
{
 
  if (mSelectionListeners)
  {
	  PRUint32 cnt;
    nsresult rv = mSelectionListeners->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i=0;i < cnt; i++)
	  {
	    mSelectionListeners->RemoveElementAt(i);
	  }
  }
  if (sInstanceCount <= 1)
  {
    NS_IF_RELEASE(sTableAtom);
    NS_IF_RELEASE(sCellAtom);
    NS_IF_RELEASE(sTbodyAtom);
  }
  sInstanceCount--;
}



//END nsRangeList methods



NS_IMPL_ADDREF(nsRangeList)

NS_IMPL_RELEASE(nsRangeList)


NS_IMETHODIMP
nsRangeList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsIFrameSelection::GetIID())) {
    nsIFrameSelection* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    // use *first* base class for ISupports
    nsIFrameSelection* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  NS_ASSERTION(PR_FALSE,"bad query interface in RangeList");
  return NS_NOINTERFACE;
}






nscoord
nsRangeList::FetchDesiredX() //the x position requested by the Key Handling for up down
{
  if (!mTracker)
  {
    NS_ASSERTION(0,"fetch desired X failed\n");
    return -1;
  }
  if (mDesiredXSet)
    return mDesiredX;
  else {
    nsPoint coord;
    PRBool  collapsed;
    nsCOMPtr<nsICaret> caret;
    nsCOMPtr<nsIPresContext> context;
    nsCOMPtr<nsIPresShell> shell;
    nsresult result = mTracker->GetPresContext(getter_AddRefs(context));
    if (NS_FAILED(result) || !context)
      return result;
    result = context->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(result) || !shell)
      return result;
    result = shell->GetCaret(getter_AddRefs(caret));
    if (NS_FAILED(result) || !caret)
      return result;

    result = caret->GetWindowRelativeCoordinates(coord,collapsed);
    if (NS_FAILED(result))
      return result;
    return coord.x;
  }
}



void
nsRangeList::InvalidateDesiredX() //do not listen to mDesiredX you must get another.
{
  mDesiredXSet = PR_FALSE;
}



void
nsRangeList::SetDesiredX(nscoord aX) //set the mDesiredX
{
  mDesiredX = aX;
  mDesiredXSet = PR_TRUE;
}


#ifdef XP_MAC
#pragma mark -
#endif

//BEGIN nsIFrameSelection methods


void printRange(nsIDOMRange *aDomRange)
{
  if (!aDomRange)
  {
    printf("NULL nsIDOMRange\n");
  }
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset;
  PRInt32 endOffset;
  aDomRange->GetStartParent(getter_AddRefs(startNode));
  aDomRange->GetStartOffset(&startOffset);
  aDomRange->GetEndParent(getter_AddRefs(endNode));
  aDomRange->GetEndOffset(&endOffset);
  
  printf("range: 0x%lx\t start: 0x%lx %ld, \t end: 0x%lx,%ld\n",
         (unsigned long)aDomRange,
         (unsigned long)(nsIDOMNode*)startNode, (long)startOffset,
         (unsigned long)(nsIDOMNode*)endNode, (long)endOffset);
         
}


nsCOMPtr<nsIAtom> GetTag(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIAtom> atom;
  
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsHTMLEditRules::GetTag()");
    return atom;
  }
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content)
    content->GetTag(*getter_AddRefs(atom));

  return atom;
}



nsresult
ParentOffset(nsIDOMNode *aNode, nsIDOMNode **aParent, PRInt32 *aChildOffset)
{
  if (!aNode || !aParent || !aChildOffset)
    return NS_ERROR_NULL_POINTER;
  nsresult result = NS_OK;
  nsCOMPtr<nsIContent> content;
  result = aNode->QueryInterface(nsIContent::GetIID(),getter_AddRefs(content));
  if (NS_SUCCEEDED(result) && content)
  {
    nsCOMPtr<nsIContent> parent;
    result = content->GetParent(*getter_AddRefs(parent));
    if (NS_SUCCEEDED(result) && parent)
    {
      result = parent->IndexOf(content, *aChildOffset);
      if (NS_SUCCEEDED(result))
        result = parent->QueryInterface(nsIDOMNode::GetIID(),(void **)aParent);
    }
  }
  return result;
}



NS_IMETHODIMP
nsRangeList::Init(nsIFocusTracker *aTracker)
{
  mTracker = aTracker;
  mMouseDownState = PR_FALSE;
  mDesiredXSet = PR_FALSE;
  return NS_OK;
}



NS_IMETHODIMP
nsRangeList::ShutDown()
{
  return NS_OK;
}

  
  
NS_IMETHODIMP
nsRangeList::HandleTextEvent(nsGUIEvent *aGUIEvent)
{
	if (!aGUIEvent)
		return NS_ERROR_NULL_POINTER;

#ifdef DEBUG_TAGUE
	printf("nsRangeList: HandleTextEvent\n");
#endif
  nsresult result(NS_OK);
	if (NS_TEXT_EVENT == aGUIEvent->message) {

    result = mDomSelections[SELECTION_NORMAL]->ScrollIntoView();
	}
	return result;
}



/** This raises a question, if this method is called and the aFrame does not reflect the current
 *  focus  DomNode, it is invalid?  The answer now is yes.
 */
NS_IMETHODIMP
nsRangeList::HandleKeyEvent(nsGUIEvent *aGuiEvent)
{
  if (!aGuiEvent)
    return NS_ERROR_NULL_POINTER;
  STATUS_CHECK_RETURN_MACRO();

  nsresult result = NS_OK;
  if (NS_KEY_DOWN == aGuiEvent->message) {
    nsKeyEvent *keyEvent = (nsKeyEvent *)aGuiEvent; //this is ok. It really is a keyevent
    nsCOMPtr<nsIDOMNode> weakNodeUsed;
    PRInt32 offsetused = 0;
    nsSelectionAmount amount = eSelectCharacter;
    if (keyEvent->isControl)
      amount = eSelectWord;

    PRBool isCollapsed;
    nscoord desiredX; //we must keep this around and revalidate it when its just UP/DOWN

    result = mDomSelections[SELECTION_NORMAL]->GetIsCollapsed(&isCollapsed);
    if (NS_FAILED(result))
      return result;
    if (keyEvent->keyCode == nsIDOMUIEvent::VK_UP || keyEvent->keyCode == nsIDOMUIEvent::VK_DOWN)
      desiredX= FetchDesiredX();

    if (!isCollapsed && !keyEvent->isShift) {
      switch (keyEvent->keyCode){
        case nsIDOMUIEvent::VK_LEFT  : 
        case nsIDOMUIEvent::VK_UP    : {
            if ((mDomSelections[SELECTION_NORMAL]->GetDirection() == eDirPrevious)) { //f,a
              offsetused = mDomSelections[SELECTION_NORMAL]->FetchFocusOffset();
              weakNodeUsed = mDomSelections[SELECTION_NORMAL]->FetchFocusNode();
            }
            else {
              offsetused = mDomSelections[SELECTION_NORMAL]->FetchAnchorOffset();
              weakNodeUsed = mDomSelections[SELECTION_NORMAL]->FetchAnchorNode();
            }
            result = mDomSelections[SELECTION_NORMAL]->Collapse(weakNodeUsed,offsetused);

                                       } break;
        case nsIDOMUIEvent::VK_RIGHT : 
        case nsIDOMUIEvent::VK_DOWN  : {
            if ((mDomSelections[SELECTION_NORMAL]->GetDirection() == eDirPrevious)) { //f,a
              offsetused = mDomSelections[SELECTION_NORMAL]->FetchAnchorOffset();
              weakNodeUsed = mDomSelections[SELECTION_NORMAL]->FetchAnchorNode();
            }
            else {
              offsetused = mDomSelections[SELECTION_NORMAL]->FetchFocusOffset();
              weakNodeUsed = mDomSelections[SELECTION_NORMAL]->FetchFocusNode();
            }
            result = mDomSelections[SELECTION_NORMAL]->Collapse(weakNodeUsed,offsetused);
                                       } break;
      }
      if (keyEvent->keyCode == nsIDOMUIEvent::VK_UP || keyEvent->keyCode == nsIDOMUIEvent::VK_DOWN)
        SetDesiredX(desiredX);
      return NS_OK;
    }

    offsetused = mDomSelections[SELECTION_NORMAL]->FetchFocusOffset();
    weakNodeUsed = mDomSelections[SELECTION_NORMAL]->FetchFocusNode();
    nsIFrame *frame;
    nsIFrame *resultFrame;
    nsCOMPtr<nsIContent> content;
    result = mDomSelections[SELECTION_NORMAL]->GetPrimaryFrameForFocusNode(&frame);
    if (NS_FAILED(result))
      return result;
    switch (keyEvent->keyCode){
      case nsIDOMUIEvent::VK_LEFT  : 
        {
        //we need to look for the previous PAINTED location to move the cursor to.
          if (NS_SUCCEEDED(result) && NS_SUCCEEDED(frame->PeekOffset(mTracker, desiredX, amount, eDirPrevious, offsetused, getter_AddRefs(content), &offsetused, &resultFrame, PR_FALSE)) && content){
            result = TakeFocus(content, offsetused, offsetused, keyEvent->isShift, PR_FALSE);
          }
          result = mDomSelections[SELECTION_NORMAL]->ScrollIntoView();
        }
        break;
      case nsIDOMUIEvent::VK_RIGHT : 
        {
        //we need to look for the previous PAINTED location to move the cursor to.
          if (NS_SUCCEEDED(result) && NS_SUCCEEDED(frame->PeekOffset(mTracker, desiredX, amount, eDirNext, offsetused, getter_AddRefs(content), &offsetused, &resultFrame, PR_FALSE)) && content){
            result = TakeFocus(content, offsetused, offsetused, keyEvent->isShift, PR_FALSE);
          }
          result = mDomSelections[SELECTION_NORMAL]->ScrollIntoView();
        }
       break;
      case nsIDOMUIEvent::VK_UP : 
      case nsIDOMUIEvent::VK_DOWN : 
        {
  //we need to look for the previous PAINTED location to move the cursor to.
          amount = eSelectLine;
          if (nsIDOMUIEvent::VK_UP == keyEvent->keyCode)
          {
            if (NS_SUCCEEDED(result) && NS_SUCCEEDED(frame->PeekOffset(mTracker, desiredX, amount, eDirPrevious, offsetused, getter_AddRefs(content), &offsetused, &resultFrame, PR_FALSE)) && content){
              result = TakeFocus(content, offsetused, offsetused, keyEvent->isShift, PR_FALSE);
            }
          }
          else
            if (NS_SUCCEEDED(result) && NS_SUCCEEDED(frame->PeekOffset(mTracker, desiredX, amount, eDirNext, offsetused, getter_AddRefs(content), &offsetused, &resultFrame, PR_FALSE)) && content){
              result = TakeFocus(content, offsetused, offsetused, keyEvent->isShift, PR_FALSE);
            }
          result = mDomSelections[SELECTION_NORMAL]->ScrollIntoView();
          SetDesiredX(desiredX);
        }
        break;
#ifdef DEBUG_NAVIGATION
        printf("debug vk down\n");
#endif
        break;
    default :break;
    }
  }
  return result;
}



#ifdef DEBUG
void nsRangeList::printSelection()
{
  PRInt32 cnt;
  (void)mDomSelections[SELECTION_NORMAL]->GetRangeCount(&cnt);
  printf("nsRangeList 0x%lx: %d items\n", (unsigned long)this,
         mDomSelections[0] ? (int)cnt : -99);

  // Get an iterator
  nsRangeListIterator iter(mDomSelections[0]);
  nsresult res = iter.First();
  if (!NS_SUCCEEDED(res))
  {
    printf(" Can't get an iterator\n");
    return;
  }

  while (iter.IsDone())
  {
    nsCOMPtr<nsIDOMRange> range;
    res = iter.CurrentItem(getter_AddRefs(range));
    if (!NS_SUCCEEDED(res))
    {
      printf(" OOPS\n");
      return;
    }
    printRange(range);
    iter.Next();
  }

  printf("Anchor is 0x%lx, %d\n",
         (unsigned long)(nsIDOMNode*)mDomSelections[SELECTION_NORMAL]->FetchAnchorNode(), mDomSelections[SELECTION_NORMAL]->FetchAnchorOffset());
  printf("Focus is 0x%lx, %d\n",
         (unsigned long)(nsIDOMNode*)mDomSelections[SELECTION_NORMAL]->FetchFocusNode(), mDomSelections[SELECTION_NORMAL]->FetchFocusOffset());
  printf(" ... end of selection\n");
}
#endif /* DEBUG */





/**
hard to go from nodes to frames, easy the other way!
 */
NS_IMETHODIMP
nsRangeList::TakeFocus(nsIContent *aNewFocus, PRUint32 aContentOffset, 
                       PRUint32 aContentEndOffset, PRBool aContinueSelection, PRBool aMultipleSelection)
{
  if (!aNewFocus)
    return NS_ERROR_NULL_POINTER;
  if (GetBatching())
    return NS_ERROR_FAILURE;
  STATUS_CHECK_RETURN_MACRO();
  //HACKHACKHACK
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> domNode;
  nsCOMPtr<nsIContent> parent;
  nsCOMPtr<nsIContent> parent2;
  if (NS_FAILED(aNewFocus->GetParent(*getter_AddRefs(parent))) || !parent)
    return NS_ERROR_FAILURE;
  //if (NS_FAILED(parent->GetParent(*getter_AddRefs(parent2))) || !parent2)
    //return NS_ERROR_FAILURE;

  //END HACKHACKHACK /checking for root frames/content

  domNode = do_QueryInterface(aNewFocus);
  //traverse through document and unselect crap here
  if (!aContinueSelection){ //single click? setting cursor down
    PRUint32 batching = mBatching;//hack to use the collapse code.
    PRBool changes = mChangesDuringBatching;
    mBatching = 1;

    if (aMultipleSelection){
      nsCOMPtr<nsIDOMRange> newRange;
      nsresult result;
      result = nsComponentManager::CreateInstance(kRangeCID, nsnull,
                                       nsIDOMRange::GetIID(),
                                       getter_AddRefs(newRange));
      newRange->SetStart(domNode,aContentOffset);
      newRange->SetEnd(domNode,aContentOffset);
      mDomSelections[SELECTION_NORMAL]->AddRange(newRange);
      mBatching = batching;
      mChangesDuringBatching = changes;
      mDomSelections[SELECTION_NORMAL]->SetOriginalAnchorPoint(domNode,aContentOffset);
    }
    else
    {
      mDomSelections[SELECTION_NORMAL]->Collapse(domNode, aContentOffset);
      mBatching = batching;
      mChangesDuringBatching = changes;
    }
    if (aContentEndOffset > aContentOffset)
      mDomSelections[SELECTION_NORMAL]->Extend(domNode,aContentEndOffset);
  }
  else {
    // Now update the range list:
    if (aContinueSelection && domNode)
    {
      if (mDomSelections[SELECTION_NORMAL]->GetDirection() == eDirNext && aContentEndOffset > aContentOffset) //didnt go far enough 
      {
        mDomSelections[SELECTION_NORMAL]->Extend(domNode, aContentEndOffset);//this will only redraw the diff 
      }
      else
        mDomSelections[SELECTION_NORMAL]->Extend(domNode, aContentOffset);
    }
  }
    
  return NotifySelectionListeners();
}



NS_METHOD
nsRangeList::LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails)
{
  if (!aContent || !aReturnDetails)
    return NS_ERROR_NULL_POINTER;

  STATUS_CHECK_RETURN_MACRO();

  *aReturnDetails = nsnull;
  PRInt8 j;
  for (j = (PRInt8) SELECTION_NORMAL; j < (PRInt8)NUM_SELECTIONTYPES; j++){
    if (mDomSelections[j])
     mDomSelections[j]->LookUpSelection(aContent, aContentOffset, aContentLength, aReturnDetails, (SelectionType)j);
  }
  return NS_OK;
}



NS_METHOD 
nsRangeList::SetMouseDownState(PRBool aState)
{
  mMouseDownState = aState;
  return NS_OK;
}



NS_METHOD
nsRangeList::GetMouseDownState(PRBool *aState)
{
  if (!aState)
    return NS_ERROR_NULL_POINTER;
  *aState = mMouseDownState;
  return NS_OK;
}


NS_IMETHODIMP
nsRangeList::GetSelection(SelectionType aType, nsIDOMSelection **aDomSelection)
{
  if (aType < SELECTION_NORMAL || aType >= NUM_SELECTIONTYPES)
    return NS_ERROR_FAILURE;
  if (!aDomSelection)
    return NS_ERROR_NULL_POINTER;
  *aDomSelection = mDomSelections[aType];
  (*aDomSelection)->AddRef();
  return NS_OK;
}

NS_IMETHODIMP
nsRangeList::ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion)
{
  if (aType < SELECTION_NORMAL || aType >= NUM_SELECTIONTYPES)
    return NS_ERROR_FAILURE;

  if (!mDomSelections[aType])
    return NS_ERROR_NULL_POINTER;

  return mDomSelections[aType]->ScrollIntoView(aRegion);
}

NS_IMETHODIMP
nsRangeList::RepaintSelection(SelectionType aType)
{
  if (aType < SELECTION_NORMAL || aType >= NUM_SELECTIONTYPES)
    return NS_ERROR_FAILURE;

  if (!mDomSelections[aType])
    return NS_ERROR_NULL_POINTER;

  return mDomSelections[aType]->Repaint();
}

//////////END FRAMESELECTION
NS_METHOD
nsRangeList::AddSelectionListener(nsIDOMSelectionListener* inNewListener)
{
  if (!mSelectionListeners)
    return NS_ERROR_FAILURE;
  if (!inNewListener)
    return NS_ERROR_NULL_POINTER;
  nsresult result;
  nsCOMPtr<nsISupports> isupports = do_QueryInterface(inNewListener , &result);
  if (NS_SUCCEEDED(result))
    result = mSelectionListeners->AppendElement(isupports);		// addrefs
  return result;
}



NS_IMETHODIMP
nsRangeList::RemoveSelectionListener(nsIDOMSelectionListener* inListenerToRemove)
{
  if (!mSelectionListeners)
    return NS_ERROR_FAILURE;
  if (!inListenerToRemove )
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISupports> isupports = do_QueryInterface(inListenerToRemove);
  return mSelectionListeners->RemoveElement(isupports);		// releases
}



NS_IMETHODIMP
nsRangeList::StartBatchChanges()
{
  nsresult result(NS_OK);
  mBatching++;
  return result;
}


 
NS_IMETHODIMP
nsRangeList::EndBatchChanges()
{
  nsresult result(NS_OK);
  mBatching--;
  NS_ASSERTION(mBatching >=0,"Bad mBatching");
  if (mBatching == 0 && mChangesDuringBatching){
    mChangesDuringBatching = PR_FALSE;
    NotifySelectionListeners();
  }
  return result;
}

  
  
nsresult
nsRangeList::NotifySelectionListeners()
{
  if (!mSelectionListeners)
    return NS_ERROR_FAILURE;
 
  if (GetBatching()){
    SetDirty();
    return NS_OK;
  }
  PRUint32 cnt;
  nsresult rv = mSelectionListeners->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt;i++)
  {
    nsCOMPtr<nsIDOMSelectionListener> thisListener;
    nsCOMPtr<nsISupports> isupports(dont_AddRef(mSelectionListeners->ElementAt(i)));
    thisListener = do_QueryInterface(isupports);
    if (thisListener)
    	thisListener->NotifySelectionChanged();
  }
	return NS_OK;
}

//END nsIFrameSelection methods

#ifdef XP_MAC
#pragma mark -
#endif

//BEGIN nsIDOMSelection interface implementations



/** DeleteFromDocument
 *  will return NS_OK if it handles the event or NS_COMFALSE if not.
 */
NS_IMETHODIMP
nsRangeList::DeleteFromDocument()
{
  nsresult res;

  // If we're already collapsed, then set ourselves to include the
  // last item BEFORE the current range, rather than the range itself,
  // before we do the delete.
  PRBool isCollapsed;
  mDomSelections[SELECTION_NORMAL]->GetIsCollapsed( &isCollapsed);
  if (isCollapsed)
  {
    // If the offset is positive, then it's easy:
    if (mDomSelections[SELECTION_NORMAL]->FetchFocusOffset() > 0)
    {
      mDomSelections[SELECTION_NORMAL]->Extend(mDomSelections[SELECTION_NORMAL]->FetchFocusNode(), mDomSelections[SELECTION_NORMAL]->FetchFocusOffset() - 1);
    }
    else
    {
      // Otherwise it's harder, have to find the previous node
      printf("Sorry, don't know how to delete across frame boundaries yet\n");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  // Get an iterator
  nsRangeListIterator iter(mDomSelections[SELECTION_NORMAL]);
  res = iter.First();
  if (!NS_SUCCEEDED(res))
    return res;

  nsCOMPtr<nsIDOMRange> range;
  while (iter.IsDone())
  {
    res = iter.CurrentItem(getter_AddRefs(range));
    if (!NS_SUCCEEDED(res))
      return res;
    res = range->DeleteContents();
    if (!NS_SUCCEEDED(res))
      return res;
    iter.Next();
  }

  // Collapse to the new location.
  // If we deleted one character, then we move back one element.
  // FIXME  We don't know how to do this past frame boundaries yet.
  if (isCollapsed)
    mDomSelections[SELECTION_NORMAL]->Collapse(mDomSelections[SELECTION_NORMAL]->FetchAnchorNode(), mDomSelections[SELECTION_NORMAL]->FetchAnchorOffset()-1);
  else if (mDomSelections[SELECTION_NORMAL]->FetchAnchorOffset() > 0)
    mDomSelections[SELECTION_NORMAL]->Collapse(mDomSelections[SELECTION_NORMAL]->FetchAnchorNode(), mDomSelections[SELECTION_NORMAL]->FetchAnchorOffset());
#ifdef DEBUG
  else
    printf("Don't know how to set selection back past frame boundary\n");
#endif

  return NS_OK;
}


//END nsIDOMSelection interface implementations

#ifdef XP_MAC
#pragma mark -
#endif






// nsDOMSelection implementation

// note: this can return a nil anchor node


nsDOMSelection::nsDOMSelection(nsRangeList *aList)
{
  mRangeList = aList;
  mFixupState = PR_FALSE;
  mDirection = eDirNext;
  NS_NewISupportsArray(getter_AddRefs(mRangeArray));
  mScriptObject = nsnull;
  NS_INIT_REFCNT();
}



nsDOMSelection::~nsDOMSelection()
{
  PRUint32 cnt = 0;
  nsresult rv = mRangeArray->Count(&cnt);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  PRUint32 j;
  for (j=0; j<cnt; j++)
	{
	  mRangeArray->RemoveElementAt(0);
	}
  setAnchorFocusRange(-1);
}



NS_IMPL_ADDREF(nsDOMSelection)

NS_IMPL_RELEASE(nsDOMSelection)

NS_IMETHODIMP
nsDOMSelection::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    nsIDOMSelection *temp = (nsIDOMSelection *)this;
    *aInstancePtr = (void*)(nsISupports *)temp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMSelection::GetIID())) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIEnumerator::GetIID())) {
    nsRangeListIterator *iter = new nsRangeListIterator(this);
    if (!iter)
       return NS_ERROR_OUT_OF_MEMORY;
    *aInstancePtr = (void*) (nsIEnumerator *) iter;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIBidirectionalEnumerator::GetIID())) {
    nsRangeListIterator *iter = new nsRangeListIterator(this);
    if (!iter)
       return NS_ERROR_OUT_OF_MEMORY;
    *aInstancePtr = (void*) (nsIBidirectionalEnumerator *) iter;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIScriptObjectOwner::GetIID())) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  //NS_ASSERTION(PR_FALSE,"bad query interface in nsDOMSelection");
  //do not assert here javascript will assert here sometimes thats OK
  return NS_NOINTERFACE;
}



NS_METHOD
nsDOMSelection::GetAnchorNode(nsIDOMNode** aAnchorNode)
{
	if (!aAnchorNode || !mAnchorFocusRange)
		return NS_ERROR_NULL_POINTER;
  nsresult result;
  if (GetDirection() == eDirNext){
    result = mAnchorFocusRange->GetStartParent(aAnchorNode);
  }
  else{
    result = mAnchorFocusRange->GetEndParent(aAnchorNode);
  }
	return result;
}

NS_METHOD
nsDOMSelection::GetAnchorOffset(PRInt32* aAnchorOffset)
{
	if (!aAnchorOffset || !mAnchorFocusRange)
		return NS_ERROR_NULL_POINTER;
  nsresult result;
  if (GetDirection() == eDirNext){
    result = mAnchorFocusRange->GetStartOffset(aAnchorOffset);
  }
  else{
    result = mAnchorFocusRange->GetEndOffset(aAnchorOffset);
  }
	return result;
}

// note: this can return a nil focus node
NS_METHOD
nsDOMSelection::GetFocusNode(nsIDOMNode** aFocusNode)
{
	if (!aFocusNode || !mAnchorFocusRange)
		return NS_ERROR_NULL_POINTER;
  nsresult result;
  if (GetDirection() == eDirNext){
    result = mAnchorFocusRange->GetEndParent(aFocusNode);
  }
  else{
    result = mAnchorFocusRange->GetStartParent(aFocusNode);
  }

	return result;
}

NS_METHOD nsDOMSelection::GetFocusOffset(PRInt32* aFocusOffset)
{
	if (!aFocusOffset || !mAnchorFocusRange)
		return NS_ERROR_NULL_POINTER;
  nsresult result;
  if (GetDirection() == eDirNext){
    result = mAnchorFocusRange->GetEndOffset(aFocusOffset);
  }
  else{
    result = mAnchorFocusRange->GetStartOffset(aFocusOffset);
  }
	return result;
}


void nsDOMSelection::setAnchorFocusRange(PRInt32 indx)
{
  PRUint32 cnt;
  nsresult rv = mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return;    // XXX error?
  if (((PRUint32)indx) >= cnt )
    return;
  if (indx < 0) //release all
  {
    mAnchorFocusRange = nsCOMPtr<nsIDOMRange>();
  }
  else{
    nsCOMPtr<nsISupports> indexIsupports = dont_AddRef(mRangeArray->ElementAt(indx));
    mAnchorFocusRange = do_QueryInterface(indexIsupports);
  }
}



nsIDOMNode*
nsDOMSelection::FetchAnchorNode()
{  //where did the selection begin
  nsCOMPtr<nsIDOMNode>returnval;
  GetAnchorNode(getter_AddRefs(returnval));//this queries
  return returnval;
}//at end it will release, no addreff was called



PRInt32
nsDOMSelection::FetchAnchorOffset()
{
  PRInt32 returnval;
  if (NS_SUCCEEDED(GetAnchorOffset(&returnval)))//this queries
    return returnval;
  return 0;
}



nsIDOMNode*
nsDOMSelection::FetchOriginalAnchorNode()  //where did the ORIGINAL selection begin
{
  nsCOMPtr<nsIDOMNode>returnval;
  PRInt32 unused;
  GetOriginalAnchorPoint(getter_AddRefs(returnval),  &unused);//this queries
  return returnval;
}



PRInt32
nsDOMSelection::FetchOriginalAnchorOffset()
{
  nsCOMPtr<nsIDOMNode>unused;
  PRInt32 returnval;
  if (NS_SUCCEEDED(GetOriginalAnchorPoint(getter_AddRefs(unused), &returnval)))//this queries
    return returnval;
  return NS_OK;
}



nsIDOMNode*
nsDOMSelection::FetchFocusNode()
{   //where is the carret
  nsCOMPtr<nsIDOMNode>returnval;
  GetFocusNode(getter_AddRefs(returnval));//this queries
  return returnval;
}//at end it will release, no addreff was called



PRInt32
nsDOMSelection::FetchFocusOffset()
{
  PRInt32 returnval;
  if (NS_SUCCEEDED(GetFocusOffset(&returnval)))//this queries
    return returnval;
  return NS_OK;
}



nsIDOMNode*
nsDOMSelection::FetchStartParent(nsIDOMRange *aRange)   //skip all the com stuff and give me the start/end
{
  if (!aRange)
    return nsnull;
  nsCOMPtr<nsIDOMNode> returnval;
  aRange->GetStartParent(getter_AddRefs(returnval));
  return returnval;
}



PRInt32
nsDOMSelection::FetchStartOffset(nsIDOMRange *aRange)
{
  if (!aRange)
    return nsnull;
  PRInt32 returnval;
  if (NS_SUCCEEDED(aRange->GetStartOffset(&returnval)))
    return returnval;
  return 0;
}



nsIDOMNode*
nsDOMSelection::FetchEndParent(nsIDOMRange *aRange)     //skip all the com stuff and give me the start/end
{
  if (!aRange)
    return nsnull;
  nsCOMPtr<nsIDOMNode> returnval;
  aRange->GetEndParent(getter_AddRefs(returnval));
  return returnval;
}



PRInt32
nsDOMSelection::FetchEndOffset(nsIDOMRange *aRange)
{
  if (!aRange)
    return nsnull;
  PRInt32 returnval;
  if (NS_SUCCEEDED(aRange->GetEndOffset(&returnval)))
    return returnval;
  return 0;
}

nsresult
nsDOMSelection::AddItem(nsIDOMRange *aItem)
{
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISupports> isupp;
  nsresult result = aItem->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), getter_AddRefs(isupp)); 
  if (NS_FAILED(result))
    return result;
  result = mRangeArray->AppendElement(isupp);
  return result;
}



nsresult
nsDOMSelection::RemoveItem(nsIDOMRange *aItem)
{
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  if (!aItem )
    return NS_ERROR_NULL_POINTER;
  PRUint32 cnt;
  nsresult rv = mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt;i++)
  {
    nsCOMPtr<nsISupports> indexIsupports = dont_AddRef(mRangeArray->ElementAt(i));
    nsCOMPtr<nsISupports> isupp;
    aItem->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(),getter_AddRefs(isupp));
    if (isupp.get() == indexIsupports.get())
    {
      mRangeArray->RemoveElementAt(i);
      return NS_OK;
    }
  }
  return NS_COMFALSE;
}



nsresult
nsDOMSelection::Clear()
{
  setAnchorFocusRange(-1);
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  // Get an iterator
  while (PR_TRUE)
  {
    PRUint32 cnt;
    nsresult rv = mRangeArray->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    if (cnt == 0)
      break;
    nsCOMPtr<nsIDOMRange> range;
    nsCOMPtr<nsISupports> isupportsindex = dont_AddRef(mRangeArray->ElementAt(0));
    range = do_QueryInterface(isupportsindex);
    mRangeArray->RemoveElementAt(0);
    selectFrames(range, 0);
    // Does RemoveElementAt also delete the elements?
  }

  return NS_OK;
}

//utility method to get the primary frame of node or use the offset to get frame of child node
NS_IMETHODIMP
nsDOMSelection::GetPrimaryFrameForFocusNode(nsIFrame **aReturnFrame)
{
  if (!aReturnFrame)
    return NS_ERROR_NULL_POINTER;
  
  *aReturnFrame = 0;
  
  nsresult	result = NS_OK;
  
  nsCOMPtr<nsIDOMNode> node = dont_QueryInterface(FetchFocusNode());
  if (!node)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContent> content = do_QueryInterface(node, &result);
  if (NS_FAILED(result) || !content)
    return result;
  
  PRBool canContainChildren = PR_FALSE;
  result = content->CanContainChildren(canContainChildren);
  if (NS_SUCCEEDED(result) && canContainChildren)
  {
    PRInt32 offset = FetchFocusOffset();
    if (GetDirection() == eDirNext)
      offset--;
    if (offset >= 0)
    {
      nsCOMPtr<nsIContent> child;
      result = content->ChildAt(offset, *getter_AddRefs(child));
      if (NS_FAILED(result))
        return result;
      if (!child) //out of bounds?
        return NS_ERROR_FAILURE;
      content = child;//releases the focusnode
    }
  }
  result = mRangeList->GetTracker()->GetPrimaryFrameFor(content,aReturnFrame);
  return result;
}


//the idea of this helper method is to select, deselect "top to bottom" traversing through the frames
NS_IMETHODIMP
nsDOMSelection::selectFrames(nsIDOMRange *aRange, PRBool aFlags)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContentIterator> iter;
  nsresult result = nsComponentManager::CreateInstance(kCSubtreeIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), 
                                              getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    iter->Init(aRange);
    // loop through the content iterator for each content node
    // for each text node:
    // get the frame for the content, and from it the style context
    // ask the style context about the property
    nsCOMPtr<nsIContent> content;
    nsIFrame *frame;
//we must call first one explicitly
    content = do_QueryInterface(FetchStartParent(aRange), &result);
    if (NS_FAILED(result) || !content)
      return result;
    PRBool canContainChildren = PR_FALSE;
    result = content->CanContainChildren(canContainChildren);
    if (NS_SUCCEEDED(result) && !canContainChildren)
    {
      result = mRangeList->GetTracker()->GetPrimaryFrameFor(content, &frame);
      if (NS_SUCCEEDED(result) && frame)
        frame->SetSelected(aRange,aFlags,eSpreadDown);//spread from here to hit all frames in flow
    }
//end start content
    result = iter->First();
    if (NS_SUCCEEDED(result))
    {
      while (NS_COMFALSE == iter->IsDone())
      {
        result = iter->CurrentNode(getter_AddRefs(content));
        if (NS_FAILED(result) || !content)
          return result;
        result = mRangeList->GetTracker()->GetPrimaryFrameFor(content, &frame);
        if (NS_SUCCEEDED(result) && frame)
           frame->SetSelected(aRange,aFlags,eSpreadDown);//spread from here to hit all frames in flow
        result = iter->Next();
        if (NS_FAILED(result))
      	  return result;
      }
    }
//we must now do the last one  if it is not the same as the first
    if (FetchEndParent(aRange) != FetchStartParent(aRange))
    {
      content = do_QueryInterface(FetchEndParent(aRange), &result);
      if (NS_FAILED(result) || !content)
        return result;
      canContainChildren = PR_FALSE;
      result = content->CanContainChildren(canContainChildren);
      if (NS_SUCCEEDED(result) && !canContainChildren)
      {
        result = mRangeList->GetTracker()->GetPrimaryFrameFor(content, &frame);
        if (NS_SUCCEEDED(result) && frame)
           frame->SetSelected(aRange,aFlags,eSpreadDown);//spread from here to hit all frames in flow
      }
    }
//end end parent
  }
  return result;
}


NS_IMETHODIMP
nsDOMSelection::LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                           SelectionDetails **aReturnDetails, SelectionType aType)
{
  PRInt32 cnt;
  nsresult rv = GetRangeCount(&cnt);
  if (NS_FAILED(rv)) 
    return rv;
  PRInt32 i;

  nsCOMPtr<nsIDOMNode> passedInNode;
  passedInNode = do_QueryInterface(aContent);
  if (!passedInNode)
    return NS_ERROR_FAILURE;

  SelectionDetails *details = nsnull;
  SelectionDetails *newDetails = details;

  for (i =0; i<cnt; i++){
    nsCOMPtr<nsIDOMRange> range;
    nsCOMPtr<nsISupports> isupportsindex = dont_AddRef(mRangeArray->ElementAt(i));
    range = do_QueryInterface(isupportsindex);
    if (range){
      nsCOMPtr<nsIDOMNode> startNode;
      nsCOMPtr<nsIDOMNode> endNode;
      PRInt32 startOffset;
      PRInt32 endOffset;
      range->GetStartParent(getter_AddRefs(startNode));
      range->GetStartOffset(&startOffset);
      range->GetEndParent(getter_AddRefs(endNode));
      range->GetEndOffset(&endOffset);
      if (passedInNode == startNode && passedInNode == endNode){
        if (startOffset < (aContentOffset + aContentLength)  &&
            endOffset > aContentOffset){
          if (!details){
            details = new SelectionDetails;

            newDetails = details;
          }
          else{
            newDetails->mNext = new SelectionDetails;
            newDetails = newDetails->mNext;
          }
          if (!newDetails)
            return NS_ERROR_OUT_OF_MEMORY;
          newDetails->mNext = nsnull;
          newDetails->mStart = PR_MAX(0,startOffset - aContentOffset);
          newDetails->mEnd = PR_MIN(aContentLength, endOffset - aContentOffset);
          newDetails->mType = aType;
        }
      }
      else if (passedInNode == startNode){
        if (startOffset < (aContentOffset + aContentLength)){
          if (!details){
            details = new SelectionDetails;
            newDetails = details;
          }
          else{
            newDetails->mNext = new SelectionDetails;
            newDetails = newDetails->mNext;
          }
          if (!newDetails)
            return NS_ERROR_OUT_OF_MEMORY;
          newDetails->mNext = nsnull;
          newDetails->mStart = PR_MAX(0,startOffset - aContentOffset);
          newDetails->mEnd = aContentLength;
          newDetails->mType = aType;
        }
      }
      else if (passedInNode == endNode){
        if (endOffset > aContentOffset){
          if (!details){
            details = new SelectionDetails;
            newDetails = details;
          }
          else{
            newDetails->mNext = new SelectionDetails;
            newDetails = newDetails->mNext;
          }
          if (!newDetails)
            return NS_ERROR_OUT_OF_MEMORY;
          newDetails->mNext = nsnull;
          newDetails->mStart = 0;
          newDetails->mEnd = PR_MIN(aContentLength, endOffset - aContentOffset);
          newDetails->mType = aType;
        }
      }
      else {
        if (cnt > 1){
          //we only have to look at start offset because anything else would have been in the range
          PRInt32 resultnum = ComparePoints(startNode, startOffset 
                                  ,passedInNode, aContentOffset);
          if (resultnum > 0)
            continue; 
          resultnum = ComparePoints(endNode, endOffset,
                              passedInNode, aContentOffset );
          if (resultnum <0)
            continue;
        }
        if (!details){
          details = new SelectionDetails;
          newDetails = details;
        }
        else{
          newDetails->mNext = new SelectionDetails;
          newDetails = newDetails->mNext;
        }
        if (!newDetails)
          return NS_ERROR_OUT_OF_MEMORY;
        newDetails->mNext = nsnull;
        newDetails->mStart = 0;
        newDetails->mEnd = aContentLength;
        newDetails->mType = aType;
      }
    }
    else
      continue;
  }
  if (*aReturnDetails && newDetails)
    newDetails->mNext = *aReturnDetails;
  if (details)
    *aReturnDetails = details;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMSelection::Repaint()
{
  PRUint32 arrCount = 0;

  if (!mRangeArray)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISupports> isupp;
  nsCOMPtr<nsIDOMRange> range;

  nsresult result = mRangeArray->Count(&arrCount);

  if (NS_FAILED(result))
    return result;

  if (arrCount < 1)
    return NS_OK;

  PRUint32 i;

  for (i = 0; i < arrCount; i++)
  {
    result = mRangeArray->GetElementAt(i, getter_AddRefs(isupp));

    if (NS_FAILED(result))
      return result;

    if (!isupp)
      return NS_ERROR_NULL_POINTER;

    range = do_QueryInterface(isupp);

    if (!range)
      return NS_ERROR_NULL_POINTER;

    result = selectFrames(range, PR_TRUE);

    if (NS_FAILED(result))
      return result;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMSelection::GetEnumerator(nsIEnumerator **aIterator)
{
  nsRangeListIterator *iterator =  new nsRangeListIterator(this);
  if (iterator){
    *aIterator = (nsIEnumerator *)iterator;
    return iterator->AddRef();
  }
  else 
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}



/** ClearSelection zeroes the selection
 */
NS_IMETHODIMP
nsDOMSelection::ClearSelection()
{
  nsresult	result = Clear();
  if (NS_FAILED(result))
  	return result;
  	
  return mRangeList->NotifySelectionListeners();
  // Also need to notify the frames!
  // PresShell::ContentChanged should do that on DocumentChanged
}

/** AddRange adds the specified range to the selection
 *  @param aRange is the range to be added
 */
NS_IMETHODIMP
nsDOMSelection::AddRange(nsIDOMRange* aRange)
{
  nsresult      result = AddItem(aRange);
  
  if (NS_FAILED(result))
    return result;
  PRInt32 count;
  result = GetRangeCount(&count);
  if (NS_FAILED(result))
    return result;
  if (count <= 0)
  {
    NS_ASSERTION(0,"bad count after additem\n");
    return NS_ERROR_FAILURE;
  }
  setAnchorFocusRange(count -1);
  selectFrames(aRange, PR_TRUE);        
  ScrollIntoView();

  return mRangeList->NotifySelectionListeners();
}


/*
 * Collapse sets the whole selection to be one point.
 */
NS_IMETHODIMP
nsDOMSelection::Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset)
{
  if (!aParentNode)
    return NS_ERROR_INVALID_ARG;

  nsresult result;
  mRangeList->InvalidateDesiredX();
  // Delete all of the current ranges
  if (NS_FAILED(SetOriginalAnchorPoint(aParentNode,aOffset)))
    return NS_ERROR_FAILURE; //???
  Clear();

  nsCOMPtr<nsIDOMRange> range;
  result = nsComponentManager::CreateInstance(kRangeCID, nsnull,
                                     nsIDOMRange::GetIID(),
                                     getter_AddRefs(range));
  if (NS_FAILED(result))
    return result;

  if (! range){
    NS_ASSERTION(PR_FALSE,"Create Instance Failed nsRangeList::Collapse");
    return NS_ERROR_UNEXPECTED;
  }
  result = range->SetEnd(aParentNode, aOffset);
  if (NS_FAILED(result))
    return result;
  result = range->SetStart(aParentNode, aOffset);
  if (NS_FAILED(result))
    return result;

#ifdef DEBUG_SELECTION
  if (aParentNode)
  {
    nsCOMPtr<nsIContent>content;
    content = do_QueryInterface(aParentNode);
    if (!content)
      return NS_ERROR_FAILURE;
    nsIAtom *tag;
    content->GetTag(tag);
    if (tag)
    {
	    nsString tagString;
	    tag->ToString(tagString);
	    char * tagCString = tagString.ToNewCString();
	    printf ("Sel. Collapse to %p %s %d\n", content, tagCString, aOffset);
	    delete [] tagCString;
    }
  }
  else {
    printf ("Sel. Collapse set to null parent.\n");
  }
#endif


  result = AddItem(range);
  setAnchorFocusRange(0);
  selectFrames(range,PR_TRUE);
  if (NS_FAILED(result))
    return result;
    
	return mRangeList->NotifySelectionListeners();
}

/*
 * IsCollapsed -- is the whole selection just one point, or unset?
 */
NS_IMETHODIMP
nsDOMSelection::GetIsCollapsed(PRBool* aIsCollapsed)
{
	if (!aIsCollapsed)
		return NS_ERROR_NULL_POINTER;
		
  PRUint32 cnt = 0;
  if (mRangeArray) {
    nsresult rv = mRangeArray->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
  }
  if (!mRangeArray || (cnt == 0))
  {
    *aIsCollapsed = PR_TRUE;
    return NS_OK;
  }
  
  if (cnt != 1)
  {
    *aIsCollapsed = PR_FALSE;
    return NS_OK;
  }
  
  nsCOMPtr<nsISupports> nsisup(dont_AddRef(mRangeArray->ElementAt(0)));
  nsCOMPtr<nsIDOMRange> range;
  nsresult rv;
  range = do_QueryInterface(nsisup,&rv);
  if (NS_FAILED(rv))
  {
    return rv;
  }
                             
  return (range->GetIsCollapsed(aIsCollapsed));
}

NS_IMETHODIMP
nsDOMSelection::GetRangeCount(PRInt32* aRangeCount)
{
  if (!aRangeCount) 
		return NS_ERROR_NULL_POINTER;

	if (mRangeArray)
	{
		PRUint32 cnt;
    nsresult rv = mRangeArray->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    *aRangeCount = cnt;
	}
	else
	{
		*aRangeCount = 0;
	}
	
	return NS_OK;
}

NS_IMETHODIMP
nsDOMSelection::GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn)
{
	if (!aReturn)
		return NS_ERROR_NULL_POINTER;

  if (!mRangeArray)
		return NS_ERROR_INVALID_ARG;
		
	PRUint32 cnt;
  nsresult rv = mRangeArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
	if (aIndex < 0 || ((PRUint32)aIndex) >= cnt)
		return NS_ERROR_INVALID_ARG;

	// the result of all this is one additional ref on the item, as
	// the caller would expect.
	//
	// ElementAt addrefs once
	// do_QueryInterface addrefs once
	// when the COMPtr goes out of scope, it releases.
	//
	nsISupports*	element = mRangeArray->ElementAt((PRUint32)aIndex);
	nsCOMPtr<nsIDOMRange>	foundRange = do_QueryInterface(element);
	*aReturn = foundRange;
	
	return NS_OK;
}


//may change parameters may not.
//return NS_ERROR_FAILURE if invalid new selection between anchor and passed in parameters
NS_IMETHODIMP
nsDOMSelection::FixupSelectionPoints(nsIDOMRange *aRange , nsDirection *aDir, PRBool *aFixupState)
{
  if (!aRange || !aFixupState)
    return NS_ERROR_NULL_POINTER;
  *aFixupState = PR_FALSE;
  nsresult res;

  //startNode is the beginning or "anchor" of the range
  //end Node is the end or "focus of the range
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset;
  PRInt32 endOffset;
  nsresult result;
  if (*aDir == eDirNext)
  {
    if (NS_FAILED(GetOriginalAnchorPoint(getter_AddRefs(startNode), &startOffset)))
    {
      aRange->GetStartParent(getter_AddRefs(startNode));
      aRange->GetStartOffset(&startOffset);
    }
    aRange->GetEndParent(getter_AddRefs(endNode));
    aRange->GetEndOffset(&endOffset);
  }
  else
  {
    if (NS_FAILED(GetOriginalAnchorPoint(getter_AddRefs(startNode), &startOffset)))
    {
      aRange->GetEndParent(getter_AddRefs(startNode));
      aRange->GetEndOffset(&startOffset);
    }
    aRange->GetStartParent(getter_AddRefs(endNode));
    aRange->GetStartOffset(&endOffset);
  }
  if (!startNode || !endNode)
    return NS_ERROR_FAILURE;

  // if end node is a tbody then all bets are off we cannot select "rows"
  nsCOMPtr<nsIAtom> atom;
  atom = GetTag(endNode);
  if (atom.get() == nsRangeList::sTbodyAtom)
    return NS_ERROR_FAILURE; //cannot select INTO row node ony cells

  //get common parent
  nsCOMPtr<nsIDOMNode> parent;
  res = aRange->GetCommonParent(getter_AddRefs(parent));
  if (NS_FAILED(res) || !parent)
    return res;
 
  //look for dest. if you see a cell you are in "cell mode"
  //if you see a table you select "whole" table

  //src first 
  nsCOMPtr<nsIDOMNode> tempNode;
  nsCOMPtr<nsIDOMNode> tempNode2;
  PRBool cellMode = PR_FALSE;
  PRBool dirty = PR_FALSE;
  if (startNode != endNode)
  {
    if (parent != startNode)
    {
      result = startNode->GetParentNode(getter_AddRefs(tempNode));
      if (NS_FAILED(result) || !tempNode)
        return NS_ERROR_FAILURE;
      while (tempNode != parent)
      {
        atom = GetTag(tempNode);
        if (atom.get() == nsRangeList::sTableAtom) //select whole table  if in cell mode, wait for cell
        {
          result = ParentOffset(tempNode, getter_AddRefs(startNode), &startOffset);
          if (NS_FAILED(result))
            return NS_ERROR_FAILURE;
          if (*aDir == eDirPrevious) //select after
            startOffset++;
          dirty = PR_TRUE;
          cellMode = PR_FALSE;
        }
        else if (atom.get() == nsRangeList::sCellAtom) //you are in "cell" mode put selection to end of cell
        {
          cellMode = PR_TRUE;
          result = ParentOffset(tempNode, getter_AddRefs(startNode), &startOffset);
          if (NS_FAILED(result))
            return result;
          if (*aDir == eDirPrevious) //select after
            startOffset++;
          dirty = PR_TRUE;
        }
        result = tempNode->GetParentNode(getter_AddRefs(tempNode2));
        if (NS_FAILED(result) || !tempNode2)
          return NS_ERROR_FAILURE;
        tempNode = tempNode2;
      }
    }
  
  //now for dest node
    if (parent != endNode)
    {
      result = endNode->GetParentNode(getter_AddRefs(tempNode));
      PRBool found = !cellMode;
      if (NS_FAILED(result) || !tempNode)
        return NS_ERROR_FAILURE;
      while (tempNode != parent)
      {
        atom = GetTag(tempNode);
        if (atom.get() == nsRangeList::sTableAtom) //select whole table  if in cell mode, wait for cell
        {
          if (!cellMode)
          {
            result = ParentOffset(tempNode, getter_AddRefs(endNode), &endOffset);
            if (NS_FAILED(result))
              return result;
            if (*aDir == eDirNext) //select after
              endOffset++;
            dirty = PR_TRUE;
          }
          else
            found = PR_FALSE; //didnt find the right cell yet
        }
        else if (atom.get() == nsRangeList::sCellAtom) //you are in "cell" mode put selection to end of cell
        {
          result = ParentOffset(tempNode, getter_AddRefs(endNode), &endOffset);
          if (NS_FAILED(result))
            return result;
          if (*aDir == eDirNext) //select after
            endOffset++;
          found = PR_TRUE;
          dirty = PR_TRUE;
        }
        result = tempNode->GetParentNode(getter_AddRefs(tempNode2));
        if (NS_FAILED(result) || !tempNode2)
          return NS_ERROR_FAILURE;
        tempNode = tempNode2;
      }
      if (!found)
        return NS_ERROR_FAILURE;
    }
  }
  if (FetchAnchorNode() != startNode.get() || startOffset != FetchAnchorOffset())
    dirty = PR_TRUE; //something has changed we are dirty no matter what
  if (dirty && *aDir != mDirection) //fixup took place but new direction all bets are off
  {
    *aFixupState = PR_TRUE;
    mFixupState = PR_FALSE;
  }
  else
    if (startNode.get() != FetchOriginalAnchorNode() && PR_TRUE == mFixupState) //no longer a fixup
    {
      *aFixupState = PR_TRUE;
      mFixupState = PR_FALSE;
    }
    else
    {
      mFixupState = dirty;
      *aFixupState = dirty;
    }
  if (dirty){
    if (*aDir == eDirNext)
    {
      if (NS_FAILED(aRange->SetStart(startNode,startOffset)) || NS_FAILED(aRange->SetEnd(endNode, endOffset)))
      {
        *aDir = eDirPrevious;
        aRange->SetStart(endNode, endOffset);
        aRange->SetEnd(startNode, startOffset);
      }
    }
    else
    {
      if (NS_FAILED(aRange->SetStart(endNode,endOffset)) || NS_FAILED(aRange->SetEnd(startNode, startOffset)))
      {
        *aDir = eDirNext;
        aRange->SetStart(startNode, startOffset);
        aRange->SetEnd(endNode, endOffset);
      }
    }
  }
  return NS_OK;
}



NS_IMETHODIMP
nsDOMSelection::SetOriginalAnchorPoint(nsIDOMNode *aNode, PRInt32 aOffset)
{
  if (!aNode){
    mOriginalAnchorRange = 0;
    return NS_OK;
  }
  nsCOMPtr<nsIDOMRange> newRange;
  nsresult result;
  result = nsComponentManager::CreateInstance(kRangeCID, nsnull,
                                     nsIDOMRange::GetIID(),
                                     getter_AddRefs(newRange));
  result = newRange->SetStart(aNode,aOffset);
  if (NS_FAILED(result))
    return result;
  result = newRange->SetEnd(aNode,aOffset);
  if (NS_FAILED(result))
    return result;

  mOriginalAnchorRange = newRange;
  return result;
}



NS_IMETHODIMP
nsDOMSelection::GetOriginalAnchorPoint(nsIDOMNode **aNode, PRInt32 *aOffset)
{
  if (!aNode || !aOffset || !mOriginalAnchorRange)
    return NS_ERROR_NULL_POINTER;
  nsresult result;
  result = mOriginalAnchorRange->GetStartParent(aNode);
  if (NS_FAILED(result))
    return result;
  result = mOriginalAnchorRange->GetStartOffset(aOffset);
  return result;
}



/*
Notes which might come in handy for extend:

We can tell the direction of the selection by asking for the anchors selection
if the begin is less than the end then we know the selection is to the "right".
else it is a backwards selection.
a = anchor
1 = old cursor
2 = new cursor

  if (a <= 1 && 1 <=2)    a,1,2  or (a1,2)
  if (a < 2 && 1 > 2)     a,2,1
  if (1 < a && a <2)      1,a,2
  if (a > 2 && 2 >1)      1,2,a
  if (2 < a && a <1)      2,a,1
  if (a > 1 && 1 >2)      2,1,a
then execute
a  1  2 select from 1 to 2
a  2  1 deselect from 2 to 1
1  a  2 deselect from 1 to a select from a to 2
1  2  a deselect from 1 to 2
2  1  a = continue selection from 2 to 1
*/


/*
 * Extend extends the selection away from the anchor.
 * We don't need to know the direction, because we always change the focus.
 */
NS_IMETHODIMP
nsDOMSelection::Extend(nsIDOMNode* aParentNode, PRInt32 aOffset)
{
  if (!aParentNode)
    return NS_ERROR_INVALID_ARG;

  // First, find the range containing the old focus point:
  if (!mRangeArray || !mAnchorFocusRange)
    return NS_ERROR_NOT_INITIALIZED;
  mRangeList->InvalidateDesiredX();
  nsCOMPtr<nsIDOMRange> difRange;
  nsresult res;
  res = nsComponentManager::CreateInstance(kRangeCID, nsnull,
                                     nsIDOMRange::GetIID(),
                                     getter_AddRefs(difRange));


  if (NS_FAILED(res)) 
    return res;
  nsCOMPtr<nsIDOMRange> range;
  res = mAnchorFocusRange->Clone(getter_AddRefs(range));

  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset;
  PRInt32 endOffset;

  range->GetStartParent(getter_AddRefs(startNode));
  range->GetEndParent(getter_AddRefs(endNode));
  range->GetStartOffset(&startOffset);
  range->GetEndOffset(&endOffset);


  nsDirection dir = GetDirection();
  PRBool fixupState; //if there was a previous fixup the optimal drawing erasing will NOT work
  if (NS_FAILED(res))
    return res;

  res = nsComponentManager::CreateInstance(kRangeCID, nsnull,
                                     nsIDOMRange::GetIID(),
                                     getter_AddRefs(difRange));

  if (NS_FAILED(res))
    return res;
  //compare anchor to old cursor.

  if (NS_FAILED(res))
    return res;
  PRInt32 result1 = ComparePoints(FetchAnchorNode(), FetchAnchorOffset() 
                                  ,FetchFocusNode(), FetchFocusOffset());
  //compare old cursor to new cursor
  PRInt32 result2 = ComparePoints(FetchFocusNode(), FetchFocusOffset(),
                            aParentNode, aOffset );
  //compare anchor to new cursor
  PRInt32 result3 = ComparePoints(FetchAnchorNode(), FetchAnchorOffset(),
                            aParentNode , aOffset );

  if (result2 == 0) //not selecting anywhere
    return NS_OK;

  if ((result1 == 0 && result3 < 0) || (result1 <= 0 && result2 < 0)){//a1,2  a,1,2
    //select from 1 to 2 unless they are collapsed
    res = range->SetEnd(aParentNode,aOffset);
    if (NS_FAILED(res))
      return res;
    dir = eDirNext;
    res = FixupSelectionPoints(range, &dir, &fixupState);
    if (NS_FAILED(res))
      return res;
    if (fixupState) //unselect previous and select new state has changed to not fixed up
    {
      selectFrames(mAnchorFocusRange, PR_FALSE);
      selectFrames(range, PR_TRUE);
    }
    else{
      res = difRange->SetEnd(FetchEndParent(range), FetchEndOffset(range));
      res |= difRange->SetStart(FetchFocusNode(), FetchFocusOffset());
      if (NS_FAILED(res))
        return res;
      selectFrames(difRange , PR_TRUE);
    }
  }
  else if (result1 == 0 && result3 > 0){//2, a1
    //select from 2 to 1a
    dir = eDirPrevious;
    res = range->SetStart(aParentNode,aOffset);
    if (NS_FAILED(res))
      return res;
    res = FixupSelectionPoints(range, &dir, &fixupState);
    if (NS_FAILED(res))
      return res;
    if (fixupState) //unselect previous and select new state has changed to not fixed up
    {
      selectFrames(mAnchorFocusRange, PR_FALSE);
      selectFrames(range, PR_TRUE);
    }
    else
      selectFrames(range, PR_TRUE);
  }
  else if (result3 <= 0 && result2 >= 0) {//a,2,1 or a2,1 or a,21 or a21
    //deselect from 2 to 1
    res = difRange->SetEnd(FetchFocusNode(), FetchFocusOffset());
    res |= difRange->SetStart(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;

    dir = eDirNext;
    res = range->SetEnd(aParentNode,aOffset);
    if (NS_FAILED(res))
      return res;
    res = FixupSelectionPoints(range, &dir, &fixupState);
    if (NS_FAILED(res))
      return res;
    if (fixupState) //unselect previous and select new state has changed to not fixed up
    {
      selectFrames(mAnchorFocusRange, PR_FALSE);
      selectFrames(range, PR_TRUE);
    }
    else {
      selectFrames(difRange, 0);//deselect now if fixup succeeded
      difRange->SetEnd(FetchEndParent(range),FetchEndOffset(range));
      selectFrames(difRange, PR_TRUE);//must reselect last node maybe more if fixup did something
    }
  }
  else if (result1 >= 0 && result3 <= 0) {//1,a,2 or 1a,2 or 1,a2 or 1a2
    if (GetDirection() == eDirPrevious){
      res = range->SetStart(endNode,endOffset);
      if (NS_FAILED(res))
        return res;
    }
    dir = eDirNext;
    res = range->SetEnd(aParentNode,aOffset);
    if (NS_FAILED(res))
      return res;
    res = FixupSelectionPoints(range, &dir, &fixupState);
    if (NS_FAILED(res))
      return res;
    if (fixupState) //unselect previous and select new state has changed to not fixed up
    {
      selectFrames(mAnchorFocusRange, PR_FALSE);
      selectFrames(range, PR_TRUE);
    }
    else {
      if (FetchFocusNode() != FetchAnchorNode() || FetchFocusOffset() != FetchAnchorOffset() ){//if collapsed diff dont do anything
        res = difRange->SetStart(FetchFocusNode(), FetchFocusOffset());
        res |= difRange->SetEnd(FetchAnchorNode(), FetchAnchorOffset());
        if (NS_FAILED(res))
          return res;
        //deselect from 1 to a
        selectFrames(difRange , PR_FALSE);
      }
      //select from a to 2
      selectFrames(range , PR_TRUE);
    }
  }
  else if (result2 <= 0 && result3 >= 0) {//1,2,a or 12,a or 1,2a or 12a
    //deselect from 1 to 2
    res = difRange->SetEnd(aParentNode, aOffset);
    res |= difRange->SetStart(FetchFocusNode(), FetchFocusOffset());
    if (NS_FAILED(res))
      return res;
    dir = eDirPrevious;
    res = range->SetStart(aParentNode,aOffset);
    if (NS_FAILED(res))
      return res;

    res = FixupSelectionPoints(range, &dir, &fixupState);
    if (NS_FAILED(res))
      return res;
    if (fixupState) //unselect previous and select new state has changed to not fixed up
    {
      selectFrames(mAnchorFocusRange, PR_FALSE);
      selectFrames(range, PR_TRUE);
    }
    else {
      selectFrames(difRange , PR_FALSE);
      difRange->SetStart(FetchStartParent(range),FetchStartOffset(range));
      selectFrames(difRange, PR_TRUE);//must reselect last node
    }
  }
  else if (result3 >= 0 && result1 <= 0) {//2,a,1 or 2a,1 or 2,a1 or 2a1
    if (GetDirection() == eDirNext){
      range->SetEnd(startNode,startOffset);
    }
    dir = eDirPrevious;
    res = range->SetStart(aParentNode,aOffset);
    if (NS_FAILED(res))
      return res;
    res = FixupSelectionPoints(range, &dir, &fixupState);
    if (NS_FAILED(res))
      return res;
    if (fixupState) //unselect previous and select new state has changed to not fixed up
    {
      selectFrames(mAnchorFocusRange, PR_FALSE);
      selectFrames(range, PR_TRUE);
    }
    else
    {
      //deselect from a to 1
      if (FetchFocusNode() != FetchAnchorNode() || FetchFocusOffset() != FetchAnchorOffset() ){//if collapsed diff dont do anything
        res = difRange->SetStart(FetchAnchorNode(), FetchAnchorOffset());
        res |= difRange->SetEnd(FetchFocusNode(), FetchFocusOffset());
        selectFrames(difRange, 0);
      }
      //select from 2 to a
      selectFrames(range , PR_TRUE);
    }
  }
  else if (result2 >= 0 && result1 >= 0) {//2,1,a or 21,a or 2,1a or 21a
    //select from 2 to 1
    res = range->SetStart(aParentNode,aOffset);
    if (NS_FAILED(res))
      return res;
    dir = eDirPrevious;
    res = FixupSelectionPoints(range, &dir, &fixupState);
    if (NS_FAILED(res))
      return res;
    if (fixupState) //unselect previous and select new state has changed to not fixed up
    {
      selectFrames(mAnchorFocusRange, PR_FALSE);
      selectFrames(range, PR_TRUE);
    }
    else {
      res = difRange->SetEnd(FetchFocusNode(), FetchFocusOffset());
      res |= difRange->SetStart(FetchStartParent(range), FetchStartOffset(range));
      if (NS_FAILED(res))
        return res;
      selectFrames(difRange, PR_TRUE);
    }
  }

  DEBUG_OUT_RANGE(range);
#if 0
  if (eDirNext == mDirection)
    printf("    direction = 1  LEFT TO RIGHT\n");
  else
    printf("    direction = 0  RIGHT TO LEFT\n");
#endif
  SetDirection(dir);
  /*hack*/
  range->GetStartParent(getter_AddRefs(startNode));
  range->GetEndParent(getter_AddRefs(endNode));
  range->GetStartOffset(&startOffset);
  range->GetEndOffset(&endOffset);
  if (NS_FAILED(mAnchorFocusRange->SetStart(startNode,startOffset)))
  {
    if (NS_FAILED(mAnchorFocusRange->SetEnd(endNode,endOffset)))
      return NS_ERROR_FAILURE;//???
    if (NS_FAILED(mAnchorFocusRange->SetStart(startNode,startOffset)))
      return NS_ERROR_FAILURE;//???
  }
  else if (NS_FAILED(mAnchorFocusRange->SetEnd(endNode,endOffset)))
          return NS_ERROR_FAILURE;//???
  /*end hack*/
  ScrollIntoView();
#ifdef DEBUG_SELECTION
  if (aParentNode)
  {
    nsCOMPtr<nsIContent>content;
    content = do_QueryInterface(aParentNode);
    nsIAtom *tag;
    content->GetTag(tag);
    if (tag)
    {
	    nsString tagString;
	    tag->ToString(tagString);
	    char * tagCString = tagString.ToNewCString();
	    printf ("Sel. Extend to %p %s %d\n", content, tagCString, aOffset);
	    delete [] tagCString;
    }
  }
  else {
    printf ("Sel. Extend set to null parent.\n");
  }
#endif
  return mRangeList->NotifySelectionListeners();
}

NS_IMETHODIMP
nsDOMSelection::ContainsNode(nsIDOMNode* aNode, PRBool aRecursive, PRBool* aYes)
{
  if (!aYes)
    return NS_ERROR_NULL_POINTER;
  *aYes = PR_FALSE;

  // Iterate over the ranges in the selection checking for the content:
  if (!mRangeArray)
    return NS_OK;

  PRUint32 cnt;
  nsresult rv = mRangeArray->Count(&cnt);
  if (NS_FAILED(rv))
    return rv;
  for (PRUint32 i=0; i < cnt; ++i)
  {
    nsISupports* element = mRangeArray->ElementAt(i);
    nsCOMPtr<nsIDOMRange>	range = do_QueryInterface(element);
    if (!range)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIContent> content (do_QueryInterface(aNode));
    if (content)
    {
      if (IsNodeIntersectsRange(content, range))
      {
        // If recursive, then we're done -- IsNodeIntersectsRange does the right thing
        if (aRecursive)
        {
          *aYes = PR_TRUE;
          return NS_OK;
        }

        // else not recursive -- node itself must be contained,
        // so we need to do more checking
        PRBool nodeStartsBeforeRange, nodeEndsAfterRange;
        if (NS_SUCCEEDED(CompareNodeToRange(content, range,
                                            &nodeStartsBeforeRange,
                                            &nodeEndsAfterRange)))
        {
#ifdef DEBUG_ContainsNode
          nsAutoString name, value;
          aNode->GetNodeName(name);
          aNode->GetNodeValue(value);
          printf("%s [%s]: %d, %d\n", name.ToNewCString(), value.ToNewCString(),
                 nodeStartsBeforeRange, nodeEndsAfterRange);
#endif
          PRUint16 nodeType;
          aNode->GetNodeType(&nodeType);
          if ((!nodeStartsBeforeRange && !nodeEndsAfterRange)
              || (nodeType == nsIDOMNode::TEXT_NODE))
          {
            *aYes = PR_TRUE;
            return NS_OK;
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsDOMSelection::GetPresShell(nsIPresShell **aPresShell)
{
  nsresult rv = NS_OK;

  nsIFocusTracker *tracker = mRangeList->GetTracker();

  if (!tracker)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIPresContext> presContext;

  rv = tracker->GetPresContext(getter_AddRefs(presContext));

  if (NS_FAILED(rv))
    return rv;

  if (!presContext)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIPresShell> presShell;

  rv = presContext->GetShell(aPresShell);

  return rv;
}

nsresult
nsDOMSelection::GetRootScrollableView(nsIScrollableView **aScrollableView)
{
  //
  // NOTE: This method returns a NON-AddRef'd pointer
  //       to the scrollable view!
  //

  nsresult rv = NS_OK;

  nsCOMPtr<nsIPresShell> presShell;

  rv = GetPresShell(getter_AddRefs(presShell));

  if (NS_FAILED(rv))
    return rv;

  if (!presShell)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIViewManager> viewManager;

  rv = presShell->GetViewManager(getter_AddRefs(viewManager));

  if (NS_FAILED(rv))
    return rv;

  if (!viewManager)
    return NS_ERROR_NULL_POINTER;

  //
  // nsIViewManager::GetRootScrollableView() does not
  // AddRef the pointer it returns.
  //
  return viewManager->GetRootScrollableView(aScrollableView);
}

nsresult
nsDOMSelection::GetFrameToRootViewOffset(nsIFrame *aFrame, nscoord *aX, nscoord *aY)
{
  nsresult rv = NS_OK;

  if (!aFrame || !aX || !aY) {
    return NS_ERROR_NULL_POINTER;
  }

  *aX = 0;
  *aY = 0;

  nsIScrollableView* scrollingView = 0;

  rv = GetRootScrollableView(&scrollingView);

  if (NS_FAILED(rv))
    return rv;

  if (!scrollingView)
    return NS_ERROR_NULL_POINTER;

  nsIView*  scrolledView;
  nsPoint   offset;
  nsIView*  closestView;
          
  // Determine the offset from aFrame to the scrolled view. We do that by
  // getting the offset from its closest view and then walking up
  scrollingView->GetScrolledView(scrolledView);
  aFrame->GetOffsetFromView(offset, &closestView);

  // XXX Deal with the case where there is a scrolled element, e.g., a
  // DIV in the middle...
  while ((closestView != nsnull) && (closestView != scrolledView)) {
    nscoord dx, dy;

    // Update the offset
    closestView->GetPosition(&dx, &dy);
    offset.MoveBy(dx, dy);

    // Get its parent view
    closestView->GetParent(closestView);
  }

  *aX = offset.x;
  *aY = offset.y;

  return rv;
}

nsresult
nsDOMSelection::GetPointFromOffset(nsIFrame *aFrame, PRInt32 aContentOffset, nsPoint *aPoint)
{
  nsresult rv = NS_OK;
  if (!aFrame || !aPoint)
    return NS_ERROR_NULL_POINTER;

  aPoint->x = 0;
  aPoint->y = 0;

  //
  // Retrieve the device context. We need one to create
  // a rendering context.
  //

  nsIFocusTracker *tracker = mRangeList->GetTracker();

  if (!tracker)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIPresContext> presContext;

  rv = tracker->GetPresContext(getter_AddRefs(presContext));

  if (NS_FAILED(rv))
    return rv;

  if (!presContext)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDeviceContext> deviceContext;

	rv = presContext->GetDeviceContext(getter_AddRefs(deviceContext));

	if (NS_FAILED(rv))
		return rv;

  if (!deviceContext)
		return NS_ERROR_NULL_POINTER;

  //
  // Now get the closest view with a widget so we can create
  // a rendering context.
  //

  nsCOMPtr<nsIWidget> widget;
  nsIView *closestView = 0;
  nsPoint offset(0, 0);

  rv = aFrame->GetOffsetFromView(offset, &closestView);

  while (!widget && closestView)
  {
    rv = closestView->GetWidget(*getter_AddRefs(widget));

    if (NS_FAILED(rv))
      return rv;

    if (!widget)
    {
      rv = closestView->GetParent(closestView);

      if (NS_FAILED(rv))
        return rv;
    }
  }

  if (!closestView)
    return NS_ERROR_FAILURE;

  //
  // Create a rendering context. This context is used by text frames
  // to calculate text widths so it can figure out where the point is
  // in the frame.
  //

	nsCOMPtr<nsIRenderingContext> rendContext;

	rv = deviceContext->CreateRenderingContext(closestView, *getter_AddRefs(rendContext));		
  
	if (NS_FAILED(rv))
		return rv;

  if (!rendContext)
		return NS_ERROR_NULL_POINTER;

  //
  // Now get the point and return!
  //

	rv = aFrame->GetPointFromOffset(presContext, rendContext, aContentOffset, aPoint);

  return rv;
}

nsresult
nsDOMSelection::GetFocusNodeRect(nsRect *aRect)
{
  nsresult result = NS_OK;

  if (!aRect)
    return NS_ERROR_NULL_POINTER;

  // Init aRect:

  aRect->x = 0;
  aRect->y = 0;
  aRect->width  = 0;
  aRect->height = 0;

  nsIDOMNode *focusNode = FetchFocusNode();

  if (!focusNode)
    return NS_ERROR_NULL_POINTER;

  PRInt32 focusOffset = FetchFocusOffset();

  PRUint16 focusNodeType = nsIDOMNode::ELEMENT_NODE;

  result = focusNode->GetNodeType(&focusNodeType);

  if (NS_FAILED(result))
    return NS_ERROR_NULL_POINTER;

  nsIFrame *frame = 0;

  result = GetPrimaryFrameForFocusNode(&frame);

  if (NS_FAILED(result))
    return result;

  if (!frame)
    return NS_ERROR_NULL_POINTER;

  if (focusNodeType == nsIDOMNode::TEXT_NODE)
  {
    nsIFrame *childFrame = 0;
    PRInt32 frameOffset  = 0;

    result = frame->GetChildFrameContainingOffset(focusOffset, &frameOffset, &childFrame);

    if (NS_FAILED(result))
      return result;

    if (!childFrame)
      return NS_ERROR_NULL_POINTER;

    frame = childFrame;

    //
    // Get the x coordinate of the offset into the text frame.
    // The x coordinate will be relative to the frame's origin,
    // so we'll have to translate it into the root view's coordinate
    // system.
    //
    nsPoint pt;

    result = GetPointFromOffset(frame, focusOffset, &pt);

    if (NS_FAILED(result))
      return result;
    
    //
    // Get the frame's rect.
    //
    result = frame->GetRect(*aRect);

    if (NS_FAILED(result))
      return result;

    //
    // Translate the frame's rect into root view coordinates.
    //
    result = GetFrameToRootViewOffset(frame, &aRect->x, &aRect->y);

    if (NS_FAILED(result))
      return result;

    //
    // Now add the offset's x coordinate.
    //
    aRect->x += pt.x;

    //
    // Adjust the width of the rect to account for any neccessary
    // padding!
    //

    nsIScrollableView *scrollingView = 0;

    result = GetRootScrollableView(&scrollingView);

    if (NS_FAILED(result))
      return result;

    const nsIView* clipView = 0;
    nsRect clipRect;

    result = scrollingView->GetScrollPosition(clipRect.x, clipRect.y);

    if (NS_FAILED(result))
      return result;

    result = scrollingView->GetClipView(&clipView);

    if (NS_FAILED(result))
      return result;

    result = clipView->GetDimensions(&clipRect.width, &clipRect.height);

    if (NS_FAILED(result))
      return result;

    // If the point we are interested is outside the clip
    // region, we will scroll it into view with a padding
    // equal to a quarter of the clip's width.

    PRInt32 pad = clipRect.width >> 2;

    if (pad <= 0)
      pad = 3; // Arbitrary

    if (aRect->x >= clipRect.XMost())
      aRect->width = pad;
    else if (aRect->x <= clipRect.x)
    {
      aRect->x -= pad;
      aRect->width = pad;
    }
    else
      aRect->width = 60; // Arbitrary
  }
  else
  {
    //
    // Must be a non-text frame, just scroll the frame
    // into view.
    //
    result = frame->GetRect(*aRect);

    if (NS_FAILED(result))
      return result;

    result = GetFrameToRootViewOffset(frame, &aRect->x, &aRect->y);
  }

  return result;
}

nsresult
nsDOMSelection::ScrollRectIntoView(nsRect& aRect,
                              PRIntn  aVPercent, 
                              PRIntn  aHPercent)
{
  nsresult rv = NS_OK;

  nsIScrollableView *scrollingView = 0;

  rv = GetRootScrollableView(&scrollingView);

  if (NS_FAILED(rv))
    return rv;

  if (! scrollingView)
    return NS_ERROR_NULL_POINTER;

  // Determine the visible rect in the scrolled view's coordinate space.
  // The size of the visible area is the clip view size
  const nsIView*  clipView;
  nsRect          visibleRect;

  scrollingView->GetScrollPosition(visibleRect.x, visibleRect.y);
  scrollingView->GetClipView(&clipView);
  clipView->GetDimensions(&visibleRect.width, &visibleRect.height);

  // The actual scroll offsets
  nscoord scrollOffsetX = visibleRect.x;
  nscoord scrollOffsetY = visibleRect.y;

  // See how aRect should be positioned vertically
  if (NS_PRESSHELL_SCROLL_ANYWHERE == aVPercent) {
    // The caller doesn't care where aRect is positioned vertically,
    // so long as it's fully visible
    if (aRect.y < visibleRect.y) {
      // Scroll up so aRect's top edge is visible
      scrollOffsetY = aRect.y;
    } else if (aRect.YMost() > visibleRect.YMost()) {
      // Scroll down so aRect's bottom edge is visible. Make sure
      // aRect's top edge is still visible
      scrollOffsetY += aRect.YMost() - visibleRect.YMost();
      if (scrollOffsetY > aRect.y) {
        scrollOffsetY = aRect.y;
      }
    }
  } else {
    // Align the aRect edge according to the specified percentage
    nscoord frameAlignY = aRect.y + (aRect.height * aVPercent) / 100;
    scrollOffsetY = frameAlignY - (visibleRect.height * aVPercent) / 100;
  }

  // See how the aRect should be positioned horizontally
  if (NS_PRESSHELL_SCROLL_ANYWHERE == aHPercent) {
    // The caller doesn't care where the aRect is positioned horizontally,
    // so long as it's fully visible
    if (aRect.x < visibleRect.x) {
      // Scroll left so the aRect's left edge is visible
      scrollOffsetX = aRect.x;
    } else if (aRect.XMost() > visibleRect.XMost()) {
      // Scroll right so the aRect's right edge is visible. Make sure the
      // aRect's left edge is still visible
      scrollOffsetX += aRect.XMost() - visibleRect.XMost();
      if (scrollOffsetX > aRect.x) {
        scrollOffsetX = aRect.x;
      }
    }
      
  } else {
    // Align the aRect edge according to the specified percentage
    nscoord frameAlignX = aRect.x + (aRect.width * aHPercent) / 100;
    scrollOffsetX = frameAlignX - (visibleRect.width * aHPercent) / 100;
  }
      
  scrollingView->ScrollTo(scrollOffsetX, scrollOffsetY, NS_VMREFRESH_IMMEDIATE);

  return rv;
}

NS_IMETHODIMP
nsDOMSelection::ScrollIntoView(SelectionRegion aRegion)
{
  nsresult result;

  if (mRangeList->GetBatching())
    return NS_OK;

  //
  // Shut the caret off before scrolling to avoid
  // leaving caret turds on the screen!
  //
  nsCOMPtr<nsIPresShell> presShell;

  result = GetPresShell(getter_AddRefs(presShell));

  if (NS_FAILED(result))
    return result;

  PRBool caretEnabled = PR_FALSE;

  result = presShell->GetCaretEnabled(&caretEnabled);

  if (NS_FAILED(result))
    return result;

  if (caretEnabled)
  {
    result = presShell->SetCaretEnabled(PR_FALSE);

    if (NS_FAILED(result))
      return result;
  }

  //
  // Scroll the anchor focus node into view.
  //
  nsRect rect;
  result = GetFocusNodeRect(&rect);

  if (NS_FAILED(result))
    return result;

  result = ScrollRectIntoView(rect, NS_PRESSHELL_SCROLL_ANYWHERE, NS_PRESSHELL_SCROLL_ANYWHERE);

  //
  // Turn the caret back on.
  //
  if (caretEnabled)
  {
    result = presShell->SetCaretEnabled(PR_TRUE);

    if (NS_FAILED(result))
      return result;
  }

  return result;
}



NS_IMETHODIMP
nsDOMSelection::AddSelectionListener(nsIDOMSelectionListener* aNewListener)
{
  return mRangeList->AddSelectionListener(aNewListener);
}



NS_IMETHODIMP
nsDOMSelection::RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove)
{
  return mRangeList->RemoveSelectionListener(aListenerToRemove);
}



NS_IMETHODIMP
nsDOMSelection::StartBatchChanges()
{
  return mRangeList->StartBatchChanges();
}



NS_IMETHODIMP
nsDOMSelection::EndBatchChanges()
{
  return mRangeList->EndBatchChanges();
}



NS_IMETHODIMP
nsDOMSelection::DeleteFromDocument()
{
  return mRangeList->DeleteFromDocument();
}

// BEGIN nsIScriptContextOwner interface implementations
NS_IMETHODIMP
nsDOMSelection::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *globalObj = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    res = NS_NewScriptSelection(aContext, (nsISupports *)(nsIDOMSelection *)this, globalObj, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(globalObj);
  return res;
}

NS_IMETHODIMP
nsDOMSelection::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

// END nsIScriptContextOwner interface implementations
