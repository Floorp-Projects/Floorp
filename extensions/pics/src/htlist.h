
/*  W3 Copyright statement 
Copyright 1995 by: Massachusetts Institute of Technology (MIT), INRIA</H2>

This W3C software is being provided by the copyright holders under the
following license. By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with the
following terms and conditions: 

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee or royalty is hereby
granted, provided that the full text of this NOTICE appears on
<EM>ALL</EM> copies of the software and documentation or portions
thereof, including modifications, that you make. 

<B>THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  BY WAY OF EXAMPLE,
BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
OR DOCUMENTATION.

The name and trademarks of copyright holders may NOT be used
in advertising or publicity pertaining to the software without
specific, written prior permission.  Title to copyright in this
software and any associated documentation will at all times remain
with copyright holders. 
*/
/*                                                    W3C Reference Library libwww List Class
                                      THE LIST CLASS
                                             
 */
/*
**      (c) COPYRIGHT MIT 1995.
**      Please first read the full copyright statement in the file COPYRIGH.
*/
/*

   The list class defines a generic container for storing collections of things in order.
   In principle it could be implemented in many ways, but in practice knowing that it is a
   linked list is important for speed.
   
   This module is implemented by HTList.c, and it is a part of the W3C Reference Library.
   
 */
#ifndef HTLIST_H
#define HTLIST_H


PR_BEGIN_EXTERN_C


#ifndef BOOL
#define BOOL PRBool
#endif


typedef struct _HTList HTList;

struct _HTList {
  void * object;
  HTList * next;
};
/*

CREATION AND DELETION METHODS

   These two functions create and deletes a list
   
 */
extern HTList * HTList_new      (void);
extern PRBool     HTList_delete   (HTList *me);
/*

ADD AN ELEMENT TO LIST

   A new list element is added to the beginning of the list so that it is first element
   just after the head element.
   
 */
extern PRBool HTList_addObject (HTList *me, void *newObject);
/*

   You can also append an element to the end of the list (the end is the first entered
   object) by using the following function:
   
 */
extern PRBool HTList_appendObject (HTList * me, void * newObject);
/*

REMOVE LIST ELEMENTS

   You can delete elements in a list usin the following methods
   
 */
extern PRBool     HTList_removeObject             (HTList *me, void *oldObject);
extern void *   HTList_removeLastObject         (HTList *me);
extern void *   HTList_removeFirstObject        (HTList *me);
/*

SIZE OF A LIST

   Two small function to ask for the size
   
 */
#define         HTList_isEmpty(me)              (me ? me->next == NULL : YES)
extern int      HTList_count                    (HTList *me);
/*

REFERENCE LIST ELEMENTS BY INDEX

   In some situations is is required to use an index in order to refer to a list element.
   This is for example the case if an element can be registered multiple times.
   
 */
extern int      HTList_indexOf  (HTList *me, void *object);
extern void *   HTList_objectAt (HTList *me, int position);
/*

FIND LIST ELEMENTS

   This method returns the _last_ element to the list or NULL if list is empty
   
 */
#define         HTList_lastObject(me) \
                ((me) && (me)->next ? (me)->next->object : NULL)
/*

   This method returns the _first_ element to the list or NULL if list is empty
   
 */
extern void * HTList_firstObject  (HTList * me);
/*

TRAVERSE LIST

   Fast macro to traverse the list. Call it first with copy of list header: it returns the
   first object and increments the passed list pointer. Call it with the same variable
   until it returns NULL.
   
 */
#define         HTList_nextObject(me) \
                ((me) && ((me) = (me)->next) ? (me)->object : NULL)
/*

FREE LIST

 */
#define HTList_free(x)  HT_FREE(x)

PR_END_EXTERN_C

#endif /* HTLIST_H */
/*

   
   ___________________________________
   
                            @(#) $Id: htlist.h,v 1.1 1999/03/18 22:32:49 neeti%netscape.com Exp $
                                                                                          
    */
