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
  private:
    BitSet(unsigned int max) :
        max_(max),
        bits_(NULL) {};

    unsigned int max_;

    unsigned long *bits_;

    static inline unsigned long bitForValue(unsigned int value) {
        return 1l << (unsigned long)(value % (8 * sizeof(unsigned long)));
    }

    static inline unsigned int wordForValue(unsigned int value) {
        return value / (8 * sizeof(unsigned long));
    }

    inline unsigned int numWords() const {
        return 1 + max_ / (8 * sizeof(*bits_));
    }

    bool init();

  public:
    class Iterator;

    static BitSet *New(unsigned int max);

    unsigned int getMax() const {
        return max_;
    }

    // O(1): Check if this set contains the given value.
    bool contains(unsigned int value) const;

    // O(max): Check if this set contains any value.
    bool empty() const;

    // O(1): Insert the given value into this set.
    void insert(unsigned int value);

    // O(max): Insert every element of the given set into this set.
    void insertAll(const BitSet *other);

    // O(1): Remove the given value from this set.
    void remove(unsigned int value);

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

    // Iterator to the beginning of this set.
    Iterator begin();

    // Iterator to the end of this set.
    Iterator end();

};

class BitSet::Iterator
{
  private:
    BitSet &set_;
    unsigned index_;

  public:
    Iterator(BitSet &set, unsigned int index) :
      set_(set),
      index_(index)
    {
        if (index_ <= set_.max_ && !set_.contains(index_))
            (*this)++;
    }

    bool operator!=(const Iterator &other) const {
        return index_ != other.index_;
    }

    // FIXME (668305): Use bit scan.
    Iterator& operator++(int dummy) {
        JS_ASSERT(index_ <= set_.max_);
        do {
            index_++;
        } while (index_ <= set_.max_ && !set_.contains(index_));
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
