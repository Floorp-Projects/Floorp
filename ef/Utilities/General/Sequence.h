/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _SEQUENCE_H_
#define _SEQUENCE_H_

#include "Fundamentals.h"
#include "Memory.h"

template <class N, class MEMOPS>
class Sequence
{
protected:
    N *elts;
    Uint32 cnt;

public:
// ----------------------------------------------------------------------------
// constructors.

    Sequence() : elts(NULL), cnt(0) {}
    Sequence(Uint32 count) : elts(NULL), cnt(0) { reserve(count); }
    ~Sequence() { if (elts) MEMOPS::free(elts); }

// ----------------------------------------------------------------------------
// copy.

    Sequence(const Sequence<N, MEMOPS>& x) : elts(NULL), cnt(0)
    {
        Uint32 x_count = x.count();
        const N *src = x.elements();
        reserve(x_count);
        N *dst = elts;
        while (x_count--) *dst++ = *src++;
    }

    Sequence<N, MEMOPS>& operator=(const Sequence<N, MEMOPS>& x)
    {
        Uint32 x_count = x.count();
        const N *src = x.elements();
        N *dst;

        if ((&x == this) || (x_count == 0)) 
            return *this;

        reserve(x_count);
        dst = elts;
        while (x_count--) *dst++ = *src++;  // copy the elements.
        return *this;
    }

// ----------------------------------------------------------------------------
// memory management.

    void reserve(Uint32 amount)
    {
        if (cnt < amount) {
            if (elts) MEMOPS::free(elts);
            elts = (N*)MEMOPS::alloc(amount * sizeof(N*));
            cnt = amount;
        }
    }
  
// ----------------------------------------------------------------------------
// pointers on the elements.

    N *elements() { return elts; }
    const N *elements() const { return elts; }

// ----------------------------------------------------------------------------
// number of elements and the actual capacity.

    unsigned char empty() const { return cnt == 0; }
    Uint32 count() const { return cnt; }

// ----------------------------------------------------------------------------
// add, remove, get element

    N& operator[](Uint32 index) { assert(index < count()); return elts[index]; }
    const N& operator[](Uint32 index) const { assert(index < count()); return elts[index]; }

    void fill(const N& element)
    {
        N* src = elts;
        Uint32 i = cnt;
        while (i--) *src++ = element;
    }
};


#endif /* _SEQUENCE_H_ */
