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


class nsRegion
{
  friend class nsRegionRectIterator;
  friend class RgnRectMemoryAllocator;

  struct RgnRect : public nsRect
  {
    RgnRect* prev;
    RgnRect* next;

    RgnRect () {}                           // No need to call parent constructor to set default values
    RgnRect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) : nsRect (aX, aY, aWidth, aHeight) {}
    RgnRect (const nsRect& aRect) : nsRect (aRect) {}

#if 1   // Override nsRect methods to make them inline. These are very important to nsRegion
    #ifdef MIN
    #undef MIN
    #endif

    #ifdef MAX
    #undef MAX
    #endif

    #define MIN(a,b)\
      ((a) < (b) ? (a) : (b))
    #define MAX(a,b)\
      ((a) > (b) ? (a) : (b))

    PRBool Contains (const nsRect &aRect) const
    {
      return (PRBool) ((aRect.x >= x) && (aRect.y >= y) &&
                       (aRect.XMost() <= XMost()) && (aRect.YMost() <= YMost()));
    }

    PRBool Intersects (const nsRect &aRect) const
    {
      return (PRBool) ((x < aRect.XMost()) && (y < aRect.YMost()) &&
                       (aRect.x < XMost()) && (aRect.y < YMost()));
    }

    PRBool IntersectRect (const nsRect &aRect1, const nsRect &aRect2)
    {
      x = MAX (aRect1.x, aRect2.x);

      // Compute the destination width
      const nscoord tmpX = MIN (aRect1.XMost(), aRect2.XMost());
      if (tmpX <= x)
        return PR_FALSE;

      y = MAX (aRect1.y, aRect2.y);

      // Compute the destination height
      const nscoord tmpY = MIN (aRect1.YMost(), aRect2.YMost());
      if (tmpY <= y)
        return PR_FALSE;

      width  = tmpX - x;
      height = tmpY - y;

      return PR_TRUE;
    }

    void UnionRect (const nsRect &aRect1, const nsRect &aRect2)
    {
      x = MIN (aRect1.x, aRect2.x);
      y = MIN (aRect1.y, aRect2.y);
      width  = MAX (aRect1.XMost(), aRect2.XMost()) - x;
      height = MAX (aRect1.YMost(), aRect2.YMost()) - y;
    }
#endif

    void* operator new (size_t);
    void  operator delete (void* aRect, size_t);
    operator = (const RgnRect& aRect)       // Do not overwrite prev/next pointers
    { 
       x = aRect.x;
       y = aRect.y;
       width  = aRect.width;
       height = aRect.height;
    }
  };


public:
  nsRegion ();
  ~nsRegion () { SetToElements (0); }

  nsRegion& Copy (const nsRegion& aRegion);
  nsRegion& Copy (const nsRect& aRect);

  nsRegion& And  (const nsRegion& aRgn1, const nsRegion& aRgn2);
  nsRegion& And  (const nsRegion& aRegion, const nsRect& aRect);
  nsRegion& And  (const nsRect& aRect, const nsRegion& aRegion)
  {
    return  And  (aRegion, aRect);
  }
  nsRegion& And  (const nsRect& aRect1, const nsRect& aRect2)
  {
    nsRect TmpRect;
    TmpRect.IntersectRect (aRect1, aRect2);
    return Copy (TmpRect);
  }
    
  nsRegion& Or   (const nsRegion& aRgn1, const nsRegion& aRgn2);
  nsRegion& Or   (const nsRegion& aRegion, const nsRect& aRect);
  nsRegion& Or   (const nsRect& aRect, const nsRegion& aRegion)
  {  
    return  Or   (aRegion, aRect);
  }
  nsRegion& Or   (const nsRect& aRect1, const nsRect& aRect2)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect1);
    return Or (TmpRegion, aRect2);
  }

  nsRegion& Xor  (const nsRegion& aRgn1, const nsRegion& aRgn2);
  nsRegion& Xor  (const nsRegion& aRegion, const nsRect& aRect);
  nsRegion& Xor  (const nsRect& aRect, const nsRegion& aRegion)
  {
    return  Xor  (aRegion, aRect);
  }
  nsRegion& Xor  (const nsRect& aRect1, const nsRect& aRect2)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect1);
    return Xor (TmpRegion, aRect2);
  }

  nsRegion& Sub  (const nsRegion& aRgn1, const nsRegion& aRgn2);
  nsRegion& Sub  (const nsRegion& aRegion, const nsRect& aRect);
  nsRegion& Sub  (const nsRect& aRect, const nsRegion& aRegion)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect);
    return Sub (TmpRegion, aRegion);
  }
  nsRegion& Sub  (const nsRect& aRect1, const nsRect& aRect2)
  {
    nsRegion TmpRegion;
    TmpRegion.Copy (aRect1);
    return Sub (TmpRegion, aRect2);
  }


  PRBool GetBoundRect (nsRect* aBound) const
  {
    *aBound = mBoundRect;
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
  nsRect      mBoundRect;

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
  void Optimize ();
  void SubRectFromRegion (const nsRegion& aRegion, const nsRect& aRect);
  void SubRegionFromRegion (const nsRegion& aRgn1, const nsRegion& aRgn2);
  void Merge (const nsRegion& aRgn1, const nsRegion& aRgn2);
  
  nsRegion (const nsRegion& aRegion);       // Prevent copying of regions
  operator = (const nsRegion& aRegion);
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

  const nsRect* Next () 
  { 
    mCurPtr = mCurPtr->next; 
    return (mCurPtr != &mRegion->mRectListHead) ? mCurPtr : nsnull;
  }

  const nsRect* Prev ()
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

