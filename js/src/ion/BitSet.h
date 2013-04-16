/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_bitset_h__
#define jsion_bitset_h__

#include "IonAllocPolicy.h"

namespace js {
namespace ion {

// Provides constant time set insertion and removal, and fast linear
// set operations such as intersection, difference, and union.
// N.B. All set operations must be performed on sets with the same maximum.
class BitSet : private TempObject
{
  public:
    static size_t RawLengthForBits(size_t bits) {
        return 1 + bits / (8 * sizeof(uint32_t));
    }

  private:
    BitSet(unsigned int max) :
        max_(max),
        bits_(NULL) {}

    unsigned int max_;
    uint32_t *bits_;

    static inline uint32_t bitForValue(unsigned int value) {
        return 1l << (uint32_t)(value % (8 * sizeof(uint32_t)));
    }

    static inline unsigned int wordForValue(unsigned int value) {
        return value / (8 * sizeof(uint32_t));
    }

    inline unsigned int numWords() const {
        return RawLengthForBits(max_);
    }

    bool init();

  public:
    class Iterator;

    static BitSet *New(unsigned int max);

    unsigned int getMax() const {
        return max_;
    }

    // O(1): Check if this set contains the given value.
    bool contains(unsigned int value) const {
        JS_ASSERT(bits_);
        JS_ASSERT(value <= max_);

        return !!(bits_[wordForValue(value)] & bitForValue(value));
    }

    // O(max): Check if this set contains any value.
    bool empty() const;

    // O(1): Insert the given value into this set.
    void insert(unsigned int value) {
        JS_ASSERT(bits_);
        JS_ASSERT(value <= max_);

        bits_[wordForValue(value)] |= bitForValue(value);
    }

    // O(max): Insert every element of the given set into this set.
    void insertAll(const BitSet *other);

    // O(1): Remove the given value from this set.
    void remove(unsigned int value) {
        JS_ASSERT(bits_);
        JS_ASSERT(value <= max_);

        bits_[wordForValue(value)] &= ~bitForValue(value);
    }

    // O(max): Remove the every element of the given set from this set.
    void removeAll(const BitSet *other);

    // O(max): Intersect this set with the given set.
    void intersect(const BitSet *other);

    // O(max): Intersect this set with the given set; return whether the
    // intersection caused the set to change.
    bool fixedPointIntersect(const BitSet *other);

    // O(max): Does inplace complement of the set.
    void complement();

    // O(max): Clear this set.
    void clear();

    uint32_t *raw() const {
        return bits_;
    }
    size_t rawLength() const {
        return numWords();
    }
};

class BitSet::Iterator
{
  private:
    BitSet &set_;
    unsigned index_;
    unsigned word_;
    uint32_t value_;

  public:
    Iterator(BitSet &set) :
      set_(set),
      index_(0),
      word_(0),
      value_(set.bits_[0])
    {
        if (!set_.contains(index_))
            (*this)++;
    }

    inline bool more() const {
        return word_ < set_.numWords();
    }
    inline operator bool() const {
        return more();
    }

    inline Iterator& operator++(int dummy) {
        JS_ASSERT(more());
        JS_ASSERT(index_ <= set_.max_);

        index_++;
        value_ >>= 1;

        // Skip words containing only zeros.
        while (value_ == 0) {
            word_++;
            if (!more())
                return *this;

            index_ = word_ * sizeof(value_) * 8;
            value_ = set_.bits_[word_];
        }

        // The result of js_bitscan_ctz32 is undefined if the input is 0.
        JS_ASSERT(value_ != 0);

        int numZeros = js_bitscan_ctz32(value_);
        index_ += numZeros;
        value_ >>= numZeros;

        JS_ASSERT_IF(index_ <= set_.max_, set_.contains(index_));
        return *this;
    }

    unsigned int operator *() {
        JS_ASSERT(index_ <= set_.max_);
        return index_;
    }
};

}
}

#endif
