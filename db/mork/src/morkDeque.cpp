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

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

/*=============================================================================
 * morkNext: linked list node for very simple, singly-linked list
 */
 
morkNext::morkNext() : mNext_Link( 0 )
{
}

/*static*/ void*
morkNext::MakeNewNext(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev)
{
  void* next = 0;
  if ( &ioHeap )
  {
    ioHeap.Alloc(ev->AsMdbEnv(), inSize, (void**) &next);
    if ( !next )
      ev->OutOfMemoryError();
  }
  else
    ev->NilPointerError();
  
  return next;
}

/*static*/
void morkNext::ZapOldNext(morkEnv* ev, nsIMdbHeap* ioHeap)
{
  if ( ioHeap )
  {
    if ( this )
      ioHeap->Free(ev->AsMdbEnv(), this);
  }
  else
    ev->NilPointerError();
}

/*=============================================================================
 * morkList: simple, singly-linked list
 */

morkList::morkList() : mList_Head( 0 ), mList_Tail( 0 )
{
}

void morkList::CutAndZapAllListMembers(morkEnv* ev, nsIMdbHeap* ioHeap)
// make empty list, zapping every member by calling ZapOldNext()
{
  if ( ioHeap )
  {
    morkNext* next = 0;
    while ( (next = this->PopHead()) != 0 )
      next->ZapOldNext(ev, ioHeap);
      
    mList_Head = 0;
    mList_Tail = 0;
  }
  else
    ev->NilPointerError();
}

void morkList::CutAllListMembers()
// just make list empty, dropping members without zapping
{
  while ( this->PopHead() )
    /* empty */;

  mList_Head = 0;
  mList_Tail = 0;
}

morkNext* morkList::PopHead() // cut head of list
{
  morkNext* outHead = mList_Head;
  if ( outHead ) // anything to cut from list?
  {
    morkNext* next = outHead->mNext_Link;
    mList_Head = next;
    if ( !next ) // cut the last member, so tail no longer exists?
      mList_Tail = 0;
      
    outHead->mNext_Link = 0; // nil outgoing node link; unnecessary, but tidy
  }
  return outHead;
}


void morkList::PushHead(morkNext* ioLink) // add to head of list
{
  morkNext* head = mList_Head; // old head of list
  morkNext* tail = mList_Tail; // old tail of list
  
  MORK_ASSERT( (head && tail) || (!head && !tail));
  
  ioLink->mNext_Link = head; // make old head follow the new link
  if ( !head ) // list was previously empty?
    mList_Tail = ioLink; // head is also tail for first member added

  mList_Head = ioLink; // head of list is the new link
}

void morkList::PushTail(morkNext* ioLink) // add to tail of list
{
  morkNext* head = mList_Head; // old head of list
  morkNext* tail = mList_Tail; // old tail of list
  
  MORK_ASSERT( (head && tail) || (!head && !tail));
  
  ioLink->mNext_Link = 0; 
  if ( tail ) 
  {
	  tail->mNext_Link = ioLink;
	  mList_Tail = ioLink;
  }
  else // list was previously empty?
	  mList_Head = mList_Tail = ioLink; // tail is also head for first member added
}

/*=============================================================================
 * morkLink: linked list node embedded in objs to allow insertion in morkDeques
 */
 
morkLink::morkLink() : mLink_Next( 0 ), mLink_Prev( 0 )
{
}

/*static*/ void*
morkLink::MakeNewLink(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev)
{
  void* alink = 0;
  if ( &ioHeap )
  {
    ioHeap.Alloc(ev->AsMdbEnv(), inSize, (void**) &alink);
    if ( !alink )
      ev->OutOfMemoryError();
  }
  else
    ev->NilPointerError();
  
  return alink;
}

/*static*/
void morkLink::ZapOldLink(morkEnv* ev, nsIMdbHeap* ioHeap)
{
  if ( ioHeap )
  {
    if ( this )
      ioHeap->Free(ev->AsMdbEnv(), this);
  }
  else
    ev->NilPointerError();
}
  
/*=============================================================================
 * morkDeque: doubly linked list modeled after VAX queue instructions
 */

morkDeque::morkDeque()
{
  mDeque_Head.SelfRefer();
}


/*| RemoveFirst: 
|*/
morkLink*
morkDeque::RemoveFirst() /*i*/
{
  morkLink* alink = mDeque_Head.mLink_Next;
  if ( alink != &mDeque_Head )
  {
    (mDeque_Head.mLink_Next = alink->mLink_Next)->mLink_Prev = 
      &mDeque_Head;
    return alink;
  }
  return (morkLink*) 0;
}

/*| RemoveLast: 
|*/
morkLink*
morkDeque::RemoveLast() /*i*/
{
  morkLink* alink = mDeque_Head.mLink_Prev;
  if ( alink != &mDeque_Head )
  {
    (mDeque_Head.mLink_Prev = alink->mLink_Prev)->mLink_Next = 
      &mDeque_Head;
    return alink;
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
  register morkLink* alink;
  for ( alink = this->First(); alink; alink = this->After(alink) )
  {
    if ( ++count == (mork_num) index )
      break;
  }
  return alink;
}

/*| IndexOf: 
|*/
mork_pos
morkDeque::IndexOf(const morkLink* member) const /*i*/
  /* indexes are one based (and not zero based) */
  /* zero means member is not in deque */
{ 
  register mork_num count = 0;
  register const morkLink* alink;
  for ( alink = this->First(); alink; alink = this->After(alink) )
  {
    ++count;
    if ( member == alink )
      return (mork_pos) count;
  }
  return 0;
}

/*| Length: 
|*/
mork_num
morkDeque::Length() const /*i*/
{ 
  register mork_num count = 0;
  register morkLink* alink;
  for ( alink = this->First(); alink; alink = this->After(alink) )
    ++count;
  return count;
}

/*| LengthCompare: 
|*/
int
morkDeque::LengthCompare(mork_num c) const /*i*/
{ 
  register mork_num count = 0;
  register const morkLink* alink;
  for ( alink = this->First(); alink; alink = this->After(alink) )
  {
    if ( ++count > c )
      return 1;
  }
  return ( count == c )? 0 : -1;
}
