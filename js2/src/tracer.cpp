
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include "systemtypes.h"
#include "tracer.h"


// for each owner, we maintain a simple linked list of
// the pointers that have been allocated

struct PointerData {
    PointerData(void *p, size_t s, PointerData *n)
        : mPointer(p), mSize(s), mNext(n) { }
    void *mPointer;
    size_t mSize;
    PointerData *mNext;
};

struct AllocData {
    AllocData(char *owner)
    {
        mOwner = (char *)STD::malloc(strlen(owner) + 1);
        strcpy(mOwner, owner);
        mData = NULL;
        mTotalAllocatedCount = 0;
        mTotalReleasedCount = 0;
        mTotalAllocatedSize = 0;
        mTotalReleasedSize = 0;
    }
    uint32 mTotalAllocatedCount;
    uint32 mTotalReleasedCount;
    uint32 mTotalAllocatedSize;
    uint32 mTotalReleasedSize;
    char *mOwner;
    PointerData *mData;
};

AllocData **gAllocations = NULL;
uint32 gAllocationsCount = 0;

static AllocData *findOwner(char *owner)
{
    for (uint32 i = 0; i < gAllocationsCount; i++) {
        if (STD::strcmp(gAllocations[i]->mOwner, owner) == 0)
            return gAllocations[i];
    }
    gAllocations = (AllocData **)STD::realloc(gAllocations, (gAllocationsCount + 1) * sizeof(AllocData));
    gAllocations[gAllocationsCount] = new AllocData(owner);
    return gAllocations[gAllocationsCount++];
}


void trace_alloc(char *owner, size_t s, void *p)
{
    AllocData *ad = findOwner(owner);
    ad->mData = new PointerData(p, s, ad->mData);
    ad->mTotalAllocatedCount++;
    ad->mTotalAllocatedSize += s;
    // ?? check for duplicate or overlapping allocation ??
}

void trace_release(char *owner, void *p)
{
    AllocData *ad = findOwner(owner);
    PointerData *pd = ad->mData;
    PointerData **back = &ad->mData;
    ad->mTotalReleasedCount++;
    while (pd) {
        if (pd->mPointer == p) {
            *back = pd->mNext;
            ad->mTotalReleasedSize += pd->mSize;
            delete pd;
            return;
        }
        back = &pd->mNext;
        pd = pd->mNext;
    }
    NOT_REACHED("released pointer not found");
}

void trace_dump(JavaScript::Formatter& f)
{
    for (uint32 i = 0; i < gAllocationsCount; i++) {
        AllocData *ad = gAllocations[i];
        f << "For '" << ad->mOwner << "':" << "\n";
        f << "\t" << "Total Allocated Count = " << ad->mTotalAllocatedCount << "\n";
        f << "\t" << "Total Allocated Size  = " << ad->mTotalAllocatedSize << "\n";
        f << "\t" << "Total Released Count  = " << ad->mTotalReleasedCount << "\n";
        f << "\t" << "Total Released Size   = " << ad->mTotalReleasedSize << "\n";
/*
    if we could get stack traces, it might be fun to dump those here
    from each un-released chunk.

        PointerData *pd = ad->mData;
        while (pd) {
            f << "Unreleased data, allocated from " << pd->stackTrace << "\n";
            pd = pd->mNext;
        }
*/
        f << "\n"; 
    }

}


#include "formatter.h"

namespace JavaScript {
namespace Shell {

void do_dikdik(JavaScript::Formatter &f)
{

    
f << "   \\       /                          \n";
f << "     \\ __/     _______                \n";
f << "     /   \\    /       \\               \n";
f << "   / +   + \\/           \\             \n";
f << "  |         |             \\------*    \n";
f << "   \\       /               |          \n";
f << "     \\   /\\               /           \n";
f << "      | |   \\           /             \n";
f << "      \\_/     \\  ___  /||             \n";
f << "               ||      ||             \n";
f << "               ||      ||             \n";
f << "               ||      ||             \n";
f << "               ||                     \n";
f << "\n";

}


}
}

