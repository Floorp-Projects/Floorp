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

#ifndef _VECTOR_H_
#define _VECTOR_H_

#include "Fundamentals.h"

#define DEFAULT_START_SIZE 128

template <class N>
class Vector
{
private:
  N *start;
  N *finish;
  N *end_of_storage;

public:
// ----------------------------------------------------------------------------
// constructors.

  Vector() : start(0), finish(0), end_of_storage(0) {}
  Vector(unsigned int n) : start(0), finish(0), end_of_storage(0) {reserve(n);}
  ~Vector() {if (start) delete[] start;}

// ----------------------------------------------------------------------------
// copy.

  Vector(const Vector<N>& x) : start(0), finish(0), end_of_storage(0)
  {
    unsigned int x_size = x.size();
    const N *src = x.begin();
    if (x_size)
      {
	reserve(x_size);
	finish = start;
	while (x_size--) *finish++ = *src++;
      }
  }

  Vector<N>& operator=(const Vector<N>& x)
  {
    unsigned int x_size = x.size();
    const N *src = x.begin();
    N *dst;

    if ((&x == this) || (x_size == 0)) 
      return *this;

    reserve(x_size);
    finish = start;
    while (x_size--) *finish++ = *src++;  // copy the elements.
    for (dst = finish; dst < end_of_storage; dst++) *dst = N();
    return *this;
  }

// ----------------------------------------------------------------------------
// memory management.

  void reserve(unsigned int amount)
  {
    if (!amount) amount = DEFAULT_START_SIZE;

    if (capacity() < amount) 
      {
	N *tmp = new N[amount];
	N *src = begin();
	N *dst = tmp;
	unsigned int actual_size = size();
	
	while (actual_size--) *dst++ = *src++;
	delete[] start;
	finish = tmp + size();
	start = tmp;
	end_of_storage = start + amount;
      }
  }
  
// ----------------------------------------------------------------------------
// pointers on the elements.

  N *begin() {return start;}
  const N *begin() const {return start;}
  N *end() {return finish;}
  const N *end() const {return finish;}


// ----------------------------------------------------------------------------
// number of elements and the actual capacity.

  unsigned char empty() const {return (begin() == end());}
  unsigned int size() const {return (unsigned int)(end() - begin());}
  unsigned int capacity() const {return (unsigned int)(end_of_storage - begin());}

// ----------------------------------------------------------------------------
// add, remove, get element

  N& operator[](unsigned int index) {assert (index < size()); return start[index];}
  const N& operator[](unsigned int index) const {assert (index < size()); return start[index];}

  void fill(const N& element)
  {
    N* src = start;
    while (src < finish) *src++ = element;
  }

  void append(const N& element)
  {
    if (finish == end_of_storage)
      reserve(2 * capacity());

    *finish++ = element;
  }

  void append(unsigned int count, const N& element) 
  {
    reserve(size() + count);
    while (count--) *finish++ = element;
  }

  void eraseLast() {if (!empty()) *--finish = N();}
  void eraseAll() {while (finish > start) *--finish = N();}
  

  N& first() {return *begin();}
  N& last() {return *(end() - 1);}
};


#endif /* _VECTOR_H_ */
