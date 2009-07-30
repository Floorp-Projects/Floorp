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
 * The Original Code is Mozilla Communicator client code.
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

/* class for maintaining a linked list of child frames */

#ifndef nsFrameList_h___
#define nsFrameList_h___

#include "nscore.h"
#include "nsTraceRefcnt.h"
#include <stdio.h> /* for FILE* */
#include "nsDebug.h"

class nsIFrame;

/**
 * A class for managing a list of frames.
 */
class nsFrameList {
public:
  nsFrameList() :
    mFirstChild(nsnull)
  {
    MOZ_COUNT_CTOR(nsFrameList);
  }

  // XXX We should make this explicit when we can!
  nsFrameList(nsIFrame* aHead) :
    mFirstChild(aHead)
  {
    MOZ_COUNT_CTOR(nsFrameList);
#ifdef DEBUG
    CheckForLoops();
#endif
  }

  nsFrameList(const nsFrameList& aOther) :
    mFirstChild(aOther.mFirstChild)
  {
    MOZ_COUNT_CTOR(nsFrameList);
  }

  ~nsFrameList() {
    MOZ_COUNT_DTOR(nsFrameList);
    // Don't destroy our frames here, so that we can have temporary nsFrameLists
  }

  void DestroyFrames();

  // Delete this and destroy all its frames
  void Destroy();

  void SetFrames(nsIFrame* aFrameList) {
    mFirstChild = aFrameList;
#ifdef DEBUG
    CheckForLoops();
#endif
  }

  void Clear() { SetFrames(nsnull); }

  void SetFrames(nsFrameList& aFrameList) {
    NS_PRECONDITION(!mFirstChild, "Losing frames");
    mFirstChild = aFrameList.FirstChild();
    aFrameList.Clear();
  }

  class Slice;

  /**
   * Appends frames from aFrameList to this list. If aParent
   * is not null, reparents the newly-added frames.
   */
  void AppendFrames(nsIFrame* aParent, nsIFrame* aFrameList) {
    InsertFrames(aParent, LastChild(), aFrameList);
  }

  /**
   * Appends aFrameList to this list.  If aParent is not null,
   * reparents the newly added frames.  Clears out aFrameList and
   * returns a list slice represening the newly-appended frames.
   */
  Slice AppendFrames(nsIFrame* aParent, nsFrameList& aFrameList) {
    NS_PRECONDITION(!aFrameList.IsEmpty(), "Unexpected empty list");
    nsIFrame* firstNewFrame = aFrameList.FirstChild();
    AppendFrames(aParent, firstNewFrame);
    aFrameList.Clear();
    return Slice(*this, firstNewFrame, nsnull);
  }

  /* This is implemented in nsIFrame.h because it needs to know about
     nsIFrame. */
  inline void AppendFrame(nsIFrame* aParent, nsIFrame* aFrame);

  /**
   * Take aFrame out of the frame list. This also disconnects aFrame
   * from the sibling list. This will return PR_FALSE if aFrame is
   * nsnull or if aFrame is not in the list. The second frame is
   * a hint for the prev-sibling of aFrame; if the hint is correct,
   * then this is O(1) time. If successfully removed, the child's
   * NextSibling pointer is cleared.
   */
  PRBool RemoveFrame(nsIFrame* aFrame, nsIFrame* aPrevSiblingHint = nsnull);

  /**
   * Remove the first child from the list. The caller is assumed to be
   * holding a reference to the first child. This call is equivalent
   * in behavior to calling RemoveFrame(FirstChild()). If successfully
   * removed the first child's NextSibling pointer is cleared.
   */
  PRBool RemoveFirstChild();

  /**
   * Take aFrame out of the frame list and then destroy it. This also
   * disconnects aFrame from the sibling list. This will return
   * PR_FALSE if aFrame is nsnull or if aFrame is not in the list. The
   * second frame is a hint for the prev-sibling of aFrame; if the
   * hint is correct, then the time this method takes doesn't depend
   * on the number of previous siblings of aFrame.
   */
  PRBool DestroyFrame(nsIFrame* aFrame, nsIFrame* aPrevSiblingHint = nsnull);

  /**
   * Inserts aNewFrame right after aPrevSibling, or prepends to
   * list if aPrevSibling is null. If aParent is not null, also
   * reparents newly-added frame. Note that this method always
   * sets the frame's nextSibling pointer.
   * This is implemented in nsIFrame.h because it needs to know about nsIFrame.
   */
  inline void InsertFrame(nsIFrame* aParent, nsIFrame* aPrevSibling,
                          nsIFrame* aNewFrame);

  /**
   * Inserts aFrameList right after aPrevSibling, or prepends to
   * list if aPrevSibling is null. If aParent is not null, also
   * reparents newly-added frame.
   */
  void InsertFrames(nsIFrame* aParent,
                    nsIFrame* aPrevSibling,
                    nsIFrame* aFrameList);

  /**
   * Inserts aFrameList into this list after aPrevSibling (at the beginning if
   * aPrevSibling is null).  If aParent is not null, reparents the newly added
   * frames.  Clears out aFrameList and returns a list slice representing the
   * newly-inserted frames.
   *
   * This is implemented in nsIFrame.h because it needs to know about nsIFrame.
   */
  inline Slice InsertFrames(nsIFrame* aParent, nsIFrame* aPrevSibling,
                            nsFrameList& aFrameList);

  class FrameLinkEnumerator;

  /* Split this frame list such that all the frames before the link pointed to
   * by aLink end up in the returned list, while the remaining frames stay in
   * this list.  After this call, aLink points to the beginning of this list.
   */
  nsFrameList ExtractHead(FrameLinkEnumerator& aLink);

  /* Split this frame list such that all the frames coming after the link
   * pointed to by aLink end up in the returned list, while the frames before
   * that link stay in this list.  After this call, aLink is at end.
   */
  nsFrameList ExtractTail(FrameLinkEnumerator& aLink);

  /**
   * Sort the frames according to content order so that the first
   * frame in the list is the first in content order. Frames for
   * the same content will be ordered so that a prev in flow
   * comes before its next in flow.
   */
  void SortByContentOrder();

  nsIFrame* FirstChild() const {
    return mFirstChild;
  }

  nsIFrame* LastChild() const;

  nsIFrame* FrameAt(PRInt32 aIndex) const;
  PRInt32 IndexOf(nsIFrame* aFrame) const;

  PRBool IsEmpty() const {
    return nsnull == mFirstChild;
  }

  PRBool NotEmpty() const {
    return nsnull != mFirstChild;
  }

  PRBool ContainsFrame(const nsIFrame* aFrame) const;
  PRBool ContainsFrameBefore(const nsIFrame* aFrame, const nsIFrame* aEnd) const;

  PRInt32 GetLength() const;

  nsIFrame* GetPrevSiblingFor(nsIFrame* aFrame) const;

  /**
   * If this frame list has only one frame, return that frame.
   * Otherwise, return null.
   */
  nsIFrame* OnlyChild() const {
    if (FirstChild() == LastChild()) {
      return FirstChild();
    }
    return nsnull;
  }

#ifdef IBMBIDI
  /**
   * Return the frame before this frame in visual order (after Bidi reordering).
   * If aFrame is null, return the last frame in visual order.
   */
  nsIFrame* GetPrevVisualFor(nsIFrame* aFrame) const;

  /**
   * Return the frame after this frame in visual order (after Bidi reordering).
   * If aFrame is null, return the first frame in visual order.
   */
  nsIFrame* GetNextVisualFor(nsIFrame* aFrame) const;
#endif // IBMBIDI

#ifdef DEBUG
  void List(FILE* out) const;
#endif

  static nsresult Init();
  static void Shutdown() { delete sEmptyList; }
  static const nsFrameList& EmptyList() { return *sEmptyList; }

  class Enumerator;

  /**
   * A class representing a slice of a frame list.
   */
  class Slice {
    friend class Enumerator;

  public:
    // Implicit on purpose, so that we can easily create enumerators from
    // nsFrameList via this impicit constructor.
    Slice(const nsFrameList& aList) :
#ifdef DEBUG
      mList(aList),
#endif
      mStart(aList.FirstChild()),
      mEnd(nsnull)
    {}

    Slice(const nsFrameList& aList, nsIFrame* aStart, nsIFrame* aEnd) :
#ifdef DEBUG
      mList(aList),
#endif
      mStart(aStart),
      mEnd(aEnd)
    {}

    Slice(const Slice& aOther) :
#ifdef DEBUG
      mList(aOther.mList),
#endif
      mStart(aOther.mStart),
      mEnd(aOther.mEnd)
    {}

  private:
#ifdef DEBUG
    const nsFrameList& mList;
#endif
    nsIFrame* const mStart; // our starting frame
    const nsIFrame* const mEnd; // The first frame that is NOT in the slice.
                                // May be null.
  };

  class Enumerator {
  public:
    Enumerator(const Slice& aSlice) :
#ifdef DEBUG
      mSlice(aSlice),
#endif
      mFrame(aSlice.mStart),
      mEnd(aSlice.mEnd)
    {}

    Enumerator(const Enumerator& aOther) :
#ifdef DEBUG
      mSlice(aOther.mSlice),
#endif
      mFrame(aOther.mFrame),
      mEnd(aOther.mEnd)
    {}

    PRBool AtEnd() const {
      // Can't just check mEnd, because some table code goes and destroys the
      // tail of the frame list (including mEnd!) while iterating over the
      // frame list.
      return !mFrame || mFrame == mEnd;
    }

    /* Next() needs to know about nsIFrame, and nsIFrame will need to
       know about nsFrameList methods, so in order to inline this put
       the implementation in nsIFrame.h */
    inline void Next();

    /**
     * Get the current frame we're pointing to.  Do not call this on an
     * iterator that is at end!
     */
    nsIFrame* get() const {
      NS_PRECONDITION(!AtEnd(), "Enumerator is at end");
      return mFrame;
    }

    /**
     * Get an enumerator that is just like this one, but not limited in terms of
     * the part of the list it will traverse.
     */
    Enumerator GetUnlimitedEnumerator() const {
      return Enumerator(*this, nsnull);
    }

#ifdef DEBUG
    const nsFrameList& List() const { return mSlice.mList; }
#endif

  protected:
    Enumerator(const Enumerator& aOther, const nsIFrame* const aNewEnd):
#ifdef DEBUG
      mSlice(aOther.mSlice),
#endif
      mFrame(aOther.mFrame),
      mEnd(aNewEnd)
    {}

#ifdef DEBUG
    /* Has to be an object, not a reference, since the slice could
       well be a temporary constructed from an nsFrameList */
    const Slice mSlice;
#endif
    nsIFrame* mFrame; // our current frame.
    const nsIFrame* const mEnd; // The first frame we should NOT enumerate.
                                // May be null.
  };

  /**
   * A class that can be used to enumerate links between frames.  When created
   * from an nsFrameList, it points to the "link" immediately before the first
   * frame.  It can then be advanced until it points to the "link" immediately
   * after the last frame.  At any position, PrevFrame() and NextFrame() are
   * the frames before and after the given link.  This means PrevFrame() is
   * null when the enumerator is at the beginning of the list and NextFrame()
   * is null when it's AtEnd().
   */
  class FrameLinkEnumerator : private Enumerator {
  public:
    friend class nsFrameList;

    FrameLinkEnumerator(const nsFrameList& aList) :
      Enumerator(aList),
      mPrev(nsnull)
    {}

    FrameLinkEnumerator(const FrameLinkEnumerator& aOther) :
      Enumerator(aOther),
      mPrev(aOther.mPrev)
    {}

    void operator=(const FrameLinkEnumerator& aOther) {
      NS_PRECONDITION(&List() == &aOther.List(), "Different lists?");
      mFrame = aOther.mFrame;
      mPrev = aOther.mPrev;
    }

    void Next() {
      mPrev = mFrame;
      Enumerator::Next();
    }

    PRBool AtEnd() const { return Enumerator::AtEnd(); }

    nsIFrame* PrevFrame() const { return mPrev; }
    nsIFrame* NextFrame() const { return mFrame; }

  protected:
    nsIFrame* mPrev;
  };

private:
#ifdef DEBUG
  void CheckForLoops();
#endif

  static const nsFrameList* sEmptyList;

protected:
  nsIFrame* mFirstChild;
};

#endif /* nsFrameList_h___ */
