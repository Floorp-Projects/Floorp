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
#include <stdio.h>
#include "nscore.h"
#include "nsCRT.h"
#include "nscoord.h"
#include "nsContainerFrame.h"
#include "nsIContent.h"

static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

///////////////////////////////////////////////////////////////////////////////
// Helper functions

// Compute the number of frames in the list
static PRInt32
LengthOf(nsIFrame* aChildList)
{
  PRInt32 result = 0;

  while (nsnull != aChildList) {
    aChildList = aChildList->GetNextSibling();
    result++;
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////
//

class SimpleContent : public nsIContent {
public:
  SimpleContent();

  nsIDocument* GetDocument() const {return mDocument;}
  void         SetDocument(nsIDocument* aDocument) {mDocument = aDocument;}

  nsIContent* GetParent() const { return nsnull; }
  void SetParent(nsIContent* aParent) {}

  PRBool       CanContainChildren() const {return PR_FALSE;}
  PRInt32      ChildCount() const {return 0;}
  nsIContent*  ChildAt(PRInt32 aIndex) const {return nsnull;}
  PRInt32      IndexOf(nsIContent* aPossibleChild) const {return -1;}
  PRBool       InsertChildAt(nsIContent* aKid, PRInt32 aIndex) {return PR_FALSE;}
  PRBool       ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex) {return PR_FALSE;}
  PRBool       AppendChild(nsIContent* aKid) {return PR_FALSE;}
  PRBool       RemoveChildAt(PRInt32 aIndex) {return PR_FALSE;}

  nsIAtom* GetTag() const {return nsnull;}
  void     SetAttribute(const nsString& aName,const nsString& aValue) {;}
  nsContentAttr GetAttribute(const nsString& aName, nsString& aRet) const {return eContentAttr_NotThere;}

  nsIContentDelegate* GetDelegate(nsIPresContext* aCX) {return nsnull;}

  void List(FILE* out = stdout, PRInt32 aIndent = 0) const {;}
  PRUint32 SizeOf(nsISizeofHandler* aHandler) const { return 0; }

  NS_DECL_ISUPPORTS

protected:
  nsIDocument*  mDocument;
};

SimpleContent::SimpleContent()
{
}

NS_IMPL_ISUPPORTS(SimpleContent, kIContentIID);

///////////////////////////////////////////////////////////////////////////////
//

class SimpleContainer : public nsContainerFrame
{
public:
  SimpleContainer(nsIContent* aContent, PRInt32 aIndexInParent);

  ReflowStatus IncrementalReflow(nsIPresContext*  aPresContext,
                                 nsReflowMetrics& aDesiredSize,
                                 const nsSize&    aMaxSize,
                                 nsReflowCommand& aReflowCommand);

  void      SetFirstChild(nsIFrame* aChild, PRInt32 aChildCount);
  nsIFrame* GetOverflowList() {return mOverflowList;}
  void      SetOverflowList(nsIFrame* aChildList) {mOverflowList = aChildList;}

  // Allow public access to protected member functions
  void PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling, PRBool aLastIsComplete);
  PRBool DeleteChildsNextInFlow(nsIFrame* aChild);
};

SimpleContainer::SimpleContainer(nsIContent* aContent, PRInt32 aIndexInParent)
  : nsContainerFrame(aContent, aIndexInParent, nsnull)
{
}

nsIFrame::ReflowStatus
SimpleContainer::IncrementalReflow(nsIPresContext*  aPresContext,
                                   nsReflowMetrics& aDesiredSize,
                                   const nsSize&    aMaxSize,
                                   nsReflowCommand& aReflowCommand)
{
  NS_NOTYETIMPLEMENTED("incremental reflow");
  return frComplete;
}

// Sets mFirstChild and mChildCount, but not mContentOffset, mContentLength,
// or mLastContentIsComplete
void SimpleContainer::SetFirstChild(nsIFrame* aChild, PRInt32 aChildCount)
{
  mFirstChild = aChild;
  mChildCount = aChildCount;
  NS_ASSERTION(LengthOf(mFirstChild) == aChildCount, "bad child count argument");
}

void SimpleContainer::PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling, PRBool aLastIsComplete)
{
  nsContainerFrame::PushChildren(aFromChild, aPrevSibling, aLastIsComplete);
}

PRBool SimpleContainer::DeleteChildsNextInFlow(nsIFrame* aChild)
{
  return nsContainerFrame::DeleteChildsNextInFlow(aChild);
}

///////////////////////////////////////////////////////////////////////////////
//

class SimpleSplittableFrame : public nsSplittableFrame {
public:
  SimpleSplittableFrame(nsIContent* aContent,
                        PRInt32     aIndexInParent,
                        nsIFrame*   aParent);
};

SimpleSplittableFrame::SimpleSplittableFrame(nsIContent* aContent,
                                             PRInt32     aIndexInParent,
                                             nsIFrame*   aParent)
  : nsSplittableFrame(aContent, aIndexInParent, aParent)
{
}

///////////////////////////////////////////////////////////////////////////////
//

// Basic test of the child enumeration member functions
static PRBool
TestChildEnumeration()
{
  // Create a simple test container
  SimpleContainer* f = new SimpleContainer(new SimpleContent(), 0);

  // Add three child frames
  SimpleContent* childContent = new SimpleContent();
  nsFrame*       c1;
  nsFrame*       c2;
  nsFrame*       c3;

  nsFrame::NewFrame((nsIFrame**)&c1, childContent, 0, f);
  nsFrame::NewFrame((nsIFrame**)&c2, childContent, 1, f);
  nsFrame::NewFrame((nsIFrame**)&c3, childContent, 2, f);

  c1->SetNextSibling(c2);
  c2->SetNextSibling(c3);
  f->SetFirstChild(c1, 3);
  f->SetLastContentOffset(2);

  // Make sure the child count is correct
  if (f->ChildCount() != 3) {
    printf("ChildEnumeration: wrong child count: %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // Test indexing of child frames. nsnull should be returned for index
  // values that are out of range
  if (nsnull != f->ChildAt(-1)) {
    printf("ChildEnumeration: child index failed for index < 0\n");
    return PR_FALSE;
  }
  if (c1 != f->ChildAt(0)) {
    printf("ChildEnumeration: wrong child at index: %d\n", 0);
    return PR_FALSE;
  }
  if (c2 != f->ChildAt(1)) {
    printf("ChildEnumeration: wrong child at index: %d\n", 1);
    return PR_FALSE;
  }
  if (c3 != f->ChildAt(2)) {
    printf("ChildEnumeration: wrong child at index: %d\n", 2);
    return PR_FALSE;
  }
  if (nsnull != f->ChildAt(3)) {
    printf("ChildEnumeration: child index failed for index >= child countn");
    return PR_FALSE;
  }

  // Test first and last child member functions
  if (f->FirstChild() != c1) {
    printf("ChildEnumeration: wrong first child\n");
    return PR_FALSE;
  }
  if (f->LastChild() != c3) {
    printf("ChildEnumeration: wrong last child\n");
    return PR_FALSE;
  }

  // Test IndexOf()
  if ((f->IndexOf(c1) != 0) || (f->IndexOf(c2) != 1) || (f->IndexOf(c3) != 2)) {
    printf("ChildEnumeration: index of failed\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//

// Test the push children method
//
// This tests the following:
// 1. pushing children to the overflow list when there's no next-in-flow
// 2. pushing to a next-in-flow that's empty
// 3. pushing to a next-in-flow that isn't empty
static PRBool
TestPushChildren()
{
  // Create a simple test container
  SimpleContainer* f = new SimpleContainer(new SimpleContent(), 0);

  // Add five child frames
  SimpleContent* childContent = new SimpleContent();
  nsFrame*       c1;
  nsFrame*       c2;
  nsFrame*       c3;
  nsFrame*       c4;
  nsFrame*       c5;

  nsFrame::NewFrame((nsIFrame**)&c1, childContent, 0, f);
  nsFrame::NewFrame((nsIFrame**)&c2, childContent, 1, f);
  nsFrame::NewFrame((nsIFrame**)&c3, childContent, 2, f);
  nsFrame::NewFrame((nsIFrame**)&c4, childContent, 3, f);
  nsFrame::NewFrame((nsIFrame**)&c5, childContent, 4, f);

  c1->SetNextSibling(c2);
  c2->SetNextSibling(c3);
  c3->SetNextSibling(c4);
  c4->SetNextSibling(c5);
  f->SetFirstChild(c1, 5);
  f->SetLastContentOffset(4);

  ///////////////////////////////////////////////////////////////////////////
  // #1

  // Push the last two children. Since there's no next-in-flow the children
  // should be placed on the overflow list
  f->PushChildren(c4, c3, PR_TRUE);

  // Verify that the next sibling pointer of c3 has been cleared
  if (nsnull != c3->GetNextSibling()) {
    printf("PushChildren: sibling pointer isn't null\n");
    return PR_FALSE;
  }

  // Verify that there are two frames on the overflow list
  nsIFrame* overflowList = f->GetOverflowList();
  if ((nsnull == overflowList) || (LengthOf(overflowList) != 2)) {
    printf("PushChildren: bad overflow list\n");
    return PR_FALSE;
  }

  // and that the children on the overflow still have the same geometric parent
  while (nsnull != overflowList) {
    if (f != overflowList->GetGeometricParent()) {
      printf("PushChildren: bad geometric parent\n");
      return PR_FALSE;
    }

    overflowList = overflowList->GetNextSibling();
  }

  ///////////////////////////////////////////////////////////////////////////
  // #2

  // Link the overflow list back into the child list
  c3->SetNextSibling(f->GetOverflowList());
  f->SetOverflowList(nsnull);

  // Create a continuing frame
  SimpleContainer* f1 = new SimpleContainer(f->GetContent(), 0);

  // Link it into the flow
  f->SetNextInFlow(f1);
  f1->SetPrevInFlow(f);

  // Push the last two children to the next-in-flow which is empty.
  f->PushChildren(c4, c3, PR_TRUE);

  // Verify there are two children in f1
  if (f1->ChildCount() != 2) {
    printf("PushChildren: continuing frame bad child count: %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // Verify the content offsets are correct
  if ((f1->GetFirstContentOffset() != 3) || (f1->GetLastContentOffset() != 4)) {
    printf("PushChildren: continuing frame bad mapping\n");
    return PR_FALSE;
  }

  // Verify that the first child is correct
  if (c4 != f1->FirstChild()) {
    printf("PushChildren: continuing frame first child is wrong\n");
    return PR_FALSE;
  }

  // Verify that the next sibling pointer of c3 has been cleared
  if (nsnull != c3->GetNextSibling()) {
    printf("PushChildren: sibling pointer isn't null\n");
    return PR_FALSE;
  }

  // Verify that the geometric parent and content parent have been changed
  nsIFrame* firstChild = f1->FirstChild();
  while (nsnull != firstChild) {
    if (f1 != firstChild->GetGeometricParent()) {
      printf("PushChildren: bad geometric parent\n");
      return PR_FALSE;
    }

    if (f1 != firstChild->GetContentParent()) {
      printf("PushChildren: bad content parent\n");
      return PR_FALSE;
    }

    firstChild = firstChild->GetNextSibling();
  }

  ///////////////////////////////////////////////////////////////////////////
  // #3

  // Test pushing two children to a next-in-flow that already has children
  f->PushChildren(c2, c1, PR_TRUE);

  // Verify there are four children in f1
  if (f1->ChildCount() != 4) {
    printf("PushChildren: continuing frame bad child count: %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // Verify the content offset/length are correct
  if ((f1->GetFirstContentOffset() != 1) || (f1->GetLastContentOffset() != 4)) {
    printf("PushChildren: continuing frame bad mapping\n");
    return PR_FALSE;
  }

  // Verify that the first child is correct
  if (c2 != f1->FirstChild()) {
    printf("PushChildren: continuing frame first child is wrong\n");
    return PR_FALSE;
  }

  // Verify that c3 points to c4
  if (c4 != c3->GetNextSibling()) {
    printf("PushChildren: bad sibling pointers\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//

// Test the delete child's next-in-flow method
//
// This tests the following:
// 1. one next-in-flow in same geometric parent
//   a) next-in-flow is parent's last child
//   b) next-in-flow is not the parent's last child
// 2. one next-in-flow in different geometric parents
//   a) next-in-flow is parent's first and last child
//   b) next-in-flow is not the parent's last child
// 3. multiple next-in-flow frames
static PRBool
TestDeleteChildsNext()
{
  // Create two simple test containers
  SimpleContainer* f = new SimpleContainer(new SimpleContent(), 0);

  // Create two child frames
  SimpleContent* childContent = new SimpleContent();
  nsFrame*       c1 = new SimpleSplittableFrame(childContent, 0, f);
  nsFrame*       c11 = new SimpleSplittableFrame(childContent, 0, f);

  ///////////////////////////////////////////////////////////////////////////
  // #1a

  // Make the child frames siblings and in the same flow
  c1->SetNextSibling(c11);
  c11->AppendToFlow(c1);
  f->SetFirstChild(c1, 2);

  // Delete the next-in-flow
  f->DeleteChildsNextInFlow(c1);

  // Verify the child count
  if (f->ChildCount() != 1) {
    printf("DeleteNextInFlow: bad child count (#1a): %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // Verify the sibling pointer is null
  if (nsnull != c1->GetNextSibling()) {
    printf("DeleteNextInFlow: bad sibling pointer (#1a):\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is null
  if (nsnull != c1->GetNextInFlow()) {
    printf("DeleteNextInFlow: bad next-in-flow (#1a)\n");
    return PR_FALSE;
  }

  // Verify the first/last content offsets are still 0
  if ((f->GetFirstContentOffset() != 0) || (f->GetLastContentOffset() != 0)) {
    printf("DeleteNextInFlow: bad content mapping (#1a)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // #2a

  // Create a second container frame
  SimpleContainer* f1 = new SimpleContainer(new SimpleContent(), 0);

  // Re-create the continuing child frame
  c11 = new SimpleSplittableFrame(childContent, 0, f1);

  // Put the first child in the first container and the continuing child frame in
  // the second container
  c11->AppendToFlow(c1);
  f->SetFirstChild(c1, 1);
  f->SetLastContentOffset(0);
  f1->SetFirstChild(c11, 1);

  // Link the containers together
  f1->AppendToFlow(f);

  // Delete the next-in-flow
  f->DeleteChildsNextInFlow(c1);

  // Verify that the second container frame is empty
  if ((f1->ChildCount() != 0) || (f1->FirstChild() != nsnull)) {
    printf("DeleteNextInFlow: continuing frame not empty (#2a)\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is null
  if (nsnull != c1->GetNextInFlow()) {
    printf("DeleteNextInFlow: bad next-in-flow (#1b)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // #1b

  // Re-create the continuing child frame
  c11 = new SimpleSplittableFrame(childContent, 0, f);

  // Create a third child frame
  nsFrame*  c2 = new SimpleSplittableFrame(childContent, 1, f);

  c1->SetNextSibling(c11);
  c11->AppendToFlow(c1);
  c11->SetNextSibling(c2);
  f->SetFirstChild(c1, 3);
  f->SetLastContentOffset(1);

  // Delete the next-in-flow
  f->DeleteChildsNextInFlow(c1);

  // Verify the child count
  if (f->ChildCount() != 2) {
    printf("DeleteNextInFlow: bad child count (#1b): %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // Verify the sibling pointer is correct
  if (c1->GetNextSibling() != c2) {
    printf("DeleteNextInFlow: bad sibling pointer (#1b):\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is correct
  if (nsnull != c1->GetNextInFlow()) {
    printf("DeleteNextInFlow: bad next-in-flow (#1b)\n");
    return PR_FALSE;
  }

  // Verify the first/last content offsets are correct
  if ((f->GetFirstContentOffset() != 0) || (f->GetLastContentOffset() != 1)) {
    printf("DeleteNextInFlow: bad content mapping (#1a)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // #2b

  // Re-create the continuing frame and the third child frame
  c11 = new SimpleSplittableFrame(childContent, 0, f1);
  c2 = new SimpleSplittableFrame(childContent, 1, f1);

  // Put the first child in the first container, and the continuing child frame
  // and the third frame in the second container
  c1->SetNextSibling(nsnull);
  c11->AppendToFlow(c1);
  f->SetFirstChild(c1, 1);
  f->SetLastContentOffset(0);

  c11->SetNextSibling(c2);
  f1->SetFirstChild(c11, 2);
  f1->SetFirstContentOffset(0);
  f1->SetLastContentOffset(1);

  // Link the containers together
  f1->AppendToFlow(f);

  // Delete the next-in-flow
  f->DeleteChildsNextInFlow(c1);

  // Verify the next-in-flow pointer is null
  if (nsnull != c1->GetNextInFlow()) {
    printf("DeleteNextInFlow: bad next-in-flow (#2b)\n");
    return PR_FALSE;
  }

  // Verify that the second container frame has one child
  if (f1->ChildCount() != 1) {
    printf("DeleteNextInFlow: continuing frame bad child count (#2b): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // Verify that the second container's first child is correct
  if (f1->FirstChild() != c2) {
    printf("DeleteNextInFlow: continuing frame bad first child (#2b)\n");
    return PR_FALSE;
  }

  // Verify the second container's first content offset
  if (f1->GetFirstContentOffset() != 1) {
    printf("DeleteNextInFlow: continuing frame bad first content offset (#2b): %d\n",
           f1->GetFirstContentOffset());
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // #3

  // Re-create the continuing frame and the third child frame
  c11 = new SimpleSplittableFrame(childContent, 0, f);
  c2 = new SimpleSplittableFrame(childContent, 1, f);

  // Create a second continuing frame
  SimpleSplittableFrame*  c12 = new SimpleSplittableFrame(childContent, 0, f);

  // Put all the child frames in the first container
  c1->SetNextSibling(c11);
  c11->SetNextSibling(c12);
  c12->SetNextSibling(c2);
  c11->AppendToFlow(c1);
  c12->AppendToFlow(c11);
  f->SetFirstChild(c1, 4);
  f->SetLastContentOffset(1);

  // Delete the next-in-flow
  f->DeleteChildsNextInFlow(c1);

  // Verify the next-in-flow pointer is null
  if (nsnull != c1->GetNextInFlow()) {
    printf("DeleteNextInFlow: bad next-in-flow (#3)\n");
    return PR_FALSE;
  }

  // Verify the child count is correct
  if (f->ChildCount() != 2) {
    printf("DeleteNextInFlow: bad child count (#3): %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // and the last content offset is still correct
  if (f->GetLastContentOffset() != 1) {
    printf("DeleteNextInFlow: bad last content offset (#3): %d\n", f->GetLastContentOffset());
    return PR_FALSE;
  }

  // Verify the sibling list is correct
  if ((c1->GetNextSibling() != c2) || (c2->GetNextSibling()!= nsnull)) {
    printf("DeleteNextInFlow: bad sibling list (#3)\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is null
  if (nsnull != c1->GetNextInFlow()) {
    printf("DeleteNextInFlow: bad next-in-flow (#3)\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//

// Test the container frame being used as a pseudo frame
static PRBool
TestAsPseudo()
{
  return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//

int main(int argc, char** argv)
{
  // Test the child frame enumeration member functions
  if (!TestChildEnumeration()) {
    return -1;
  }

  // Test the push children method
  if (!TestPushChildren()) {
    return -1;
  }

  // Test the push children method
  if (!TestDeleteChildsNext()) {
    return -1;
  }

  // Test being used as a pseudo frame
  if (!TestAsPseudo()) {
    return -1;
  }

  return 0;
}
