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
#include "nsIFocusTracker.h"
#include "nsIComponentManager.h"
#include "nsLayoutCID.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsRange.h"
#include "nsISupportsArray.h"
#include "nsIDOMEvent.h"
#include "nsIDOMSelectionListener.h"

static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFrameSelectionIID, NS_IFRAMESELECTION_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kIDOMRangeIID, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

//PROTOTYPES
static void selectFrames(nsIFrame *aBegin, PRUint32 aBeginOffset, nsIFrame *aEnd, PRUint32 aEndOffset, nsDirection aDir, PRUint32 aFlags);
static PRInt32 compareFrames(nsIFrame *aBegin, nsIFrame *aEnd);
static nsIFrame * getNextFrame(nsIFrame *aStart);
static void printRange(nsIDOMRange *aDomRange);
//static nsIFrame *findFrameFromContent(nsIFrame *aParent, nsIContent *aContent, PRBool aTurnOff);

enum {FORWARD  =1, BACKWARD = 0};

#if 1
#define DEBUG_OUT_RANGE(x)  printRange(x)
#else
#define DEBUG_OUT_RANGE(x)  
#endif //MOZ_DEBUG

//#define DEBUG_SELECTION // uncomment for printf describing every collapse and extend.

class nsRangeListIterator;

class nsRangeList : public nsIFrameSelection,
                    public nsIDOMSelection
{
public:
  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_ISUPPORTS

/*BEGIN nsIFrameSelection interfaces*/
  NS_IMETHOD HandleKeyEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGuiEvent);
  NS_IMETHOD TakeFocus(nsIFocusTracker *aTracker, nsIFrame *aFrame, PRInt32 aOffset, PRInt32 aContentOffset, PRBool aContinueSelection);
  NS_IMETHOD ResetSelection(nsIFocusTracker *aTracker, nsIFrame *aStartFrame);
  NS_IMETHOD EnableFrameNotification(PRBool aEnable){mNotifyFrames = aEnable; return NS_OK;}
/*END nsIFrameSelection interfacse*/

/*BEGIN nsIDOMSelection interface implementations*/
  NS_IMETHOD DeleteFromDocument();
  NS_IMETHOD Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset);
  NS_IMETHOD IsCollapsed(PRBool* aIsCollapsed);
  NS_IMETHOD Extend(nsIDOMNode* aParentNode, PRInt32 aOffset);
  NS_IMETHOD ClearSelection();
  NS_IMETHOD AddRange(nsIDOMRange* aRange);
  NS_IMETHOD GetAnchorNodeAndOffset(nsIDOMNode** outAnchorNode, PRInt32 *outAnchorOffset);
  NS_IMETHOD GetFocusNodeAndOffset(nsIDOMNode** outFocusNode, PRInt32 *outFocusOffset);

  NS_IMETHOD AddSelectionListener(nsIDOMSelectionListener* inNewListener);
  NS_IMETHOD RemoveSelectionListener(nsIDOMSelectionListener* inListenerToRemove);
  NS_IMETHOD StartBatchChanges();
  NS_IMETHOD EndBatchChanges();

/*END nsIDOMSelection interface implementations*/

  nsRangeList();
  virtual ~nsRangeList();

private:
  nsresult AddItem(nsISupports *aRange);

  nsresult RemoveItem(nsISupports *aRange);

  nsresult Clear();
  NS_IMETHOD ScrollIntoView(nsIFocusTracker *aTracker);

  friend class nsRangeListIterator;

#ifdef DEBUG
  void printSelection();       // for debugging
#endif /* DEBUG */

  void ResizeBuffer(PRUint32 aNewBufSize);

  nsIDOMNode*  GetAnchorNode(); //where did the selection begin
  PRInt32      GetAnchorOffset();
  void         setAnchor(nsIDOMNode*, PRInt32);
  nsIDOMNode*  GetFocusNode();  //where is the carret
  PRInt32      GetFocusOffset();
  void         setFocus(nsIDOMNode*, PRInt32);
  PRBool       GetBatching(){return mBatching;}
  PRBool       GetNotifyFrames(){return mNotifyFrames;}
  void         SetDirty(PRBool aDirty=PR_TRUE){if (mBatching) mChangesDuringBatching = aDirty;}

  nsresult     NotifySelectionListeners();			// add parameters to say collapsed etc?

  nsCOMPtr<nsISupportsArray> mRangeArray;

  nsCOMPtr<nsIDOMNode> mAnchorNode; //where did the selection begin
  PRInt32 mAnchorOffset;
  nsCOMPtr<nsIDOMNode> mFocusNode; //where is the carret
  PRInt32 mFocusOffset;

  //batching
  PRBool mBatching;
  PRBool mChangesDuringBatching;
  PRBool mNotifyFrames;


  nsCOMPtr<nsISupportsArray> mSelectionListeners;
};

class nsRangeListIterator : public nsIBidirectionalEnumerator
{
public:
/*BEGIN nsIEnumerator interfaces
see the nsIEnumerator for more details*/

  NS_DECL_ISUPPORTS

  NS_IMETHOD First();

  NS_IMETHOD Last();

  NS_IMETHOD Next();

  NS_IMETHOD Prev();

  NS_IMETHOD CurrentItem(nsISupports **aRange);

  NS_IMETHOD IsDone();

/*END nsIEnumerator interfaces*/
/*BEGIN Helper Methods*/
  NS_IMETHOD CurrentItem(nsIDOMRange **aRange);
/*END Helper Methods*/
private:
  friend class nsRangeList;
  nsRangeListIterator(nsRangeList *);
  virtual ~nsRangeListIterator();
  PRInt32     mIndex;
  nsRangeList *mRangeList;
};


nsresult NS_NewRangeList(nsIDOMSelection **);

nsresult NS_NewRangeList(nsIDOMSelection **aRangeList)
{
  nsRangeList *rlist = new nsRangeList;
  if (!rlist)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult result = rlist->QueryInterface(kIDOMSelectionIID ,
                                          (void **)aRangeList);
  if (!NS_SUCCEEDED(result))
  {
    delete rlist;
  }
  return result;
}




///////////BEGIN nsRangeListIterator methods

nsRangeListIterator::nsRangeListIterator(nsRangeList *aList)
:mIndex(0)
{
  if (!aList)
  {
    NS_NOTREACHED("nsRangeList");
    return;
  }
  mRangeList = aList;
  NS_INIT_REFCNT();
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
  if (mIndex < (PRInt32)mRangeList->mRangeArray->Count())
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
  if (!mRangeList)
    return NS_ERROR_NULL_POINTER;
  mIndex = 0;
  return NS_OK;
}



NS_IMETHODIMP
nsRangeListIterator::Last()
{
  if (!mRangeList)
    return NS_ERROR_NULL_POINTER;
  mIndex = (PRInt32)mRangeList->mRangeArray->Count()-1;
  return NS_OK;
}



NS_IMETHODIMP 
nsRangeListIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (mIndex >=0 && mIndex < (PRInt32)mRangeList->mRangeArray->Count()){
    nsCOMPtr<nsISupports> indexIsupports = dont_AddRef(mRangeList->mRangeArray->ElementAt(mIndex));
    return indexIsupports->QueryInterface(kISupportsIID, (void **)aItem);
  }
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP 
nsRangeListIterator::CurrentItem(nsIDOMRange **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (mIndex >=0 && mIndex < (PRInt32)mRangeList->mRangeArray->Count()){
    nsCOMPtr<nsISupports> indexIsupports = dont_AddRef(mRangeList->mRangeArray->ElementAt(mIndex));
    return indexIsupports->QueryInterface(kIDOMRangeIID, (void **)aItem);
  }
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsRangeListIterator::IsDone()
{
  if (mIndex >= 0 && mIndex < (PRInt32)mRangeList->mRangeArray->Count() ) { 
    return NS_COMFALSE;
  }
  return NS_OK;
}



NS_IMPL_ADDREF(nsRangeListIterator)

NS_IMPL_RELEASE(nsRangeListIterator)



NS_IMETHODIMP
nsRangeListIterator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  // XXX shouldn't this just do mRangeList->QueryInterface instead of
  // having a complete copy of that method here? What about AddRef and
  // Release? shouldn't they be delegated too?
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMSelection* tmp = mRangeList;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF(mRangeList);
    return NS_OK;
  }
  if (aIID.Equals(kIDOMSelectionIID)) {
    *aInstancePtr = (void *)mRangeList;
    NS_ADDREF(mRangeList);
    return NS_OK;
  }
  if (aIID.Equals(kIFrameSelectionIID)) {
    *aInstancePtr = (void *)mRangeList;
    NS_ADDREF(mRangeList);
    return NS_OK;
  }
  if (aIID.Equals(kIEnumeratorIID) ||
      aIID.Equals(nsIBidirectionalEnumerator::GetIID())) {
    nsIBidirectionalEnumerator* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

////////////END nsIRangeListIterator methods

////////////BEGIN nsRangeList methods



nsRangeList::nsRangeList()
{
  NS_INIT_REFCNT();
  NS_NewISupportsArray(getter_AddRefs(mRangeArray));
  NS_NewISupportsArray(getter_AddRefs(mSelectionListeners));
  mBatching = PR_FALSE;
  mChangesDuringBatching = PR_FALSE;
  mNotifyFrames = PR_TRUE;
}



nsRangeList::~nsRangeList()
{
  if (mRangeArray)
  {
	  for (PRUint32 i=0;i < mRangeArray->Count(); i++)
	  {
	    mRangeArray->RemoveElementAt(i);
	  }
  }
  
  if (mSelectionListeners)
  {
	  for (PRUint32 i=0;i < mSelectionListeners->Count(); i++)
	  {
	    mSelectionListeners->RemoveElementAt(i);
	  }
  }
  
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
  if (aIID.Equals(kIDOMSelectionIID)) {
    nsIDOMSelection* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIFrameSelectionIID)) {
    nsIFrameSelection* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEnumeratorIID)) {
    nsRangeListIterator *iterator =  new nsRangeListIterator(this);
    return iterator->QueryInterface(kIEnumeratorIID, aInstancePtr);
  }
  if (aIID.Equals(kISupportsIID)) {
    // use *first* base class for ISupports
    nsIFrameSelection* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsIDOMNode* nsRangeList::GetAnchorNode()
{
  return mAnchorNode;
}

PRInt32 nsRangeList::GetAnchorOffset()
{
  return mAnchorOffset;
}

void nsRangeList::setAnchor(nsIDOMNode* node, PRInt32 offset)
{
  mAnchorNode = dont_QueryInterface(node);
  mAnchorOffset = offset;
}

nsIDOMNode* nsRangeList::GetFocusNode()
{
  return mFocusNode;
}

PRInt32 nsRangeList::GetFocusOffset()
{
  return mFocusOffset;
}

void nsRangeList::setFocus(nsIDOMNode* node, PRInt32 offset)
{
  mFocusNode = dont_QueryInterface(node);
  mFocusOffset = offset;
}


 
nsresult
nsRangeList::AddItem(nsISupports *aItem)
{
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  mRangeArray->AppendElement(aItem);
  return NS_OK;
}



nsresult
nsRangeList::RemoveItem(nsISupports *aItem)
{
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  if (!aItem )
    return NS_ERROR_NULL_POINTER;
  for (PRUint32 i = 0; i < mRangeArray->Count();i++)
  {
    nsCOMPtr<nsISupports> indexIsupports = dont_AddRef(mRangeArray->ElementAt(i));

    if (aItem == indexIsupports.get())
    {
      mRangeArray->RemoveElementAt(i);
      return NS_OK;
    }
  }
  return NS_COMFALSE;
}



nsresult
nsRangeList::Clear()
{
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  for (PRUint32 i = 0; i < mRangeArray->Count();i++)
  {
    mRangeArray->RemoveElementAt(i);
    // Does RemoveElementAt also delete the elements?
  }
  return NS_OK;
}




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
  /*
  printf("print DOMRANGE 0x%lx\t start: 0x%lx %ld, \t end: 0x%lx,%ld \n",
         (unsigned long)aDomRange,
         (unsigned long)(nsIDOMNode*)startNode, (long)startOffset,
         (unsigned long)(nsIDOMNode*)endNode, (long)endOffset);
         */
}


/** This raises a question, if this method is called and the aFrame does not reflect the current
 *  focus  DomNode, it is invalid?  The answer now is yes.
 */
NS_IMETHODIMP
nsRangeList::HandleKeyEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGuiEvent)
{
  if (!aGuiEvent ||!aTracker)
    return NS_ERROR_NULL_POINTER;

  nsIFrame *anchor;
  nsIFrame *frame;
  nsresult result = aTracker->GetFocus(&frame, &anchor);
  if (NS_FAILED(result))
    return result;
  if (NS_KEY_DOWN == aGuiEvent->message) {

    PRBool selected;
    PRInt32 beginoffset = 0;
    PRInt32 endoffset;
    PRInt32 contentoffset;
    nsresult result = NS_OK;

    nsKeyEvent *keyEvent = (nsKeyEvent *)aGuiEvent; //this is ok. It really is a keyevent
    nsIFrame *resultFrame;
    PRInt32   frameOffset;
    PRInt32   contentOffset;
    PRInt32   offsetused = beginoffset;
    nsIFrame *frameused;
    nsSelectionAmount amount = eSelectCharacter;
    result = frame->GetSelected(&selected,&beginoffset,&endoffset, &contentoffset);
    if (NS_FAILED(result)){
      return result;
    }
    if (keyEvent->isControl)
      amount = eSelectWord;
    switch (keyEvent->keyCode){
      case nsIDOMEvent::VK_LEFT  : 
        //we need to look for the previous PAINTED location to move the cursor to.
        printf("debug vk left\n");
        if (keyEvent->isShift || (endoffset < beginoffset)){ //f,a
          offsetused = endoffset;
          frameused = frame;
        }
        else {
          result = anchor->GetSelected(&selected,&beginoffset,&endoffset, &contentoffset);
          if (NS_FAILED(result)){
            return result;
          }
          offsetused = beginoffset;
          frameused = anchor;
        }
        if (NS_SUCCEEDED(frameused->PeekOffset(amount, eDirPrevious, offsetused, &resultFrame, &frameOffset, &contentOffset, PR_FALSE)) && resultFrame){
          result = TakeFocus(aTracker, resultFrame, frameOffset, contentOffset, keyEvent->isShift);
        }
        break;
      case nsIDOMEvent::VK_RIGHT : 
        //we need to look for the next PAINTED location to move the cursor to.
        printf("debug vk right\n");
        if (!keyEvent->isShift && (endoffset < beginoffset)){ //f,a
          result = anchor->GetSelected(&selected,&beginoffset,&endoffset, &contentoffset);
          if (NS_FAILED(result)){
            return result;
          }
          offsetused = beginoffset;
          frameused = anchor;
        }
        else {
          offsetused = endoffset;
          frameused = frame;
        }
        if (NS_SUCCEEDED(frameused->PeekOffset(amount, eDirNext, offsetused, &resultFrame, &frameOffset, &contentOffset, PR_FALSE)) && resultFrame){
          result = TakeFocus(aTracker, resultFrame, frameOffset, contentOffset, keyEvent->isShift);
        }
        break;
      case nsIDOMEvent::VK_UP : 
        printf("debug vk up\n");
        break;
    default :break;
    }
    result = ScrollIntoView(aTracker);
  }
  return result;
}

#ifdef DEBUG
void nsRangeList::printSelection()
{
  printf("nsRangeList 0x%lx: %d items\n", (unsigned long)this,
         mRangeArray ? mRangeArray->Count() : -99);

  // Get an iterator
  nsRangeListIterator iter(this);
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
         (unsigned long)(nsIDOMNode*)GetAnchorNode(), GetAnchorOffset());
  printf("Focus is 0x%lx, %d\n",
         (unsigned long)(nsIDOMNode*)GetFocusNode(), GetFocusOffset());
  printf(" ... end of selection\n");
}
#endif /* DEBUG */



//recursive-oid method to get next frame
nsIFrame *
getNextFrame(nsIFrame *aStart)
{
  nsIFrame *result;
  nsIFrame *parent = aStart;
  if (NS_SUCCEEDED(parent->FirstChild(nsnull, &result)) && result){
    return result;
  }
  while(parent){
    if (NS_SUCCEEDED(parent->GetNextSibling(&result)) && result){
      parent = result;
      return result;
    }
    else
      if (NS_FAILED(parent->GetParent(&result)) || !result)
        return nsnull;
      else 
        parent = result;
  }
  return nsnull;
}



//compare the 2 passed in frames -1 first is smaller 1 second is smaller 0 they are the same
PRInt32
compareFrames(nsIFrame *aBegin, nsIFrame *aEnd)
{
  if (!aBegin || !aEnd)
    return 0;
  if (aBegin == aEnd)
    return 0;
  nsCOMPtr<nsIContent> beginContent;
  if (NS_SUCCEEDED(aBegin->GetContent(getter_AddRefs(beginContent))) && beginContent){
    nsCOMPtr<nsIDOMNode>beginNode (do_QueryInterface(beginContent));
    nsCOMPtr<nsIContent> endContent;
    if (NS_SUCCEEDED(aEnd->GetContent(getter_AddRefs(endContent))) && endContent){
      nsCOMPtr<nsIDOMNode>endNode (do_QueryInterface(endContent));
      PRBool storage;
      PRInt32 int1;
      PRInt32 int2;
      PRInt32 contentOffset1;
      PRInt32 contentOffset2;
      aBegin->GetSelected(&storage,&int1,&int2,&contentOffset1);
      aEnd->GetSelected(&storage,&int1,&int2,&contentOffset2);
      return ComparePoints(beginNode, contentOffset1, endNode, contentOffset2);
    }
  }
  return 0;
}



//the idea of this helper method is to select, deselect "top to bottom" traversing through the frames
void
selectFrames(nsIFrame *aBegin, PRUint32 aBeginOffset, nsIFrame *aEnd, PRUint32 aEndOffset, nsDirection aDir, PRUint32 aFlags)
{
  if (!aBegin || !aEnd)
    return;
  PRBool done = PR_FALSE;
  while (!done)
  {
    PRUint32 type(aFlags);

    nsSelectionStruct ss = {0};
    if (aBegin == aEnd)
    {
      ss.mType = type;
      ss.mStartFrame = aBeginOffset;
      ss.mEndFrame = aEndOffset;
      ss.mDir = aDir;
      aBegin->SetSelected(&ss);
      done = PR_TRUE;
    }
    else {
      //else we neeed to select from begin to end 
      type |= nsSelectionStruct::SELTOEND;
      ss.mType = type;
      ss.mStartFrame = aBeginOffset;
      ss.mEndFrame = aEndOffset;
      ss.mDir = aDir;
      aBegin->SetSelected(&ss);
      aBeginOffset = 0;
      if (!(aBegin = getNextFrame(aBegin)))
      {
        done = PR_TRUE;
      }
    }
  }
}


/**
hard to go from nodes to frames, easy the other way!
 */
NS_IMETHODIMP
nsRangeList::TakeFocus(nsIFocusTracker *aTracker, nsIFrame *aFrame, PRInt32 aOffset, PRInt32 aContentOffset, PRBool aContinueSelection)
{
  if (!aTracker || !aFrame)
    return NS_ERROR_NULL_POINTER;
  if (GetBatching())
    return NS_ERROR_FAILURE;
  //HACKHACKHACK
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> domNode;
  if (NS_SUCCEEDED(aFrame->GetContent(getter_AddRefs(content))) && content){
    domNode = do_QueryInterface(content);
    nsCOMPtr<nsIDOMNode> parent;
    nsCOMPtr<nsIDOMNode> parent2;
    if (NS_FAILED(domNode->GetParentNode(getter_AddRefs(parent))) || !parent)
      return NS_ERROR_FAILURE;
    if (NS_FAILED(parent->GetParentNode(getter_AddRefs(parent2))) || !parent2)
      return NS_ERROR_FAILURE;
    parent = nsCOMPtr<nsIDOMNode>();//just force a release now even though we dont have to.
    parent2 = nsCOMPtr<nsIDOMNode>();

    nsIFrame *frame;
    nsIFrame *anchor;
    PRBool direction(BACKWARD);//true == left to right
    if (domNode && NS_SUCCEEDED(aTracker->GetFocus(&frame, &anchor))){
      //traverse through document and unselect crap here
      if (!aContinueSelection){ //single click? setting cursor down
        if (anchor && frame && anchor != frame ){
          //selected across frames, must "deselect" frames between in correct order
          PRInt32 compareResult = compareFrames(anchor,frame);
          if ( compareResult < 0 )
            selectFrames(anchor,0,frame, 0, eDirNext, nsSelectionStruct::SELTOEND); //unselect all between
          else if (compareResult > 0 )
            selectFrames(frame,0,anchor, 0, eDirPrevious, nsSelectionStruct::SELTOEND); //unselect all between
//          else real bad put in assert here
        }
        else if (frame && frame != aFrame){
          nsSelectionStruct ss={nsSelectionStruct::SELTOEND, 0,0, 0,0, 0,0, eDirNext, PR_FALSE};
          frame->SetSelected(&ss);
        }
        nsSelectionStruct ss={nsSelectionStruct::SELON, 0,0, aOffset, aOffset, 0,0, eDirNext, PR_FALSE};
        aFrame->SetSelected(&ss);
        aTracker->SetFocus(aFrame,aFrame);
        PRBool batching = mBatching;//hack to use the collapse code.
        PRBool changes = mChangesDuringBatching;
        mBatching = PR_TRUE;
        Collapse(domNode, aContentOffset + aOffset);
        mBatching = batching;
        mChangesDuringBatching = changes;
      }
      else {
        if (aFrame == frame){ //drag to same frame
          PRInt32 beginoffset;
          PRInt32 begincontentoffset;
          PRInt32 endoffset;
          PRBool selected;
          if (NS_SUCCEEDED(aFrame->GetSelected(&selected,&beginoffset,&endoffset, &begincontentoffset))){
            nsSelectionStruct ss={nsSelectionStruct::SELON, 0,0, beginoffset, aOffset, 0,0, eDirNext, PR_FALSE};
            aFrame->SetSelected(&ss);
            //PR_ASSERT(beginoffset == GetAnchorOffset());
            aTracker->SetFocus(aFrame,anchor);
            if (beginoffset <= aOffset)
              direction = FORWARD; //selecting "english" right if true
          }
          else return NS_ERROR_FAILURE;
        }
        else if (frame){ //we need to check to see what the order is.
          nsCOMPtr<nsIContent>oldContent;
          if (NS_SUCCEEDED(frame->GetContent(getter_AddRefs(oldContent))) && oldContent){
            nsCOMPtr<nsIDOMNode> oldDomNode(do_QueryInterface(oldContent));
            if (oldDomNode && (oldDomNode.get() == GetFocusNode())) {
              nsCOMPtr<nsIContent> anchorContent;
              if (NS_SUCCEEDED(anchor->GetContent(getter_AddRefs(anchorContent))) && anchorContent){
                nsCOMPtr<nsIDOMNode>anchorDomNode(do_QueryInterface(anchorContent));
                if (anchorDomNode && anchorDomNode.get() == GetAnchorNode()) {


                  //get offsets
                  PRBool selected = PR_FALSE;
                  PRInt32 anchorFrameOffsetBegin = 0;
                  PRInt32 anchorFrameOffsetEnd = 0;
                  PRInt32 anchorFrameContentOffset = 0;

                  if (NS_FAILED(anchor->GetSelected(&selected, &anchorFrameOffsetBegin, &anchorFrameOffsetEnd, &anchorFrameContentOffset)) || !selected)
                    return NS_ERROR_FAILURE;

                  selected = PR_FALSE;
                  PRInt32 focusFrameOffsetBegin = 0;
                  PRInt32 focusFrameOffsetEnd = 0;
                  PRInt32 focusFrameContentOffset = 0;

                  if (NS_FAILED(frame->GetSelected(&selected, &focusFrameOffsetBegin, &focusFrameOffsetEnd, &focusFrameContentOffset)) || !selected)
                    return NS_ERROR_FAILURE;

                  //compare anchor to old cursor.
                  PRInt32 result1 = compareFrames(anchor,frame); //nothing else matters. if they are the same frame.
                  //compare old cursor to new cursor
                  PRInt32 result2 = compareFrames(frame,aFrame);
                  if (result2 == 0)
                    result2 = ComparePoints(GetFocusNode(), GetFocusOffset(),
                                            domNode, aOffset );
                  //compare anchor to new cursor
                  PRInt32 result3 = compareFrames(anchor,aFrame);
                  if (result3 == 0)
                    result3 = ComparePoints(GetAnchorNode(), GetAnchorOffset(),
                                            domNode , aOffset );

                  if (result1 == 0 && result3 < 0)
                  {
                    selectFrames(anchor,anchorFrameOffsetBegin, aFrame, aOffset , eDirNext, nsSelectionStruct::SELON); 
                  }
                  else if (result1 == 0 && result3 > 0)
                  {
                    selectFrames(aFrame, aOffset , anchor,anchorFrameOffsetBegin, eDirPrevious,nsSelectionStruct::SELON);
                  }
                  else if (result1 <= 0 && result2 <= 0) {//a,1,2 or a1,2 or a,12 or a12
                    //continue selection from 1 to 2
                    selectFrames(frame,PR_MIN(focusFrameOffsetEnd,focusFrameOffsetBegin), aFrame, aOffset, eDirNext, nsSelectionStruct::SELON);
                  }
                  else if (result3 <= 0 && result2 >= 0) {//a,2,1 or a2,1 or a,21 or a21
                    //deselect from 2 to 1
                    selectFrames(aFrame, aOffset, frame,focusFrameOffsetEnd, eDirNext, 0);
                    if (anchor != aFrame)
                      selectFrames(aFrame, 0, aFrame,aOffset, eDirNext, nsSelectionStruct::SELON);
                    else
                      selectFrames(anchor, anchorFrameOffsetBegin, aFrame, aOffset, eDirNext, nsSelectionStruct::SELON);
                  }
                  else if (result1 >= 0 && result3 <= 0) {//1,a,2 or 1a,2 or 1,a2 or 1a2
                    //deselect from 1 to a
                    selectFrames(frame, focusFrameOffsetEnd, anchor, anchorFrameOffsetBegin, eDirPrevious, 0);
                    //select from a to 2
                    selectFrames(anchor, anchorFrameOffsetBegin, aFrame, aOffset, eDirNext, nsSelectionStruct::SELON);
                  }
                  else if (result2 <= 0 && result3 >= 0) {//1,2,a or 12,a or 1,2a or 12a
                    //deselect from 1 to 2
                    selectFrames(frame, focusFrameOffsetEnd, aFrame, aOffset, eDirPrevious, 0);
                    if (anchor != aFrame)
                      selectFrames(aFrame, aOffset, aFrame, -1, eDirPrevious, nsSelectionStruct::SELON);
                    else
                      selectFrames(aFrame, aOffset, anchor, anchorFrameOffsetBegin, eDirPrevious, nsSelectionStruct::SELON);

                  }
                  else if (result3 >= 0 && result1 <= 0) {//2,a,1 or 2a,1 or 2,a1 or 2a1
                    //deselect from a to 1
                    selectFrames(anchor, anchorFrameOffsetBegin, frame, focusFrameOffsetEnd, eDirNext, 0);
                    //select from 2 to a
                    selectFrames(aFrame, aOffset, anchor, anchorFrameOffsetBegin, eDirPrevious, nsSelectionStruct::SELON);
                  }
                  else if (result2 >= 0 && result1 >= 0) {//2,1,a or 21,a or 2,1a or 21a
                    //continue selection from 2 to 1
                    selectFrames(aFrame, aOffset, frame,PR_MAX(focusFrameOffsetEnd,focusFrameOffsetBegin), eDirPrevious, nsSelectionStruct::SELON);
                  }
                  if (result3 <= 0)
                    direction = FORWARD;
                }
                aTracker->SetFocus(aFrame,anchor);
              }
            }
          }
        }
      }

      // Now update the range list:
      if (aContinueSelection && domNode)
      {
        Extend(domNode, aOffset + aContentOffset);
      }
    }
  }
  else
    return NS_ERROR_FAILURE;
    
  return NotifySelectionListeners();
}



//the start frame is the "root" of the tree. we need to traverse the tree to look for the content we want
NS_IMETHODIMP 
nsRangeList::ResetSelection(nsIFocusTracker *aTracker, nsIFrame *aStartFrame)
{
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset;
  PRInt32 endOffset;
  nsIFrame *result;
  nsresult res;
  nsCOMPtr<nsIDOMRange> range;
  if (!GetNotifyFrames())
    return NS_OK;
  if (GetBatching()){
    SetDirty();
    return NS_OK;
  }
  //we will need to check if any "side" is the anchor and send a direction order to the frames.
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  //reset the focus and anchor points.
  nsCOMPtr<nsIContent> anchorContent;
  nsCOMPtr<nsIContent> frameContent;
  if (GetAnchorNode() && GetFocusNode()){
    anchorContent =  do_QueryInterface(GetAnchorNode(),&res);
    if (NS_SUCCEEDED(res))
      frameContent = do_QueryInterface(GetFocusNode(),&res);
  }
  else
    res = NS_OK;
  if (NS_SUCCEEDED(res)){
    for (PRUint32 i =0; i<mRangeArray->Count(); i++){
      //end content and start content do NOT necessarily mean anchor and focus frame respectively
      PRInt32 anchorOffset = -1; //the frames themselves can talk to the presentation manager.  we will tell them
      PRInt32 frameOffset = -1;  // where we would "like" to have the anchor pt.  actually we count on it.
      nsCOMPtr<nsISupports> isupportsindex = dont_AddRef(mRangeArray->ElementAt(i));
      range = do_QueryInterface(isupportsindex);
      DEBUG_OUT_RANGE(range);
      range->GetStartParent(getter_AddRefs(startNode));
      range->GetStartOffset(&startOffset);
      range->GetEndParent(getter_AddRefs(endNode));
      range->GetEndOffset(&endOffset);

      nsCOMPtr<nsIContent> startContent;
      startContent = do_QueryInterface(startNode,&res);
      if (startContent){
        res = aTracker->GetPrimaryFrameFor(startContent,&result);
          //findFrameFromContent(aStartFrame, startContent,PR_TRUE);
      }
      if (result && NS_SUCCEEDED(res)){
        nsCOMPtr<nsIContent> endContent(do_QueryInterface(endNode));
        if (endContent == startContent){
          if (startContent == frameContent)
            frameOffset = GetFocusOffset();
          if ( startContent == anchorContent ) 
            anchorOffset = GetAnchorOffset();
          nsSelectionStruct ss={nsSelectionStruct::SELON|nsSelectionStruct::CHECKANCHOR|nsSelectionStruct::CHECKFOCUS
            , startOffset, endOffset, 0,0, anchorOffset,frameOffset, eDirNext, PR_FALSE};
          if (anchorOffset> frameOffset)
            ss.mDir = eDirPrevious;
          result->SetSelectedContentOffsets(&ss, 
                                            aTracker, &result);
        }
        else{
          if (startContent == frameContent)
            frameOffset = GetFocusOffset();
          if ( startContent == anchorContent ) 
            anchorOffset = GetAnchorOffset();
          nsSelectionStruct ss={nsSelectionStruct::SELON|nsSelectionStruct::SELTOEND|nsSelectionStruct::CHECKANCHOR|nsSelectionStruct::CHECKFOCUS
            , startOffset, 0, 0,0, anchorOffset,frameOffset, eDirNext, PR_FALSE};
          result->SetSelectedContentOffsets(&ss, aTracker, &result);//select from start to end

          //now we keep selecting until we hit the last content, or the end of the page.
          anchorOffset = -1;
          frameOffset = -1;
          while((result = getNextFrame(result)) != nsnull){
            nsCOMPtr<nsIContent> content;
            result->GetContent(getter_AddRefs(content));
            if (content == endContent){
              if (endContent == frameContent)
                frameOffset = GetFocusOffset();
              if ( endContent == anchorContent ) 
                anchorOffset = GetAnchorOffset();
              nsSelectionStruct ss={nsSelectionStruct::SELON|nsSelectionStruct::CHECKANCHOR|nsSelectionStruct::CHECKFOCUS
                , 0, endOffset, 0,0, anchorOffset,frameOffset, eDirNext, PR_FALSE};
              result->SetSelectedContentOffsets(&ss, aTracker, &result);//select from beginning to endOffset
              break;
            }
            else{
              nsSelectionStruct ss={nsSelectionStruct::SELON|nsSelectionStruct::SELTOEND, 0,0, 0,0, 0,0, eDirNext, PR_FALSE};
              result->SetSelected(&ss);
            }
          }
        }
      }
      res = ScrollIntoView(aTracker);
    }
  }
  //printf("debug reset selection has been called\n");
  return res;
}


NS_METHOD
nsRangeList::AddSelectionListener(nsIDOMSelectionListener* inNewListener)
{
  if (!mSelectionListeners)
    return NS_ERROR_FAILURE;
  if (!inNewListener)
    return NS_ERROR_NULL_POINTER;
  mSelectionListeners->AppendElement(inNewListener);		// addrefs
  return NS_OK;
}



NS_IMETHODIMP
nsRangeList::RemoveSelectionListener(nsIDOMSelectionListener* inListenerToRemove)
{
  if (!mSelectionListeners)
    return NS_ERROR_FAILURE;
  if (!inListenerToRemove )
    return NS_ERROR_NULL_POINTER;
    
  return mSelectionListeners->RemoveElement(inListenerToRemove);		// releases
}



NS_IMETHODIMP
nsRangeList::StartBatchChanges()
{
  nsresult result(NS_OK);
  if (PR_TRUE == mBatching)
    result = NS_ERROR_FAILURE; 
  mBatching = PR_TRUE;
  return result;
}


 
NS_IMETHODIMP
nsRangeList::EndBatchChanges()
{
  nsresult result(NS_OK);
  if (PR_FALSE == mBatching)
    result = NS_ERROR_FAILURE; 
  mBatching = PR_FALSE;
  if (mChangesDuringBatching){
    mChangesDuringBatching = PR_FALSE;
    NotifySelectionListeners();
  }
  return result;
}

  
  
NS_IMETHODIMP
nsRangeList::ScrollIntoView(nsIFocusTracker *aTracker)
{
  if (!aTracker)
    return NS_ERROR_NULL_POINTER;

  nsresult result;
  nsIFrame *anchor;
  nsIFrame *frame;
  result = aTracker->GetFocus(&frame, &anchor);
  if (NS_FAILED(result))
    return result;
  result = aTracker->ScrollFrameIntoView(frame);
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
  for (PRUint32 i = 0; i < mSelectionListeners->Count();i++)
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

//BEGIN nsIDOMSelection interface implementations
#ifdef XP_MAC
#pragma mark -
#endif

/** ClearSelection zeroes the selection
 */
NS_IMETHODIMP
nsRangeList::ClearSelection()
{
  nsresult	result = Clear();
  if (NS_FAILED(result))
  	return result;
  	
  return NotifySelectionListeners();
  // Also need to notify the frames!
  // PresShell::ContentChanged should do that on DocumentChanged
}

/** AddRange adds the specified range to the selection
 *  @param aRange is the range to be added
 */
NS_IMETHODIMP
nsRangeList::AddRange(nsIDOMRange* aRange)
{
  nsresult	result = AddItem(aRange);
  
	if (NS_FAILED(result))
		return result;
	
	return NotifySelectionListeners();
	
  // Also need to notify the frames!
}



/*
 * Collapse sets the whole selection to be one point.
 */
NS_IMETHODIMP
nsRangeList::Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset)
{
  nsresult result;

  // Delete all of the current ranges
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  Clear();

  nsCOMPtr<nsIDOMRange> range;
  result = nsComponentManager::CreateInstance(kRangeCID, nsnull,
                                     kIDOMRangeIID,
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

  setAnchor(aParentNode, aOffset);
  setFocus(aParentNode, aOffset);

#ifdef DEBUG_SELECTION
  if (aParentNode)
  {
    nsCOMPtr<nsIContent>content;
    content = do_QueryInterface(aParentNode);
    nsIAtom *tag;
    content->GetTag(tag);
    nsString tagString;
    tag->ToString(tagString);
    char * tagCString = tagString.ToNewCString();
    printf ("Sel. Collapse to %p %s %d\n", content, tagCString, aOffset);
    delete [] tagCString;
  }
  else {
    printf ("Sel. Collapse set to null parent.\n");
  }
#endif


  result = AddItem(range);
  if (NS_FAILED(result))
    return result;
    
	return NotifySelectionListeners();
}

/*
 * IsCollapsed -- is the whole selection just one point, or unset?
 */
NS_IMETHODIMP
nsRangeList::IsCollapsed(PRBool* aIsCollapsed)
{
  if (!mRangeArray || (mRangeArray->Count() == 0))
  {
    *aIsCollapsed = PR_TRUE;
    return NS_OK;
  }
  
  if (mRangeArray->Count() != 1)
  {
    *aIsCollapsed = PR_FALSE;
    return NS_OK;
  }
  
  nsCOMPtr<nsISupports> nsisup(dont_AddRef(mRangeArray->ElementAt(0)));
  nsCOMPtr<nsIDOMRange> range;
  if (!NS_SUCCEEDED(nsisup->QueryInterface(kIDOMRangeIID,
                                           getter_AddRefs(range))))
  {
    *aIsCollapsed = PR_TRUE;
    return NS_OK;
  }
                             
  return (range->GetIsCollapsed(aIsCollapsed));
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
nsRangeList::Extend(nsIDOMNode* aParentNode, PRInt32 aOffset)
{
  // First, find the range containing the old focus point:
  if (!mRangeArray)
    return NS_ERROR_FAILURE;

  PRUint32 i;
  for (i = 0; i < mRangeArray->Count(); i++)
  {
    nsCOMPtr<nsISupports> isupportsindex = dont_AddRef(mRangeArray->ElementAt(i));
    nsCOMPtr<nsIDOMRange> range (do_QueryInterface(isupportsindex));

    nsCOMPtr<nsIDOMNode> endNode;
    PRInt32 endOffset;
    nsCOMPtr<nsIDOMNode> startNode;
    PRInt32 startOffset;
    range->GetEndParent(getter_AddRefs(endNode));
    range->GetEndOffset(&endOffset);
    range->GetStartParent(getter_AddRefs(startNode));
    range->GetStartOffset(&startOffset);
    nsresult res;

    if ((GetFocusNode() == endNode.get()) && (GetFocusOffset() == endOffset))
    {
      res = range->SetEnd(aParentNode, aOffset);
      if (res == NS_ERROR_ILLEGAL_VALUE)
      {
        res = range->SetEnd(startNode, startOffset);
        if (NS_SUCCEEDED(res))
          res = range->SetStart(aParentNode, aOffset);
      }
      if (NS_SUCCEEDED(res))
        setFocus(aParentNode, aOffset);

      if (NS_FAILED(res)) return res;
#ifdef DEBUG_SELECTION
  nsCOMPtr<nsIContent>content;
  content = do_QueryInterface(aParentNode);
  nsIAtom *tag;
  content->GetTag(tag);
  nsString tagString;
  tag->ToString(tagString);
  char * tagCString = tagString.ToNewCString();
  printf ("Sel. Extend to %p %s %d\n", content, tagCString, aOffset);
  delete [] tagCString;
#endif
      return NotifySelectionListeners();
    }
    
    if ((GetFocusNode() == startNode.get()) && (GetFocusOffset() == startOffset))
    {
      res = range->SetStart(aParentNode, aOffset);
      if (res == NS_ERROR_ILLEGAL_VALUE)
      {
        res = range->SetStart(endNode, endOffset);
        if (NS_SUCCEEDED(res))
          res = range->SetEnd(aParentNode, aOffset);
      }
      if (NS_SUCCEEDED(res))
        setFocus(aParentNode, aOffset);

      if (NS_FAILED(res)) return res;
#ifdef DEBUG_SELECTION
  nsCOMPtr<nsIContent>content;
  content = do_QueryInterface(aParentNode);
  nsIAtom *tag;
  content->GetTag(tag);
  nsString tagString;
  tag->ToString(tagString);
  char * tagCString = tagString.ToNewCString();
  printf ("Sel. Extend to %p %s %d\n", content, tagCString, aOffset);
  delete [] tagCString;
#endif
      return NotifySelectionListeners();
    }
  }

  // If we get here, the focus wasn't contained in any of the ranges.
#ifdef DEBUG
  printf("nsRangeList::Extend: focus not contained in any ranges\n");
#endif
  return NS_ERROR_UNEXPECTED;
}


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
  IsCollapsed(&isCollapsed);
  if (isCollapsed)
  {
    // If the offset is positive, then it's easy:
    if (GetFocusOffset() > 0)
    {
      Extend(GetFocusNode(), GetFocusOffset() - 1);
    }
    else
    {
      // Otherwise it's harder, have to find the previous node
      printf("Sorry, don't know how to delete across frame boundaries yet\n");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  // Get an iterator
  nsRangeListIterator iter(this);
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
    Collapse(GetAnchorNode(), GetAnchorOffset()-1);
  else if (GetAnchorOffset() > 0)
    Collapse(GetAnchorNode(), GetAnchorOffset());
#ifdef DEBUG
  else
    printf("Don't know how to set selection back past frame boundary\n");
#endif

  return NS_OK;
}


/*
 * Return the anchor node and offset for the anchor point
 */
NS_IMETHODIMP
nsRangeList::GetAnchorNodeAndOffset(nsIDOMNode** outAnchorNode, PRInt32 *outAnchorOffset)
{
	if (!outAnchorNode || !outAnchorOffset)
		return NS_ERROR_NULL_POINTER;

	*outAnchorNode = mAnchorNode;
  *outAnchorOffset = mAnchorOffset;
	NS_IF_ADDREF(*outAnchorNode);
	return NS_OK;
}


/*
 * Return the anchor node and offset for the focus point
 */
NS_IMETHODIMP
nsRangeList::GetFocusNodeAndOffset(nsIDOMNode** outFocusNode, PRInt32 *outFocusOffset)
{
	if (!outFocusNode || !outFocusOffset)
		return NS_ERROR_NULL_POINTER;
	
  *outFocusNode = mFocusNode;
  *outFocusOffset = mFocusOffset;  
  NS_IF_ADDREF(*outFocusNode);
  return NS_OK;
}


//END nsIDOMSelection interface implementations

