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

/*
 * File:      abdeque.h 
 * Contains:  Ferrum deque (double ended queue (linked list))
 *
 * Copied directly from public domain IronDoc, with minor naming tweaks.
 *
 * Change History:
 * <2>  25Nov1997  copy verbatim from IronDoc to use for Netscape AB db
 * <1>  24Jul1997  Fe v0.11
 * <0>  04Jul1997  first draft
 */

#ifndef _ABDEQUE_
#define _ABDEQUE_ 1

#ifndef _ABTABLE_
#include "abtable.h"
#endif

/*-------------------------------------
 * classes defined by this interface
 */

#ifndef AB_Link_typedef
typedef struct AB_Link AB_Link;
#define AB_Link_typedef 1
#endif

#ifndef AB_Deque_typedef
typedef struct AB_Deque AB_Deque;
#define AB_Deque_typedef 1
#endif


/* `````` `````` protos `````` `````` */
AB_BEGIN_C_PROTOS
/* `````` `````` protos `````` `````` */

/*=============================================================================
 * AB_Link: linked list node embedded in structs to allow insertion in AB_Deques
 */

struct AB_Link /*d*/ {  
  AB_Link*  sLink_Next;
  AB_Link*  sLink_Prev;
};

#define AB_Link_Next(self) /*i*/ ((self)->sLink_Next)
#define AB_Link_Prev(self) /*i*/ ((self)->sLink_Prev)

#define AB_Link_SelfRefer(self) /*i*/ \
  ((self)->sLink_Next = (self)->sLink_Prev = (self))

#define AB_Link_Clear(self) /*i*/ \
  ((self)->sLink_Next = (self)->sLink_Prev = 0)

#define AB_Link_AddBefore(self,old) /*i*/ \
  ( ((old)->sLink_Prev->sLink_Next = (self))->sLink_Prev = (old)->sLink_Prev, \
    ((self)->sLink_Next = (old))->sLink_Prev = (self) )

#define AB_Link_AddAfter(self,old) /*i*/ \
  ( ((old)->sLink_Next->sLink_Prev = (self))->sLink_Next = (old)->sLink_Next, \
    ((self)->sLink_Prev = (old))->sLink_Next = (self) )

#define AB_Link_Remove(self) /*i*/ \
  ( ((self)->sLink_Prev->sLink_Next = (self)->sLink_Next)->sLink_Prev = \
      (self)->sLink_Prev )


/*=============================================================================
 * AB_AnyLink: untyped link element for untyped lists
 */

struct AB_AnyLink /*d*/ {  /* concrete AB_Link subclass */
  AB_Link*  sLink_Next;
  AB_Link*  sLink_Prev;
  
  void*     sAnyLink_Any; /* pointer to anything */
};

#define AB_AnyLink_AsLink(self) /*i*/ ((AB_Link*) self) /* to supertype */

/*=============================================================================
 * AB_Deque: doubly linked list modeled after VAX queue instructions
 */

struct AB_Deque /*d*/ {
  AB_Link  sDeque_Head;
};

/*-------------------------------------
 * AB_Deque method macros
 */

#define AB_Deque_Init(d) /*i*/ AB_Link_SelfRefer(&(d)->sDeque_Head)
 
#define AB_Deque_IsEmpty(d) /*i*/ \
  ((d)->sDeque_Head.sLink_Next == (AB_Link*) (d))

#define AB_Deque_After(d,old) /*i*/ \
  (((old)->sLink_Next != (AB_Link*) (d))? (old)->sLink_Next : (AB_Link*) 0)

#define AB_Deque_Before(d,old) /*i*/ \
  (((old)->sLink_Prev != (AB_Link*) (d))? (old)->sLink_Prev : (AB_Link*) 0)

#define AB_Deque_First(d) /*i*/ \
  (((d)->sDeque_Head.sLink_Next != (AB_Link*) (d))? \
    (d)->sDeque_Head.sLink_Next : (AB_Link*) 0)

#define AB_Deque_Last(d) /*i*/ \
  (((d)->sDeque_Head.sLink_Prev != (AB_Link*) (d))? \
    (d)->sDeque_Head.sLink_Prev : (AB_Link*) 0)
    
/*
From IronDoc documentation for AddFirst:
+--------+   +--------+      +--------+   +--------+   +--------+
| h.next |-->| b.next |      | h.next |-->| a.next |-->| b.next |
+--------+   +--------+  ==> +--------+   +--------+   +--------+
| h.prev |<--| b.prev |      | h.prev |<--| a.prev |<--| b.prev |
+--------+   +--------+      +--------+   +--------+   +--------+
*/

#define AB_Deque_AddFirst(d,in) /*i*/ \
  ( ((d)->sDeque_Head.sLink_Next->sLink_Prev = \
    (in))->sLink_Next = (d)->sDeque_Head.sLink_Next, \
      ((in)->sLink_Prev = &(d)->sDeque_Head)->sLink_Next = (in) )
    
/*
From IronDoc documentation for AddLast:
+--------+   +--------+      +--------+   +--------+   +--------+
| y.next |-->| h.next |      | y.next |-->| z.next |-->| h.next |
+--------+   +--------+  ==> +--------+   +--------+   +--------+
| y.prev |<--| h.prev |      | y.prev |<--| z.prev |<--| h.prev |
+--------+   +--------+      +--------+   +--------+   +--------+
*/

#define AB_Deque_AddLast(d,in) /*i*/ \
  ( ((d)->sDeque_Head.sLink_Prev->sLink_Next = \
    (in))->sLink_Prev = (d)->sDeque_Head.sLink_Prev, \
      ((in)->sLink_Next = &(d)->sDeque_Head)->sLink_Prev = (in) )

/*-------------------------------------
 * AB_Deque method functions
 */

AB_PUBLIC_API(AB_Link*)
AB_Deque_RemoveFirst(AB_Deque* d);

AB_PUBLIC_API(AB_Link*)
AB_Deque_RemoveLast(AB_Deque* d);

AB_PUBLIC_API(AB_Link*)
AB_Deque_At(const AB_Deque* d, ab_pos index); /* one-based, not zero-based */

AB_PUBLIC_API(ab_pos)
AB_Deque_IndexOf(const AB_Deque* d, const AB_Link* member); 
  /* one-based index ; zero means member is not in deque */

AB_PUBLIC_API(ab_num)
AB_Deque_Length(const AB_Deque* d);

/* the following method is more efficient for long lists: */

AB_PUBLIC_API(int)
AB_Deque_LengthCompare(const AB_Deque* d, ab_num count);
/* -1: length < count, 0: length == count,  1: length > count */


/* `````` `````` protos `````` `````` */
AB_END_C_PROTOS
/* `````` `````` protos `````` `````` */

#endif
/* _ABDEQUE_ */
