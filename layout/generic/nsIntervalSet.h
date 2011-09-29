/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
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
 * The Original Code is nsIntervalSet. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by the Initial Developer are Copyright
 * (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* a set of ranges on a number-line */

#ifndef nsIntervalSet_h___
#define nsIntervalSet_h___

#include "prtypes.h"
#include "nsCoord.h"
#include "nsDebug.h"

typedef void *
(* IntervalSetAlloc)(size_t aBytes, void *aClosure);

typedef void
(* IntervalSetFree) (size_t aBytes, void *aPtr, void *aClosure);

/*
 * A list-based class (hopefully tree-based when I get around to it)
 * for representing a set of ranges on a number-line.
 */
class nsIntervalSet {

public:

    typedef nscoord coord_type;

    nsIntervalSet(IntervalSetAlloc aAlloc, IntervalSetFree aFree,
                  void* aAllocatorClosure);
    ~nsIntervalSet();

    /*
     * Include the interval [aBegin, aEnd] in the set.
     *
     * Removal of intervals added is not supported because that would
     * require keeping track of the individual intervals that were
     * added (nsIntervalMap should do that).  It would be simple to
     * implement ExcludeInterval if anyone wants it, though.
     */
    void IncludeInterval(coord_type aBegin, coord_type aEnd);

    /*
     * Are _some_ points in [aBegin, aEnd] contained within the set
     * of intervals?
     */
    bool Intersects(coord_type aBegin, coord_type aEnd) const;

    /*
     * Are _all_ points in [aBegin, aEnd] contained within the set
     * of intervals?
     */
    bool Contains(coord_type aBegin, coord_type aEnd) const;

    bool IsEmpty() const
    {
        return !mList;
    }

private:

    class Interval {

    public:
        Interval(coord_type aBegin, coord_type aEnd)
            : mBegin(aBegin),
              mEnd(aEnd),
              mPrev(nsnull),
              mNext(nsnull)
        {
        }

        coord_type mBegin;
        coord_type mEnd;
        Interval *mPrev;
        Interval *mNext;
    };

    void FreeInterval(Interval *aInterval);

    Interval           *mList;
    IntervalSetAlloc    mAlloc;
    IntervalSetFree     mFree;
    void               *mAllocatorClosure;
        
};

#endif // !defined(nsIntervalSet_h___)
