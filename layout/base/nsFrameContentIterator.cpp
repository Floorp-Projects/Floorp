/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCOMPtr.h"
#include "nsIContentIterator.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIEnumerator.h"
#include "nsFrameList.h"

class nsFrameContentIterator : public nsIContentIterator
{
public:
  nsFrameContentIterator(nsIPresContext* aPresContext, nsIFrame* aFrame);
  virtual ~nsFrameContentIterator();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentIterator
  NS_IMETHOD Init(nsIContent* aRoot);
  NS_IMETHOD Init(nsIDOMRange* aRange);
  
  NS_IMETHOD First();
  NS_IMETHOD Last();
  NS_IMETHOD Next();
  NS_IMETHOD Prev();

  NS_IMETHOD CurrentNode(nsIContent **aNode);
  NS_IMETHOD IsDone();
  NS_IMETHOD PositionAt(nsIContent* aCurNode);

private:
  nsCOMPtr<nsIPresContext>  mPresContext;
  nsIFrame*                 mParentFrame;
  nsIFrame*                 mCurrentChild;
  PRBool                    mIsDone;
};

nsFrameContentIterator::nsFrameContentIterator(nsIPresContext* aPresContext,
                                                   nsIFrame*       aFrame)
  : mPresContext(aPresContext), mParentFrame(aFrame), mIsDone(PR_FALSE)
{
  First();
}

NS_IMPL_ISUPPORTS1(nsFrameContentIterator, nsIContentIterator);

nsFrameContentIterator::~nsFrameContentIterator()
{
}

NS_IMETHODIMP
nsFrameContentIterator::Init(nsIContent* aRoot)
{
  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP
nsFrameContentIterator::Init(nsIDOMRange* aRange)
{
  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP
nsFrameContentIterator::First()
{
  // Get the first child frame and make it the current node
  mParentFrame->FirstChild(mPresContext, nsnull, &mCurrentChild);
  if (!mCurrentChild) {
    return NS_ERROR_FAILURE;
  }
  mIsDone = PR_FALSE;
  return NS_OK;
}


static nsIFrame*
GetNextChildFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null pointer");

  // Get the last-in-flow
  aFrame = aFrame->GetLastInFlow();

  // Get its next sibling
  nsIFrame* nextSibling;
  aFrame->GetNextSibling(&nextSibling);

  // If there's no next sibling, then check if the parent frame
  // has a next-in-flow and look there
  if (!nextSibling) {
    nsIFrame* parent;
    aFrame->GetParent(&parent);
    parent->GetNextInFlow(&parent);

    if (parent) {
      parent->FirstChild(aPresContext, nsnull, &nextSibling);
    }
  }

  return nextSibling;
}

NS_IMETHODIMP
nsFrameContentIterator::Last()
{
  nsIFrame* nextChild;

  // Starting with the first child walk and find the last child
  mCurrentChild = nsnull;
  mParentFrame->FirstChild(mPresContext, nsnull, &nextChild);
  while (nextChild) {
    mCurrentChild = nextChild;
    nextChild = ::GetNextChildFrame(mPresContext, nextChild);
  }

  if (!mCurrentChild) {
    return NS_ERROR_FAILURE;
  }
  mIsDone = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameContentIterator::Next()
{
  nsIFrame* nextChild = ::GetNextChildFrame(mPresContext, mCurrentChild);

  if (nextChild) {
    // Advance to the next child
    mCurrentChild = nextChild;

    // If we're at the end then the collection is at the end
    mIsDone = (nsnull == ::GetNextChildFrame(mPresContext, mCurrentChild));
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

static nsIFrame*
GetPrevChildFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null pointer");

  // Get its previous sibling. Because we have a singly linked list we
  // need to search from the first child
  nsIFrame* parent;
  nsIFrame* firstChild;
  nsIFrame* prevSibling;

  aFrame->GetParent(&parent);
  parent->FirstChild(aPresContext, nsnull, &firstChild);

  NS_ASSERTION(firstChild, "parent has no first child");
  nsFrameList frameList(firstChild);
  prevSibling = frameList.GetPrevSiblingFor(aFrame);

  // If there's no previous sibling, then check if the parent frame
  // has a prev-in-flow and look there
  if (!prevSibling) {
    parent->GetPrevInFlow(&parent);

    if (parent) {
      parent->FirstChild(aPresContext, nsnull, &firstChild);
      frameList.SetFrames(firstChild);
      prevSibling = frameList.LastChild();
    }
  }
  
  // Get the first-in-flow
  while (PR_TRUE) {
    nsIFrame* prevInFlow;
    prevSibling->GetPrevInFlow(&prevInFlow);
    if (prevInFlow) {
      prevSibling = prevInFlow;
    } else {
      break;
    }
  }

  return prevSibling;
}

NS_IMETHODIMP
nsFrameContentIterator::Prev()
{
  nsIFrame* prevChild = ::GetPrevChildFrame(mPresContext, mCurrentChild);

  if (prevChild) {
    // Make the previous child the current child
    mCurrentChild = prevChild;
    
    // If we're at the beginning then the collection is at the end
    mIsDone = (nsnull == ::GetPrevChildFrame(mPresContext, mCurrentChild));
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameContentIterator::CurrentNode(nsIContent **aNode)
{
  if (mCurrentChild) {
    mCurrentChild->GetContent(aNode);
    return NS_OK;

  } else {
    *aNode = nsnull;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsFrameContentIterator::IsDone()
{
  return mIsDone ? NS_OK : NS_ENUMERATOR_FALSE;
}

NS_IMETHODIMP
nsFrameContentIterator::PositionAt(nsIContent* aCurNode)
{
  nsIFrame* child;

  // Starting with the first child frame search for the child frame
  // with the matching content object
  mParentFrame->FirstChild(mPresContext, nsnull, &child);
  while (child) {
    nsCOMPtr<nsIContent>  content;

    child->GetContent(getter_AddRefs(content));
    if (content.get() == aCurNode) {
      break;
    }
    child = ::GetNextChildFrame(mPresContext, child);
  }

  if (child) {
    // Make it the current child
    mCurrentChild = child;
    mIsDone = PR_FALSE;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
NS_NewFrameContentIterator(nsIPresContext*      aPresContext,
                           nsIFrame*            aFrame,
                           nsIContentIterator** aIterator)
{
  NS_ENSURE_ARG_POINTER(aIterator);
  if (!aIterator) {
    return NS_ERROR_NULL_POINTER;
  }
  NS_ENSURE_ARG_POINTER(aFrame);
  if (!aFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  
  // Make sure the frame corresponds to generated content
#ifdef DEBUG
  nsFrameState  frameState;
  aFrame->GetFrameState(&frameState);
  NS_ASSERTION(frameState & NS_FRAME_GENERATED_CONTENT, "unexpected frame");
#endif

  nsFrameContentIterator* it = new nsFrameContentIterator(aPresContext, aFrame);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContentIterator), (void **)aIterator);
}
