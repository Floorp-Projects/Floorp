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
static void selectFrames(nsIFrame *aBegin, PRInt32 aBeginOffset, nsIFrame *aEnd, PRInt32 aEndOffset, PRBool aSelect, PRBool aForwards);
static PRInt32 compareFrames(nsIFrame *aBegin, nsIFrame *aEnd);
static nsIFrame * getNextFrame(nsIFrame *aStart);


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
  PRInt32     mIndex;
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
  mIndex++;
  if (mIndex < (PRInt32)mRangeList->mCount)
    return NS_OK;
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::Prev()
{
  mIndex--;
  if (mIndex >= 0 )
    return NS_OK;
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
  mIndex = (PRInt32)mRangeList->mCount-1;
  return NS_OK;
}



nsresult 
nsRangeListIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (mIndex >=0 && mIndex < (PRInt32)mRangeList->mCount)
  {
    return mRangeList->mRangeArray[mIndex]->QueryInterface(kISupportsIID, (void **)aItem);
  }
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::IsDone()
{
  if (mIndex >= 0 && mIndex < (PRInt32)mRangeList->mCount ) { 
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


//recursive-oid method to get next frame
nsIFrame *
getNextFrame(nsIFrame *aStart)
{
  nsIFrame *result;
  nsIFrame *parent = aStart;
  while(1){
    if (NS_SUCCEEDED(parent->GetNextSibling(result)) && result){
      parent = result;
      while (NS_SUCCEEDED(parent->FirstChild(nsnull, parent)) && parent)
        result = parent;//eventually will have no children
      return result;
    }
    else
      if (NS_FAILED(parent->GetParent(parent)) || !parent)
        return nsnull;
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
  if (NS_SUCCEEDED(aBegin->GetContent(*getter_AddRefs(beginContent)))){
    nsCOMPtr<nsIDOMNode>beginNode (beginContent);
    nsCOMPtr<nsIContent> endContent;
    if (NS_SUCCEEDED(aEnd->GetContent(*getter_AddRefs(endContent)))){
      nsCOMPtr<nsIDOMNode>endNode (endContent);
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



//the idea of this helper method is to select, deselect "top to bootom" traversing through the frames
void
selectFrames(nsIFrame *aBegin, PRInt32 aBeginOffset, nsIFrame *aEnd, PRInt32 aEndOffset, PRBool aSelect, PRBool aForward)
{
  if (!aBegin || !aEnd)
    return;
  PRBool done = PR_FALSE;
  while (!done)
  {
    if (aBegin == aEnd)
    {
      if (aForward)
        aBegin->SetSelected(aSelect, aBeginOffset, aEndOffset, PR_TRUE);
      else
        aBegin->SetSelected(aSelect, aEndOffset, aBeginOffset, PR_TRUE);
      done = PR_TRUE;
    }
    else {
      //else we neeed to select from begin to end 
      if (aForward){
        aBegin->SetSelected(aSelect, aBeginOffset, -1, PR_TRUE);
      }
      else{
        aBegin->SetSelected(aSelect, -1, aBeginOffset, PR_TRUE);
      }
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
nsresult
nsRangeList::TakeFocus(nsIFocusTracker *aTracker, nsIFrame *aFrame, PRInt32 aOffset, PRInt32 aContentOffset, PRBool aContinueSelection)
{
  if (!aTracker || !aFrame)
    return NS_ERROR_NULL_POINTER;
  //HACKHACKHACK
  nsIFrame *parent = aFrame;
  for (int i=0; i <3; i++){
    if (NS_FAILED(parent->GetParent(parent)) || !parent)
      return NS_ERROR_NULL_POINTER;
  }
  //END HACK
  Clear(); //change this later 
  nsIFrame *frame;
  nsIFrame *anchor;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> domNode;
  if (NS_SUCCEEDED(aFrame->GetContent(*getter_AddRefs(content)))){
    domNode = (nsIContent *)content;
    if (domNode && NS_SUCCEEDED(aTracker->GetFocus(&frame, &anchor))){
      //traverse through document and unselect crap here
      if (!aContinueSelection){
        if (anchor && frame && anchor != frame ){//selected across frames, must "deselect" frames between in correct order
          PRInt32 compareResult = compareFrames(anchor,frame);
          if ( compareResult < 0 )
            selectFrames(anchor,0,frame, -1, PR_FALSE, PR_TRUE); //unselect all between
          else if (compareResult > 0 )
            selectFrames(frame,0,anchor, -1, PR_FALSE, PR_FALSE); //unselect all between
//          else real bad put in assert here
        }
        else if (frame && frame != aFrame){
          frame->SetSelected(PR_FALSE, 0, -1, PR_TRUE);//just turn off selection if the previous frame
        }

//DEBUG CRAP
#if 0
        nsCOMPtr<nsIContent>oldContent;
        if (frame && NS_SUCCEEDED(frame->GetContent(*getter_AddRefs(oldContent)))){
          nsCOMPtr<nsIDOMNode>oldDomNode(oldContent);
          if (oldDomNode  && oldDomNode == mFocusNode) {
            PRInt32 result1 = ComparePoints(domNode, aOffset + aContentOffset, mFocusNode, mFocusOffset);
            printf("result1 = %i\n",result1);
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
            nsCOMPtr<nsISupports> rangeISupports (range);
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
            aFrame->SetSelected(PR_TRUE, beginoffset, aOffset,PR_FALSE);
            aTracker->SetFocus(aFrame,anchor);
          }
  
          nsCOMPtr<nsIDOMRange> range;
          if (NS_SUCCEEDED(nsRepository::CreateInstance(kRangeCID, nsnull, kIDOMRangeIID, getter_AddRefs(range)))){ //create an irange
            if (domNode){
              mFocusNode = domNode;
              mFocusOffset = aOffset + aContentOffset;
              range->SetStart(mAnchorNode,mAnchorOffset);
              range->SetEnd(domNode,aOffset + aContentOffset);
              nsCOMPtr<nsISupports> rangeISupports(range);
              if (rangeISupports) {
                AddItem(rangeISupports);
                return NS_OK;
              }
            }
          }
        }
        else if (frame){ //we need to check to see what the order is.
          nsCOMPtr<nsIContent>oldContent;
          if (NS_SUCCEEDED(frame->GetContent(*getter_AddRefs(oldContent)))){
            nsCOMPtr<nsIDOMNode>oldDomNode(oldContent);
            if (oldDomNode && oldDomNode == mFocusNode) {
              nsCOMPtr<nsIContent>anchorContent;
              if (NS_SUCCEEDED(anchor->GetContent(*getter_AddRefs(anchorContent)))){
                nsCOMPtr<nsIDOMNode>anchorDomNode(anchorContent);
                if (anchorDomNode && anchorDomNode == mAnchorNode) {


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
                    result2 = ComparePoints(mFocusNode, mFocusOffset, domNode, aOffset + aContentOffset);
                  //compare anchor to new cursor
                  PRInt32 result3 = compareFrames(anchor,aFrame);
                  if (result3 == 0)
                    result3 = ComparePoints(mAnchorNode, mAnchorOffset ,domNode , aOffset + aContentOffset);

                  if (result1 == 0 && result3 < 0)
                  {
                    selectFrames(anchor,anchorFrameOffsetBegin, aFrame, aOffset + aContentOffset, PR_TRUE, PR_TRUE); //last true is forwards
                  }
                  else if (result1 == 0 && result3 > 0)
                  {
                    selectFrames(aFrame, aOffset , anchor,anchorFrameOffsetBegin, PR_TRUE, PR_FALSE);//last true is backwards
                  }
                  else if (result1 <= 0 && result2 <= 0) {//a,1,2 or a1,2 or a,12 or a12
                    //continue selection from 1 to 2
                    selectFrames(frame,PR_MIN(focusFrameOffsetEnd,focusFrameOffsetBegin), aFrame, aOffset, PR_TRUE, PR_TRUE);
                  }
                  else if (result3 <= 0 && result2 >= 0) {//a,2,1 or a2,1 or a,21 or a21
                    //deselect from 2 to 1
                    selectFrames(aFrame, aOffset, frame,focusFrameOffsetEnd, PR_FALSE, PR_TRUE);
                    if (anchor != aFrame)
                      selectFrames(aFrame, 0, aFrame,aOffset, PR_TRUE, PR_TRUE);
                    else
                      selectFrames(anchor, anchorFrameOffsetBegin, aFrame, aOffset, PR_TRUE ,PR_TRUE);
                  }
                  else if (result1 >= 0 && result3 <= 0) {//1,a,2 or 1a,2 or 1,a2 or 1a2
                    //deselect from 1 to a
                    selectFrames(frame, focusFrameOffsetEnd, anchor, anchorFrameOffsetBegin, PR_FALSE, PR_FALSE);
                    //select from a to 2
                    selectFrames(anchor, anchorFrameOffsetBegin, aFrame, aOffset, PR_TRUE, PR_TRUE);
                  }
                  else if (result2 <= 0 && result3 >= 0) {//1,2,a or 12,a or 1,2a or 12a
                    //deselect from 1 to 2
                    selectFrames(frame, focusFrameOffsetEnd, aFrame, aOffset, PR_FALSE, PR_FALSE);
                    if (anchor != aFrame)
                      selectFrames(aFrame, aOffset, aFrame, -1, PR_TRUE, PR_FALSE);
                    else
                      selectFrames(aFrame, aOffset, anchor, anchorFrameOffsetBegin, PR_TRUE ,PR_FALSE);

                  }
                  else if (result3 >= 0 && result1 <= 0) {//2,a,1 or 2a,1 or 2,a1 or 2a1
                    //deselect from a to 1
                    selectFrames(anchor, anchorFrameOffsetBegin, frame, focusFrameOffsetEnd, PR_FALSE, PR_TRUE);
                    //select from 2 to a
                    selectFrames(aFrame, aOffset, anchor, anchorFrameOffsetBegin, PR_TRUE, PR_FALSE);
                  }
                  else if (result2 >= 0 && result1 >= 0) {//2,1,a or 21,a or 2,1a or 21a
                    //continue selection from 2 to 1
                    selectFrames(aFrame, aOffset, frame,PR_MAX(focusFrameOffsetEnd,focusFrameOffsetBegin), PR_TRUE, PR_FALSE);
                  }
                }
                mFocusNode = domNode;
                mFocusOffset = aOffset + aContentOffset;
                aTracker->SetFocus(aFrame,anchor);
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

