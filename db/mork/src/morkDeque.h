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
 * File:      morkDeque.h 
 * Contains:  Ferrum deque (double ended queue (linked list))
 *
 * Copied directly from public domain IronDoc, with minor naming tweaks:
 * Designed and written by David McCusker, but all this code is public domain.
 * There are no warranties, no guarantees, no promises, and no remedies.
 */

#ifndef _MORKDEQUE_
#define _MORKDEQUE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

/*=============================================================================
 * morkLink: linked list node embedded in objs to allow insertion in morkDeques
 */

class morkLink /*d*/ {  
public:
  morkLink*  mLink_Next;
  morkLink*  mLink_Prev;
  
public:
  morkLink*  Next() const { return mLink_Next; }
  morkLink*  Prev() const { return mLink_Prev; }
  
  void SelfRefer() { mLink_Next = mLink_Prev = this; }
  void Clear() { mLink_Next = mLink_Prev = 0; }
  
  void AddBefore(morkLink* old)
  {
    ((old)->mLink_Prev->mLink_Next = (this))->mLink_Prev = (old)->mLink_Prev;
    ((this)->mLink_Next = (old))->mLink_Prev = this;
  }
  
  void AddAfter(morkLink* old)
  {
    ((old)->mLink_Next->mLink_Prev = (this))->mLink_Next = (old)->mLink_Next;
    ((this)->mLink_Prev = (old))->mLink_Next = this;
  }
  
  void Remove()
  {
    (mLink_Prev->mLink_Next = mLink_Next)->mLink_Prev =  mLink_Prev;
  }
};

/*=============================================================================
 * morkDeque: doubly linked list modeled after VAX queue instructions
 */

class morkDeque /*d*/ {
public:
  morkLink  mDeque_Head;

public: // construction
  morkDeque() { mDeque_Head.SelfRefer(); }

public:// methods
  morkLink* RemoveFirst();

  morkLink* RemoveLast();

  morkLink* At(mork_pos index) const ; /* one-based, not zero-based */

  mork_pos IndexOf(const morkLink* inMember) const; 
    /* one-based index ; zero means member is not in deque */

  mork_num Length() const;

  /* the following method is more efficient for long lists: */
  int LengthCompare(mork_num inCount) const;
  /* -1: length < count, 0: length == count,  1: length > count */

public: // inlines

  mork_bool IsEmpty()const 
  { return (mDeque_Head.mLink_Next == (morkLink*) &mDeque_Head); }

  morkLink* After(const morkLink* old) const
  { return (((old)->mLink_Next != &mDeque_Head)?
            (old)->mLink_Next : (morkLink*) 0); }

  morkLink* Before(const morkLink* old) const
  { return (((old)->mLink_Prev != &mDeque_Head)?
            (old)->mLink_Prev : (morkLink*) 0); }

  morkLink*  First() const
  { return ((mDeque_Head.mLink_Next != &mDeque_Head)?
    mDeque_Head.mLink_Next : (morkLink*) 0); }

  morkLink*  Last() const
  { return ((mDeque_Head.mLink_Prev != &mDeque_Head)?
    mDeque_Head.mLink_Prev : (morkLink*) 0); }
    
/*
From IronDoc documentation for AddFirst:
+--------+   +--------+      +--------+   +--------+   +--------+
| h.next |-->| b.next |      | h.next |-->| a.next |-->| b.next |
+--------+   +--------+  ==> +--------+   +--------+   +--------+
| h.prev |<--| b.prev |      | h.prev |<--| a.prev |<--| b.prev |
+--------+   +--------+      +--------+   +--------+   +--------+
*/

  void AddFirst(morkLink* in) /*i*/ 
  {
    ( (mDeque_Head.mLink_Next->mLink_Prev = 
      (in))->mLink_Next = mDeque_Head.mLink_Next, 
        ((in)->mLink_Prev = &mDeque_Head)->mLink_Next = (in) );
  }
/*
From IronDoc documentation for AddLast:
+--------+   +--------+      +--------+   +--------+   +--------+
| y.next |-->| h.next |      | y.next |-->| z.next |-->| h.next |
+--------+   +--------+  ==> +--------+   +--------+   +--------+
| y.prev |<--| h.prev |      | y.prev |<--| z.prev |<--| h.prev |
+--------+   +--------+      +--------+   +--------+   +--------+
*/

  void AddLast(morkLink* in)
  {
    ( (mDeque_Head.mLink_Prev->mLink_Next = 
      (in))->mLink_Prev = mDeque_Head.mLink_Prev, 
        ((in)->mLink_Next = &mDeque_Head)->mLink_Prev = (in) );
  }
};

#endif /* _MORKDEQUE_ */
