/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BitSet.h"

using namespace js;
using namespace js::jit;

BitSet *
BitSet::New(unsigned int max)
{
    BitSet *result = new BitSet(max);
    if (!result->init())
        return nullptr;
    return result;
}

bool
BitSet::init()
{
    size_t sizeRequired = numWords() * sizeof(*bits_);

    TempAllocator *alloc = GetIonContext()->temp;
    bits_ = (uint32_t *)alloc->allocate(sizeRequired);
    if (!bits_)
        return false;

    memset(bits_, 0, sizeRequired);

    return true;
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
BitSet::insertAll(const BitSet *other)
{
    JS_ASSERT(bits_);
    JS_ASSERT(other->max_ == max_);
    JS_ASSERT(other->bits_);

    for (unsigned int i = 0; i < numWords(); i++)
        bits_[i] |= other->bits_[i];
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
        uint32_t old = bits_[i];
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
