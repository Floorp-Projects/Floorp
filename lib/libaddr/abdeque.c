/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
 * File:      abdeque.c 
 * Contains:  Ferrum deque (double ended queue (linked list))
 *
 * Copied directly from public domain IronDoc, with minor naming tweaks.
 *
 * Change History:
 * <4>  25Nov1997  copy verbatim for use in Netscape address books
 * <3>  21Sep1997  Fe v0.16 add per method comments
 * <2>  01Sep1997  Fe v0.15 use "self" for first method parameter
 * <1>  24Jul1997  Fe v0.11
 * <0>  04Jul1997  first draft
 */

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABDEQUE_
#include "abdeque.h"
#endif

/*| RemoveFirst: 
|*/
AB_API_IMPL(AB_Link*)
AB_Deque_RemoveFirst(AB_Deque* self) /*i*/
{
  AB_Link* link = self->sDeque_Head.sLink_Next;
  if ( link != &self->sDeque_Head )
  {
    (self->sDeque_Head.sLink_Next = link->sLink_Next)->sLink_Prev = 
      &self->sDeque_Head;
    return link;
  }
  return (AB_Link*) 0;
}

/*| RemoveLast: 
|*/
AB_API_IMPL(AB_Link*)
AB_Deque_RemoveLast(AB_Deque* self) /*i*/
{
  AB_Link* link = self->sDeque_Head.sLink_Prev;
  if ( link != &self->sDeque_Head )
  {
    (self->sDeque_Head.sLink_Prev = link->sLink_Prev)->sLink_Next = 
      &self->sDeque_Head;
    return link;
  }
  return (AB_Link*) 0;
}

/*| At: 
|*/
AB_API_IMPL(AB_Link*)
AB_Deque_At(register const AB_Deque* self, ab_pos index) /*i*/
  /* indexes are one based (and not zero based) */
{ 
  register ab_num count = 0;
  register AB_Link* link;
  for ( link = AB_Deque_First(self); link; link = AB_Deque_After(self,link) )
  {
    if ( ++count == index )
      break;
  }
  return link;
}

/*| IndexOf: 
|*/
AB_API_IMPL(ab_pos)
AB_Deque_IndexOf(register const AB_Deque* self, const AB_Link* member) /*i*/
  /* indexes are one based (and not zero based) */
  /* zero means member is not in deque */
{ 
  register ab_num count = 0;
  register const AB_Link* link;
  for ( link = AB_Deque_First(self); link; link = AB_Deque_After(self,link) )
  {
    ++count;
    if ( member == link )
      return count;
  }
  return 0;
}

/*| Length: 
|*/
AB_API_IMPL(ab_num)
AB_Deque_Length(register const AB_Deque* self) /*i*/
{ 
  register ab_num count = 0;
  register AB_Link* link;
  for ( link = AB_Deque_First(self); link; link = AB_Deque_After(self,link) )
    ++count;
  return count;
}

/*| LengthCompare: 
|*/
AB_API_IMPL(int)
AB_Deque_LengthCompare(register const AB_Deque* self, ab_num c) /*i*/
{ 
  register ab_num count = 0;
  register const AB_Link* link;
  for ( link = AB_Deque_First(self); link; link = AB_Deque_After(self,link) )
  {
    if ( ++count > c )
      return 1;
  }
  return ( count == c )? 0 : -1;
}
