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

#include "prlock.h"
#include "nsRegion.h"


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
  PRLock*   mLock;


  void Lock ()   { PR_Lock   (mLock); }
  void Unlock () { PR_Unlock (mLock); }

  void* AllocChunk (PRUint32 aEntries, void* aNextChunk, nsRegion::RgnRect* aTailDest)
  {
    PRUint8* pBuf = new PRUint8 [aEntries * sizeof (nsRegion::RgnRect) + sizeof (void*)];
    *NS_STATIC_CAST (void**, pBuf) = aNextChunk;
    nsRegion::RgnRect* pRect = NS_STATIC_CAST (nsRegion::RgnRect*, pBuf + sizeof (void*));

    for (PRUint32 cnt = 0 ; cnt < aEntries - 1 ; cnt++)
      pRect [cnt].next = &pRect [cnt + 1];

    pRect [aEntries - 1].next = aTailDest;

    return pBuf;
  }

  void FreeChunk (void* aChunk) {  delete [] aChunk;  }
  void* NextChunk (const void* aThisChunk) const { return *NS_STATIC_CAST (void**, aThisChunk); }

  nsRegion::RgnRect* ChunkHead (const void* aThisChunk) const
  {   return NS_STATIC_CAST (nsRegion::RgnRect*, NS_STATIC_CAST (PRUint8*, aThisChunk) + sizeof (void*));  }

public:
  RgnRectMemoryAllocator (PRUint32 aNumOfEntries);
 ~RgnRectMemoryAllocator ();

  inline nsRegion::RgnRect* Alloc ();
  inline void Free (nsRegion::RgnRect* aRect);
};


RgnRectMemoryAllocator::RgnRectMemoryAllocator (PRUint32 aNumOfEntries)
{
  mLock = PR_NewLock ();
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

  PR_DestroyLock (mLock);
}

inline nsRegion::RgnRect* RgnRectMemoryAllocator::Alloc ()
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

inline void RgnRectMemoryAllocator::Free (nsRegion::RgnRect* aRect)
{
  Lock ();
  mFreeEntries++;
  aRect->next = mFreeListHead;
  mFreeListHead = aRect;
  Unlock ();
}


// Global pool for nsRegion::RgnRect allocation
static RgnRectMemoryAllocator gRectPool (INIT_MEM_CHUNK_ENTRIES);


void* nsRegion::RgnRect::operator new (size_t) 
{ 
  return gRectPool.Alloc ();
}

void nsRegion::RgnRect::operator delete (void* aRect, size_t) 
{ 
  gRectPool.Free (NS_STATIC_CAST (RgnRect*, aRect)); 
}




nsRegion::nsRegion ()
{
  mRectListHead.prev = mRectListHead.next = &mRectListHead;
  mRectListHead.width = mRectListHead.height = -100;    // This is dummy marker node
  mCurRect = &mRectListHead;
  mRectCount = 0;
  mBoundRect.SetRect (0, 0, 0, 0);
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
      mRectListHead.y = 2147483647;

      while (aRect->y > mCurRect->next->y)
        mCurRect = mCurRect->next;

      while (aRect->y == mCurRect->next->y && aRect->x > mCurRect->next->x)
        mCurRect = mCurRect->next;

      InsertAfter (aRect, mCurRect);
    } else
    if (aRect->y < mCurRect->y)
    {
      mRectListHead.y = -2147483647;

      while (aRect->y < mCurRect->prev->y)
        mCurRect = mCurRect->prev;

      while (aRect->y == mCurRect->prev->y && aRect->x < mCurRect->prev->x)
        mCurRect = mCurRect->prev;

      InsertBefore (aRect, mCurRect);
    } else
    {
      if (aRect->x > mCurRect->x)
      {
        mRectListHead.y = 2147483647;

        while (aRect->y == mCurRect->next->y && aRect->x > mCurRect->next->x)
          mCurRect = mCurRect->next;

        InsertAfter (aRect, mCurRect);
      } else
      {
        mRectListHead.y = -2147483647;

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
  mBoundRect.SetRect (0, 0, 0, 0);
  RgnRect* pRect = mRectListHead.next;

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

    mBoundRect.UnionRect (mBoundRect, *pRect);
    pRect = pRect->next;
  }
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
    Empty ();
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
    Empty ();
  else
  {
    SetToElements (1);
    *mRectListHead.next = aRect;
    mBoundRect = aRect;
  }

  return *this;
}


nsRegion& nsRegion::And (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (&aRgn1 == &aRgn2)                                       // And with self
    Copy (aRgn1);
  else
  if (aRgn1.mRectCount == 0 || aRgn2.mRectCount == 0)         // If either region is empty then result is empty
    Empty ();
  else
  {
    nsRect TmpRect;

    if (aRgn1.mRectCount == 1 && aRgn2.mRectCount == 1)       // Intersect rectangle with rectangle
    {
      TmpRect.IntersectRect (*aRgn1.mRectListHead.next, *aRgn2.mRectListHead.next);
      Copy (TmpRect);
    } else
    {
      if (!aRgn1.mBoundRect.Intersects (aRgn2.mBoundRect))    // Regions do not intersect
        Empty ();
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
          const nsRegion* pSrcRgn1 = &aRgn1;
          const nsRegion* pSrcRgn2 = &aRgn2;

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
          
          SetToElements (0);
          const RgnRect* pSrcRect1 = pSrcRgn1->mRectListHead.next;

          while (pSrcRect1 != &pSrcRgn1->mRectListHead)
          {
            if (pSrcRect1->Intersects (pSrcRgn2->mBoundRect))   // Rectangle intersects region. Process each rectangle
            {
              const RgnRect* pSrcRect2 = pSrcRgn2->mRectListHead.next;
          
              while (pSrcRect2 != &pSrcRgn2->mRectListHead)
              {
                // Vertically sorted rectangles in SrcRgn2 are below the SrcRect1 - won't intersect
                if (pSrcRect2->y >= pSrcRect1->YMost ()) 
                  break;

                if (TmpRect.IntersectRect (*pSrcRect1, *pSrcRect2))
                  InsertInPlace (new RgnRect (TmpRect));

                pSrcRect2 = pSrcRect2->next;
              }
            }        

            pSrcRect1 = pSrcRect1->next;
          }

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
    Empty ();
  else                            // Intersect region with rectangle
  {
    nsRect TmpRect;

    if (aRegion.mRectCount == 1)  // Intersect rectangle with rectangle
    {
      TmpRect.IntersectRect (*aRegion.mRectListHead.next, aRect);
      Copy (TmpRect);
    } else                        // Intersect complex region with rectangle
    {
      if (!aRect.Intersects (aRegion.mBoundRect))  // Rectangle does not intersect region
        Empty ();
      else
      {
        if (aRect.Contains (aRegion.mBoundRect))   // Rectangle fully overlays region
          Copy (aRegion);
        else
        {
          nsRegion TmpRegion;
          const nsRegion* pSrcRegion = &aRegion;

          if (&aRegion == this)   // Copy region if it is both source and result
          {
            TmpRegion.Copy (aRegion);
            pSrcRegion = &TmpRegion;
          }

          SetToElements (0);
          const RgnRect* pSrcRect = pSrcRegion->mRectListHead.next;

          while (pSrcRect != &pSrcRegion->mRectListHead)
          {
            // Vertically sorted rectangles in SrcRegion are below the aRect - won't intersect
            if (pSrcRect->y >= aRect.YMost ()) 
              break;

            if (TmpRect.IntersectRect (*pSrcRect, aRect))
              InsertInPlace (new RgnRect (TmpRect));

            pSrcRect = pSrcRect->next;
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
        TmpRegion.SubRegionFromRegion (aRgn1, aRgn2);     // Get only parts of region which not overlap the other region
        Merge (TmpRegion, aRgn2);
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
    if (!aRect.Intersects (aRegion.mBoundRect))   // Rectangle does not intersect region
    {
      Copy (aRegion);
      InsertInPlace (new RgnRect (aRect), PR_TRUE);
    } else
    {
      // Region is simple rectangle and it fully overlays rectangle
      if (aRegion.mRectCount == 1 && aRegion.mBoundRect.Contains (aRect))
        Copy (aRegion);
      else
      if (aRect.Contains (aRegion.mBoundRect))    // Rectangle fully overlays region
        Copy (aRect);
      else
      {
        SubRectFromRegion (aRegion, aRect);       // Exclude from region parts that overlap the rectangle
        InsertInPlace (new RgnRect (aRect));
        Optimize ();
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::Xor (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (&aRgn1 == &aRgn2)                 // Xor with self
    Empty ();
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
        SubRegionFromRegion (aRgn1, aRgn2);
        Optimize ();
      } else
      // Region is simple rectangle and it fully overlays other region
      if (aRgn2.mRectCount == 1 && aRgn2.mBoundRect.Contains (aRgn1.mBoundRect))
      {
        SubRegionFromRegion (aRgn2, aRgn1);
        Optimize ();
      } else
      {
        nsRegion TmpRegion1, TmpRegion2;
        TmpRegion2.And (aRgn1, aRgn2);                        // Overlay
        TmpRegion1.SubRegionFromRegion (aRgn1, TmpRegion2);   // aRgn1 - Overlay
        TmpRegion2.SubRegionFromRegion (aRgn2, TmpRegion2);   // aRgn2 - Overlay
        Merge (TmpRegion1, TmpRegion2);
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
    if (!aRect.Intersects (aRegion.mBoundRect))     // Rectangle does not intersect region
    {
      Copy (aRegion);
      InsertInPlace (new RgnRect (aRect), PR_TRUE);
    } else
    {
      // Region is simple rectangle and it fully overlays rectangle
      if (aRegion.mRectCount == 1 && aRegion.mBoundRect.Contains (aRect))
      {
        SubRectFromRegion (aRegion, aRect);
        Optimize ();
      } else
      if (aRect.Contains (aRegion.mBoundRect))      // Rectangle fully overlays region
      {
        nsRegion TmpRegion;
        TmpRegion.Copy (aRect);
        SubRegionFromRegion (TmpRegion, aRegion);
        Optimize ();
      } else
      {
        nsRegion TmpRegion1, TmpRegion2, TmpRegion3;
        TmpRegion3.Copy (aRect);
        TmpRegion2.And (aRegion, aRect);                          // Overlay
        TmpRegion1.SubRegionFromRegion (aRegion, TmpRegion2);     // aRegion - Overlay
        TmpRegion2.SubRegionFromRegion (TmpRegion3, TmpRegion2);  // aRect - Overlay
        Merge (TmpRegion1, TmpRegion2);
      }
    }
  }

  return *this;
}


nsRegion& nsRegion::Sub (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (&aRgn1 == &aRgn2)         // Sub from self
    Empty ();
  else
  if (aRgn1.mRectCount == 0)    // If source is empty then result is empty, too
    Empty ();
  else
  if (aRgn2.mRectCount == 0)    // Nothing to subtract
    Copy (aRgn1);
  else
  {
    if (!aRgn1.mBoundRect.Intersects (aRgn2.mBoundRect))   // Regions do not intersect
      Copy (aRgn1);
    else
    {
      SubRegionFromRegion (aRgn1, aRgn2);
      Optimize ();
    }
  }

  return *this;
}


nsRegion& nsRegion::Sub (const nsRegion& aRegion, const nsRect& aRect)
{
  if (aRegion.mRectCount == 0)    // If source is empty then result is empty, too
    Empty ();
  else
  if (aRect.IsEmpty ())           // Nothing to subtract
    Copy (aRegion);
  else
  {
    if (!aRect.Intersects (aRegion.mBoundRect))  // Rectangle does not intersect region
      Copy (aRegion);
    else
    {
      if (aRect.Contains (aRegion.mBoundRect))   // Rectangle fully overlays region
        Empty ();
      else
      {
        SubRectFromRegion (aRegion, aRect);
        Optimize ();
      }
    }
  }

  return *this;
}


// Subtract one region from another. 
// Both regions are non-empty and they intersect each other.
// Result could be empty if aRgn2 is rectangle that fully overlays aRgn1.
// Optimize () is not called (bound rectangle is not updated).

void nsRegion::SubRegionFromRegion (const nsRegion& aRgn1, const nsRegion& aRgn2)
{
  if (aRgn2.mRectCount == 1)      // Subtract simple rectangle
    SubRectFromRegion (aRgn1, *aRgn2.mRectListHead.next);
  else
  {
    nsRegion TmpRegion;
    const nsRegion* pSrcRgn2 = &aRgn2;

    if (&aRgn2 == this)           // Copy region if it is both source and result
    {
      TmpRegion.Copy (aRgn2);
      pSrcRgn2 = &TmpRegion;
    }

    const RgnRect* pRect = pSrcRgn2->mRectListHead.next;

    SubRectFromRegion (aRgn1, *pRect);
    pRect = pRect->next;

    while (pRect != &pSrcRgn2->mRectListHead)
    {
      SubRectFromRegion (*this, *pRect);
      pRect = pRect->next;
    }
  }
}


// Subtract rectangle from region. 
// Both region and rectangle are non-empty and they intersect each other.
// Result could be empty if aRect fully overlays aRegion.
// Optimize () is not called (bound rectangle is not updated).

void nsRegion::SubRectFromRegion (const nsRegion& aRegion, const nsRect& aRect)
{
  if (aRect.Contains (aRegion.mBoundRect))
    Empty ();
  else
  {
    nsRegion TmpRegion;
    const nsRegion* pSrcRegion = &aRegion;

    if (&aRegion == this)         // Copy region if it is both source and result
    {
      TmpRegion.Copy (aRegion);
      pSrcRegion = &TmpRegion;
    }
      
    SetToElements (0);
    const RgnRect* pSrcRect = pSrcRegion->mRectListHead.next;

    while (pSrcRect != &pSrcRegion->mRectListHead)
    {
      nsRect TmpRect;

      if (!TmpRect.IntersectRect (*pSrcRect, aRect))
        InsertInPlace (new RgnRect (*pSrcRect));
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
        const nscoord bw  = aRect.width;
        const nscoord by  = aRect.y;
        const nscoord bym = aRect.YMost ();
        const nscoord bh  = aRect.height;
        // Rectangle I. Area where rectangles A and B intersect
        const nscoord ix  = TmpRect.x;
        const nscoord ixm = TmpRect.XMost ();
        const nscoord iw  = TmpRect.width;
        const nscoord iy  = TmpRect.y;
        const nscoord iym = TmpRect.YMost ();
        const nscoord ih  = TmpRect.height;

        // There are 16 combinations how rectangles could intersect

        if (bx <= ax && by <= ay)
        {
          if (bxm < axm && bym < aym)     // 1.
          {
            InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ih));
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm >= axm && bym < aym)    // 2.
          {
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm < axm && bym >= aym)    // 3.
          {
            InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ah));
          }
        } else
        if (bx > ax && by <= ay)
        {
          if (bxm < axm && bym < aym)     // 5.
          {
            InsertInPlace (new RgnRect (ax, ay, ix - ax, ih));
            InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ih));
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm >= axm && bym < aym)    // 6.
          {
            InsertInPlace (new RgnRect (ax, ay, ix - ax, ih));
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm < axm && bym >= aym)    // 7.
          {
            InsertInPlace (new RgnRect (ax, ay, ix - ax, ah));
            InsertInPlace (new RgnRect (ixm, ay, axm - ixm, ah));
          } else
          if (bxm >= axm && bym >= aym)   // 8.
          {
            InsertInPlace (new RgnRect (ax, ay, ix - ax, ah));
          }
        } else
        if (bx <= ax && by > ay)
        {
          if (bxm < axm && bym < aym)     // 9.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
            InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm >= axm && bym < aym)    // 10.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm < axm && bym >= aym)    // 11.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
            InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
          } else
          if (bxm >= axm && bym >= aym)   // 12.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
          }
        } else
        if (bx > ax && by > ay)
        {
          if (bxm < axm && bym < aym)     // 13.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
            InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
            InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm >= axm && bym < aym)    // 14.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
            InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
            InsertInPlace (new RgnRect (ax, iym, aw, aym - iym));
          } else
          if (bxm < axm && bym >= aym)    // 15.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
            InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
            InsertInPlace (new RgnRect (ixm, iy, axm - ixm, ih));
          } else
          if (bxm >= axm && bym >= aym)   // 16.
          {
            InsertInPlace (new RgnRect (ax, ay, aw, iy - ay));
            InsertInPlace (new RgnRect (ax, iy, ix - ax, ih));
          }
        }
      }

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


void nsRegion::Offset (PRInt32 aXOffset, PRInt32 aYOffset)
{
  if (aXOffset || aYOffset)
  {
    RgnRect* pRect = mRectListHead.next;

    while (pRect != &mRectListHead)
    {
      pRect->MoveBy (aXOffset, aYOffset);
      pRect = pRect->next;
    }

    mBoundRect.MoveBy (aXOffset, aYOffset);
  }
}
