/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include <stdio.h>
#include "nscore.h"
#include "nsIAtom.h"
#include "nsStringBuf.h"
#include "nsDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContentDelegate.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsIFrame.h"
#include "nsIStyleSet.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "..\..\..\src\nsTablePart.h"

#include "nsContentCID.h"
static NS_DEFINE_CID(kStyleSetCID, NS_STYLESET_CID);

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

  void CreateGeometry(BasicTest *aDoc, nsPresContext *aPC);

  void VerifyGeometry(BasicTest *aDoc, nsPresContext *aPC);

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
  GetRootContent(&root);
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
  GetRootContent(&root);
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
  GetRootContent(&root);
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
  GetRootContent(&root);
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
    rv = NS_NewHTMLText(&text, tmp.get(), tmp.Length());
    if (NS_OK == rv) {
      span->AppendChildTo(text);
      NS_RELEASE(text);
    }
    aContainer->AppendChildTo(span);
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
  nsIAtom* atom = NS_NewAtom("body");
  rv = NS_NewBodyPart(&body, atom);
  if (NS_OK != rv) {
    fprintf(out, "NS_NewBodyPart failed\n");
    NS_ASSERTION(PR_FALSE, "NS_NewBodyPart failed");
  }
  NS_RELEASE(atom);

  nsIHTMLContent* table;
  nsIHTMLContent* row;
  nsIHTMLContent* cell;

  nsIAtom* tatom = NS_NewAtom("table");
  rv = NS_NewTablePart(&table, tatom);
  NS_RELEASE(tatom);
  if (NS_OK == rv) {
    PRInt32 rowIndex;
    for (rowIndex = 0; (NS_OK == rv) && (rowIndex < aRows); rowIndex++) {
      nsIAtom* tratom = NS_NewAtom("tr");
      rv = NS_NewTableRowPart(&row, tratom);
      NS_RELEASE(tratom);
      if (NS_OK == rv) {
        PRInt32 colIndex;
        for (colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
          nsIAtom* tdatom = NS_NewAtom("td");
          rv = NS_NewTableCellPart(&cell, tdatom);
          NS_RELEASE(tdatom);
          if (NS_OK == rv) {
            rv = AppendSimpleSpan (cell, "p", "test");   
          }
          row->AppendChildTo(cell);
          NS_RELEASE(cell);
        }
        table->AppendChildTo(row);
        NS_RELEASE(row);
      }
    }
    ((nsTablePart *)table)->GetMaxColumns();  // has important side effect of creating pseudo-columns
    body->AppendChildTo(table);
    NS_RELEASE(table);
  }
  root->AppendChildTo(body);
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
  nsIAtom* atom = NS_NewAtom("body");
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

  nsIAtom* tatom = NS_NewAtom("table");
  rv = NS_NewTablePart(&table, tatom);
  NS_RELEASE(tatom);
  
  // add caption
  nsIAtom* captionAtom = NS_NewAtom("caption");
  rv = NS_NewTableCaptionPart(&caption, captionAtom);
  NS_RELEASE(captionAtom);
  table->AppendChildTo(caption);
  
  // add column group
  PRInt32 colIndex;
  nsIAtom* colGroupAtom = NS_NewAtom("colgroup");
  rv = NS_NewTableColGroupPart(&colGroup, colGroupAtom);
  NS_RELEASE(colGroupAtom);
  table->AppendChildTo(colGroup);

  // add columns
  nsIAtom* colAtom = NS_NewAtom("col");
  for (colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
    rv = NS_NewTableColPart(&col, colAtom);
    colGroup->AppendChildTo(col);
  }
  NS_RELEASE(colAtom);

  // add rows and cells
  if (NS_OK == rv) {
    PRInt32 rowIndex;
    for (rowIndex = 0; (NS_OK == rv) && (rowIndex < aRows); rowIndex++) {
      nsIAtom* tratom = NS_NewAtom("tr");
      rv = NS_NewTableRowPart(&row, tratom);
      NS_RELEASE(tratom);
      if (NS_OK == rv) {
        for (colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
          nsIAtom* tdatom = NS_NewAtom("td");
          rv = NS_NewTableCellPart(&cell, tdatom);
          NS_RELEASE(tdatom);
          if (NS_OK == rv) {
            rv = AppendSimpleSpan (cell, "P", "test");   
          }
          row->AppendChildTo(cell);
          NS_RELEASE(cell);
        }
        table->AppendChildTo(row);
        NS_RELEASE(row);
      }
    }
    body->AppendChildTo(table);
    NS_RELEASE(table);
  }
  root->AppendChildTo(body);
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
  nsIAtom* atom = NS_NewAtom("body");
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

  nsIAtom* tatom = NS_NewAtom("table");
  rv = NS_NewTablePart(&table, tatom);
  NS_RELEASE(tatom);
  
  // add rows and cells
  if (NS_OK == rv) {
    PRInt32 rowIndex;
    for (rowIndex = 0; (NS_OK == rv) && (rowIndex < aRows); rowIndex++) {
      nsIAtom* tratom = NS_NewAtom("tr");
      rv = NS_NewTableRowPart(&row, tratom);
      NS_RELEASE(tratom);
      if (NS_OK == rv) {
        for (PRInt32 colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
          nsIAtom* tdatom = NS_NewAtom("td");
          rv = NS_NewTableCellPart(&cell, tdatom);
          NS_RELEASE(tdatom);
          if (NS_OK == rv) {
            rv = AppendSimpleSpan (cell, "p", "test");   
          }
          row->AppendChildTo(cell);
          NS_RELEASE(cell);
          if (1==rowIndex && 0==colIndex)
          {
            // add column after cell[1,0], force implicit column group to be built
            PRInt32 colIndex;
            nsIAtom* colGroupAtom = NS_NewAtom("colgroup");
            rv = NS_NewTableColGroupPart(&colGroup, colGroupAtom);
            NS_RELEASE(colGroupAtom);
            table->AppendChildTo(colGroup);
          }
        }
        table->AppendChildTo(row);
        NS_RELEASE(row);
      }
    }
    ((nsTablePart *)table)->GetMaxColumns();  // has important side effect of creating pseudo-columns
    body->AppendChildTo(table);
  }
  root->AppendChildTo(body);

  // add caption in middle
  nsIAtom* captionAtom = NS_NewAtom("caption");
  rv = NS_NewTableCaptionPart(&caption, captionAtom);
  NS_RELEASE(captionAtom);
  table->AppendChildTo(caption);

  // add columns
  nsIAtom* colAtom = NS_NewAtom("col");
  for (PRInt32 colIndex = 0; (NS_OK == rv) && (colIndex < aCols); colIndex++) {
    rv = NS_NewTableColPart(&col, colAtom);
    colGroup->AppendChildTo(col);
  }
  NS_RELEASE(colAtom);

  // add caption at end
  captionAtom = NS_NewAtom("caption");
  rv = NS_NewTableCaptionPart(&caption, captionAtom);
  NS_RELEASE(captionAtom);
  table->AppendChildTo(caption);

  NS_RELEASE(table);
  NS_RELEASE(body);
}

void BasicTest::CreateIllegalContent(int aRows, int aCols)
{
}

void BasicTest::VerifyContent(PRInt32 aRows, PRInt32 aCols)
{
  nsIContent* root;
  GetRootContent(&root);
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
  NS_NewWindow(NULL, NS_GET_IID(nsIWidget), nsnull);

#endif

  nsIDeviceContext *dx;
  
  static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

  nsresult rv = CallCreateInstance(kDeviceContextCID, &dx);

  if (NS_OK == rv) {
    dx->Init(nsnull);
    dx->SetDevUnitsToAppUnits(dx->DevUnitsToTwips());
    dx->SetAppUnitsToDevUnits(dx->TwipsToDevUnits());
  }

  nsPresContext * pc = nsnull;
  nsresult status = NS_NewGalleyContext(&pc);
  if ((NS_FAILED(status)) ||  nsnull==pc)
  {
    fprintf(out, "bad galley pc");
    NS_ASSERTION(PR_FALSE, "bad galley pc");
  }

  pc->Init(dx, nsnull);

  // create a view manager
  nsIViewManager * vm = nsnull;

  static NS_DEFINE_IID(kViewManagerCID, NS_VIEW_MANAGER_CID);

  status = CallCreateInstance(kViewManagerCID, &vm);

  if ((NS_FAILED(status)) ||  nsnull==vm)
  {
    fprintf(out, "bad view manager");
    NS_ASSERTION(PR_FALSE, "bad view manager");
  }
  vm->Init(pc);

  nsIView * rootView = nsnull;

  // Create a view
  static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLL_PORT_VIEW_CID);

  status = CallCreateInstance(kScrollingViewCID, &rootView);

  if ((NS_FAILED(status)) ||  nsnull==rootView)
  {
    fprintf(out, "bad view");
    NS_ASSERTION(PR_FALSE, "bad view");
  }
  nsRect bounds(0, 0, 10000, 10000);
  rootView->Init(vm, bounds, nsnull);

  vm->SetRootView(rootView);

  nsCOMPtr<nsIStyleSet> ss(do_CreateInstance(kStyleSetCID,&status));
  if ((NS_FAILED(status)))
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
  pc->Init(dx, nsnull);
  aDoc->CreateCorrectContent(rows, cols);
  CreateGeometry(aDoc, pc);
  VerifyGeometry(aDoc, pc);

  aDoc->CreateCorrectFullContent(rows, cols);
  CreateGeometry(aDoc, pc);
  VerifyGeometry(aDoc, pc);
  */
}

/** given a content model, create a geometry model */
void GeometryTest::CreateGeometry(BasicTest * aDoc, nsPresContext *aPC)
{
  nsIContent *root;
  aDoc->GetRootContent(&root);
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

  nsHTMLReflowMetrics desiredSize;
  nsSize          maxSize(400, 600);
  nsSize          maxElementSize;


  mRootFrame->ResizeReflow(aPC, desiredSize, maxSize, &maxElementSize);

  
}

void GeometryTest::VerifyGeometry(BasicTest *aDoc, nsPresContext *aPC)
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






