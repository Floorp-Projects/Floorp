/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the JavaScript 2 Prototype.
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "hash.h"
#include <new>

namespace JS = JavaScript;


//
// Hash Codes
//


// General-purpose null-terminated C string hash function
JS::HashNumber JS::hashString(const char *s)
{
    HashNumber h = 0;
    uchar ch;
    
    while ((ch = (uchar)*s++) != 0)
        h = (h >> 28) ^ (h << 4) ^ ch;
    return h;
}

// General-purpose String hash function
JS::HashNumber JS::hashString(const String &s)
{
    HashNumber h = 0;
    String::const_iterator p = s.begin();
    String::size_type n = s.size();
    
    if (n < 16)
        // Hash every character in a short string.
        while (n--)
            h = (h >> 28) ^ (h << 4) ^ *p++;
    else
        // Sample a la java.lang.String.hash().
        for (String::size_type m = n / 8; n >= m; p += m, n -= m)
            h = (h >> 28) ^ (h << 4) ^ *p;
    return h;
}


//
// Hash Tables
//

const uint minLgNBuckets = 4;

JS::GenericHashTableIterator::GenericHashTableIterator(GenericHashTable &ht):
        ht(ht), entry(0), nextBucket(ht.buckets)
{
    DEBUG_ONLY(++ht.nReferences);
    operator++();
}



JS::GenericHashTableIterator &JS::GenericHashTableIterator::operator++()
{
    GenericHashEntry *e = entry;

    if (e) {
        backpointer = &e->next;
        e = e->next;
    }
    if (!e) {
        GenericHashEntry **const bucketsEnd = ht.bucketsEnd;
        GenericHashEntry **bucket = nextBucket;

        while (bucket != bucketsEnd) {
            e = *bucket++;
            if (e) {
                backpointer = bucket-1;
                break;
            }
        }
        nextBucket = bucket;
    }
    entry = e;
    return *this;
}


JS::GenericHashTable::GenericHashTable(uint32 nEntriesDefault):
        nEntries(0)
{
    DEBUG_ONLY(nReferences = 0);

    uint lgNBuckets = ceilingLog2(nEntriesDefault);
    if (lgNBuckets < minLgNBuckets)
        lgNBuckets = minLgNBuckets;
    defaultLgNBuckets = lgNBuckets;

    recomputeMinMaxNEntries(lgNBuckets);
    uint32 nBuckets = JS_BIT(lgNBuckets);
    buckets = new GenericHashEntry*[nBuckets];
    // No exceptions after this point unless buckets is deleted.

    bucketsEnd = buckets + nBuckets;
    zero(buckets, bucketsEnd);
}


// Initialize shift, minNEntries, and maxNEntries based on the lg2 of the
// number of buckets.
void JS::GenericHashTable::recomputeMinMaxNEntries(uint lgNBuckets)
{
    uint32 nBuckets = JS_BIT(lgNBuckets);
    shift = 32 - lgNBuckets;
    maxNEntries = nBuckets;   // Maximum ratio is 100%
    // Minimum ratio is 37.5%
    minNEntries = lgNBuckets <= defaultLgNBuckets ? 0 : 3*(nBuckets>>3);
}


// Rehash the table.  This method cannot throw out-of-memory exceptions, so it is
// safe to call from a destructor.
void JS::GenericHashTable::rehash()
{
    uint32 newLgNBuckets = ceilingLog2(nEntries);
    if (newLgNBuckets < defaultLgNBuckets)
        newLgNBuckets = defaultLgNBuckets;
    uint32 newNBuckets = JS_BIT(newLgNBuckets);
    try {
        GenericHashEntry **newBuckets = new GenericHashEntry*[newNBuckets];
        // No exceptions after this point.

        GenericHashEntry **newBucketsEnd = newBuckets + newNBuckets;
        zero(newBuckets, newBucketsEnd);
        recomputeMinMaxNEntries(newLgNBuckets);
        GenericHashEntry **be = bucketsEnd;
        for (GenericHashEntry **b = buckets; b != be; b++) {
            GenericHashEntry *e = *b;
            while (e) {
                GenericHashEntry *next = e->next;
                // Place e in the new set of buckets.
                GenericHashEntry **nb = newBuckets + (e->keyHash*goldenRatio >> shift);
                e->next = *nb;
                *nb = e;
                e = next;
            }
        }
        delete[] buckets;
        buckets = newBuckets;
        bucketsEnd = newBucketsEnd;
    } catch (std::bad_alloc) {
        // Out of memory.  Ignore the error and just relax the resizing boundaries.
        if (buckets + JS_BIT(newLgNBuckets) > bucketsEnd)
            maxNEntries >>= 1;
        else
            minNEntries <<= 1;
    }
}
