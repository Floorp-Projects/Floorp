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
#ifndef nsSpaceManager_h___
#define nsSpaceManager_h___

#include "nsISpaceManager.h"
#include "prclist.h"

#define NS_SPACE_MANAGER_CACHE_SIZE 4

/**
 * Implementation of nsISpaceManager that maintains a region data structure of
 * unavailable space
 */
class nsSpaceManager : public nsISpaceManager {
public:
  static nsSpaceManager *Create(nsIFrame* aFrame);
  static void Shutdown();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISpaceManager
  NS_IMETHOD GetFrame(nsIFrame*& aFrame) const;

  NS_IMETHOD Translate(nscoord aDx, nscoord aDy);
  NS_IMETHOD GetTranslation(nscoord& aX, nscoord& aY) const;
  NS_IMETHOD YMost(nscoord& aYMost) const;

  NS_IMETHOD GetBandData(nscoord       aYOffset,
                         const nsSize& aMaxSize,
                         nsBandData&   aBandData) const;

  NS_IMETHOD AddRectRegion(nsIFrame*     aFrame,
                           const nsRect& aUnavailableSpace);
  NS_IMETHOD ResizeRectRegion(nsIFrame*    aFrame,
                              nscoord      aDeltaWidth,
                              nscoord      aDeltaHeight,
                              AffectedEdge aEdge);
  NS_IMETHOD OffsetRegion(nsIFrame* aFrame, nscoord aDx, nscoord aDy);
  NS_IMETHOD RemoveRegion(nsIFrame* aFrame);

  NS_IMETHOD ClearRegions();

#ifdef DEBUG
  NS_IMETHOD List(FILE* out);
  void       SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  nsSpaceManager(nsIFrame* aFrame);

  // Structure that maintains information about the region associated
  // with a particular frame
  struct FrameInfo {
    nsIFrame* const mFrame;
    nsRect          mRect;       // rectangular region
    FrameInfo*      mNext;

    FrameInfo(nsIFrame* aFrame, const nsRect& aRect);
#ifdef NS_BUILD_REFCNT_LOGGING
    ~FrameInfo();
#endif
  };

public:
  // Doubly linked list of band rects
  struct BandRect : PRCListStr {
    nscoord   mLeft, mTop;
    nscoord   mRight, mBottom;
    PRIntn    mNumFrames;    // number of frames occupying this rect
    union {
      nsIFrame*    mFrame;   // single frame occupying the space
      nsVoidArray* mFrames;  // list of frames occupying the space
    };

    BandRect(nscoord aLeft, nscoord aTop,
             nscoord aRight, nscoord aBottom,
             nsIFrame*);
    BandRect(nscoord aLeft, nscoord aTop,
             nscoord aRight, nscoord aBottom,
             nsVoidArray*);
    ~BandRect();

    // List operations
    BandRect* Next() const {return (BandRect*)PR_NEXT_LINK(this);}
    BandRect* Prev() const {return (BandRect*)PR_PREV_LINK(this);}
    void      InsertBefore(BandRect* aBandRect) {PR_INSERT_BEFORE(aBandRect, this);}
    void      InsertAfter(BandRect* aBandRect) {PR_INSERT_AFTER(aBandRect, this);}
    void      Remove() {PR_REMOVE_LINK(this);}

    // Split the band rect into two vertically, with this band rect becoming
    // the top part, and a new band rect being allocated and returned for the
    // bottom part
    //
    // Does not insert the new band rect into the linked list
    BandRect* SplitVertically(nscoord aBottom);

    // Split the band rect into two horizontally, with this band rect becoming
    // the left part, and a new band rect being allocated and returned for the
    // right part
    //
    // Does not insert the new band rect into the linked list
    BandRect* SplitHorizontally(nscoord aRight);

    // Accessor functions
    PRBool  IsOccupiedBy(const nsIFrame*) const;
    void    AddFrame(const nsIFrame*);
    void    RemoveFrame(const nsIFrame*);
    PRBool  HasSameFrameList(const BandRect* aBandRect) const;
    PRInt32 Length() const;
  };

  // Circular linked list of band rects
  struct BandList : BandRect {
    BandList();

    // Accessors
    PRBool    IsEmpty() const {return PR_CLIST_IS_EMPTY((PRCListStr*)this);}
    BandRect* Head() const {return (BandRect*)PR_LIST_HEAD(this);}
    BandRect* Tail() const {return (BandRect*)PR_LIST_TAIL(this);}

    // Operations
    void      Append(BandRect* aBandRect) {PR_APPEND_LINK(aBandRect, this);}

    // Remove and delete all the band rects in the list
    void      Clear();
  };

protected:
  nsIFrame* const mFrame;     // frame associated with the space manager
  nscoord         mX, mY;     // translation from local to global coordinate space
  BandList        mBandList;  // header/sentinel for circular linked list of band rects
  FrameInfo*      mFrameInfoMap;

protected:
  virtual ~nsSpaceManager();
  FrameInfo* GetFrameInfoFor(nsIFrame* aFrame);
  FrameInfo* CreateFrameInfo(nsIFrame* aFrame, const nsRect& aRect);
  void       DestroyFrameInfo(FrameInfo*);

  void       ClearFrameInfo();
  void       ClearBandRects();

  BandRect*  GetNextBand(const BandRect* aBandRect) const;
  void       DivideBand(BandRect* aBand, nscoord aBottom);
  PRBool     CanJoinBands(BandRect* aBand, BandRect* aPrevBand);
  PRBool     JoinBands(BandRect* aBand, BandRect* aPrevBand);
  void       AddRectToBand(BandRect* aBand, BandRect* aBandRect);
  void       InsertBandRect(BandRect* aBandRect);

  nsresult   GetBandAvailableSpace(const BandRect* aBand,
                                   nscoord         aY,
                                   const nsSize&   aMaxSize,
                                   nsBandData&     aAvailableSpace) const;

private:
  static PRInt32 sCachedSpaceManagerCount;
  static nsSpaceManager *sCachedSpaceManagers[NS_SPACE_MANAGER_CACHE_SIZE];

  void LastRelease();

  nsSpaceManager(const nsSpaceManager&);  // no implementation
  void operator=(const nsSpaceManager&);  // no implementation
};

#endif /* nsSpaceManager_h___ */

