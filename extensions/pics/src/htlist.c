
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
/*								       HTList.c
**	MANAGEMENT OF LINKED LISTS
**
**	(c) COPYRIGHT MIT 1995.
**	Please first read the full copyright statement in the file COPYRIGH.
**	@(#) $Id: htlist.c,v 1.1 1999/03/18 22:32:48 neeti%netscape.com Exp $
**
**	A list is represented as a sequence of linked nodes of type HTList.
**	The first node is a header which contains no object.
**	New nodes are inserted between the header and the rest of the list.
*/

/* Library include files */
/* #include "sysdep.h"  jhines -- 7/9/97 */
#include "htutils.h"
#include "htlist.h"

PUBLIC HTList * HTList_new (void)
{
    HTList *newList;
    if ((newList = (HTList  *) HT_CALLOC(1, sizeof (HTList))) == NULL)
        HT_OUTOFMEM("HTList_new");
    newList->object = NULL;
    newList->next = NULL;
    return newList;
}

PUBLIC PRBool HTList_delete (HTList * me)
{
    if (me) {
	HTList *current;
	while ((current = me) != NULL) {
	    me = me->next;
	    HT_FREE(current);
	}
	return YES;
    }
    return NO;
}

PUBLIC PRBool HTList_addObject (HTList * me, void * newObject)
{
    if (me) {
	HTList *newNode;
	if ((newNode = (HTList  *) HT_CALLOC(1, sizeof(HTList))) == NULL)
	    HT_OUTOFMEM("HTList_addObject");
	newNode->object = newObject;
	newNode->next = me->next;
	me->next = newNode;
	return YES;
    } else {
	if (WWWTRACE)
	    HTTrace(
		    "HTList...... Can not add object %p to nonexisting list\n",
		    newObject);
    }
    return NO;
}

PUBLIC PRBool HTList_appendObject (HTList * me, void * newObject)
{
    if (me) {
	while (me->next) me = me->next;
	return HTList_addObject(me, newObject);
    }
    return NO;
}

PUBLIC PRBool HTList_removeObject (HTList *  me, void *  oldObject)
{
    if (me) {
	HTList *previous;
	while (me->next) {
	    previous = me;
	    me = me->next;
	    if (me->object == oldObject) {
		previous->next = me->next;
		HT_FREE(me);
		return YES;	/* Success */
	    }
	}
    }
    return NO;			/* object not found or NULL list */
}

PUBLIC void * HTList_removeLastObject  (HTList * me)
{
    if (me && me->next) {
	HTList *lastNode = me->next;
	void * lastObject = lastNode->object;
	me->next = lastNode->next;
	HT_FREE(lastNode);
	return lastObject;
    } else			/* Empty list */
	return NULL;
}

PUBLIC void * HTList_removeFirstObject  (HTList * me)
{
    if (me && me->next) {
	HTList * prevNode;
	void *firstObject;
	while (me->next) {
	    prevNode = me;
	    me = me->next;
	}
	firstObject = me->object;
	prevNode->next = NULL;
	HT_FREE(me);
	return firstObject;
    } else			/* Empty list */
	return NULL;
}

PUBLIC void * HTList_firstObject  (HTList * me)
{
    if (me && me->next) {
	HTList * prevNode;
	while (me->next) {
	    prevNode = me;
	    me = me->next;
	}
	return me->object;
    } else			/* Empty list */
	return NULL;
}

PUBLIC int HTList_count  (HTList * me)
{
    int count = 0;
    if (me)
	while ((me = me->next) != NULL)
	    count++;
    return count;
}

PUBLIC int HTList_indexOf (HTList * me, void * object)
{
    if (me) {
	int position = 0;
	while ((me = me->next) != NULL) {
	    if (me->object == object)
		return position;
	    position++;
	}
    }
    return -1;
}

PUBLIC void * HTList_objectAt  (HTList * me, int position)
{
    if (position < 0)
	return NULL;
    if (me) {
	while ((me = me->next) != NULL) {
	    if (position == 0)
		return me->object;
	    position--;
	}
    }
    return NULL;		/* Reached the end of the list */
}

PRIVATE void * HTList_removeObjectAt  (HTList * me, int position)
{
    if (position < 0)
	return NULL;
    if (me) {
	HTList * prevNode;
	prevNode = me;
	while ((me = me->next) != NULL) {
	    if (position == 0) {
		prevNode->next = me->next;
		return me->object;
	    }
	    prevNode = me;
	    position--;
	}
    }
    return NULL;  /* Reached the end of the list */
}
