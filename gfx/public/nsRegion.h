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
 * Dainis Jonitis, <Dainis_Jonitis@swh-t.lv>.
 * Portions created by the Initial Developer are Copyright (C) 2001
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


#ifndef nsRegion_h__
#define nsRegion_h__


// Implementation of region.
// Region is represented as circular double-linked list of nsRegion::RgnRect structures.
// Rectangles in this list do not overlap and are sorted by (y, x) coordinates.

#include "nsRect.h"
#include "nsPoint.h"

class NS_GFX nsRegion
{
  friend class nsRegionRectIterator;
  friend class RgnRectMemoryAllocator;


// Special version of nsRect structure for speed optimizations in nsRegion code.
// Most important functions could be made inline and be sure that passed rectangles
// will always be non-empty.
// 
// Do not add any new member variables to this structure! 
// Otherwise it will break casts from nsRect to nsRectFast, which expect data parts to be identical.
  struct nsRectFast : public nsRect
  {
    nsRectFast () {}      // No need to call parent constructor to set default values
    nsRectFast (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) : nsRect (aX, aY, aWidth, aHeight) {}
    nsRectFast (const nsRect& aRect) : nsRect (aRect) {}

    // Override nsRect methods to make them inline. Do not check for emptiness.
    inline PRBool Contains (const nsRect& aRect) const;
    inline PRBool Intersects (const nsRect& aRect) const;
    inline PRBool IntersectRect (const nsRect& aRect1, const nsRect& aRect2);
    inline void UnionRect (const nsRect& aRect1, const nsRect& aRect2);
  };


  struct RgnRect : public nsRectFast
  {
    RgnRect* prev;
    RgnRect* next;

    RgnRect () {}                           // No need to call parent constructor to set default values
    RgnRect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) : nsRectFast (aX, aY, aWidth, aHeight) {}
    RgnRect (const nsRectFast& aRect) : nsRectFast (aRect) {}

    inline void* operator new (size_t) CPP_THROW_NEW;
    inline void  operator delete (void* aRect, size_t);

    RgnRect& operator = (const RgnRect& aRect)      // Do not overwrite prev/next pointers
    {
      x = aRect.x;
      y = aRect.y;
      width = aRect.width;
      height = aRect.height;
      return *this;
    }
  };


public:
  nsRegion () { Init (); }
  nsRegion (const nsRect& aRect) { Init (); Copy (aRect); }
  nsRegion (const nsRegion& aRegion) { Init (); Copy (aRegion); }
 ~nsRegion () { SetToElements (0); }
  nsRegion& operator = (const nsRect& aRect) { Copy (aRect); return *this; }
  nsRegion& operator = (const nsRegion& aRegion) { Copy (aRegion); return *this; }


  nsRegion& And  (const nsRegion& aRgn1,   const nsRegion& aRgn2);
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

  nsRegion& Or   (const nsRegion& aRgn1,   const nsRegion& aRgn2);
  nsRegion& Or   (const nsRegion& aRegion, const nsRect& aRect);
  nsRegion& Or   (const nsRect& aRect, const nsRegion& aRegion)
  {
    return  Or   (aRegion, aRect);
  }
  nsRegion& Or   (const nsRect& aRect1, const nsRect& aRect2)
  {
    Copy (aRect1);
    return Or (*this, aRect2);
  }

  nsRegion& Xor  (const nsRegion& aRgn1,   const nsRegion& aRgn2);
  nsRegion& Xor  (const nsRegion& aRegion, const nsRect& aRect);
  nsRegion& Xor  (const nsRect& aRect, const nsRegion& aRegion)
  {
    return  Xor  (aRegion, aRect);
  }
  nsRegion& Xor  (const nsRect& aRect1, const nsRect& aRect2)
  {
    Copy (aRect1);
    return Xor (*this, aRect2);
  }

  nsRegion& Sub  (const nsRegion& aRgn1,   const nsRegion& aRgn2);
  nsRegion& Sub  (const nsRegion& aRegion, const nsRect& aRect);
  nsRegion& Sub  (const nsRect& aRect, const nsRegion& aRegion)
  {
    return Sub (nsRegion (aRect), aRegion);
  }
  nsRegion& Sub  (const nsRect& aRect1, const nsRect& aRect2)
  {
    Copy (aRect1);
    return Sub (*this, aRect2);
  }


  void MoveBy (PRInt32 aXOffset, PRInt32 aYOffset)
  {
    MoveBy (nsPoint (aXOffset, aYOffset));
  }
  void MoveBy (nsPoint aPt);
  void SetEmpty ()
  {
    SetToElements (0);
    mBoundRect.SetRect (0, 0, 0, 0);
  }

  PRBool IsEmpty () const { return mRectCount == 0; }
  PRBool IsComplex () const { return mRectCount > 1; }
  PRBool IsEqual (const nsRegion& aRegion) const;
  PRUint32 GetNumRects () const { return mRectCount; }
  const nsRect& GetBounds () const { return mBoundRect; }

  /**
   * Make sure the region has at most aMaxRects by adding area to it
   * if necessary. The simplified region will be a superset of the
   * original region. The simplified region's bounding box will be
   * the same as for the current region.
   */
  void SimplifyOutward (PRUint32 aMaxRects);
  /**
   * Make sure the region has at most aMaxRects by removing area from
   * it if necessary. The simplified region will be a subset of the
   * original region.
   */
  void SimplifyInward (PRUint32 aMaxRects);

private:
  PRUint32    mRectCount;
  RgnRect*    mCurRect;
  RgnRect     mRectListHead;
  nsRectFast  mBoundRect;

  void Init ();
  nsRegion& Copy (const nsRegion& aRegion);
  nsRegion& Copy (const nsRect& aRect);
  void InsertBefore (RgnRect* aNewRect, RgnRect* aRelativeRect);
  void InsertAfter (RgnRect* aNewRect, RgnRect* aRelativeRect);
  void SetToElements (PRUint32 aCount);
  RgnRect* Remove (RgnRect* aRect);
  void InsertInPlace (RgnRect* aRect, PRBool aOptimizeOnFly = PR_FALSE);
  inline void SaveLinkChain ();
  inline void RestoreLinkChain ();
  void Optimize ();
  void SubRegion (const nsRegion& aRegion, nsRegion& aResult) const;
  void SubRect (const nsRectFast& aRect, nsRegion& aResult, nsRegion& aCompleted) const;
  void SubRect (const nsRectFast& aRect, nsRegion& aResult) const
  {    SubRect (aRect, aResult, aResult);  }
  void Merge (const nsRegion& aRgn1, const nsRegion& aRgn2);
  void MoveInto (nsRegion& aDestRegion, const RgnRect* aStartRect);
  void MoveInto (nsRegion& aDestRegion)
  {    MoveInto (aDestRegion, mRectListHead.next);  }
};



// Allow read-only access to region rectangles by iterating the list

class NS_GFX nsRegionRectIterator
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
