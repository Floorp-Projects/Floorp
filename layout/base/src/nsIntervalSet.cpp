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
 *   L. David Baron <dbaron@fas.harvard.edu> (original author)
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

#include "nsIntervalSet.h"
#include <new.h>

nsIntervalSet::nsIntervalSet(IntervalSetAlloc aAlloc, IntervalSetFree aFree,
                             void* aAllocatorClosure)
    : mList(nsnull),
      mAlloc(aAlloc),
      mFree(aFree),
      mAllocatorClosure(aAllocatorClosure)
{
    NS_ASSERTION(mAlloc && mFree, "null callback params");
}

nsIntervalSet::~nsIntervalSet()
{
    Interval *current = mList;
    while (current) {
        Interval *trash = current;
        current = current->mNext;
        FreeInterval(trash);
    }
}

void nsIntervalSet::FreeInterval(nsIntervalSet::Interval *aInterval)
{
    NS_ASSERTION(aInterval, "null interval");

    aInterval->Interval::~Interval();
    (*mFree)(sizeof(Interval), aInterval, mAllocatorClosure);
}

void nsIntervalSet::IncludeInterval(coord_type aBegin, coord_type aEnd)
{
    Interval *newInterval = NS_STATIC_CAST(Interval*,
                               (*mAlloc)(sizeof(Interval), mAllocatorClosure));
    if (!newInterval) {
        NS_NOTREACHED("allocation failure");
        return;
    }
    new(newInterval) Interval(aBegin, aEnd);

    Interval **current = &mList;
    while (*current && (*current)->mEnd < aBegin)
        current = &(*current)->mNext;

    newInterval->mNext = *current;
    *current = newInterval;

    Interval *subsumed = newInterval->mNext;
    while (subsumed && subsumed->mBegin <= aEnd) {
        newInterval->mEnd = PR_MAX(newInterval->mEnd, subsumed->mEnd);
        newInterval->mNext = subsumed->mNext;
        FreeInterval(subsumed);
        subsumed = newInterval->mNext;
    }
}

PRBool nsIntervalSet::HasPoint(coord_type aPoint) const
{
    Interval *current = mList;
    while (current && current->mBegin <= aPoint) {
        if (current->mEnd >= aPoint)
            return PR_TRUE;
        current = current->mNext;
    }
    return PR_FALSE;
}

PRBool nsIntervalSet::Intersects(coord_type aBegin, coord_type aEnd) const
{
    Interval *current = mList;
    while (current && current->mBegin <= aEnd) {
        if (current->mEnd >= aBegin)
            return PR_TRUE;
        current = current->mNext;
    }
    return PR_FALSE;
}

PRBool nsIntervalSet::Contains(coord_type aBegin, coord_type aEnd) const
{
    Interval *current = mList;
    while (current && current->mBegin <= aBegin) {
        if (current->mEnd >= aEnd)
            return PR_TRUE;
        current = current->mNext;
    }
    return PR_FALSE;
}
