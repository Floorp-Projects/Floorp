/*************************************************************************
This software is part of a public domain IronDoc source code distribution,
and is provided on an "AS IS" basis, with all risks borne by the consumers
or users of the IronDoc software.  There are no warranties, guarantees, or
promises about quality of any kind; and no remedies for failure exist. 

Permission is hereby granted to use this IronDoc software for any purpose
at all, without need for written agreements, without royalty or license
fees, and without fees or obligations of any other kind.  Anyone can use,
copy, change and distribute this software for any purpose, and nothing is
required, implicitly or otherwise, in exchange for this usage.

You cannot apply your own copyright to this software, but otherwise you
are encouraged to enjoy the use of this software in any way you see fit.
However, it would be rude to remove names of developers from the code.
(IronDoc is also known by the short name "Fe" and a longer name "Ferrum",
which are used interchangeably with the name IronDoc in the sources.)
*************************************************************************/
/*
 * File:      morkDeque.cpp
 * Contains:  Ferrum deque (double ended queue (linked list))
 *
 * Copied directly from public domain IronDoc, with minor naming tweaks:
 * Designed and written by David McCusker, but all this code is public domain.
 * There are no warranties, no guarantees, no promises, and no remedies.
 */

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

/*| RemoveFirst: 
|*/
morkLink*
morkDeque::RemoveFirst() /*i*/
{
  morkLink* link = mDeque_Head.mLink_Next;
  if ( link != &mDeque_Head )
  {
    (mDeque_Head.mLink_Next = link->mLink_Next)->mLink_Prev = 
      &mDeque_Head;
    return link;
  }
  return (morkLink*) 0;
}

/*| RemoveLast: 
|*/
morkLink*
morkDeque::RemoveLast() /*i*/
{
  morkLink* link = mDeque_Head.mLink_Prev;
  if ( link != &mDeque_Head )
  {
    (mDeque_Head.mLink_Prev = link->mLink_Prev)->mLink_Next = 
      &mDeque_Head;
    return link;
  }
  return (morkLink*) 0;
}

/*| At: 
|*/
morkLink*
morkDeque::At(mork_pos index) const /*i*/
  /* indexes are one based (and not zero based) */
{ 
  register mork_num count = 0;
  register morkLink* link;
  for ( link = this->First(); link; link = this->After(link) )
  {
    if ( ++count == index )
      break;
  }
  return link;
}

/*| IndexOf: 
|*/
mork_pos
morkDeque::IndexOf(const morkLink* member) const /*i*/
  /* indexes are one based (and not zero based) */
  /* zero means member is not in deque */
{ 
  register mork_num count = 0;
  register const morkLink* link;
  for ( link = this->First(); link; link = this->After(link) )
  {
    ++count;
    if ( member == link )
      return count;
  }
  return 0;
}

/*| Length: 
|*/
mork_num
morkDeque::Length() const /*i*/
{ 
  register mork_num count = 0;
  register morkLink* link;
  for ( link = this->First(); link; link = this->After(link) )
    ++count;
  return count;
}

/*| LengthCompare: 
|*/
int
morkDeque::LengthCompare(mork_num c) const /*i*/
{ 
  register mork_num count = 0;
  register const morkLink* link;
  for ( link = this->First(); link; link = this->After(link) )
  {
    if ( ++count > c )
      return 1;
  }
  return ( count == c )? 0 : -1;
}
