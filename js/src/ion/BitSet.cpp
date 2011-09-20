/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Drake <adrake@adrake.org>
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

#include "jsutil.h"
#include "BitSet.h"

using namespace js;
using namespace js::ion;

BitSet *
BitSet::New(unsigned int max)
{
    BitSet *result = new BitSet(max);
    if (!result->init())
        return NULL;
    return result;
}

bool
BitSet::init()
{
    size_t sizeRequired = numWords() * sizeof(*bits_);

    TempAllocator *alloc = GetIonContext()->temp;
    bits_ = (unsigned long *)alloc->allocate(sizeRequired);
    if (!bits_)
        return false;

    memset(bits_, 0, sizeRequired);

    return true;
}

bool
BitSet::contains(unsigned int value) const
{
    JS_ASSERT(bits_);
    JS_ASSERT(value <= max_);

    return bits_[wordForValue(value)] & bitForValue(value);
}

bool
BitSet::empty() const
{
    JS_ASSERT(bits_);
    for (unsigned int i = 0; i < numWords(); i++) {
        if (bits_[i])
            return false;
    }
    return true;
}

void
BitSet::insert(unsigned int value)
{
    JS_ASSERT(bits_);
    JS_ASSERT(value <= max_);

    bits_[wordForValue(value)] |= bitForValue(value);
}

void
BitSet::insertAll(const BitSet *other)
{
    JS_ASSERT(bits_);
    JS_ASSERT(other->max_ == max_);
    JS_ASSERT(other->bits_);

    for (unsigned int i = 0; i < numWords(); i++)
        bits_[i] |= other->bits_[i];
}

void
BitSet::remove(unsigned int value)
{
    JS_ASSERT(bits_);
    JS_ASSERT(value <= max_);

    bits_[wordForValue(value)] &= ~bitForValue(value);
}

void
BitSet::removeAll(const BitSet *other)
{
    JS_ASSERT(bits_);
    JS_ASSERT(other->max_ == max_);
    JS_ASSERT(other->bits_);

    for (unsigned int i = 0; i < numWords(); i++)
        bits_[i] &= ~other->bits_[i];
}

void
BitSet::intersect(const BitSet *other)
{
    JS_ASSERT(bits_);
    JS_ASSERT(other->max_ == max_);
    JS_ASSERT(other->bits_);

    for (unsigned int i = 0; i < numWords(); i++)
        bits_[i] &= other->bits_[i];
}

// returns true if the intersection caused the contents of the set to change.
bool
BitSet::fixedPointIntersect(const BitSet *other)
{
    JS_ASSERT(bits_);
    JS_ASSERT(other->max_ == max_);
    JS_ASSERT(other->bits_);

    bool changed = false;

    for (unsigned int i = 0; i < numWords(); i++) {
        unsigned long old = bits_[i];
        bits_[i] &= other->bits_[i];

        if (!changed && old != bits_[i])
            changed = true;
    }
    return changed;
}

void
BitSet::complement()
{
    JS_ASSERT(bits_);
    for (unsigned int i = 0; i < numWords(); i++)
        bits_[i] = ~bits_[i];
}

void
BitSet::clear()
{
    JS_ASSERT(bits_);
    for (unsigned int i = 0; i < numWords(); i++)
        bits_[i] = 0;
}

BitSet::Iterator
BitSet::begin()
{
    return Iterator(*this, 0);
}

BitSet::Iterator
BitSet::end()
{
    return Iterator(*this, max_ + 1);
}
