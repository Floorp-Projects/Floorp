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

#ifndef _FIFO_H_
#define _FIFO_H_

#include "Fundamentals.h"

template <class N>
class Fifo
{
private:
  N* begin;
  N* limit;
  N* first;
  N* last;

public:
  Fifo(Uint32 size) {begin = first = last = new N[size]; limit = &begin[size];}
  ~Fifo() {delete begin;}
  
  N& get();                                    // get at first element inserted.
  void put(N n);                               // append an element.
  bool empty() const {return (last == first);} // is the fifo empty ?
  Uint32 count() const;                        // number of elements in the fifo.
};

template <class N> inline N&
Fifo<N>::get()
{
  assert(last != first);
  N* pos = first++;
  if (first == limit)
	first = begin;
  return *pos;
}

template <class N> inline void
Fifo<N>::put(N n)
{
  *last++ = n;
  if (last == limit)
	last = begin;
  assert(last != first);
}

template <class N> inline Uint32
Fifo<N>::count() const
{
  if (last >= first)
	return Uint32(last - first);
  else
	return  Uint32(limit - first) + Uint32(last - begin);
}

#endif /* _FIFO_H_ */
