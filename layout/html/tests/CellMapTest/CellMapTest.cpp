/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <stdio.h>
#include "nscore.h"
#include "..\..\src\nsCellMap.h"

static FILE * out;

class CellData
{
public:
  PRInt32 mID;

  CellData(PRInt32 aID)
  { mID = aID;};

};

class BasicTest
{
public:
  BasicTest();
  virtual ~BasicTest() {};

  void DumpCellMap(nsCellMap *aMap, PRInt32 aRows, PRInt32 aCols);
  void VerifyRowsAndCols(nsCellMap *aMap, PRInt32 aRows, PRInt32 aCols);
};

BasicTest::BasicTest()
{
  PRInt32 rows = 3;
  PRInt32 cols = 4;
  nsCellMap *map = new nsCellMap(rows, cols);
  fprintf(out, "\nCreating %d by %d table with these values...\n", rows, cols);
  for (PRInt32 i=0; i<rows; i++)
  {
    for (PRInt32 j=0; j<cols; j++)
    {
      CellData * cellData = new CellData((i*cols)+j);
      fprintf(out, "cell_%d ", cellData->mID);
      map->SetCellAt(cellData, i, j);
    }
    fprintf(out, "\n");
  }
  // verify input
  DumpCellMap(map, rows, cols);
  VerifyRowsAndCols(map, rows, cols);

  // cell map's own output should match above output
  map->DumpCellMap();

  // append a column
  fprintf(out, "\nadding a column, so cols=%d\n", cols);
  cols++;
  map->GrowTo(cols);
  DumpCellMap(map, rows, cols);
  VerifyRowsAndCols(map, rows, cols);

  // reset the map
  rows++;  cols++;
  fprintf(out, "\nresetting the map to %d,%d\n", rows, cols);
  map->Reset(rows, cols);
  DumpCellMap(map, rows, cols);
  VerifyRowsAndCols(map, rows, cols);

}

void BasicTest::DumpCellMap(nsCellMap *aMap, PRInt32 aRows, PRInt32 aCols)
{
  fprintf(out, "\nPrinting out %d by %d table...\n", aRows, aCols);
  for (PRInt32 i=0; i<aRows; i++)
  {
    for (PRInt32 j=0; j<aCols; j++)
    {
      CellData * cellData = aMap->GetCellAt(i, j);
      if (nsnull!=cellData)
        fprintf(out, "Cell_%d ", cellData->mID);
      else
        fprintf(out, "null ");
    }
    fprintf(out, "\n");
  }
}

void BasicTest::VerifyRowsAndCols(nsCellMap *aMap, PRInt32 aRows, PRInt32 aCols)
{
  if (aMap->GetRowCount()!=aRows)
    fprintf(out, "ERROR:  map->mRowCount!=rows, %d != %d\n", aMap->GetRowCount(), aRows);
  if (aMap->GetColCount()!=aCols)
    fprintf(out, "ERROR:  map->mColCount!=cols, %d != %d\n", aMap->GetColCount(), aCols);
}

void main (int argc, char **argv)
{
  out = fopen("CellMapTest.txt", "w+t");
  if (nsnull==out)
  {
    printf("test failed to open output file\n");
    exit(-1);
  }
  fprintf(out, "Test starting...\n\n");
  BasicTest basicTest;
  fprintf(out, "\nTest completed.\n");
}



