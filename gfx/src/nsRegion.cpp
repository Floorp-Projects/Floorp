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

#include "prlock.h"
#include "nsRegion.h"
#include "nsISupportsImpl.h"


#define MIN_INT32 (-PR_INT32 (0x7FFFFFFF) - 1)
#define MAX_INT32 (PR_INT32 (0x7FFFFFFF))


// Fast inline analogues of nsRect methods for nsRegion::nsRectFast.
// Check for emptiness is not required - it is guaranteed by caller.

inline PRBool nsRegion::nsRectFast::Contains (const nsRect& aRect) const
{
  return (PRBool) ((aRect.x >= x) && (aRect.y >= y) &&
                   (aRect.XMost () <= XMost ()) && (aRect.YMost () <= YMost ()));
}

inline PRBool nsRegion::nsRectFast::Intersects (const nsRect& aRect) const
{
  return (PRBool) ((x < aRect.XMost ()) && (y < aRect.YMost ()) &&
                   (aRect.x < XMost ()) && (aRect.y < YMost ()));
}

inline PRBool nsRegion::nsRectFast::IntersectRect (const nsRect& aRect1, const nsRect& aRect2)
{
  const nscoord xmost = PR_MIN (aRect1.XMost (), aRect2.XMost ());
  x = PR_MAX (aRect1.x, aRect2.x);
  width = xmost - x;
  if (width <= 0) return PR_FALSE;

  const nscoord ymost = PR_MIN (aRect1.YMost (), aRect2.YMost ());
  y = PR_MAX (aRect1.y, aRect2.y);
  height = ymost - y;
  if (height <= 0) return PR_FALSE;

  return PR_TRUE;
}

inline void nsRegion::nsRectFast::UnionRect (const nsRect& aRect1, const nsRect& aRect2)
{
  const nscoord xmost = PR_MAX (aRect1.XMost (), aRect2.XMost ());
  const nscoord ymost = PR_MAX (aRect1.YMost (), aRect2.YMost ());
  x = PR_MIN (aRect1.x, aRect2.x);
  y = PR_MIN (aRect1.y, aRect2.y);
  width  = xmost - x;
  height = ymost - y;
}



// Custom memory allocator for nsRegion::RgnRect structures.
// Entries are allocated from global memory pool.
// Memory pool can grow in size, but it can't shrink.

#define INIT_MEM_CHUNK_ENTRIES 100
#define INCR_MEM_CHUNK_ENTRIES 100

class RgnRectMemoryAllocator
{
  nsRegion::RgnRect*  mFreeListHead;
  PRUint32  mFreeEntries;
  void*     mChunkListHead;
#if 0
  PRLock*   mLock;

  void InitLock ()    { mLock = PR_NewLock (); }
  void DestroyLock () { PR_DestroyLock (mLock); }
  void Lock ()        { PR_Lock   (mLock); }
  void Unlock ()      { PR_Unlock (mLock); }
#elif defined (DEBUG)
  NS_DECL_OWNINGTHREAD

  void InitLock ()    { NS_ASSERT_OWNINGTHREAD (RgnRectMemoryAllocator); }
  void DestroyLock () { NS_ASSERT_OWNINGTHREAD (RgnRectMemoryAllocator); }
  void Lock ()        { NS_ASSERT_OWNINGTHREAD (RgnRectMemoryAllocator); }
  void Unlock ()      { NS_ASSERT_OWNINGTHREAD (RgnRectMemoryAllocator); }
#else
  void InitLock ()    { }
  void DestroyLock () { }
  void Lock ()        { }
  void Unlock ()      { }
#endif

  void* AllocChunk (PRUint32 aEntries, void* aNextChunk, nsRegion::RgnRect* aTailDest)
  {
    PRUint8* pBuf = new PRUint8 [aEntries * sizeof (nsRegion::RgnRect) + sizeof (void*)];
    *NS_REINTERPRET_CAST (void**, pBuf) = aNextChunk;
    nsRegion::RgnRect* pRect = NS_REINTERPRET_CAST (nsRegion::RgnRect*, pBuf + sizeof (void*));

    for (PRUint32 cnt = 0 ; cnt < aEntries - 1 ; cnt++)
      pRect [cnt].next = &pRect [cnt + 1];

    pRect [aEntries - 1].next = aTailDest;

    return pBuf;
  }

  void FreeChunk (void* aChunk) {  delete [] (PRUint8 *) aChunk;  }
  void* NextChunk (void* aThisChunk) const { return *NS_STATIC_CAST (void**, aThisChunk); }

  nsRegion::RgnRect* ChunkHead (void* aThisChunk) const
  {   return NS_REINTERPRET_CAST (nsRegion::RgnRect*, NS_STATIC_CAST (PRUint8*, aThisChunk) + sizeof (void*));  }

public:
  RgnRectMemoryAllocator (PRUint32 aNumOfEntries);
 ~RgnRectMemoryAllocator ();

  nsRegion::RgnRect* Alloc ();
  void Free (nsRegion::RgnRect* aRect);
};


RgnRectMemoryAllocator::RgnRectMemoryAllocator (PRUint32 aNumOfEntries)
{
  InitLock ();
  mChunkListHead = AllocChunk (aNumOfEntries, nsnull, nsnull);
  mFreeEntries   = aNumOfEntries;
  mFreeListHead  = ChunkHead (mChunkListHead);
}

RgnRectMemoryAllocator::~RgnRectMemoryAllocator ()
{
  while (mChunkListHead)
  {
    void* tmp = mChunkListHead;
    mChunkListHead = NextChunk (mChunkListHead);
    FreeChunk (tmp);
  }

#if 0
  /*
   * As a static object this class outlives any library which would implement
   * locking. So we intentionally leak the 'lock'.
   *
   * Currently RgnRectMemoryAllocator is only used from the primary thread,
   * so we aren't using a lock which means that there is no lock to leak.
   * If we ever switch to multiple GUI threads (e.g. one per window),
   * we'd probably use one allocator per window-thread to avoid the
   * locking overhead and just require consumers not to pass regions
   * across threads/windows, which would be a reasonable restriction
   * because they wouldn't be useful outside their window.
   */
  DestroyLock ();
#endif
}

nsRegion::RgnRect* RgnRectMemoryAllocator::Alloc ()
{
  Lock ();

  if (mFreeEntries == 0)
  {
    mChunkListHead = AllocChunk (INCR_MEM_CHUNK_ENTRIES, mChunkListHead, mFreeListHead);
    mFreeEntries   = INCR_MEM_CHUNK_ENTRIES;
    mFreeListHead  = ChunkHead (mChunkListHead);
  }

  nsRegion::RgnRect* tmp = mFreeListHead;
  mFreeListHead = mFreeListHead->next;
  mFreeEntries--;
  Unlock ();

  return tmp;
}

void RgnRectMemoryAllocator::Free (nsRegion::RgnRect* aRect)
{
  Lock ();
  mFreeEntries++;
  aRect->next = mFreeListHead;
  mFreeListHead = aRect;
  Unlock ();
}


// Global pool for nsRegion::RgnRect allocation
static RgnRectMemoryAllocator gRectPool (INIT_MEM_CHUNK_ENTRIES);


inline void* nsRegion::RgnRect::operator new (size_t) CPP_THROW_NEW
{
  return gRectPool.Alloc ();
}

inline void nsRegion::RgnRect::operator delete (void* aRect, size_t)
{
  gRectPool.Free (NS_STATIC_CAST (RgnRect*, aRect));
}



void nsRegion::Init()
{
  mRectListHead.prev = mRectListHead.next = &mRectListHead;
  mCurRect = &mRectListHead;
  mRectCount = 0;
  mBoundRect.SetRect (0, 0, 0, 0);
}

inline void nsRegion::InsertBefore (RgnRect* aNewRect, RgnRect* aRelativeRect)
{
  aNewRect->prev = aRelativeRect->prev;
  aNewRect->next = aRelativeRect;
  aRelativeRect->prev->next = aNewRect;
  aRelativeRect->prev = aNewRect;
  mCurRect = aNewRect;
  mRectCount++;
}

inline void nsRegion::InsertAfter (RgnRect* aNewRect, RgnRect* aRelativeRect)
{
  aNewRect->prev = aRelativeRect;
  aNewRect->next = aRelativeRect->next;
  aRelativeRect->next->prev = aNewRect;
  aRelativeRect->next = aNewRect;
  mCurRect = aNewRect;
  mRectCount++;
}


// Adjust the number of rectangles in region.
// Content of rectangles should be changed by caller.

void nsRegion::SetToElements (PRUint32 aCount)
{
  if (mRectCount < aCount)        // Add missing rectangles
  {
    PRUint32 InsertCount = aCount - mRectCount;
    mRectCount = aCount;
    RgnRect* pPrev = &mRectListHead;
    RgnRect* pNext = mRectListHead.next;

    while (InsertCount--)
    {
      mCurRect = new RgnRect;
      mCurRect->prev = pPrev;
      pPrev->next = mCurRect;
      pPrev = mCurRect;
    }

    pPrev->next = pNext;
    pNext->prev = pPrev;
  } else
  if (mRectCount > aCount)        // Remove unnecessary rectangles
  {
    PRUint32 RemoveCount = mRectCount - aCount;
    mRectCount = aCount;
    mCurRect = mRectListHead.next;

    while (RemoveCount--)
    {
      RgnRect* tmp = mCurRect;
      mCurRect = mCurRect->next;
      delete tmp;
    }

    mRectListHead.next = mCurRect;
    mCurRect->prev = &mRectListHead;
  }
}


// Save the entire chain of linked elements in 'prev' field of the RgnRect structure.
// After that forward-only iterations using 'next' field could still be used.
// Some elements from forward-only chain could be temporarily removed to optimize inner loops.
// The original double linked state could be restored by call to RestoreLinkChain ().
// Both functions despite size can be inline because they are called only from one function.

inline void nsRegion::SaveLinkChain ()
{
  RgnRect* pRect = &mRectListHead;

  do
  {
    pRect->prev = pRect->next;
    pRect = pRect->next;
  } while (pRect != &mRectListHead);
}


inline void nsRegion::RestoreLinkChain ()
{
  RgnRect* pPrev = &mRectListHead;
  RgnRect* pRect = mRectListHead.next = mRectListHead.prev;

  while (pRect != &mRectListHead)
  {
    pRect->next = pRect->prev;
    pRect->prev = pPrev;
    pPrev = pRect;
    pRect = pRect->next;
  }

  mRectListHead.prev = pPrev;
}


// Insert node in right place of sorted list
// If necessary then bounding rectangle could be updated and rectangle combined
// with neighbour rectangles. This is usually done in Optimize ()

void nsRegion::InsertInPlace (RgnRect* aRect, PRBool aOptimizeOnFly)
{
  if (mRectCount == 0)
    InsertAfter (aRect, &mRectListHead);
  else
  {
    if (aRect->y > mCurRect->y)
    {
      mRectListHead.y = MAX_INT32;

      while (aRect->y > mCurRect->next->y)
        mCurRect = mCurRect->next;

      while (aRect->y == mCurRect->next->y && aRect->x > mCurRect->next->x)
        mCurRect = mCurRect->next;

      InsertAfter (aRect, mCurRect);
    } else
    if (aRect->y < mCurRect->y)
    {
      mRectListHead.y = MIN_INT32;

      while (aRect->y < mCurRect->prev->y)
        mCurRect = mCurRect->prev;

      while (aRect->y == mCurRect->prev->y && aRect->x < mCurRect->prev->x)
        mCurRect = mCurRect->prev;

      InsertBefore (aRect, mCurRect);
    } else
    {
      if (aRect->x > mCurRect->x)
      {
        mRectListHead.y = MAX_INT32;

        while (aRect->y == mCurRect->next->y && aRect->x > mCurRect->next->x)
          mCurRect = mCurRect->next;

        InsertAfter (aRect, mCurRect);
      } else
      {
        mRectListHead.y = MIN_INT32;

        while (aRect->y == mCurRect->prev->y && aRect->x < mCurRect->prev->x)
          mCurRect = mCurRect->prev;

        InsertBefore (aRect, mCurRect);
      }
    }
  }


  if (aOptimizeOnFly)
  {
    if (mRectCount == 1)
      mBoundRect = *mCurRect;
    else
    {
      mBoundRect.UnionRect (mBoundRect, *mCurRect);

      // Check if we can go left or up before starting to combine rectangles
      if ((mCurRect->y == mCurRect->prev->y && mCurRect->height == mCurRect->prev->height &&
           mCurRect->x == mCurRect->prev->XMost ()) ||
          (mCurRect->x == mCurRect->prev->x && mCurRect->width == mCurRect->prev->width &&
           mCurRect->y == mCurRect->prev->YMost ()) )
        mCurRect = mCurRect->prev;

      // Try to combine with rectangle on right side
      while (mCurRect->y == mCurRect->next->y && mCurRect->height == mCurRect->next->height &&
             mCurRect->XMost () == mCurRect->next->x)
      {
        mCurRect->width += mCurRect->next->width;
        delete Remove (mCurRect->next);
      }

      // Try to combine with rectangle under this one
      while (mCurRect->x == mCurRect->next->x && mCurRect->width == mCurRect->next->width &&
             mCurRect->YMost () == mCurRect->next->y)
      {
        mCurRect->height += mCurRect->next->height;
        delete Remove (mCurRect->next);
      }
    }
  }
}


nsRegion::RgnRect* nsRegion::Remove (RgnRect* aRect)
{
  aRect->prev->next = aRect->next;
  aRect->next->prev = aRect->prev;
  mRectCount--;

  if (mCurRect == aRect)
    mCurRect = (aRect->next != &mRectListHead) ? aRect->next : aRect->prev;

  return aRect;
}


// Try to reduce the number of rectangles in complex region by combining with
// surrounding ones on right and bottom sides of each rectangle in list.
// Update bounding rectangle

void nsRegion::Optimize ()
{
  if (mRectCount == 0)
    mBoundRect.SetRect (0, 0, 0, 0);
  else
  {
    RgnRect* pRect = mRectListHead.next;
    PRInt32 xmost = mRectListHead.prev->XMost ();
    PRInt32 ymost = mRectListHead.prev->YMost ();
    mBoundRect.x = mRectListHead.next->x;
    mBoundRect.y = mRectListHead.next->y;

    while (pRect != &mRectListHead)
    {
      // Try to combine with rectangle on right side
      while (pRect->y == pRect->next->y && pRect->height == pRect->next->height &&
             pRect->XMost () == pRect->next->x)
      {
        pRect->width += pRect->next->width;
        delete Remove (pRect->next);
      }

      // Try to combine with rectangle under this one
      while (pRect->x == pRect->next->x && pRect->width == pRect->next->width &&
             pRect->YMost () == pRect->next->y)
      {
        pRect->height += pRect->next->height;
        delete Remove (pRect->next);
      }

      // Determine bound rectangle. Use fact that rectangles are sorted.
      if (pRect->x < mBoundRect.x) mBoundRect.x = pRect->x;
      if (pRect->XMost () > xmost) xmost = pRect->XMost ();
      if (pRect->YMost () > ymost) ymost = pRect->YMost ();

      pRect = pRect->next;
    }

    mBoundRect.width  = xmost - mBoundRect.x;
    mBoundRect.height = ymost - mBoundRect.y;
  }
}


// Move rectangles starting from 'aStartRect' till end of the list to the destionation region.
// Important for temporary objects - instead of copying rectangles with Merge () and then
// emptying region in destructor they could be moved to destination region in one step.

void nsRegion::MoveInto (nsRegion& aDestRegion, const RgnRect* aStartRect)
{
  RgnRect* pRect = NS_CONST_CAST (RgnRect*, aStartRect);
  RgnRect* pPrev = pRect->prev;

  while (pRect != &mRectListHead)
  {
    RgnRect* next = pRect->next;
    aDestRegion.InsertInPlace (pRect);

    mRectCount--;
    pRect = next;
  }

  pPrev->next = &mRectListHead;
  mRectListHead.prev = pPrev;
  mCurRect = mRectListHead.next;
}


// Merge two non-overlapping regions into one.
// Automatically optimize region by calling Optimize ()

void nsRegion::Merge (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (aRgn1.mRectCount == 0)            // Region empty. Result is equal to other region
    Copy (aRgn2);
  else
  if (aRgn2.mRectCount == 0)            // Region empty. Result is equal to other region
    Copy (aRgn1);
  if (aRgn1.mRectCount == 1)            // Region is single rectangle. Optimize on fly
  {
    RgnRect* TmpRect = new RgnRect (*aRgn1.mRectListHead.next);
    Copy (aRgn2);
    InsertInPlace (TmpRect, PR_TRUE);
  } else
  if (aRgn2.mRectCount == 1)            // Region is single rectangle. Optimize on fly
  {
    RgnRect* TmpRect = new RgnRect (*aRgn2.mRectListHead.next);
    Copy (aRgn1);
    InsertInPlace (TmpRect, PR_TRUE);
  } else
  {
    const nsRegion* pCopyRegion, *pInsertRegion;

    // Determine which region contains more rectangles. Copy the larger one
    if (aRgn1.mRectCount >= aRgn2.mRectCount)
    {
      pCopyRegion = &aRgn1;
      pInsertRegion = &aRgn2;
    } else
    {
      pCopyRegion = &aRgn2;
      pInsertRegion = &aRgn1;
    }

    if (pInsertRegion == this)          // Do merge in-place
      pInsertRegion = pCopyRegion;
    else
      Copy (*pCopyRegion);

    const RgnRect* pSrcRect = pInsertRegion->mRectListHead.next;

    while (pSrcRect != &pInsertRegion->mRectListHead)
    {
      InsertInPlace (new RgnRect (*pSrcRect));

      pSrcRect = pSrcRect->next;
    }

    Optimize ();
  }
}


nsRegion& nsRegion::Copy (const nsRegion& aRegion)
{
  if (&aRegion == this)
    return *this;

  if (aRegion.mRectCount == 0)
    SetEmpty ();
  else
  {
    SetToElements (aRegion.mRectCount);

    const RgnRect* pSrc = aRegion.mRectListHead.next;
    RgnRect* pDest = mRectListHead.next;

    while (pSrc != &aRegion.mRectListHead)
    {
      *pDest = *pSrc;

      pSrc  = pSrc->next;
      pDest = pDest->next;
    }

    mCurRect = mRectListHead.next;
    mBoundRect = aRegion.mBoundRect;
  }

  return *this;
}


nsRegion& nsRegion::Copy (const nsRect& aRect)
{
  if (aRect.IsEmpty ())
    SetEmpty ();
  else
  {
    SetToElements (1);
    *mRectListHead.next = NS_STATIC_CAST (const RgnRect&, aRect);
    mBoundRect = NS_STATIC_CAST (const nsRectFast&, aRect);
  }

  return *this;
}


nsRegion& nsRegion::And (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (&aRgn1 == &aRgn2)                                       // And with self
    Copy (aRgn1);
  else
  if (aRgn1.mRectCount == 0 || aRgn2.mRectCount == 0)         // If either region is empty then result is empty
    SetEmpty ();
  else
  {
    nsRectFast TmpRect;

    if (aRgn1.mRectCount == 1 && aRgn2.mRectCount == 1)       // Intersect rectangle with rectangle
    {
      TmpRect.IntersectRect (*aRgn1.mRectListHead.next, *aRgn2.mRectListHead.next);
      Copy (TmpRect);
    } else
    {
      if (!aRgn1.mBoundRect.Intersects (aRgn2.mBoundRect))    // Regions do not intersect
        SetEmpty ();
      else
      {
        // Region is simple rectangle and it fully overlays other region
        if (aRgn1.mRectCount == 1 && aRgn1.mBoundRect.Contains (aRgn2.mBoundRect))
          Copy (aRgn2);
        else
        // Region is simple rectangle and it fully overlays other region
        if (aRgn2.mRectCount == 1 && aRgn2.mBoundRect.Contains (aRgn1.mBoundRect))
          Copy (aRgn1);
        else
        {
          nsRegion TmpRegion;
          nsRegion* pSrcRgn1 = NS_CONST_CAST (nsRegion*, &aRgn1);
          nsRegion* pSrcRgn2 = NS_CONST_CAST (nsRegion*, &aRgn2);

          if (&aRgn1 == this)     // Copy region if it is both source and result
          {
            TmpRegion.Copy (aRgn1);
            pSrcRgn1 = &TmpRegion;
          }

          if (&aRgn2 == this)     // Copy region if it is both source and result
          {
            TmpRegion.Copy (aRgn2);
            pSrcRgn2 = &TmpRegion;
          }

          // For outer loop prefer region for which at least one rectangle is below other's bound rectangle
          if (pSrcRgn2->mRectListHead.prev->y >= pSrcRgn1->mBoundRect.YMost ())
          {
            nsRegion* Tmp = pSrcRgn1;
            pSrcRgn1 = pSrcRgn2;
            pSrcRgn2 = Tmp;
          }


          SetToElements (0);
          pSrcRgn2->SaveLinkChain ();

          pSrcRgn1->mRectListHead.y = MAX_INT32;
          pSrcRgn2->mRectListHead.y = MAX_INT32;

          for (RgnRect* pSrcRect1 = pSrcRgn1->mRectListHead.next ;
               pSrcRect1->y < pSrcRgn2->mBoundRect.YMost () ; pSrcRect1 = pSrcRect1->next)
          {
            if (pSrcRect1->Intersects (pSrcRgn2->mBoundRect))   // Rectangle intersects region. Process each rectangle
            {
              RgnRect* pPrev2 = &pSrcRgn2->mRectListHead;

              for (RgnRect* pSrcRect2 = pSrcRgn2->mRectListHead.next ;
                   pSrcRect2->y < pSrcRect1->YMost () ; pSrcRect2 = pSrcRect2->next)
              {
                if (pSrcRect2->YMost () <= pSrcRect1->y)        // Rect2's bottom is above the top of Rect1.
                {                                               // No successive rectangles in Rgn1 can intersect it.
                  pPrev2->next = pSrcRect2->next;               // Remove Rect2 from Rgn2's checklist
                  continue;
                }

                if (pSrcRect1->Contains (*pSrcRect2))           // Rect1 fully overlays Rect2.
                {                                               // No any other rectangle in Rgn1 can intersect it.
                  pPrev2->next = pSrcRect2->next;               // Remove Rect2 from Rgn2's checklist
                  InsertInPlace (new RgnRect (*pSrcRect2));
                  continue;
                }


                if (TmpRect.IntersectRect (*pSrcRect1, *pSrcRect2))
                  InsertInPlace (new RgnRect (TmpRect));

                pPrev2 = pSrcRect2;
              }
            }
          }

          pSrcRgn2->RestoreLinkChain ();
          Optimize ();
        }
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::And (const nsRegion& aRegion, const nsRect& aRect)
{
  // If either region or rectangle is empty then result is empty
  if (aRegion.mRectCount == 0 || aRect.IsEmpty ())
    SetEmpty ();
  else                            // Intersect region with rectangle
  {
    const nsRectFast& aRectFast = NS_STATIC_CAST (const nsRectFast&, aRect);
    nsRectFast TmpRect;

    if (aRegion.mRectCount == 1)  // Intersect rectangle with rectangle
    {
      TmpRect.IntersectRect (*aRegion.mRectListHead.next, aRectFast);
      Copy (TmpRect);
    } else                        // Intersect complex region with rectangle
    {
      if (!aRectFast.Intersects (aRegion.mBoundRect))   // Rectangle does not intersect region
        SetEmpty ();
      else
      {
        if (aRectFast.Contains (aRegion.mBoundRect))    // Rectangle fully overlays region
          Copy (aRegion);
        else
        {
          nsRegion TmpRegion;
          nsRegion* pSrcRegion = NS_CONST_CAST (nsRegion*, &aRegion);

          if (&aRegion == this)   // Copy region if it is both source and result
          {
            TmpRegion.Copy (aRegion);
            pSrcRegion = &TmpRegion;
          }

          SetToElements (0);
          pSrcRegion->mRectListHead.y = MAX_INT32;

          for (const RgnRect* pSrcRect = pSrcRegion->mRectListHead.next ;
               pSrcRect->y < aRectFast.YMost () ; pSrcRect = pSrcRect->next)
          {
            if (TmpRect.IntersectRect (*pSrcRect, aRectFast))
              InsertInPlace (new RgnRect (TmpRect));
          }

          Optimize ();
        }
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::Or (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (&aRgn1 == &aRgn2)                 // Or with self
    Copy (aRgn1);
  else
  if (aRgn1.mRectCount == 0)            // Region empty. Result is equal to other region
    Copy (aRgn2);
  else
  if (aRgn2.mRectCount == 0)            // Region empty. Result is equal to other region
    Copy (aRgn1);
  else
  {
    if (!aRgn1.mBoundRect.Intersects (aRgn2.mBoundRect))  // Regions do not intersect
      Merge (aRgn1, aRgn2);
    else
    {
      // Region is simple rectangle and it fully overlays other region
      if (aRgn1.mRectCount == 1 && aRgn1.mBoundRect.Contains (aRgn2.mBoundRect))
        Copy (aRgn1);
      else
      // Region is simple rectangle and it fully overlays other region
      if (aRgn2.mRectCount == 1 && aRgn2.mBoundRect.Contains (aRgn1.mBoundRect))
        Copy (aRgn2);
      else
      {
        nsRegion TmpRegion;
        aRgn1.SubRegion (aRgn2, TmpRegion);               // Get only parts of region which not overlap the other region
        Copy (aRgn2);
        TmpRegion.MoveInto (*this);
        Optimize ();
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::Or (const nsRegion& aRegion, const nsRect& aRect)
{
  if (aRegion.mRectCount == 0)          // Region empty. Result is equal to rectangle
    Copy (aRect);
  else
  if (aRect.IsEmpty ())                 // Rectangle is empty. Result is equal to region
    Copy (aRegion);
  else
  {
    const nsRectFast& aRectFast = NS_STATIC_CAST (const nsRectFast&, aRect);

    if (!aRectFast.Intersects (aRegion.mBoundRect))     // Rectangle does not intersect region
    {
      Copy (aRegion);
      InsertInPlace (new RgnRect (aRectFast), PR_TRUE);
    } else
    {
      // Region is simple rectangle and it fully overlays rectangle
      if (aRegion.mRectCount == 1 && aRegion.mBoundRect.Contains (aRectFast))
        Copy (aRegion);
      else
      if (aRectFast.Contains (aRegion.mBoundRect))      // Rectangle fully overlays region
        Copy (aRectFast);
      else
      {
        aRegion.SubRect (aRectFast, *this);             // Exclude from region parts that overlap the rectangle
        InsertInPlace (new RgnRect (aRectFast));
        Optimize ();
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::Xor (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (&aRgn1 == &aRgn2)                 // Xor with self
    SetEmpty ();
  else
  if (aRgn1.mRectCount == 0)            // Region empty. Result is equal to other region
    Copy (aRgn2);
  else
  if (aRgn2.mRectCount == 0)            // Region empty. Result is equal to other region
    Copy (aRgn1);
  else
  {
    if (!aRgn1.mBoundRect.Intersects (aRgn2.mBoundRect))      // Regions do not intersect
      Merge (aRgn1, aRgn2);
    else
    {
      // Region is simple rectangle and it fully overlays other region
      if (aRgn1.mRectCount == 1 && aRgn1.mBoundRect.Contains (aRgn2.mBoundRect))
      {
        aRgn1.SubRegion (aRgn2, *this);
        Optimize ();
      } else
      // Region is simple rectangle and it fully overlays other region
      if (aRgn2.mRectCount == 1 && aRgn2.mBoundRect.Contains (aRgn1.mBoundRect))
      {
        aRgn2.SubRegion (aRgn1, *this);
        Optimize ();
      } else
      {
        nsRegion TmpRegion;
        aRgn1.SubRegion (aRgn2, TmpRegion);
        aRgn2.SubRegion (aRgn1, *this);
        TmpRegion.MoveInto (*this);
        Optimize ();
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::Xor (const nsRegion& aRegion, const nsRect& aRect)
{
  if (aRegion.mRectCount == 0)          // Region empty. Result is equal to rectangle
    Copy (aRect);
  else
  if (aRect.IsEmpty ())                 // Rectangle is empty. Result is equal to region
    Copy (aRegion);
  else
  {
    const nsRectFast& aRectFast = NS_STATIC_CAST (const nsRectFast&, aRect);

    if (!aRectFast.Intersects (aRegion.mBoundRect))     // Rectangle does not intersect region
    {
      Copy (aRegion);
      InsertInPlace (new RgnRect (aRectFast), PR_TRUE);
    } else
    {
      // Region is simple rectangle and it fully overlays rectangle
      if (aRegion.mRectCount == 1 && aRegion.mBoundRect.Contains (aRectFast))
      {
        aRegion.SubRect (aRectFast, *this);
        Optimize ();
      } else
      if (aRectFast.Contains (aRegion.mBoundRect))      // Rectangle fully overlays region
      {
        nsRegion TmpRegion;
        TmpRegion.Copy (aRectFast);
        TmpRegion.SubRegion (aRegion, *this);
        Optimize ();
      } else
      {
        nsRegion TmpRegion;
        TmpRegion.Copy (aRectFast);
        TmpRegion.SubRegion (aRegion, TmpRegion);
        aRegion.SubRect (aRectFast, *this);
        TmpRegion.MoveInto (*this);
        Optimize ();
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::Sub (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (&aRgn1 == &aRgn2)         // Sub from self
    SetEmpty ();
  else
  if (aRgn1.mRectCount == 0)    // If source is empty then result is empty, too
    SetEmpty ();
  else
  if (aRgn2.mRectCount == 0)    // Nothing to subtract
    Copy (aRgn1);
  else
  {
    if (!aRgn1.mBoundRect.Intersects (aRgn2.mBoundRect))   // Regions do not intersect
      Copy (aRgn1);
    else
    {
      aRgn1.SubRegion (aRgn2, *this);
      Optimize ();
    }
  }

  return *this;
}


nsRegion& nsRegion::Sub (const nsRegion& aRegion, const nsRect& aRect)
{
  if (aRegion.mRectCount == 0)    // If source is empty then result is empty, too
    SetEmpty ();
  else
  if (aRect.IsEmpty ())           // Nothing to subtract
    Copy (aRegion);
  else
  {
    const nsRectFast& aRectFast = NS_STATIC_CAST (const nsRectFast&, aRect);

    if (!aRectFast.Intersects (aRegion.mBoundRect))   // Rectangle does not intersect region
      Copy (aRegion);
    else
    {
      if (aRectFast.Contains (aRegion.mBoundRect))    // Rectangle fully overlays region
        SetEmpty ();
      else
      {
        aRegion.SubRect (aRectFast, *this);
        Optimize ();
      }
    }
  }

  return *this;
}


// Subtract region from current region.
// Both regions are non-empty and they intersect each other.
// Result could be empty region if aRgn2 is rectangle that fully overlays aRgn1.
// Optimize () is not called on exit (bound rectangle is not updated).

void nsRegion::SubRegion (const nsRegion& aRegion, nsRegion& aResult) const
{
  if (aRegion.mRectCount == 1)    // Subtract simple rectangle
  {
    if (aRegion.mBoundRect.Contains (mBoundRect))
      aResult.SetEmpty ();
    else
      SubRect (*aRegion.mRectListHead.next, aResult);
  } else
  {
    nsRegion TmpRegion, CompletedRegion;
    const nsRegion* pSubRgn = &aRegion;

    if (&aResult == &aRegion)     // Copy region if it is both source and result
    {
      TmpRegion.Copy (aRegion);
      pSubRgn = &TmpRegion;
    }

    const RgnRect* pSubRect = pSubRgn->mRectListHead.next;

    SubRect (*pSubRect, aResult, CompletedRegion);
    pSubRect = pSubRect->next;

    while (pSubRect != &pSubRgn->mRectListHead)
    {
      aResult.SubRect (*pSubRect, aResult, CompletedRegion);
      pSubRect = pSubRect->next;
    }

    CompletedRegion.MoveInto (aResult);
  }
}


// Subtract rectangle from current region.
// Both region and rectangle are non-empty and they intersect each other.
// Result could be empty region if aRect fully overlays aRegion.
// Could be called repeatedly with 'this' as input and result - bound rectangle is not known.
// Optimize () is not called on exit (bound rectangle is not updated).
//
// aCompleted is filled with rectangles which are already checked and could be safely
// removed from further examination in case aRect rectangles come from ordered list.
// aCompleted is not automatically emptied. aCompleted and aResult could be the same region.

void nsRegion::SubRect (const nsRectFast& aRect, nsRegion& aResult, nsRegion& aCompleted) const
{
  nsRegion TmpRegion;
  const nsRegion* pSrcRegion = this;

  if (&aResult == this)           // Copy region if it is both source and result
  {
    TmpRegion.Copy (*this);
    pSrcRegion = &TmpRegion;
  }

  aResult.SetToElements (0);

  (NS_CONST_CAST (nsRegion*, pSrcRegion))->mRectListHead.y = MAX_INT32;
  const RgnRect* pSrcRect = pSrcRegion->mRectListHead.next;

  for ( ; pSrcRect->y < aRect.YMost () ; pSrcRect = pSrcRect->next)
  {
    nsRectFast TmpRect;

    // If bottom of current rectangle is above the top of aRect then this rectangle
    // could be moved to aCompleted region. Successive aRect rectangles from ordered
    // list do not have to check this rectangle again.
    if (pSrcRect->YMost () <= aRect.y)
    {
      aCompleted.InsertInPlace (new RgnRect (*pSrcRect));
      continue;
    }

    if (!TmpRect.IntersectRect (*pSrcRect, aRect))
      aResult.InsertInPlace (new RgnRect (*pSrcRect));
    else
    {
      // Rectangle A. Subtract from this rectangle B
      const nscoord ax  = pSrcRect->x;
      const nscoord axm = pSrcRect->XMost ();
      const nscoord aw  = pSrcRect->width;
      const nscoord ay  = pSrcRect->y;
      const nscoord aym = pSrcRect->YMost ();
      const nscoord ah  = pSrcRect->height;
      // Rectangle B. Subtract this from rectangle A
      const nscoord bx  = aRect.x;
      const nscoord bxm = aRect.XMost ();
      const nscoord by  = aRect.y;
      const nscoord bym = aRect.YMost ();
      // Rectangle I. Area where rectangles A and B intersect
      const nscoord ix  = TmpRect.x;
      const nscoord ixm = TmpRect.XMost ();
      const nscoord iy  = TmpRect.y;
      const nscoord iym = TmpRect.YMost ();
      const nscoord ih  = TmpRect.height;

      // There are 16 combinations how rectangles could intersect

      if (bx <= ax && by <= ay)
      {
        if (bxm < axm && bym < aym)     // 1.
        {
          aResult.InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ih));
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
        } else
        if (bxm >= axm && bym < aym)    // 2.
        {
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
        } else
        if (bxm < axm && bym >= aym)    // 3.
        {
          aResult.InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ah));
        } else
        if (*pSrcRect == aRect)         // 4. subset
        {                               // Current rectangle is equal to aRect
          break;                        // No any other rectangle in region can intersect it
        }
      } else
      if (bx > ax && by <= ay)
      {
        if (bxm < axm && bym < aym)     // 5.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, ix - ax, ih));
          aResult.InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ih));
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
        } else
        if (bxm >= axm && bym < aym)    // 6.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, ix - ax, ih));
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
        } else
        if (bxm < axm && bym >= aym)    // 7.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, ix - ax, ah));
          aResult.InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ah));
        } else
        if (bxm >= axm && bym >= aym)   // 8.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, ix - ax, ah));
        }
      } else
      if (bx <= ax && by > ay)
      {
        if (bxm < axm && bym < aym)     // 9.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          aResult.InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
        } else
        if (bxm >= axm && bym < aym)    // 10.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
        } else
        if (bxm < axm && bym >= aym)    // 11.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          aResult.InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
        } else
        if (bxm >= axm && bym >= aym)   // 12.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
        }
      } else
      if (bx > ax && by > ay)
      {
        if (bxm < axm && bym < aym)     // 13.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          aResult.InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
          aResult.InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));

          // Current rectangle fully overlays aRect. No any other rectangle can intersect it.
          break;
        } else
        if (bxm >= axm && bym < aym)    // 14.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          aResult.InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
          aResult.InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
        } else
        if (bxm < axm && bym >= aym)    // 15.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          aResult.InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
          aResult.InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
        } else
        if (bxm >= axm && bym >= aym)   // 16.
        {
          aResult.InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          aResult.InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
        }
      }
    }
  }

  // Just copy remaining rectangles in region which are below aRect and can't intersect it.
  // If rectangles are in temporary region then they could be moved.
  if (pSrcRegion == &TmpRegion)
    TmpRegion.MoveInto (aResult, pSrcRect);
  else
  {
    while (pSrcRect != &pSrcRegion->mRectListHead)
    {
      aResult.InsertInPlace (new RgnRect (*pSrcRect));
      pSrcRect = pSrcRect->next;
    }
  }
}


PRBool nsRegion::IsEqual (const nsRegion& aRegion) const
{
  if (mRectCount == 0)
    return (aRegion.mRectCount == 0) ? PR_TRUE : PR_FALSE;

  if (aRegion.mRectCount == 0)
    return (mRectCount == 0) ? PR_TRUE : PR_FALSE;

  if (mRectCount == 1 && aRegion.mRectCount == 1) // Both regions are simple rectangles
    return (*mRectListHead.next == *aRegion.mRectListHead.next);
  else                                            // At least one is complex region.
  {
    if (mBoundRect != aRegion.mBoundRect)         // If regions are equal then bounding rectangles should match
      return PR_FALSE;
    else
    {
      nsRegion TmpRegion;
      TmpRegion.Xor (*this, aRegion);             // Get difference between two regions

      return (TmpRegion.mRectCount == 0);
    }
  }
}


void nsRegion::MoveBy (nsPoint aPt)
{
  if (aPt.x || aPt.y)
  {
    RgnRect* pRect = mRectListHead.next;

    while (pRect != &mRectListHead)
    {
      pRect->MoveBy (aPt.x, aPt.y);
      pRect = pRect->next;
    }

    mBoundRect.MoveBy (aPt.x, aPt.y);
  }
}

void nsRegion::SimplifyOutward (PRUint32 aMaxRects)
{
  NS_ASSERTION(aMaxRects >= 1, "Invalid max rect count");
  
  if (mRectCount <= aMaxRects)
    return;

  *this = GetBounds();
}

void nsRegion::SimplifyInward (PRUint32 aMaxRects)
{
  NS_ASSERTION(aMaxRects >= 1, "Invalid max rect count");

  if (mRectCount <= aMaxRects)
    return;

  SetEmpty();
}
