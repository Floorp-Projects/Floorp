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
#include "nsIDOMEvent.h"

#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"

static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFrameSelectionIID, NS_IFRAMESELECTION_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kIDOMRangeIID, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

//PROTOTYPES
static void selectFrames(nsIFrame *aBegin, PRUint32 aBeginOffset, nsIFrame *aEnd, PRUint32 aEndOffset, nsDirection aDir, PRUint32 aFlags);
static void unselectFrames(nsIFrame *aRootFrame);
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
//#define DEBUG_NAVIGATION

class nsRangeListIterator;

class nsRangeList : public nsIFrameSelection,
                    public nsIDOMSelection,
                    public nsIScriptObjectOwner
{
public:
  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_ISUPPORTS

/*BEGIN nsIFrameSelection interfaces*/
  NS_IMETHOD HandleTextEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGUIEvent);
  NS_IMETHOD HandleKeyEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGuiEvent);
  NS_IMETHOD TakeFocus(nsIFocusTracker *aTracker, nsIFrame *aFrame, PRInt32 aOffset, PRInt32 aContentOffset, PRBool aContinueSelection);
  NS_IMETHOD ResetSelection(nsIFocusTracker *aTracker, nsIFrame *aStartFrame);
  NS_IMETHOD EnableFrameNotification(PRBool aEnable){mNotifyFrames = aEnable; return NS_OK;}
/*END nsIFrameSelection interfacse*/

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
  NS_IMETHOD    DeleteFromDocument();
  NS_IMETHOD    AddRange(nsIDOMRange* aRange);

  NS_IMETHOD    StartBatchChanges();
  NS_IMETHOD    EndBatchChanges();

  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener);
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove);
/*END nsIDOMSelection interface implementations*/

/*BEGIN nsIScriptObjectOwner interface implementations*/
  NS_IMETHOD 		GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD 		SetScriptObject(void *aScriptObject);
/*END nsIScriptObjectOwner interface implementations*/

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

	// inline methods for convenience. Note, these don't addref
  nsIDOMNode*  FetchAnchorNode()        { return mAnchorNode;    } 			//where did the selection begin
  PRInt32      FetchAnchorOffset()			{ return mAnchorOffset;  }

  nsIDOMNode*  FetchFocusNode()         { return mFocusNode;     }  		//where is the carret
  PRInt32      FetchFocusOffset()       { return mFocusOffset;   }
  
  void         setAnchor(nsIDOMNode*, PRInt32);
  void         setFocus(nsIDOMNode*, PRInt32);
  
  PRUint32     GetBatching(){return mBatching;}
  PRBool       GetNotifyFrames(){return mNotifyFrames;}
  void         SetDirty(PRBool aDirty=PR_TRUE){if (mBatching) mChangesDuringBatching = aDirty;}

  nsresult     NotifySelectionListeners();			// add parameters to say collapsed etc?

  nsCOMPtr<nsISupportsArray> mRangeArray;

  nsCOMPtr<nsIDOMNode> mAnchorNode; //where did the selection begin
  PRInt32 mAnchorOffset;
  nsCOMPtr<nsIDOMNode> mFocusNode; //where is the carret
  PRInt32 mFocusOffset;

  //batching
  PRUint32 mBatching;
  PRBool mChangesDuringBatching;
  PRBool mNotifyFrames;
  
  nsCOMPtr<nsISupportsArray> mSelectionListeners;
  
  // for nsIScriptContextOwner
  void*		mScriptObject;
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

#ifdef XP_MAC
#pragma mark -
#endif

////////////BEGIN nsRangeList methods

nsRangeList::nsRangeList()
{
  NS_INIT_REFCNT();
  NS_NewISupportsArray(getter_AddRefs(mRangeArray));
  NS_NewISupportsArray(getter_AddRefs(mSelectionListeners));
  mBatching = 0;
  mChangesDuringBatching = PR_FALSE;
  mNotifyFrames = PR_TRUE;
  mScriptObject = nsnull;
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
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
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

// note: this can return a nil anchor node
NS_METHOD nsRangeList::GetAnchorNode(nsIDOMNode** aAnchorNode)
{
	if (!aAnchorNode)
		return NS_ERROR_NULL_POINTER;
	
	*aAnchorNode = mAnchorNode;
	NS_IF_ADDREF(*aAnchorNode);

	return NS_OK;
}

NS_METHOD nsRangeList::GetAnchorOffset(PRInt32* aAnchorOffset)
{
	*aAnchorOffset = mAnchorOffset;
	return NS_OK;
}

// note: this can return a nil focus node
NS_METHOD nsRangeList::GetFocusNode(nsIDOMNode** aFocusNode)
{
	if (!aFocusNode)
		return NS_ERROR_NULL_POINTER;
	
	*aFocusNode = mFocusNode;
	NS_IF_ADDREF(*aFocusNode);

	return NS_OK;
}

NS_METHOD nsRangeList::GetFocusOffset(PRInt32* aFocusOffset)
{
	*aFocusOffset = mFocusOffset;
	return NS_OK;
}


void nsRangeList::setAnchor(nsIDOMNode* node, PRInt32 offset)
{
  mAnchorNode = dont_QueryInterface(node);
  mAnchorOffset = offset;
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
  setFocus(nsnull,0);
  setAnchor(nsnull,0);
  if (!mRangeArray)
    return NS_ERROR_FAILURE;
  for (PRUint32 i = 0; i < mRangeArray->Count();i++)
  {
    mRangeArray->RemoveElementAt(i);
    // Does RemoveElementAt also delete the elements?
  }
  return NS_OK;
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
  /*
  printf("print DOMRANGE 0x%lx\t start: 0x%lx %ld, \t end: 0x%lx,%ld \n",
         (unsigned long)aDomRange,
         (unsigned long)(nsIDOMNode*)startNode, (long)startOffset,
         (unsigned long)(nsIDOMNode*)endNode, (long)endOffset);
         */
}

NS_IMETHODIMP
nsRangeList::HandleTextEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGUIEvent)
{
	if (!aGUIEvent || !aTracker)
		return NS_ERROR_NULL_POINTER;

#ifdef DEBUG_TAGUE
	printf("nsRangeList: HandleTextEvent\n");
#endif

	nsIFrame *anchor;
	nsIFrame *frame;
	nsresult result = aTracker->GetFocus(&frame, &anchor);
	if (NS_FAILED(result))
		return result;
	if (NS_TEXT_EVENT == aGUIEvent->message) {
		PRBool selected;
		PRInt32 beginoffset = 0;
		PRInt32 endoffset;
		PRInt32 contentoffset;
		nsresult result = NS_OK;

		nsTextEvent *textEvent = (nsTextEvent *)aGUIEvent; //this is ok. It really is a textevent

		PRInt32   offsetused = beginoffset;
		nsSelectionAmount amount = eSelectCharacter;		// for now
		result = frame->GetSelected(&selected,&beginoffset,&endoffset, &contentoffset);

		result = ScrollIntoView(aTracker);
	}
	return NS_OK;
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
#ifdef DEBUG_NAVIGATION
        printf("debug vk left\n");
#endif
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
        result = ScrollIntoView(aTracker);
        break;
      case nsIDOMEvent::VK_RIGHT : 
        //we need to look for the next PAINTED location to move the cursor to.
#ifdef DEBUG_NAVIGATION
        printf("debug vk right\n");
#endif
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
        result = ScrollIntoView(aTracker);
        break;
      case nsIDOMEvent::VK_UP : 
#ifdef DEBUG_NAVIGATION
        printf("debug vk up\n");
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
         (unsigned long)(nsIDOMNode*)FetchAnchorNode(), FetchAnchorOffset());
  printf("Focus is 0x%lx, %d\n",
         (unsigned long)(nsIDOMNode*)FetchFocusNode(), FetchFocusOffset());
  printf(" ... end of selection\n");
}
#endif /* DEBUG */



//recursive-oid method to get next frame
nsIFrame *
getNextFrame(nsIFrame *aStart)
{
  nsIFrame *result;
  nsIFrame *parent = aStart;
  if (parent && NS_SUCCEEDED(parent->FirstChild(nsnull, &result)) && result){
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

// unselect all frames under the root frame
void
unselectFrames(nsIFrame *aRootFrame)
{
  nsIFrame *theFrame = aRootFrame;
  nsSelectionStruct ss = {0};  // all that's important is that the selection_on flag is not lit.
  while (theFrame)
  {
    theFrame->SetSelected(&ss);
    theFrame = getNextFrame(theFrame);
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
    nsCOMPtr<nsIContent> parent;
    nsCOMPtr<nsIContent> parent2;
    if (NS_FAILED(content->GetParent(*getter_AddRefs(parent))) || !parent)
      return NS_ERROR_FAILURE;
    if (NS_FAILED(parent->GetParent(*getter_AddRefs(parent2))) || !parent2)
      return NS_ERROR_FAILURE;
    domNode = do_QueryInterface(content);
    parent = nsCOMPtr<nsIContent>();//just force a release now even though we dont have to.
    parent2 = nsCOMPtr<nsIContent>();

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
        PRUint32 batching = mBatching;//hack to use the collapse code.
        PRBool changes = mChangesDuringBatching;
        mBatching = 1;
        Collapse(domNode, aContentOffset + aOffset);
        mBatching = batching;
        mChangesDuringBatching = changes;
      }
      else {
        if (aFrame == frame){ //drag to same frame
          PRInt32 beginoffset = 0;
          PRInt32 begincontentoffset = 0;
          PRInt32 endoffset = 0;
          PRBool selected = PR_FALSE;
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
            if (oldDomNode && (oldDomNode.get() == FetchFocusNode())) {
              nsCOMPtr<nsIContent> anchorContent;
              if (NS_SUCCEEDED(anchor->GetContent(getter_AddRefs(anchorContent))) && anchorContent){
                nsCOMPtr<nsIDOMNode>anchorDomNode(do_QueryInterface(anchorContent));
                if (anchorDomNode && anchorDomNode.get() == FetchAnchorNode()) {


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
                    result2 = ComparePoints(FetchFocusNode(), FetchFocusOffset(),
                                            domNode, aOffset );
                  //compare anchor to new cursor
                  PRInt32 result3 = compareFrames(anchor,aFrame);
                  if (result3 == 0)
                    result3 = ComparePoints(FetchAnchorNode(), FetchAnchorOffset(),
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
  if (FetchAnchorNode() && FetchFocusNode()){
    anchorContent =  do_QueryInterface(FetchAnchorNode(),&res);
    if (NS_SUCCEEDED(res))
      frameContent = do_QueryInterface(FetchFocusNode(),&res);
  }
  else
    res = NS_OK;
  if (NS_SUCCEEDED(res)){
    // hack - joe 3/16/99: first tell all frames to unselect
    unselectFrames(aStartFrame);
  
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
            frameOffset = FetchFocusOffset();
          if ( startContent == anchorContent ) 
            anchorOffset = FetchAnchorOffset();
          nsSelectionStruct ss={nsSelectionStruct::SELON|nsSelectionStruct::CHECKANCHOR|nsSelectionStruct::CHECKFOCUS
            , startOffset, endOffset, 0,0, anchorOffset,frameOffset, eDirNext, PR_FALSE};
          if (anchorOffset> frameOffset)
            ss.mDir = eDirPrevious;
          result->SetSelectedContentOffsets(&ss, 
                                            aTracker, &result);
        }
        else{
          if (startContent == frameContent)
            frameOffset = FetchFocusOffset();
          if ( startContent == anchorContent ) 
            anchorOffset = FetchAnchorOffset();
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
                frameOffset = FetchFocusOffset();
              if ( endContent == anchorContent ) 
                anchorOffset = FetchAnchorOffset();
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

#ifdef XP_MAC
#pragma mark -
#endif

//BEGIN nsIDOMSelection interface implementations

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
  if (NS_FAILED(result))
    return result;
    
	return NotifySelectionListeners();
}

/*
 * IsCollapsed -- is the whole selection just one point, or unset?
 */
NS_IMETHODIMP
nsRangeList::GetIsCollapsed(PRBool* aIsCollapsed)
{
	if (!aIsCollapsed)
		return NS_ERROR_NULL_POINTER;
		
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


NS_IMETHODIMP
nsRangeList::GetRangeCount(PRInt32* aRangeCount)
{
	if (!aRangeCount)
		return NS_ERROR_NULL_POINTER;

	if (mRangeArray)
	{
		*aRangeCount = mRangeArray->Count();
	}
	else
	{
		*aRangeCount = 0;
	}
	
	return NS_OK;
}

NS_IMETHODIMP
nsRangeList::GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn)
{
	if (!aReturn)
		return NS_ERROR_NULL_POINTER;

	if (!mRangeArray)
		return NS_ERROR_INVALID_ARG;
		
	if (aIndex < 0 || aIndex >= mRangeArray->Count())
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

    if ((FetchFocusNode() == endNode.get()) && (FetchFocusOffset() == endOffset))
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
    
    if ((FetchFocusNode() == startNode.get()) && (FetchFocusOffset() == startOffset))
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
  GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
  {
    // If the offset is positive, then it's easy:
    if (FetchFocusOffset() > 0)
    {
      Extend(FetchFocusNode(), FetchFocusOffset() - 1);
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
    Collapse(FetchAnchorNode(), FetchAnchorOffset()-1);
  else if (FetchAnchorOffset() > 0)
    Collapse(FetchAnchorNode(), FetchAnchorOffset());
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

// BEGIN nsIScriptContextOwner interface implementations
NS_IMETHODIMP
nsRangeList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
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
nsRangeList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

// END nsIScriptContextOwner interface implementations

