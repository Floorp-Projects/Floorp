/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
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

#include "nsCOMPtr.h"
#include "nsIContentIterator.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIEnumerator.h"
#include "nsFrameList.h"

class nsFrameContentIterator : public nsIContentIterator
{
public:
  nsFrameContentIterator(nsPresContext* aPresContext, nsIFrame* aFrame);
  virtual ~nsFrameContentIterator();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentIterator
  virtual nsresult Init(nsIContent* aRoot);
  virtual nsresult Init(nsIDOMRange* aRange);

  virtual void First();
  virtual void Last();
  virtual void Next();
  virtual void Prev();

  virtual nsIContent *GetCurrentNode();
  virtual PRBool IsDone();
  virtual nsresult PositionAt(nsIContent* aCurNode);

private:
  nsCOMPtr<nsPresContext>  mPresContext;
  nsIFrame*                 mParentFrame;
  nsIFrame*                 mCurrentChild;
  PRBool                    mIsDone;
};

nsFrameContentIterator::nsFrameContentIterator(nsPresContext* aPresContext,
                                               nsIFrame*       aFrame)
  : mPresContext(aPresContext), mParentFrame(aFrame), mIsDone(PR_FALSE)
{
  First();
}

NS_IMPL_ISUPPORTS1(nsFrameContentIterator, nsIContentIterator)

nsFrameContentIterator::~nsFrameContentIterator()
{
}

nsresult
nsFrameContentIterator::Init(nsIContent* aRoot)
{
  return NS_ERROR_ALREADY_INITIALIZED;
}

nsresult
nsFrameContentIterator::Init(nsIDOMRange* aRange)
{
  return NS_ERROR_ALREADY_INITIALIZED;
}

void
nsFrameContentIterator::First()
{
  // Get the first child frame and make it the current node
  mCurrentChild = mParentFrame->GetFirstChild(nsnull);

  mIsDone = !mCurrentChild;
}


static nsIFrame*
GetNextChildFrame(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null pointer");

  // Get the last-in-flow
  aFrame = aFrame->GetLastInFlow();

  // Get its next sibling
  nsIFrame* nextSibling = aFrame->GetNextSibling();

  // If there's no next sibling, then check if the parent frame
  // has a next-in-flow and look there
  if (!nextSibling) {
    nsIFrame* parent = aFrame->GetParent()->GetNextInFlow();

    if (parent) {
      nextSibling = parent->GetFirstChild(nsnull);
    }
  }

  return nextSibling;
}

void
nsFrameContentIterator::Last()
{
  // Starting with the first child walk and find the last child
  mCurrentChild = nsnull;
  nsIFrame* nextChild = mParentFrame->GetFirstChild(nsnull);
  while (nextChild) {
    mCurrentChild = nextChild;
    nextChild = ::GetNextChildFrame(mPresContext, nextChild);
  }

  mIsDone = !mCurrentChild;
}

void
nsFrameContentIterator::Next()
{
  nsIFrame* nextChild = ::GetNextChildFrame(mPresContext, mCurrentChild);

  if (nextChild) {
    // Advance to the next child
    mCurrentChild = nextChild;

    // If we're at the end then the collection is at the end
    mIsDone = (nsnull == ::GetNextChildFrame(mPresContext, mCurrentChild));

    return;
  }

  // No next frame, we're done.
  mIsDone = PR_TRUE;
}

static nsIFrame*
GetPrevChildFrame(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null pointer");

  // Get its previous sibling. Because we have a singly linked list we
  // need to search from the first child
  nsIFrame* parent = aFrame->GetParent();
  nsIFrame* prevSibling;
  nsIFrame* firstChild = parent->GetFirstChild(nsnull);

  NS_ASSERTION(firstChild, "parent has no first child");
  nsFrameList frameList(firstChild);
  prevSibling = frameList.GetPrevSiblingFor(aFrame);

  // If there's no previous sibling, then check if the parent frame
  // has a prev-in-flow and look there
  if (!prevSibling) {
    parent = parent->GetPrevInFlow();

    if (parent) {
      firstChild = parent->GetFirstChild(nsnull);
      frameList.SetFrames(firstChild);
      prevSibling = frameList.LastChild();
    }
  }
  
  // Get the first-in-flow
  while (PR_TRUE) {
    nsIFrame* prevInFlow = prevSibling->GetPrevInFlow();
    if (prevInFlow) {
      prevSibling = prevInFlow;
    } else {
      break;
    }
  }

  return prevSibling;
}

void
nsFrameContentIterator::Prev()
{
  nsIFrame* prevChild = ::GetPrevChildFrame(mPresContext, mCurrentChild);

  if (prevChild) {
    // Make the previous child the current child
    mCurrentChild = prevChild;
    
    // If we're at the beginning then the collection is at the end
    mIsDone = (nsnull == ::GetPrevChildFrame(mPresContext, mCurrentChild));

    return;
  }

  // No previous frame, we're done.
  mIsDone = PR_TRUE;
}

nsIContent *
nsFrameContentIterator::GetCurrentNode()
{
  if (mCurrentChild && !mIsDone) {
    return mCurrentChild->GetContent();
  }

  return nsnull;
}

PRBool
nsFrameContentIterator::IsDone()
{
  return mIsDone;
}

nsresult
nsFrameContentIterator::PositionAt(nsIContent* aCurNode)
{
  // Starting with the first child frame search for the child frame
  // with the matching content object
  nsIFrame* child = mParentFrame->GetFirstChild(nsnull);
  while (child) {
    if (child->GetContent() == aCurNode) {
      break;
    }
    child = ::GetNextChildFrame(mPresContext, child);
  }

  if (child) {
    // Make it the current child
    mCurrentChild = child;
    mIsDone = PR_FALSE;
  }

  return NS_OK;
}

nsresult
NS_NewFrameContentIterator(nsPresContext*      aPresContext,
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
  NS_ASSERTION(aFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT, "unexpected frame");

  nsFrameContentIterator* it = new nsFrameContentIterator(aPresContext, aFrame);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContentIterator), (void **)aIterator);
}
