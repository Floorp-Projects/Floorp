/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Dainis Jonitis,
 * <Dainis_Jonitis@swh-t.lv>.  Portions created by Dainis Jonitis are
 * Copyright (C) 2001 Dainis Jonitis. All Rights Reserved.
 *
 * Contributor(s):
 */


#ifndef nsRegion_h__
#define nsRegion_h__


// Implementation of region. 
// Region is represented as circular double-linked list of nsRegion::RgnRect structures.
// Rectangles in this list do not overlap and are sorted by (y, x) coordinates.

#include "nsRect.h"


// Special version of nsRect structure for speed optimizations in nsRegion code.
// Most important functions could be made inline and be sure that passed rectangles
// will always be non-empty. 

struct nsRectFast : public nsRect
{
  nsRectFast () {}      // No need to call parent constructor to set default values
  nsRectFast (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) : nsRect (aX, aY, aWidth, aHeight) {}
  nsRectFast (const nsRect& aRect) : nsRect (aRect) {}

#if 1   // Override nsRect methods to make them inline. Do not check for emptiness.
  PRBool Contains (const nsRectFast &aRect) const
  {
    return (PRBool) ((aRect.x >= x) && (aRect.y >= y) &&
                     (aRect.XMost() <= XMost()) && (aRect.YMost() <= YMost()));
  }

  PRBool Intersects (const nsRectFast &aRect) const
  {
    return (PRBool) ((x < aRect.XMost()) && (y < aRect.YMost()) &&
                     (aRect.x < XMost()) && (aRect.y < YMost()));
  }

  PRBool IntersectRect (const nsRectFast &aRect1, const nsRectFast &aRect2)
  {
    const nscoord xmost = PR_MIN (aRect1.XMost(), aRect2.XMost());
    x = PR_MAX (aRect1.x, aRect2.x);
    width = xmost - x;
    if (width <= 0) return PR_FALSE;

    const nscoord ymost = PR_MIN (aRect1.YMost(), aRect2.YMost());
    y = PR_MAX (aRect1.y, aRect2.y);
    height = ymost - y;
    if (height <= 0) return PR_FALSE;

    return PR_TRUE;
  }

  void UnionRect (const nsRectFast &aRect1, const nsRectFast &aRect2)
  {
    const nscoord xmost = PR_MAX (aRect1.XMost(), aRect2.XMost());
    const nscoord ymost = PR_MAX (aRect1.YMost(), aRect2.YMost());
    x = PR_MIN (aRect1.x, aRect2.x);
    y = PR_MIN (aRect1.y, aRect2.y);
    width  = xmost - x;
    height = ymost - y;
  }
#endif
};



class NS_GFX nsRegion
{
  friend class nsRegionRectIterator;
  friend class RgnRectMemoryAllocator;

  struct NS_GFX RgnRect : public nsRectFast
  {
    RgnRect* prev;
    RgnRect* next;

    RgnRect () {}                           // No need to call parent constructor to set default values
    RgnRect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) : nsRectFast (aX, aY, aWidth, aHeight) {}
    RgnRect (const nsRectFast& aRect) : nsRectFast (aRect) {}

    inline void* operator new (size_t) CPP_THROW_NEW;
    inline void  operator delete (void* aRect, size_t);
    RgnRect& operator = (const RgnRect& aRect)       // Do not overwrite prev/next pointers
    { 
       x = aRect.x;
       y = aRect.y;
       width  = aRect.width;
       height = aRect.height;
       return *this;
    }
  };


public:
  nsRegion ();
 ~nsRegion () { SetToElements (0); }

  nsRegion& Copy (const nsRegion& aRegion);
  nsRegion& Copy (const nsRectFast& aRect);

  nsRegion& And  (const nsRegion& aRgn1,   const nsRegion& aRgn2);
  nsRegion& And  (const nsRegion& aRegion, const nsRectFast& aRect);
  nsRegion& And  (const nsRectFast& aRect, const nsRegion& aRegion)
  {
    return  And  (aRegion, aRect);
  }
  nsRegion& And  (const nsRectFast& aRect1, const nsRectFast& aRect2)
  {
    nsRectFast TmpRect;
    
    // Force generic nsRect::IntersectRect to handle empty rectangles!
    TmpRect.nsRect::IntersectRect (aRect1, aRect2);
    return Copy (TmpRect);
  }
    
  nsRegion& Or   (const nsRegion& aRgn1,   const nsRegion& aRgn2);
  nsRegion& Or   (const nsRegion& aRegion, const nsRectFast& aRect);
  nsRegion& Or   (const nsRectFast& aRect, const nsRegion& aRegion)
  {  
    return  Or   (aRegion, aRect);
  }
  nsRegion& Or   (const nsRectFast& aRect1, const nsRectFast& aRect2)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect1);
    return Or (TmpRegion, aRect2);
  }

  nsRegion& Xor  (const nsRegion& aRgn1,   const nsRegion& aRgn2);
  nsRegion& Xor  (const nsRegion& aRegion, const nsRectFast& aRect);
  nsRegion& Xor  (const nsRectFast& aRect, const nsRegion& aRegion)
  {
    return  Xor  (aRegion, aRect);
  }
  nsRegion& Xor  (const nsRectFast& aRect1, const nsRectFast& aRect2)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect1);
    return Xor (TmpRegion, aRect2);
  }

  nsRegion& Sub  (const nsRegion& aRgn1,   const nsRegion& aRgn2);
  nsRegion& Sub  (const nsRegion& aRegion, const nsRectFast& aRect);
  nsRegion& Sub  (const nsRectFast& aRect, const nsRegion& aRegion)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect);
    return Sub (TmpRegion, aRegion);
  }
  nsRegion& Sub  (const nsRectFast& aRect1, const nsRectFast& aRect2)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect1);
    return Sub (TmpRegion, aRect2);
  }


  PRBool GetBoundRect (nsRect& aBound) const
  {
    aBound = mBoundRect;
    return !mBoundRect.IsEmpty ();
  }

  void Offset (PRInt32 aXOffset, PRInt32 aYOffset);
  void Empty () 
  { 
    SetToElements (0);
    mBoundRect.SetRect (0, 0, 0, 0);
  }
  PRBool IsEmpty () const { return mRectCount == 0; }
  PRBool IsComplex () const { return mRectCount > 1; }
  PRBool IsEqual (const nsRegion& aRegion) const;
  PRUint32 GetNumRects () const { return mRectCount; }


private:
  PRUint32    mRectCount;
  RgnRect*    mCurRect;
  RgnRect     mRectListHead;
  nsRectFast  mBoundRect;

  void InsertBefore (RgnRect* aNewRect, RgnRect* aRelativeRect)
  {
    aNewRect->prev = aRelativeRect->prev;
    aNewRect->next = aRelativeRect;
    aRelativeRect->prev->next = aNewRect;
    aRelativeRect->prev = aNewRect;
    mCurRect = aNewRect;
    mRectCount++;
  }

  void InsertAfter (RgnRect* aNewRect, RgnRect* aRelativeRect)
  {
    aNewRect->prev = aRelativeRect;
    aNewRect->next = aRelativeRect->next;
    aRelativeRect->next->prev = aNewRect;
    aRelativeRect->next = aNewRect;
    mCurRect = aNewRect;
    mRectCount++;
  }

  void SetToElements (PRUint32 aCount);
  RgnRect* Remove (RgnRect* aRect);
  void InsertInPlace (RgnRect* aRect, PRBool aOptimizeOnFly = PR_FALSE);
  void SaveLinkChain ();
  void RestoreLinkChain ();
  void Optimize ();
  void SubRegion (const nsRegion& aRegion, nsRegion& aResult) const;
  void SubRect (const nsRectFast& aRect, nsRegion& aResult, nsRegion& aCompleted) const;
  void SubRect (const nsRectFast& aRect, nsRegion& aResult) const
  {    SubRect (aRect, aResult, aResult);  }
  void Merge (const nsRegion& aRgn1, const nsRegion& aRgn2);
  void MoveInto (nsRegion& aDestRegion, const RgnRect* aStartRect);
  void MoveInto (nsRegion& aDestRegion)
  {    MoveInto (aDestRegion, mRectListHead.next);  }

  nsRegion (const nsRegion& aRegion);       // Prevent copying of regions
  nsRegion& operator = (const nsRegion& aRegion);
};



// Allow read-only access to region rectangles by iterating the list

class nsRegionRectIterator
{
  const nsRegion*  mRegion;
  const nsRegion::RgnRect* mCurPtr;

public:
  nsRegionRectIterator (const nsRegion& aRegion) 
  { 
    mRegion = &aRegion; 
    mCurPtr = &aRegion.mRectListHead; 
  }

  const nsRectFast* Next () 
  { 
    mCurPtr = mCurPtr->next; 
    return (mCurPtr != &mRegion->mRectListHead) ? mCurPtr : nsnull;
  }

  const nsRectFast* Prev ()
  { 
    mCurPtr = mCurPtr->prev; 
    return (mCurPtr != &mRegion->mRectListHead) ? mCurPtr : nsnull;
  }

  void Reset () 
  { 
    mCurPtr = &mRegion->mRectListHead; 
  }
};


#endif
