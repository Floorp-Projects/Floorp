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
#include "nsIAtom.h"
#include "nsStringBuf.h"
#include "nsDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContentDelegate.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsIFrame.h"
#include "nsIStyleSet.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "..\..\..\src\nsTablePart.h"


static FILE * out;


class BasicTest : public nsDocument // yep, I'm a document
{
public:
  BasicTest();
  virtual ~BasicTest() {};

  nsresult AppendSimpleSpan(nsIContent* aContainer, const char* aTag, const char* aText);

  void CreateCorrectContent(PRInt32 aRows, PRInt32 aCols);
  void CreateCorrectFullContent(PRInt32 aRows, PRInt32 aCols);
  void CreateOutOfOrderContent(PRInt32 aRows, PRInt32 aCols);
  void CreateIllegalContent(PRInt32 aRows, PRInt32 aCols);

  void VerifyContent(PRInt32 aRows, PRInt32 aCols);

  void TestGeometry();
};

class GeometryTest
{
public:
  GeometryTest(BasicTest *aDoc);

  virtual ~GeometryTest() {};

  void CreateGeometry(BasicTest *aDoc, nsIPresContext *aPC);

  void VerifyGeometry(BasicTest *aDoc, nsIPresContext *aPC);

  nsIPresShell * mShell;
  nsIViewManager * mViewManager;
  nsIFrame * mRootFrame;
};

/* ---------- BasicTest Definitions ---------- */

BasicTest::BasicTest()
  : nsDocument()
{
  Init();

  PRInt32 rows = 3;
  PRInt32 cols = 4;

  // Build a basic table content model the "right" way
  CreateCorrectContent(rows, cols);
  VerifyContent(rows, cols);
  nsIContent *root;
  root = GetRootContent();
  if (nsnull!=root && 1==root->ChildCount())
    root->RemoveChildAt(0);
  else
  {
    fprintf(out, "corrupt root after CreateCorrectContent\n");
    NS_ASSERTION(PR_FALSE, "");
  }

  // Build a content model the "right" way with every kind of table object
  CreateCorrectFullContent(rows, cols);
  VerifyContent(rows, cols);
  root = GetRootContent();
  if (nsnull!=root && 1==root->ChildCount())
    root->RemoveChildAt(0);
  else
  {
    fprintf(out, "corrupt root after CreateCorrectFullContent\n");
    NS_ASSERTION(PR_FALSE, "");
  }

  // Build a content model with things out of order
  CreateOutOfOrderContent(rows, cols);
  VerifyContent(rows, cols);
  root = GetRootContent();
  if (nsnull!=root && 1==root->ChildCount())
    root->RemoveChildAt(0);
  else
  {
    fprintf(out, "corrupt root after CreateOutOfOrderContent\n");
    NS_ASSERTION(PR_FALSE, "");
  }

  // Build a content model with illegal things
  /* not ready for prime time...
  CreateIllegalContent(rows, cols);
  VerifyContent(rows, cols);
  root = GetRootContent();
  if (nsnull!=root && 1==root->ChildCount())
    root->RemoveChildAt(0);
  else
  {
    fprintf(out, "corrupt root after CreateIllegalContent\n");
    NS_ASSERTION(PR_FALSE);
  }
  */

  TestGeometry();
}

nsresult BasicTest::AppendSimpleSpan(nsIContent* aContainer, const char* aTag, const char* aText)
{
  nsIHTMLContent* span;
  nsIHTMLContent* text;
  nsIAtom* atom = NS_NewAtom(aTag);
  nsresult rv = NS_NewHTMLContainer(&span, atom);
  if (NS_OK == rv) {
    nsStringBuf tmp;
    tmp.Append(aText);
    rv = NS_NewHTMLText(&text, tmp.GetUnicode(), tmp.Length());
    if (NS_OK == rv) {
      span->AppendChild(text);
      NS_RELEASE(text);
    }
    aContainer->AppendChild(span);
    NS_RELEASE(span);
  }
  NS_RELEASE(atom);
  return rv;
}

void BasicTest::CreateCorrectContent(int aRows, int aCols)
{
  fprintf(out, "CreateCorrectContent %d %d\n", aRows, aCols);
  nsIHTMLContent* root;
  nsresult rv = NS_NewRootPart(&root, this);  // does a SetRootPart on the returned root object
  if (NS_OK != rv) {
    fprintf(out, "NS_NewRootPart failed\n");
    NS_ASSERTION(PR_FALSE, "NS_NewRootPart failed");
  }

  nsIHTMLContent* body;
  nsIAtom* atom = NS_NewAtom("BODY");
  rv = NS_NewBodyPart(&body, atom);
  if (NS_OK != rv) {
    fprintf(out, "NS_NewBodyPart failed\n");
    NS_ASSERTION(PR_FALSE, "NS_NewBodyPart failed");
  }
  NS_RELEASE(atom);

  nsIHTMLContent* table;
  nsIHTMLContent* row;
  nsIHTMLContent* cell;

  nsIAtom* tatom = NS_NewAtom("TABLE");
  rv = NS_NewTablePart(&table, tatom);
  NS_RELEASE(tatom);
  if (NS_OK == rv) {
    PRInt32 rowIndex;
    for (rowIndex = 0; (NS_OK == rv) && (rowIndex < aRows); rowIndex++) {
      nsIAtom* tratom = NS_NewAtom("TR");
      rv = NS_NewTableRowPart(&row, tratom);
      NS_RELEASE(tratom);
      if (NS_OK == rv) {
        PRInt32 colIndex;
        for (colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
          nsIAtom* tdatom = NS_NewAtom("TD");
          rv = NS_NewTableCellPart(&cell, tdatom);
          NS_RELEASE(tdatom);
          if (NS_OK == rv) {
            rv = AppendSimpleSpan (cell, "P", "test");   
          }
          row->AppendChild(cell);
          NS_RELEASE(cell);
        }
        table->AppendChild(row);
        NS_RELEASE(row);
      }
    }
    ((nsTablePart *)table)->GetMaxColumns();  // has important side effect of creating pseudo-columns
    body->AppendChild(table);
    NS_RELEASE(table);
  }
  root->AppendChild(body);
  NS_RELEASE(body);
}

void BasicTest::CreateCorrectFullContent(int aRows, int aCols)
{
  fprintf(out, "CreateCorrectFullContent %d %d, 1 caption\n", aRows, aCols);
  nsIHTMLContent* root;
  nsresult rv = NS_NewRootPart(&root, this);  // does a SetRootPart on the returned root object
  if (NS_OK != rv) {
    fprintf(out, "NS_NewRootPart failed\n");
    NS_ASSERTION(PR_FALSE, "NS_NewRootPart failed\n");
  }

  nsIHTMLContent* body;
  nsIAtom* atom = NS_NewAtom("BODY");
  rv = NS_NewBodyPart(&body, atom);
  if (NS_OK != rv) {
    fprintf(out, "NS_NewBodyPart failed\n");
    NS_ASSERTION(PR_FALSE, "NS_NewBodyPart failed\n");
  }
  NS_RELEASE(atom);

  nsIHTMLContent* table;
  nsIHTMLContent* caption;
  nsIHTMLContent* colGroup;
  nsIHTMLContent* col;
  nsIHTMLContent* row;
  nsIHTMLContent* cell;

  nsIAtom* tatom = NS_NewAtom("TABLE");
  rv = NS_NewTablePart(&table, tatom);
  NS_RELEASE(tatom);
  
  // add caption
  nsIAtom* captionAtom = NS_NewAtom("CAPTION");
  rv = NS_NewTableCaptionPart(&caption, captionAtom);
  NS_RELEASE(captionAtom);
  table->AppendChild(caption);
  
  // add column group
  PRInt32 colIndex;
  nsIAtom* colGroupAtom = NS_NewAtom("COLGROUP");
  rv = NS_NewTableColGroupPart(&colGroup, colGroupAtom);
  NS_RELEASE(colGroupAtom);
  table->AppendChild(colGroup);

  // add columns
  nsIAtom* colAtom = NS_NewAtom("COL");
  for (colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
    rv = NS_NewTableColPart(&col, colAtom);
    colGroup->AppendChild(col);
  }
  NS_RELEASE(colAtom);

  // add rows and cells
  if (NS_OK == rv) {
    PRInt32 rowIndex;
    for (rowIndex = 0; (NS_OK == rv) && (rowIndex < aRows); rowIndex++) {
      nsIAtom* tratom = NS_NewAtom("TR");
      rv = NS_NewTableRowPart(&row, tratom);
      NS_RELEASE(tratom);
      if (NS_OK == rv) {
        for (colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
          nsIAtom* tdatom = NS_NewAtom("TD");
          rv = NS_NewTableCellPart(&cell, tdatom);
          NS_RELEASE(tdatom);
          if (NS_OK == rv) {
            rv = AppendSimpleSpan (cell, "P", "test");   
          }
          row->AppendChild(cell);
          NS_RELEASE(cell);
        }
        table->AppendChild(row);
        NS_RELEASE(row);
      }
    }
    body->AppendChild(table);
    NS_RELEASE(table);
  }
  root->AppendChild(body);
  NS_RELEASE(body);
}


void BasicTest::CreateOutOfOrderContent(int aRows, int aCols)
{
  fprintf(out, "CreateOutOfOrderContent %d %d, 2 captions\n", aRows, aCols);
  nsIHTMLContent* root;
  nsresult rv = NS_NewRootPart(&root, this);  // does a SetRootPart on the returned root object
  if (NS_OK != rv) {
    fprintf(out, "NS_NewRootPart failed\n");
    NS_ASSERTION(PR_FALSE, "NS_NewRootPart failed\n");
  }

  nsIHTMLContent* body;
  nsIAtom* atom = NS_NewAtom("BODY");
  rv = NS_NewBodyPart(&body, atom);
  if (NS_OK != rv) {
    fprintf(out, "NS_NewBodyPart failed\n");
    NS_ASSERTION(PR_FALSE, "NS_NewBodyPart failed\n");
  }
  NS_RELEASE(atom);

  nsIHTMLContent* table;
  nsIHTMLContent* caption;
  nsIHTMLContent* colGroup;
  nsIHTMLContent* col;
  nsIHTMLContent* row;
  nsIHTMLContent* cell;

  nsIAtom* tatom = NS_NewAtom("TABLE");
  rv = NS_NewTablePart(&table, tatom);
  NS_RELEASE(tatom);
  
  // add rows and cells
  if (NS_OK == rv) {
    PRInt32 rowIndex;
    for (rowIndex = 0; (NS_OK == rv) && (rowIndex < aRows); rowIndex++) {
      nsIAtom* tratom = NS_NewAtom("TR");
      rv = NS_NewTableRowPart(&row, tratom);
      NS_RELEASE(tratom);
      if (NS_OK == rv) {
        for (PRInt32 colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
          nsIAtom* tdatom = NS_NewAtom("TD");
          rv = NS_NewTableCellPart(&cell, tdatom);
          NS_RELEASE(tdatom);
          if (NS_OK == rv) {
            rv = AppendSimpleSpan (cell, "P", "test");   
          }
          row->AppendChild(cell);
          NS_RELEASE(cell);
          if (1==rowIndex && 0==colIndex)
          {
            // add column after cell[1,0], force implicit column group to be built
            PRInt32 colIndex;
            nsIAtom* colGroupAtom = NS_NewAtom("COLGROUP");
            rv = NS_NewTableColGroupPart(&colGroup, colGroupAtom);
            NS_RELEASE(colGroupAtom);
            table->AppendChild(colGroup);
          }
        }
        table->AppendChild(row);
        NS_RELEASE(row);
      }
    }
    ((nsTablePart *)table)->GetMaxColumns();  // has important side effect of creating pseudo-columns
    body->AppendChild(table);
  }
  root->AppendChild(body);

  // add caption in middle
  nsIAtom* captionAtom = NS_NewAtom("CAPTION");
  rv = NS_NewTableCaptionPart(&caption, captionAtom);
  NS_RELEASE(captionAtom);
  table->AppendChild(caption);

  // add columns
  nsIAtom* colAtom = NS_NewAtom("COL");
  for (PRInt32 colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
    rv = NS_NewTableColPart(&col, colAtom);
    colGroup->AppendChild(col);
  }
  NS_RELEASE(colAtom);

  // add caption at end
  captionAtom = NS_NewAtom("CAPTION");
  rv = NS_NewTableCaptionPart(&caption, captionAtom);
  NS_RELEASE(captionAtom);
  table->AppendChild(caption);

  NS_RELEASE(table);
  NS_RELEASE(body);
}

void BasicTest::CreateIllegalContent(int aRows, int aCols)
{
}

void BasicTest::VerifyContent(PRInt32 aRows, PRInt32 aCols)
{
  nsIContent* root = GetRootContent();
  if (nsnull==root)
  {
    fprintf(out, "GetRootContent failed\n");
    NS_ASSERTION(PR_FALSE, "GetRootContent failed\n");
  }
  fprintf(out, "VerifyContent for rows=%d, cols=%d\n", aRows, aCols);
  root->List(out);
}

void BasicTest::TestGeometry()
{
  GeometryTest test(this);
}

/* ---------- GeometryTest Definitions ---------- */

GeometryTest::GeometryTest(BasicTest *aDoc)
{
  PRInt32 rows = 4;
  PRInt32 cols = 3;

#if 0
  NS_InitToolkit(PR_GetCurrentThread());

  //NS_NewDeviceContext(&scribbleData.mContext);
  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
  NS_NewWindow(NULL, kIWidgetIID, nsnull);

#endif

  nsIPresContext * pc = nsnull;
  nsresult status = NS_NewGalleyContext(&pc);
  if ((NS_FAILED(status)) ||  nsnull==pc)
  {
    fprintf(out, "bad galley pc");
    NS_ASSERTION(PR_FALSE, "bad galley pc");
  }

  // create a view manager
  nsIViewManager * vm = nsnull;

  static NS_DEFINE_IID(kViewManagerCID, NS_VIEW_MANAGER_CID);
  static NS_DEFINE_IID(kIViewManagerIID, NS_IVIEWMANAGER_IID);

  status = NSRepository::CreateInstance(kViewManagerCID, 
                                        nsnull, 
                                        kIViewManagerIID, 
                                        (void **)&vm);

  if ((NS_FAILED(status)) ||  nsnull==vm)
  {
    fprintf(out, "bad view manager");
    NS_ASSERTION(PR_FALSE, "bad view manager");
  }
  vm->Init(pc);

  nsIView * rootView = nsnull;

  // Create a view
  static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
  static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

  status = NSRepository::CreateInstance(kScrollingViewCID, 
                                        nsnull, 
                                        kIViewIID, 
                                        (void **)&rootView);

  if ((NS_FAILED(status)) ||  nsnull==rootView)
  {
    fprintf(out, "bad view");
    NS_ASSERTION(PR_FALSE, "bad view");
  }
  nsRect bounds(0, 0, 10000, 10000);
  rootView->Init(vm, bounds, nsnull);

  vm->SetRootView(rootView);

  nsIStyleSet * ss = nsnull;
  status = NS_NewStyleSet(&ss);
  if ((NS_FAILED(status)) ||  nsnull==ss)
  {
    fprintf(out, "bad style set");
    NS_ASSERTION(PR_FALSE, "bad style set");
  }

  mShell = nsnull;
  status = NS_NewPresShell(&mShell);
  if ((NS_FAILED(status)) ||  nsnull==mShell)
  {
    fprintf(out, "bad presentation shell.");
    NS_ASSERTION(PR_FALSE, "");
  }
  mShell->Init(aDoc, pc, vm, ss);

  aDoc->CreateCorrectContent(rows, cols);
  CreateGeometry(aDoc, pc);
  VerifyGeometry(aDoc, pc);

  aDoc->CreateCorrectFullContent(rows, cols);
  CreateGeometry(aDoc, pc);
  VerifyGeometry(aDoc, pc);

  /* paginated tests not ready yet...
  NS_RELEASE(pc);
  pc = nsnull;
  status = NS_NewPrintPreviewContext(&pc);
  if ((NS_FAILED(status)) ||  nsnull==pc)
  {
    fprintf(out, "bad paginated pc");
    NS_ASSERTION(PR_FALSE, "");
  }
  aDoc->CreateCorrectContent(rows, cols);
  CreateGeometry(aDoc, pc);
  VerifyGeometry(aDoc, pc);

  aDoc->CreateCorrectFullContent(rows, cols);
  CreateGeometry(aDoc, pc);
  VerifyGeometry(aDoc, pc);
  */
}

/** given a content model, create a geometry model */
void GeometryTest::CreateGeometry(BasicTest * aDoc, nsIPresContext *aPC)
{
  nsIContent *root = aDoc->GetRootContent();
  nsIContentDelegate* cd = root->GetDelegate(aPC);
  if (nsnull != cd) {
    mRootFrame = cd->CreateFrame(aPC, root, -1, nsnull);
    NS_RELEASE(cd);
    if (nsnull==mRootFrame)
    {
      fprintf(out, "mRootFrame failed\n");
      NS_ASSERTION(PR_FALSE, "mRootFrame failed\n");
    }

    // Bind root frame to root view (and root window)
    mViewManager = mShell->GetViewManager();
    if (nsnull==mViewManager)
    {
      fprintf(out, "bad view manager");
      NS_ASSERTION(PR_FALSE, "");
    }
    nsIView* rootView = mViewManager->GetRootView();
    NS_ASSERTION(nsnull!=rootView, "bad root view");
    mRootFrame->SetView(rootView);
    NS_RELEASE(rootView);
  }
  else {
    fprintf(out, "ERROR: no root delegate\n");
    NS_ASSERTION(PR_FALSE, "no root delegate");
  }
  NS_RELEASE(root);

  nsReflowMetrics desiredSize;
  nsSize          maxSize(400, 600);
  nsSize          maxElementSize;


  mRootFrame->ResizeReflow(aPC, desiredSize, maxSize, &maxElementSize);

  
}

void GeometryTest::VerifyGeometry(BasicTest *aDoc, nsIPresContext *aPC)
{
  mRootFrame->List(out);
}


/* ---------- Global Functions ---------- */

void main (int argc, char **argv)
{
  out = fopen("TableContentTest.txt", "w+t");
  if (nsnull==out)
  {
    fprintf(out, "test failed to open output file\n");
    NS_ASSERTION(PR_FALSE, "test failed to open output file\n");
  }
  fprintf(out, "Test starting...\n\n");
  BasicTest basicTest;
  fprintf(out, "\nTest completed.\n");
}






