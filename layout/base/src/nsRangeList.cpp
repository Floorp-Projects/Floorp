#include "nsICollection.h"
#include "nsIFactory.h"
#include "nsIEnumerator.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"
#include "nsIFocusTracker.h"
#include "nsRepository.h"
#include "nsLayoutCID.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsRange.h"

static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);
static NS_DEFINE_IID(kICollectionIID, NS_ICOLLECTION_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kISelectionIID, NS_ISELECTION_IID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kIDOMRangeIID, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

//PROTOTYPES
void selectFrames(nsIFrame *aBegin, PRInt32 aBeginOffset, nsIFrame *aEnd, PRInt32 aEndOffset, PRBool aSelect);


class nsRangeListIterator;

class nsRangeList : public nsICollection , public nsISelection
{
public:
/*BEGIN nsICollection interfaces
see the nsICollection for more details*/

  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_ISUPPORTS
  virtual nsresult AddItem(nsISupports *aRange);

  virtual nsresult RemoveItem(nsISupports *aRange);

  virtual nsresult Clear();
/*END nsICollection interfaces*/
/*BEGIN nsISelection interfaces*/
  virtual nsresult HandleKeyEvent(nsGUIEvent *aGuiEvent, nsIFrame *aFrame);
  virtual nsresult TakeFocus(nsIFocusTracker *aTracker, nsIFrame *aFrame, PRInt32 aOffset, PRInt32 aContentOffset, PRBool aContinueSelection);
/*END nsISelection interfacse*/
  nsRangeList();
  virtual ~nsRangeList();

private:
  friend class nsRangeListIterator;

  void ResizeBuffer(PRUint32 aNewBufSize);
  nsIDOMRange **mRangeArray;
  PRUint32 mCount;
  PRUint32 mBufferSize;
  nsCOMPtr<nsIDOMNode> mAnchorNode; //where did the selection begin
  PRInt32 mAnchorOffset;
  nsCOMPtr<nsIDOMNode> mFocusNode; //where is the carret
  PRInt32 mFocusOffset;
};


class nsRangeListIterator : public nsIEnumerator
{
public:
/*BEGIN nsIEnumerator interfaces
see the nsIEnumerator for more details*/

  NS_DECL_ISUPPORTS

  virtual nsresult First();

  virtual nsresult Last();

  virtual nsresult Next();

  virtual nsresult Prev();

  virtual nsresult CurrentItem(nsISupports **aRange);

  virtual nsresult IsDone();

/*END nsIEnumerator interfaces*/

private:
  friend class nsRangeList;
  nsRangeListIterator(nsRangeList *);
  virtual ~nsRangeListIterator();

  nsRangeList *mRangeList;
  PRUint32 mIndex;
};



nsresult NS_NewRangeList(nsICollection **);
nsresult NS_NewRangeList(nsICollection **aRangeList)
{
  nsRangeList *rlist = new nsRangeList;
  if (!rlist)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult result = rlist->QueryInterface(kICollectionIID , (void **)aRangeList);
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



nsresult
nsRangeListIterator::Next()
{
  if (mIndex < mRangeList->mCount -1 )
  {
    mIndex++;
    return NS_OK;
  }
  mIndex = mRangeList->mCount -1;
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::Prev()
{
  if (mIndex > 0 )
  {
    mIndex--;
    return NS_OK;
  }
  mIndex = 0;
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::First()
{
  if (!mRangeList)
    return NS_ERROR_NULL_POINTER;
  mIndex = 0;
  return NS_OK;
}



nsresult
nsRangeListIterator::Last()
{
  if (!mRangeList)
    return NS_ERROR_NULL_POINTER;
  mIndex = mRangeList->mCount -1;
  return NS_OK;
}



nsresult 
nsRangeListIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (mIndex >=0 && mIndex< mRangeList->mCount)
  {
    return mRangeList->mRangeArray[mIndex]->QueryInterface(kISupportsIID, (void **)aItem);
  }
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::IsDone()
{
  if ((mIndex == mRangeList->mCount -1) || !mRangeList->mCount) { //empty lists are always done
    return NS_OK;
  }
  else{
    return NS_COMFALSE;
  }
  return NS_OK;
}



NS_IMPL_ADDREF(nsRangeListIterator)

NS_IMPL_RELEASE(nsRangeListIterator)



nsresult
nsRangeListIterator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsICollection* tmp = mRangeList;
    nsISupports* tmp2 = tmp;

    *aInstancePtr = (void*)tmp2;
    NS_ADDREF(mRangeList);
    return NS_OK;
  }
  if (aIID.Equals(kICollectionIID)) {
    *aInstancePtr = (void *)mRangeList;
    NS_ADDREF(mRangeList);
    return NS_OK;
  }
  if (aIID.Equals(kIEnumeratorIID)) {
    *aInstancePtr = (void*)(nsIEnumerator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

////////////END nsIRangeListIterator methods

////////////BEGIN nsRangeList methods



nsRangeList::nsRangeList()
:mCount(0),mRangeArray(NULL),mBufferSize(0)
{
  NS_INIT_REFCNT();
}



nsRangeList::~nsRangeList()
{
  for (PRUint32 i=0;i < mCount; i++)
  {
    NS_IF_RELEASE(mRangeArray[i]);
  }
  delete []mRangeArray;
  mCount = 0;
  mBufferSize = 0;
}



void
nsRangeList::ResizeBuffer(PRUint32 aNewBufSize)
{
  if (aNewBufSize < mCount)
  {
    NS_NOTREACHED("ResizeBuffer");
    return;
  }
  if (aNewBufSize <= mBufferSize)
    return;
  nsIDOMRange **rangeArray = new nsIDOMRange *[aNewBufSize];
  if (!rangeArray)
  {
    NS_NOTREACHED("nsRangeList::ResizeBuffer");
    return ;
  }
  if (mRangeArray)
    memcpy(rangeArray,mRangeArray, aNewBufSize * sizeof (nsIDOMRange *));
  if (mRangeArray) 
    delete [] mRangeArray;
  mRangeArray = rangeArray;
  mBufferSize = aNewBufSize;
}



//END nsRangeList methods

//BEGIN nsICollection interface implementations


NS_IMPL_ADDREF(nsRangeList)

NS_IMPL_RELEASE(nsRangeList)


nsresult
nsRangeList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsICollection* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICollectionIID)) {
    *aInstancePtr = (void*)(nsICollection *)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(  kISelectionIID)) {
    *aInstancePtr = (void*)(nsISelection *)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEnumeratorIID)) {
    nsRangeListIterator *iterator =  new nsRangeListIterator(this);
    iterator->QueryInterface(kIEnumeratorIID,aInstancePtr);
    *aInstancePtr = (void*)(iterator);
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult
nsRangeList::AddItem(nsISupports *aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  ResizeBuffer(++mCount);
  aItem->QueryInterface(kISupportsIID,(void **)&mRangeArray[mCount -1]);
  return NS_OK;
}



nsresult
nsRangeList::RemoveItem(nsISupports *aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  for (PRUint32 i = 0; i < mCount;i++)
  {
    if (mRangeArray[i] == aItem)
    {
      NS_RELEASE(mRangeArray[i]);
      mRangeArray[i] = mRangeArray[--mCount];
      return NS_OK;
    }
  }
  return NS_COMFALSE;
}



nsresult
nsRangeList::Clear()
{
  for (PRUint32 i = 0; i < mCount;i++)
  {
    NS_RELEASE(mRangeArray[i]);
  }
  if (mRangeArray){
    delete []mRangeArray;
    mBufferSize = 0;
  }
  mRangeArray = nsnull;
  mCount = 0;
  return NS_OK;
}



//END nsICollection methods

//BEGIN nsISelection methods



nsresult
nsRangeList::HandleKeyEvent(nsGUIEvent *aGuiEvent, nsIFrame *aFrame)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//the idea of this helper method is to select, deselect "top to bootom" traversing through the frames
void
selectFrames(nsIFrame *aBegin, PRInt32 aBeginOffset, nsIFrame *aEnd, PRInt32 aEndOffset, PRBool aSelect)
{
  if (!aBegin || !aEnd)
    return;
  PRBool done = PR_FALSE;
#if 1
  printf("select frames");
  if (!aSelect)
    printf("deselect \n");
  else
    printf("select \n");
#endif
  while (!done)
  {
    if (aBegin == aEnd)
    {
      aBegin->SetSelected(aSelect, aBeginOffset, aEndOffset, PR_TRUE);
      done = PR_TRUE;
    }
    else {
      //else we neeed to select from begin to end 
      aBegin->SetSelected(aSelect, aBeginOffset, -1, PR_TRUE);
      aBeginOffset = -1;
      if (NS_FAILED(aBegin->GetNextSibling(aBegin)) || !aBegin)
      {
        done = PR_TRUE;
      }
    }
  }
}



nsresult
nsRangeList::TakeFocus(nsIFocusTracker *aTracker, nsIFrame *aFrame, PRInt32 aOffset, PRInt32 aContentOffset, PRBool aContinueSelection)
{
  if (!aTracker || !aFrame)
    return NS_ERROR_NULL_POINTER;
  nsIFrame *frame;
  nsIFrame *anchor;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> domNode;
  if (NS_SUCCEEDED(aFrame->GetContent(*getter_AddRefs(content)))){
    domNode = content;
    if (domNode && NS_SUCCEEDED(aTracker->GetFocus(&frame, &anchor))){
      //traverse through document and unselect crap here
      if (!aContinueSelection){
        if (frame && aFrame != frame)
          frame->SetSelected(PR_FALSE,0,0,PR_TRUE);
        if (anchor && aFrame != anchor)
          anchor->SetSelected(PR_FALSE,0,0,PR_TRUE);


//DEBUG CRAP
#if 1
        nsCOMPtr<nsIContent>oldContent;
        if (frame && NS_SUCCEEDED(frame->GetContent(*getter_AddRefs(oldContent)))){
          nsCOMPtr<nsIDOMNode>oldDomNode = oldContent;
          if (oldDomNode  && oldDomNode == mFocusNode) {
            PRInt32 result1 = ComparePoints(domNode, aOffset + aContentOffset, mFocusNode, mFocusOffset);
            if (result1 < 0)
              printf("result1 < 0\n");
            if (result1 == 0)
              printf("result1 = 0\n");
            if (result1  > 0)
              printf("result1 > 0\n");
          }
        }
#endif
        
        
        aFrame->SetSelected(PR_TRUE,aOffset,aOffset,PR_FALSE);
        aTracker->SetFocus(aFrame,aFrame);
        nsCOMPtr<nsIDOMRange> range;
        if (NS_SUCCEEDED(nsRepository::CreateInstance(kRangeCID, nsnull, kIDOMRangeIID, getter_AddRefs(range)))){ //create an irange
          if (domNode) {//get the node interface for the range object
            mFocusNode = domNode;
            mFocusOffset = aOffset + aContentOffset;
            mAnchorNode = domNode;
            mAnchorOffset = aOffset + aContentOffset;

            range->SetStart(domNode,aOffset + aContentOffset);
            range->SetEnd(domNode,aOffset + aContentOffset);
            nsCOMPtr<nsISupports> rangeISupports;
            rangeISupports = range;
            if (rangeISupports) {
              AddItem(rangeISupports);
            }
          }
        }
      }
      else {
        if (aFrame == frame){ //drag to same frame
          PRInt32 beginoffset;
          PRInt32 begincontentoffset;
          PRInt32 endoffset;
          PRBool selected;
          if (NS_SUCCEEDED(aFrame->GetSelected(&selected,&beginoffset,&endoffset, &begincontentoffset))){
            aFrame->SetSelected(PR_TRUE,beginoffset,aOffset,PR_FALSE);
            aTracker->SetFocus(aFrame,anchor);
          }
  
          if (anchor && NS_SUCCEEDED(anchor->GetSelected(&selected, &beginoffset, &endoffset, &begincontentoffset))){
            nsCOMPtr<nsIDOMRange> range;
            if (NS_SUCCEEDED(nsRepository::CreateInstance(kRangeCID, nsnull, kIDOMRangeIID, getter_AddRefs(range)))){ //create an irange
              if (domNode){
                mFocusNode = domNode;
                mFocusOffset = aOffset + aContentOffset;
                range->SetStart(domNode,begincontentoffset);
                range->SetEnd(domNode,aOffset + aContentOffset);
                nsCOMPtr<nsISupports> rangeISupports;
                if (rangeISupports = range) {
                  AddItem(rangeISupports);
                  return NS_OK;
                }
              }
            }
          }
        }
        else if (frame){ //we need to check to see what the order is.
          nsCOMPtr<nsIContent>oldContent;
          if (NS_SUCCEEDED(frame->GetContent(*getter_AddRefs(oldContent)))){
            nsCOMPtr<nsIDOMNode>oldDomNode;
            if ((oldDomNode =  oldContent) && oldDomNode == mFocusNode) {
              nsCOMPtr<nsIContent>anchorContent;
              if (NS_SUCCEEDED(anchor->GetContent(*getter_AddRefs(anchorContent)))){
                nsCOMPtr<nsIDOMNode>anchorDomNode;
                if ((anchorDomNode = anchorContent) && anchorDomNode == mAnchorNode) {


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
                  PRInt32 result1 = ComparePoints(mAnchorNode, mAnchorOffset, mFocusNode, mFocusOffset);
                  printf("result1 = %i\n",result1);
                  //compare old cursor to new cursor
                  PRInt32 result2 = ComparePoints(mFocusNode, mFocusOffset, domNode, aOffset + aContentOffset);
                  printf("result2 = %i\n",result2);
                  //compare anchor to new cursor
                  PRInt32 result3 = ComparePoints(mAnchorNode, mAnchorOffset ,domNode , aOffset + aContentOffset);
                  printf("result3 = %i\n",result3);

                  //heuristic babble
                  if (result1 <= 0 && result2 <= 0) {//a,1,2 or a1,2 or a,12 or a12
                    //continue selection from 1 to 2
                    printf("continue selection from OLD FOCUS point to NEW FOCUS (growth)\n");
                    selectFrames(frame,focusFrameOffsetEnd, aFrame, aOffset, PR_TRUE);
                  }
                  if (result3 <= 0 && result2 >= 0) {//a,2,1 or a2,1 or a,21 or a21
                    //deselect from 2 to 1
                    printf("deselect from NEW FOCUS point to OLD FOCUS ( shrinkage) \n");
                    selectFrames(aFrame, aOffset, frame,focusFrameOffsetEnd, PR_FALSE);
                  }
                  if (result1 >= 0 && result3 <= 0) {//1,a,2 or 1a,2 or 1,a2 or 1a2
                    //deselect from 1 to a
                    printf("deselect from OLD FOCUS to ANCHOR\n");
                    selectFrames(frame, focusFrameOffsetEnd, anchor, anchorFrameOffsetBegin, PR_FALSE);
                    //select from a to 2
                    printf("select from ANCHOR to NEW FOCUS point \n");
                    selectFrames(anchor, anchorFrameOffsetBegin, aFrame, aOffset, PR_TRUE);
                  }
                  if (result2 <= 0 && result3 >= 0) {//1,2,a or 12,a or 1,2a or 12a
                    //deselect from 1 to 2
                    printf("deselect from OLD FOCUS to NEW FOCUS (shrinkage)\n");
                    selectFrames(frame, focusFrameOffsetEnd, aFrame, aOffset, PR_FALSE);
                  }
                  if (result3 >= 0 && result1 <= 0) {//2,a,1 or 2a,1 or 2,a1 or 2a1
                    //deselect from a to 1
                    printf("deselect from ANCHOR to OLD FOCUS \n");
                    selectFrames(anchor, anchorFrameOffsetBegin, frame, focusFrameOffsetEnd, PR_FALSE);
                    //select from 2 to a
                    printf("select from NEW FOCUS to ANCHOR \n");
                    selectFrames(aFrame, aOffset, anchor, anchorFrameOffsetBegin, PR_TRUE);
                  }
                  if (result2 <= 0 && result1 >= 0) {//2,1,a or 21,a or 2,1a or 21a
                    //continue selection from 2 to 1
                    printf("select from NEW FOCUS to OLD FOCUS (growth)\n");
                    selectFrames(aFrame, aOffset, frame,focusFrameOffsetEnd, PR_TRUE);
                  }
                  mFocusNode = domNode;
                  mFocusOffset = aOffset + aContentOffset;
                  aTracker->SetFocus(aFrame,anchor);
                }
              }
            }
          }
        }
/*
#1 figure out the order
we can tell the direction of the selection by asking for the anchors selection
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
      }
    }
  }
  return NS_OK;
}

//END nsISelection methods

