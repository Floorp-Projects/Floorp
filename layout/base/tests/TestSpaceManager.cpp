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
#include "nsCRT.h"
#include "nsSpaceManager.h"

class MySpaceManager: public nsSpaceManager {
public:
  MySpaceManager(nsIPresShell* aPresShell, nsIFrame* aFrame)
      : nsSpaceManager(aPresShell, aFrame) {}

  PRBool  TestAddBand();
  PRBool  TestAddBandOverlap();
  PRBool  TestAddRectToBand();
  PRBool  TestRemoveRegion();
  PRBool  TestOffsetRegion();
  PRBool  TestResizeRectRegion();
  PRBool  TestGetBandData();

  struct BandInfo {
    nscoord   yOffset;
    nscoord   height;
    BandRect* firstRect;
    PRIntn    numRects;
  };

  struct BandsInfo {
    PRIntn   numBands;
    BandInfo bands[25];
  };

protected:
  void  GetBandsInfo(BandsInfo&);
};

void MySpaceManager::GetBandsInfo(BandsInfo& aBandsInfo)
{
  aBandsInfo.numBands = 0;

  if (!mBandList.IsEmpty()) {
    BandRect* band = mBandList.Head();
    while (nsnull != band) {
      BandInfo& info = aBandsInfo.bands[aBandsInfo.numBands];

      info.yOffset = band->mTop;
      info.height = band->mBottom - band->mTop;
      info.firstRect = band;

      aBandsInfo.numBands++;

      // Get the next band, and count the number of rects in this band
      info.numRects = 0;
      while (info.yOffset == band->mTop) {
        info.numRects++;

        band = band->Next();
        if (band == &mBandList) {
          // No bands left
          band = nsnull;
          break;
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//

// Test of adding a rect region that causes a new band to be created (no
// overlap with an existing band)
//
// This tests the following:
// 1. when there are no existing bands
// 2. adding a new band above the topmost band
// 3. inserting a new band between two existing bands
// 4. adding a new band below the bottommost band
PRBool MySpaceManager::TestAddBand()
{
  BandsInfo bandsInfo;
  nsresult  status;
  
  // Clear any existing regions
  ClearRegions();
  NS_ASSERTION(mBandList.IsEmpty(), "clear regions failed");

  /////////////////////////////////////////////////////////////////////////////
  // #1. Add a rect region. Verify the return status, and that a band rect is
  // added
  status = AddRectRegion((nsIFrame*)0x01, nsRect(10, 100, 100, 100));
  if (NS_FAILED(status)) {
    printf("TestAddBand: add failed (#1)\n");
    return PR_FALSE;
  }
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 1) {
    printf("TestAddBand: wrong number of bands (#1): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != 100) || (bandsInfo.bands[0].height != 100)) {
    printf("TestAddBand: wrong band size (#1)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #2. Add another band rect completely above the first band rect
  status = AddRectRegion((nsIFrame*)0x02, nsRect(10, -10, 100, 20));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 2) {
    printf("TestAddBand: wrong number of bands (#2): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != -10) || (bandsInfo.bands[0].height != 20) ||
      (bandsInfo.bands[1].yOffset != 100) || (bandsInfo.bands[1].height != 100)) {
    printf("TestAddBand: wrong band sizes (#2)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #3. Now insert a new band between the two existing bands
  status = AddRectRegion((nsIFrame*)0x03, nsRect(10, 40, 100, 30));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 3) {
    printf("TestAddBand: wrong number of bands (#3): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != -10) || (bandsInfo.bands[0].height != 20) ||
      (bandsInfo.bands[1].yOffset != 40) || (bandsInfo.bands[1].height != 30) ||
      (bandsInfo.bands[2].yOffset != 100) || (bandsInfo.bands[2].height != 100)) {
    printf("TestAddBand: wrong band sizes (#3)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #4. Append a new bottommost band
  status = AddRectRegion((nsIFrame*)0x04, nsRect(10, 210, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 4) {
    printf("TestAddBand: wrong number of bands (#4): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != -10) || (bandsInfo.bands[0].height != 20) ||
      (bandsInfo.bands[1].yOffset != 40) || (bandsInfo.bands[1].height != 30) ||
      (bandsInfo.bands[2].yOffset != 100) || (bandsInfo.bands[2].height != 100) ||
      (bandsInfo.bands[3].yOffset != 210) || (bandsInfo.bands[3].height != 100)) {
    printf("TestAddBand: wrong band sizes (#4)\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Test of adding a rect region that overlaps an existing band
//
// This tests the following:
// 1. Adding a rect that's above and partially overlaps an existing band
// 2. Adding a rect that's completely contained by an existing band
// 3. Adding a rect that overlaps and is below an existing band
// 3. Adding a rect that contains an existing band
PRBool MySpaceManager::TestAddBandOverlap()
{
  BandsInfo bandsInfo;
  nsresult  status;
  
  // Clear any existing regions
  ClearRegions();
  NS_ASSERTION(mBandList.IsEmpty(), "clear regions failed");

  // Add a new band
  status = AddRectRegion((nsIFrame*)0x01, nsRect(100, 25, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  /////////////////////////////////////////////////////////////////////////////
  // #1. Add a rect region that's above and partially overlaps an existing band
  status = AddRectRegion((nsIFrame*)0x02, nsRect(10, -25, 50, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 3) {
    printf("TestAddBandOverlap: wrong number of bands (#1): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != -25) || (bandsInfo.bands[0].height != 50) ||
      (bandsInfo.bands[1].yOffset != 25) || (bandsInfo.bands[1].height != 50) ||
      (bandsInfo.bands[2].yOffset != 75) || (bandsInfo.bands[2].height != 50)) {
    printf("TestAddBandOverlap: wrong band sizes (#1)\n");
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].numRects != 1) ||
      (bandsInfo.bands[1].numRects != 2) ||
      (bandsInfo.bands[2].numRects != 1)) {
    printf("TestAddBandOverlap: wrong number of rects (#1)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #2. Add a rect region that's contained by the first band
  status = AddRectRegion((nsIFrame*)0x03, nsRect(200, -15, 50, 10));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 5) {
    printf("TestAddBandOverlap: wrong number of bands (#2): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != -25) || (bandsInfo.bands[0].height != 10) ||
      (bandsInfo.bands[1].yOffset != -15) || (bandsInfo.bands[1].height != 10) ||
      (bandsInfo.bands[2].yOffset != -5) || (bandsInfo.bands[2].height != 30) ||
      (bandsInfo.bands[3].yOffset != 25) || (bandsInfo.bands[3].height != 50) ||
      (bandsInfo.bands[4].yOffset != 75) || (bandsInfo.bands[4].height != 50)) {
    printf("TestAddBandOverlap: wrong band sizes (#2)\n");
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].numRects != 1) ||
      (bandsInfo.bands[1].numRects != 2) ||
      (bandsInfo.bands[2].numRects != 1) ||
      (bandsInfo.bands[3].numRects != 2) ||
      (bandsInfo.bands[4].numRects != 1)) {
    printf("TestAddBandOverlap: wrong number of rects (#2)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #3. Add a rect that overlaps and is below an existing band
  status = AddRectRegion((nsIFrame*)0x04, nsRect(200, 100, 50, 50));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 7) {
    printf("TestAddBandOverlap: wrong number of bands (#3): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != -25) || (bandsInfo.bands[0].height != 10) ||
      (bandsInfo.bands[1].yOffset != -15) || (bandsInfo.bands[1].height != 10) ||
      (bandsInfo.bands[2].yOffset != -5) || (bandsInfo.bands[2].height != 30) ||
      (bandsInfo.bands[3].yOffset != 25) || (bandsInfo.bands[3].height != 50) ||
      (bandsInfo.bands[4].yOffset != 75) || (bandsInfo.bands[4].height != 25) ||
      (bandsInfo.bands[5].yOffset != 100) || (bandsInfo.bands[5].height != 25) ||
      (bandsInfo.bands[6].yOffset != 125) || (bandsInfo.bands[6].height != 25)) {
    printf("TestAddBandOverlap: wrong band sizes (#3)\n");
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].numRects != 1) ||
      (bandsInfo.bands[1].numRects != 2) ||
      (bandsInfo.bands[2].numRects != 1) ||
      (bandsInfo.bands[3].numRects != 2) ||
      (bandsInfo.bands[4].numRects != 1) ||
      (bandsInfo.bands[5].numRects != 2) ||
      (bandsInfo.bands[6].numRects != 1)) {
    printf("TestAddBandOverlap: wrong number of rects (#3)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #4. Now test adding a rect that contains an existing band
  ClearRegions();
  status = AddRectRegion((nsIFrame*)0x01, nsRect(100, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  // Now add a rect that contains the existing band vertically
  status = AddRectRegion((nsIFrame*)0x02, nsRect(200, 50, 100, 200));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 3) {
    printf("TestAddBandOverlap: wrong number of bands (#4): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].yOffset != 50) || (bandsInfo.bands[0].height != 50) ||
      (bandsInfo.bands[1].yOffset != 100) || (bandsInfo.bands[1].height != 100) ||
      (bandsInfo.bands[2].yOffset != 200) || (bandsInfo.bands[2].height != 50)) {
    printf("TestAddBandOverlap: wrong band sizes (#4)\n");
    return PR_FALSE;
  }
  if ((bandsInfo.bands[0].numRects != 1) ||
      (bandsInfo.bands[1].numRects != 2) ||
      (bandsInfo.bands[2].numRects != 1)) {
    printf("TestAddBandOverlap: wrong number of rects (#4)\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Test of adding rects within an existing band
//
// This tests the following:
// 1. Add a rect to the left of an existing rect
// 2. Add a rect to the right of the rightmost rect
// 3. Add a rect that's to the left of an existing rect and that overlaps it
// 4. Add a rect that's to the right of an existing rect and that overlaps it
// 5. Add a rect over top of an existing rect (existing rect contains new rect)
// 6. Add a new rect that completely contains an existing rect
PRBool MySpaceManager::TestAddRectToBand()
{
  BandsInfo bandsInfo;
  BandRect* bandRect;
  nsresult  status;

  // Clear any existing regions
  ClearRegions();
  NS_ASSERTION(mBandList.IsEmpty(), "clear regions failed");

  // Add a new band
  status = AddRectRegion((nsIFrame*)0x01, nsRect(100, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  /////////////////////////////////////////////////////////////////////////////
  // #1. Add a rect region that's to the left of the existing rect
  status = AddRectRegion((nsIFrame*)0x02, nsRect(10, 100, 50, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 1) {
    printf("TestAddRectToBand: wrong number of bands (#1): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if (bandsInfo.bands[0].numRects != 2) {
    printf("TestAddRectToBand: wrong number of rects (#1): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 60)) {
    printf("TestAddRectToBand: wrong first rect (#1)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 100) || (bandRect->mRight != 200)) {
    printf("TestAddRectToBand: wrong second rect (#1)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #2. Add a rect region that's to the right of the rightmost rect
  status = AddRectRegion((nsIFrame*)0x03, nsRect(250, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  NS_ASSERTION(bandsInfo.numBands == 1, "wrong number of bands");
  if (bandsInfo.bands[0].numRects != 3) {
    printf("TestAddRectToBand: wrong number of rects (#2): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 60)) {
    printf("TestAddRectToBand: wrong first rect (#2)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 100) || (bandRect->mRight != 200)) {
    printf("TestAddRectToBand: wrong second rect (#2)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 250) || (bandRect->mRight != 350)) {
    printf("TestAddRectToBand: wrong third rect (#2)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #3. Add a rect region that's to the left of an existing rect and that
  // overlaps the rect
  status = AddRectRegion((nsIFrame*)0x04, nsRect(80, 100, 40, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  NS_ASSERTION(bandsInfo.numBands == 1, "wrong number of bands");
  if (bandsInfo.bands[0].numRects != 5) {
    printf("TestAddRectToBand: wrong number of rects (#3): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 60)) {
    printf("TestAddRectToBand: wrong first rect (#3)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 80) || (bandRect->mRight != 100)) {
    printf("TestAddRectToBand: wrong second rect (#3)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 100) || (bandRect->mRight != 120) ||
      (bandRect->mNumFrames != 2) || !bandRect->IsOccupiedBy((nsIFrame*)0x04)) {
    printf("TestAddRectToBand: wrong third rect (#3)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 120) || (bandRect->mRight != 200)) {
    printf("TestAddRectToBand: wrong fourth rect (#3)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 250) || (bandRect->mRight != 350)) {
    printf("TestAddRectToBand: wrong fifth rect (#3)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #4. Add a rect region that's to the right of an existing rect and that
  // overlaps the rect
  status = AddRectRegion((nsIFrame*)0x05, nsRect(50, 100, 20, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  NS_ASSERTION(bandsInfo.numBands == 1, "wrong number of bands");
  if (bandsInfo.bands[0].numRects != 7) {
    printf("TestAddRectToBand: wrong number of rects (#4): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 50)) {
    printf("TestAddRectToBand: wrong first rect (#4)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 50) || (bandRect->mRight != 60) ||
      (bandRect->mNumFrames != 2) || !bandRect->IsOccupiedBy((nsIFrame*)0x05)) {
    printf("TestAddRectToBand: wrong second rect (#4)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 60) || (bandRect->mRight != 70)) {
    printf("TestAddRectToBand: wrong third rect (#4)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 80) || (bandRect->mRight != 100)) {
    printf("TestAddRectToBand: wrong fourth rect (#4)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #5. Add a new rect over top of an existing rect (existing rect contains
  // the new rect)
  status = AddRectRegion((nsIFrame*)0x06, nsRect(20, 100, 20, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  NS_ASSERTION(bandsInfo.numBands == 1, "wrong number of bands");
  if (bandsInfo.bands[0].numRects != 9) {
    printf("TestAddRectToBand: wrong number of rects (#5): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 20)) {
    printf("TestAddRectToBand: wrong first rect (#5)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 20) || (bandRect->mRight != 40) ||
      (bandRect->mNumFrames != 2) || !bandRect->IsOccupiedBy((nsIFrame*)0x06)) {
    printf("TestAddRectToBand: wrong second rect (#5)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 40) || (bandRect->mRight != 50)) {
    printf("TestAddRectToBand: wrong third rect (#5)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 50) || (bandRect->mRight != 60) || (bandRect->mNumFrames != 2)) {
    printf("TestAddRectToBand: wrong fourth rect (#5)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #6. Add a new rect that completely contains an existing rect
  status = AddRectRegion((nsIFrame*)0x07, nsRect(0, 100, 30, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  NS_ASSERTION(bandsInfo.numBands == 1, "wrong number of bands");
  if (bandsInfo.bands[0].numRects != 11) {
    printf("TestAddRectToBand: wrong number of rects (#6): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 0) || (bandRect->mRight != 10)) {
    printf("TestAddRectToBand: wrong first rect (#6)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 20) ||
      (bandRect->mNumFrames != 2) || !bandRect->IsOccupiedBy((nsIFrame*)0x07)) {
    printf("TestAddRectToBand: wrong second rect (#6)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 20) || (bandRect->mRight != 30) ||
      (bandRect->mNumFrames != 3) || !bandRect->IsOccupiedBy((nsIFrame*)0x07)) {
    printf("TestAddRectToBand: wrong third rect (#6)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  if ((bandRect->mLeft != 30) || (bandRect->mRight != 40) || (bandRect->mNumFrames != 2)) {
    printf("TestAddRectToBand: wrong fourth rect (#6)\n");
    return PR_FALSE;
  }
  bandRect = bandRect->Next();
  NS_ASSERTION(1 == bandRect->mNumFrames, "unexpected shared rect");
  if ((bandRect->mLeft != 40) || (bandRect->mRight != 50)) {
    printf("TestAddRectToBand: wrong fifth rect (#6)\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Test of removing regions. We especially need to test that we correctly
// coalesce adjacent rects and bands
//
// This tests the following:
// 1. simple test of removing the one and only band rect
// 2. removing a shared rect and verifying adjacent rects are coalesced
// 3. removing a band rect and making sure adjacent bands are combined
PRBool MySpaceManager::TestRemoveRegion()
{
  BandsInfo bandsInfo;
  BandRect* bandRect;
  nsresult  status;

  // Clear any existing regions
  ClearRegions();
  NS_ASSERTION(mBandList.IsEmpty(), "clear regions failed");

  /////////////////////////////////////////////////////////////////////////////
  // #1. A simple test of removing the one and only band rect
  status = AddRectRegion((nsIFrame*)0x01, nsRect(10, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  status = RemoveRegion((nsIFrame*)0x01);
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 0) {
    printf("TestRemoveRegion: wrong number of bands (#1): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #2. Test removing a rect that's shared. Make sure adjacent rects are
  // coalesced
  status = AddRectRegion((nsIFrame*)0x01, nsRect(10, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  status = AddRectRegion((nsIFrame*)0x02, nsRect(40, 100, 20, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  // Verify there are three rects in the band
  GetBandsInfo(bandsInfo);
  if (bandsInfo.bands[0].numRects != 3) {
    printf("TestRemoveRegion: wrong number of rects (#2): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }

  // Remove the region associated with the second frame
  status = RemoveRegion((nsIFrame*)0x02);
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.bands[0].numRects != 1) {
    printf("TestRemoveRegion: failed to coalesce adjacent rects (#2)\n");
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 110)) {
    printf("TestRemoveRegion: wrong size rect (#2)\n");
    return PR_FALSE;
  }

  /////////////////////////////////////////////////////////////////////////////
  // #3. Test removing a band rect and making sure adjacent bands are combined
  status = AddRectRegion((nsIFrame*)0x02, nsRect(10, 140, 20, 20));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  // Verify there are three bands and that each band has three rects
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 3) {
    printf("TestRemoveRegion: wrong number of bands (#3): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if (bandsInfo.bands[0].numRects != 1) {
    printf("TestRemoveRegion: band #1 wrong number of rects (#3): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }
  if (bandsInfo.bands[1].numRects != 2) {
    printf("TestRemoveRegion: band #2 wrong number of rects (#3): %i\n", bandsInfo.bands[1].numRects);
    return PR_FALSE;
  }
  if (bandsInfo.bands[2].numRects != 1) {
    printf("TestRemoveRegion: band #3 wrong number of rects (#3): %i\n", bandsInfo.bands[2].numRects);
    return PR_FALSE;
  }

  // Remove the region associated with the second frame
  status = RemoveRegion((nsIFrame*)0x02);
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  GetBandsInfo(bandsInfo);
  if (bandsInfo.bands[0].numRects != 1) {
    printf("TestRemoveRegion: failed to coalesce adjacent rects (#3)\n");
    return PR_FALSE;
  }
  bandRect = bandsInfo.bands[0].firstRect;
  if ((bandRect->mLeft != 10) || (bandRect->mRight != 110)) {
    printf("TestRemoveRegion: wrong size rect (#3)\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Test of offseting regions
//
// This tests the following:
// 1. simple test of offseting the one and only band rect
PRBool MySpaceManager::TestOffsetRegion()
{
  BandsInfo bandsInfo;
  BandRect* bandRect;
  nsresult  status;
  
  // Clear any existing regions
  ClearRegions();
  NS_ASSERTION(mBandList.IsEmpty(), "clear regions failed");

  /////////////////////////////////////////////////////////////////////////////
  // #1. A simple test of offseting the one and only band rect
  status = AddRectRegion((nsIFrame*)0x01, nsRect(10, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  status = OffsetRegion((nsIFrame*)0x01, 50, 50);
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  // Verify there is one band with one rect
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 1) {
    printf("TestOffsetRegion: wrong number of bands (#1): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if (bandsInfo.bands[0].numRects != 1) {
    printf("TestOffsetRegion: wrong number of rects (#1): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }

  // Verify the position
  bandRect = bandsInfo.bands[0].firstRect;
  if ((bandRect->mLeft != 60) || (bandRect->mTop != 150)) {
    printf("TestOffsetRegion: wrong rect origin (#1)\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Test of resizing rect regions
//
// This tests the following:
// 1. simple test of resizing the one and only band rect
PRBool MySpaceManager::TestResizeRectRegion()
{
  BandsInfo bandsInfo;
  BandRect* bandRect;
  nsresult  status;
  
  // Clear any existing regions
  ClearRegions();
  NS_ASSERTION(mBandList.IsEmpty(), "clear regions failed");

  /////////////////////////////////////////////////////////////////////////////
  // #1. A simple test of resizing the right edge of the one and only band rect
  status = AddRectRegion((nsIFrame*)0x01, nsRect(10, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");
  status = ResizeRectRegion((nsIFrame*)0x01, 50, 50, nsSpaceManager::RightEdge);
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  // Verify there is one band with one rect
  GetBandsInfo(bandsInfo);
  if (bandsInfo.numBands != 1) {
    printf("TestResizeRectRegion: wrong number of bands (#1): %i\n", bandsInfo.numBands);
    return PR_FALSE;
  }
  if (bandsInfo.bands[0].numRects != 1) {
    printf("TestResizeRectRegion: wrong number of rects (#1): %i\n", bandsInfo.bands[0].numRects);
    return PR_FALSE;
  }

  // Verify the position and size of the rect
  bandRect = bandsInfo.bands[0].firstRect;
  if ((bandRect->mLeft != 10) || (bandRect->mTop != 100) ||
      (bandRect->mRight != 160) || (bandRect->mBottom != 250)) {
    printf("TestResizeRectRegion: wrong rect shape (#1)\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Test of getting the band data
PRBool MySpaceManager::TestGetBandData()
{
  nsresult  status;
  nscoord   yMost;

  // Clear any existing regions
  ClearRegions();
  NS_ASSERTION(mBandList.IsEmpty(), "clear regions failed");

  // Make sure YMost() returns the correct result
  if (NS_ERROR_ABORT != YMost(yMost)) {
    printf("TestGetBandData: YMost() returned wrong result (#1)\n");
    return PR_FALSE;
  }

  // Make a band with three rects
  status = AddRectRegion((nsIFrame*)0x01, nsRect(100, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  status = AddRectRegion((nsIFrame*)0x02, nsRect(300, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  status = AddRectRegion((nsIFrame*)0x03, nsRect(500, 100, 100, 100));
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  // Verify that YMost() is correct
  if ((NS_OK != YMost(yMost)) || (yMost != 200)) {
    printf("TestGetBandData: YMost() returned wrong value (#2)\n");
    return PR_FALSE;
  }

  // Get the band data using a very large clip rect and a band data struct
  // that's large enough
  nsBandData      bandData;
  nsBandTrapezoid trapezoids[16];
  bandData.mSize = 16;
  bandData.mTrapezoids = trapezoids;
  status = GetBandData(100, nsSize(10000,10000), bandData);
  NS_ASSERTION(NS_SUCCEEDED(status), "unexpected status");

  // Verify that there are seven trapezoids
  if (bandData.mCount != 7) {
    printf("TestGetBandData: wrong trapezoid count (#3)\n");
    return PR_FALSE;
  }
  
  // Get the band data using a very large clip rect and a band data struct
  // that's too small
  bandData.mSize = 3;
  status = GetBandData(100, nsSize(10000,10000), bandData);
  if (NS_SUCCEEDED(status)) {
    printf("TestGetBandData: ignored band data count (#4)\n");
    return PR_FALSE;
  }

  // Make sure the count has been updated to reflect the number of trapezoids
  // required
  if (bandData.mCount <= bandData.mSize) {
    printf("TestGetBandData: bad band data count (#5)\n");
    return PR_FALSE;
  }

  // XXX We need lots more tests here...

  return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//

int main(int argc, char** argv)
{
  // Create a space manager
  MySpaceManager* spaceMgr = new MySpaceManager(nsnull, nsnull);
  
  // Test adding rect regions
  if (!spaceMgr->TestAddBand()) {
    delete spaceMgr;
    return -1;
  }

  // Test adding rect regions that overlap existing bands
  if (!spaceMgr->TestAddBandOverlap()) {
    delete spaceMgr;
    return -1;
  }

  // Test adding rects within an existing band
  if (!spaceMgr->TestAddRectToBand()) {
    delete spaceMgr;
    return -1;
  }

  // Test removing regions
  if (!spaceMgr->TestRemoveRegion()) {
    return -1;
  }

  // Test offseting regions
  if (!spaceMgr->TestOffsetRegion()) {
    return -1;
  }

  // Test resizing rect regions
  if (!spaceMgr->TestResizeRectRegion()) {
    return -1;
  }

  // Test getting the band data
  if (!spaceMgr->TestGetBandData()) {
    return -1;
  }

  delete spaceMgr;
  return 0;
}
