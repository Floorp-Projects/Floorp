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
    aChildList->GetNextSibling(aChildList);
    result++;
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////
//

class SimpleContent : public nsIContent {
public:
  SimpleContent();

  NS_IMETHOD GetDocument(nsIDocument*& adoc) const {
    adoc = mDocument;
    return NS_OK;
  }

  void         SetDocument(nsIDocument* aDocument) {mDocument = aDocument;}

  nsIContent* GetParent() const { return nsnull; }
  void SetParent(nsIContent* aParent) {}

  PRBool       CanContainChildren() const {return PR_FALSE;}
  PRInt32      ChildCount() const {return 0;}
  nsIContent*  ChildAt(PRInt32 aIndex) const {return nsnull;}
  PRInt32      IndexOf(nsIContent* aPossibleChild) const {return -1;}
  NS_IMETHOD   InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify) { return NS_OK; }
  NS_IMETHOD   ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                              PRBool aNotify){ return NS_OK; }
  NS_IMETHOD   AppendChild(nsIContent* aKid, PRBool aNotify) {return NS_OK;}
  NS_IMETHOD   RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {return NS_OK;}

  NS_IMETHOD IsSynthetic(PRBool& aResult) {
    aResult = PR_FALSE;
    return NS_OK;
  }

  nsIAtom* GetTag() const {return nsnull;}
  void     SetAttribute(const nsString& aName,const nsString& aValue) {;}
  nsContentAttr GetAttribute(const nsString& aName, nsString& aRet) const {return eContentAttr_NotThere;}

  nsIContentDelegate* GetDelegate(nsIPresContext* aCX) {return nsnull;}

  void List(FILE* out = stdout, PRInt32 aIndent = 0) const {;}
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const { return NS_OK; }

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
  SimpleContainer(nsIContent* aContent);

  void      SetFirstChild(nsIFrame* aChild, PRInt32 aChildCount);
  nsIFrame* GetOverflowList() {return mOverflowList;}
  void      SetOverflowList(nsIFrame* aChildList) {mOverflowList = aChildList;}

  // Allow public access to protected member functions
  void PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling, PRBool aLastIsComplete);
  PRBool DeleteChildsNextInFlow(nsIFrame* aChild);
};

SimpleContainer::SimpleContainer(nsIContent* aContent)
  : nsContainerFrame(aContent, nsnull)
{
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
  SimpleSplittableFrame(nsIContent* aContent, nsIFrame* aParent);
};

SimpleSplittableFrame::SimpleSplittableFrame(nsIContent* aContent,
                                             nsIFrame*   aParent)
  : nsSplittableFrame(aContent, aParent)
{
}

///////////////////////////////////////////////////////////////////////////////
//

// Basic test of the child enumeration member functions
static PRBool
TestChildEnumeration()
{
  // Create a simple test container
  SimpleContent*   content = new SimpleContent;
  SimpleContainer* f = new SimpleContainer(content);

  // Add three child frames
  SimpleContent* childContent = new SimpleContent();
  nsFrame*       c1;
  nsFrame*       c2;
  nsFrame*       c3;

  nsFrame::NewFrame((nsIFrame**)&c1, childContent, f);
  nsFrame::NewFrame((nsIFrame**)&c2, childContent, f);
  nsFrame::NewFrame((nsIFrame**)&c3, childContent, f);

  c1->SetNextSibling(c2);
  c2->SetNextSibling(c3);
  f->SetFirstChild(c1, 3);
  f->SetLastContentOffset(2);

  // Make sure the child count is correct
  PRInt32 childCount;
  f->ChildCount(childCount);
  if (childCount != 3) {
    printf("ChildEnumeration: wrong child count: %d\n", childCount);
    return PR_FALSE;
  }

  // Test indexing of child frames. nsnull should be returned for index
  // values that are out of range
  nsIFrame* child;
  f->ChildAt(-1, child);
  if (nsnull != child) {
    printf("ChildEnumeration: child index failed for index < 0\n");
    return PR_FALSE;
  }
  f->ChildAt(0, child);
  if (c1 != child) {
    printf("ChildEnumeration: wrong child at index: %d\n", 0);
    return PR_FALSE;
  }
  f->ChildAt(1, child);
  if (c2 != child) {
    printf("ChildEnumeration: wrong child at index: %d\n", 1);
    return PR_FALSE;
  }
  f->ChildAt(2, child);
  if (c3 != child) {
    printf("ChildEnumeration: wrong child at index: %d\n", 2);
    return PR_FALSE;
  }
  f->ChildAt(3, child);
  if (nsnull != child) {
    printf("ChildEnumeration: child index failed for index >= child countn");
    return PR_FALSE;
  }

  // Test first and last child member functions
  f->FirstChild(child);
  if (child != c1) {
    printf("ChildEnumeration: wrong first child\n");
    return PR_FALSE;
  }
  f->LastChild(child);
  if (child != c3) {
    printf("ChildEnumeration: wrong last child\n");
    return PR_FALSE;
  }

  // Test IndexOf()
  PRInt32 index;
  f->IndexOf(c1, index);
  if (index != 0) {
    printf("ChildEnumeration: index of c1 failed\n");
    return PR_FALSE;
  }
  f->IndexOf(c2, index);
  if (index != 1) {
    printf("ChildEnumeration: index of c2 failed\n");
    return PR_FALSE;
  }
  f->IndexOf(c3, index);
  if (index != 2) {
    printf("ChildEnumeration: index of c3 failed\n");
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
  SimpleContent*   content = new SimpleContent;
  SimpleContainer* f = new SimpleContainer(content);

  // Add five child frames
  SimpleContent* childContent = new SimpleContent;
  nsFrame*       c1;
  nsFrame*       c2;
  nsFrame*       c3;
  nsFrame*       c4;
  nsFrame*       c5;

  nsFrame::NewFrame((nsIFrame**)&c1, childContent, f);
  nsFrame::NewFrame((nsIFrame**)&c2, childContent, f);
  nsFrame::NewFrame((nsIFrame**)&c3, childContent, f);
  nsFrame::NewFrame((nsIFrame**)&c4, childContent, f);
  nsFrame::NewFrame((nsIFrame**)&c5, childContent, f);

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
  nsIFrame* nextSibling;
  c3->GetNextSibling(nextSibling);
  if (nsnull != nextSibling) {
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
    nsIFrame* parent;

    overflowList->GetGeometricParent(parent);
    if (f != parent) {
      printf("PushChildren: bad geometric parent\n");
      return PR_FALSE;
    }

    overflowList->GetNextSibling(overflowList);
  }

  ///////////////////////////////////////////////////////////////////////////
  // #2

  // Link the overflow list back into the child list
  c3->SetNextSibling(f->GetOverflowList());
  f->SetOverflowList(nsnull);

  // Create a continuing frame
  SimpleContainer* f1 = new SimpleContainer(content);

  // Link it into the flow
  f->SetNextInFlow(f1);
  f1->SetPrevInFlow(f);

  // Push the last two children to the next-in-flow which is empty.
  f->PushChildren(c4, c3, PR_TRUE);

  // Verify there are two children in f1
  PRInt32 childCount;
  f1->ChildCount(childCount);
  if (childCount != 2) {
    printf("PushChildren: continuing frame bad child count: %d\n", childCount);
    return PR_FALSE;
  }

  // Verify the content offsets are correct
  if (f1->GetFirstContentOffset() != 3) {
    printf("PushChildren: continuing frame bad first content offset\n");
    return PR_FALSE;
  }
  if (f1->GetLastContentOffset() != 4) {
    printf("PushChildren: continuing frame bad last content offset\n");
    return PR_FALSE;
  }

  // Verify that the first child is correct
  nsIFrame* firstChild;
  f1->FirstChild(firstChild);
  if (c4 != firstChild) {
    printf("PushChildren: continuing frame first child is wrong\n");
    return PR_FALSE;
  }

  // Verify that the next sibling pointer of c3 has been cleared
  c3->GetNextSibling(nextSibling);
  if (nsnull != nextSibling) {
    printf("PushChildren: sibling pointer isn't null\n");
    return PR_FALSE;
  }

  // Verify that the geometric parent and content parent have been changed
  f1->FirstChild(firstChild);
  while (nsnull != firstChild) {
    nsIFrame* parent;
    firstChild->GetGeometricParent(parent);
    if (f1 != parent) {
      printf("PushChildren: bad geometric parent\n");
      return PR_FALSE;
    }

    firstChild->GetContentParent(parent);
    if (f1 != parent) {
      printf("PushChildren: bad content parent\n");
      return PR_FALSE;
    }

    firstChild->GetNextSibling(firstChild);
  }

  ///////////////////////////////////////////////////////////////////////////
  // #3

  // Test pushing two children to a next-in-flow that already has children
  f->PushChildren(c2, c1, PR_TRUE);

  // Verify there are four children in f1
  f1->ChildCount(childCount);
  if (childCount != 4) {
    printf("PushChildren: continuing frame bad child count: %d\n", childCount);
    return PR_FALSE;
  }

  // Verify the content offset/length are correct
  if (f1->GetFirstContentOffset() != 1) {
    printf("PushChildren: continuing frame bad first content offset\n");
    return PR_FALSE;
  }
  if (f1->GetLastContentOffset() != 4) {
    printf("PushChildren: continuing frame bad last content offset\n");
    return PR_FALSE;
  }

  // Verify that the first child is correct
  f1->FirstChild(firstChild);
  if (c2 != firstChild) {
    printf("PushChildren: continuing frame first child is wrong\n");
    return PR_FALSE;
  }

  // Verify that c3 points to c4
  c3->GetNextSibling(nextSibling);
  if (c4 != nextSibling) {
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
  SimpleContent*   content = new SimpleContent;
  SimpleContainer* f = new SimpleContainer(content);

  // Create two child frames
  SimpleContent* childContent = new SimpleContent;
  nsFrame*       c1 = new SimpleSplittableFrame(childContent, f);
  nsFrame*       c11 = new SimpleSplittableFrame(childContent, f);

  ///////////////////////////////////////////////////////////////////////////
  // #1a

  // Make the child frames siblings and in the same flow
  c1->SetNextSibling(c11);
  c11->AppendToFlow(c1);
  f->SetFirstChild(c1, 2);

  // Delete the next-in-flow
  f->DeleteChildsNextInFlow(c1);

  // Verify the child count
  PRInt32 childCount;
  f->ChildCount(childCount);
  if (childCount != 1) {
    printf("DeleteNextInFlow: bad child count (#1a): %d\n", childCount);
    return PR_FALSE;
  }

  // Verify the sibling pointer is null
  nsIFrame* nextSibling;
  c1->GetNextSibling(nextSibling);
  if (nsnull != nextSibling) {
    printf("DeleteNextInFlow: bad sibling pointer (#1a):\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is null
  nsIFrame* nextInFlow;
  c1->GetNextInFlow(nextInFlow);
  if (nsnull != nextInFlow) {
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
  SimpleContainer* f1 = new SimpleContainer(content);

  // Re-create the continuing child frame
  c11 = new SimpleSplittableFrame(childContent, f1);

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
  nsIFrame* firstChild;

  f1->ChildCount(childCount);
  f1->FirstChild(firstChild);
  if ((childCount != 0) || (firstChild != nsnull)) {
    printf("DeleteNextInFlow: continuing frame not empty (#2a)\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is null
  c1->GetNextInFlow(nextInFlow);
  if (nsnull != nextInFlow) {
    printf("DeleteNextInFlow: bad next-in-flow (#1b)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // #1b

  // Re-create the continuing child frame
  c11 = new SimpleSplittableFrame(childContent, f);

  // Create a third child frame
  nsFrame*  c2 = new SimpleSplittableFrame(childContent, f);

  c1->SetNextSibling(c11);
  c11->AppendToFlow(c1);
  c11->SetNextSibling(c2);
  f->SetFirstChild(c1, 3);
  f->SetLastContentOffset(1);

  // Delete the next-in-flow
  f->DeleteChildsNextInFlow(c1);

  // Verify the child count
  f->ChildCount(childCount);
  if (childCount != 2) {
    printf("DeleteNextInFlow: bad child count (#1b): %d\n", childCount);
    return PR_FALSE;
  }

  // Verify the sibling pointer is correct
  c1->GetNextSibling(nextSibling);
  if (nextSibling != c2) {
    printf("DeleteNextInFlow: bad sibling pointer (#1b):\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is correct
  c1->GetNextInFlow(nextInFlow);
  if (nsnull != nextInFlow) {
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
  c11 = new SimpleSplittableFrame(childContent, f1);
  c2 = new SimpleSplittableFrame(childContent, f1);

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
  c1->GetNextInFlow(nextInFlow);
  if (nsnull != nextInFlow) {
    printf("DeleteNextInFlow: bad next-in-flow (#2b)\n");
    return PR_FALSE;
  }

  // Verify that the second container frame has one child
  f1->ChildCount(childCount);
  if (childCount != 1) {
    printf("DeleteNextInFlow: continuing frame bad child count (#2b): %d\n", childCount);
    return PR_FALSE;
  }

  // Verify that the second container's first child is correct
  f1->FirstChild(firstChild);
  if (firstChild != c2) {
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
  c11 = new SimpleSplittableFrame(childContent, f);
  c2 = new SimpleSplittableFrame(childContent, f);

  // Create a second continuing frame
  SimpleSplittableFrame*  c12 = new SimpleSplittableFrame(childContent, f);

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
  c1->GetNextInFlow(nextInFlow);
  if (nsnull != nextInFlow) {
    printf("DeleteNextInFlow: bad next-in-flow (#3)\n");
    return PR_FALSE;
  }

  // Verify the child count is correct
  f->ChildCount(childCount);
  if (childCount != 2) {
    printf("DeleteNextInFlow: bad child count (#3): %d\n", childCount);
    return PR_FALSE;
  }

  // and the last content offset is still correct
  if (f->GetLastContentOffset() != 1) {
    printf("DeleteNextInFlow: bad last content offset (#3): %d\n", f->GetLastContentOffset());
    return PR_FALSE;
  }

  // Verify the sibling list is correct
  c1->GetNextSibling(nextSibling);
  if (nextSibling != c2) {
    printf("DeleteNextInFlow: bad sibling list (#3)\n");
    return PR_FALSE;
  }
  c2->GetNextSibling(nextSibling);
  if (nextSibling != nsnull) {
    printf("DeleteNextInFlow: bad sibling list (#3)\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow pointer is null
  c1->GetNextInFlow(nextInFlow);
  if (nsnull != nextInFlow) {
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

#if 0
  // Test the push children method
  if (!TestPushChildren()) {
    return -1;
  }

  // Test the push children method
  if (!TestDeleteChildsNext()) {
    return -1;
  }
#endif

  // Test being used as a pseudo frame
  if (!TestAsPseudo()) {
    return -1;
  }

  return 0;
}
