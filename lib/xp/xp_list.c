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
  inked list manipulation routines
 
  this is a very standard linked list structure
  used by many many programmers all over the world

  The lists have been modified to be doubly linked.  The
    first element in a list is always the header.  The 'next'
	pointer of the header is the first element in the list.
	The 'prev' pointer of the header is the last element in
	the list.

  The 'prev' pointer of the first real element in the list
    is NULL as is the 'next' pointer of the last real element
	in the list
 
 */

#include "xp.h"

#ifdef PROFILE
#pragma profile on
#endif

/*
  Delete all of the elements in the list.
  This function does *NOT* free the objects in the list, just the
    list structure itself (seems like a bug...)
*/
PUBLIC void 
XP_ListDestroy (XP_List *list)
{
    XP_List *list_ptr;

    while (list) 
      {
        list_ptr = list;
        list = list->next;
        XP_FREE(list_ptr);
      }
}

/*
  Create a new list
*/
PUBLIC XP_List * 
XP_ListNew (void)
{
	XP_List *new_struct = XP_NEW(XP_List);

	if (!new_struct) 
		return(NULL);

	new_struct->object = NULL;
	new_struct->next   = NULL;
	new_struct->prev   = NULL;
	return new_struct;
}

/*
  Add an object to the front of the list (which could also be the end)
*/
PUBLIC void 
XP_ListAddObject (XP_List *list, void *object)
{

	XP_ASSERT (list);

	if (list) {
		XP_List *new_struct = XP_NEW(XP_List);

		if (!new_struct) 
		  return;

		/* set back pointer of old first element */
		if(list->next) {
			list->next->prev = new_struct;
		}

		/* we might have a next but since we are the first we WONT have a previous */
		new_struct->object = object;
		new_struct->next = list->next;
		new_struct->prev = NULL;

		/* the new element is the first one */
		list->next = new_struct;

		/* if this is the only element it is the end too */
		if(NULL == list->prev)
			list->prev = new_struct;

	}
}

/*
  Add an object to the end of the list
*/
PUBLIC void
XP_ListAddObjectToEnd (XP_List *list, void *object)
{
	XP_List * new_struct;

	if(NULL == list)
		return;

	/* handle the case where the list is empty */
	if(NULL == list->prev) {
		XP_ListAddObject(list, object);
		return;
	}
		
	XP_ASSERT(list->prev);

	/* check for out of memory */
	new_struct = XP_NEW(XP_List);
	if (!new_struct) 
	  return;

	/* initialize our pointers */
	new_struct->prev   = list->prev;
	new_struct->next   = NULL;
	new_struct->object = object;

	/* make old last element point to us */
	list->prev->next = new_struct;

	/* update end of list pointer */
	list->prev = new_struct;

}

/*
  Return the 'object' pointer of the last list entry
*/
PUBLIC void *
XP_ListGetEndObject (XP_List *list)
{
    if (list && list->prev)
		return(list->prev->object);
	else
		return(NULL);
}

/* 
  Dunno what this is doing since it doesn't need to be rewritten
 */
PUBLIC void *
XP_ListGetObjectNum (XP_List *list, int num)
{
	if(list == NULL)
		return(NULL);

	if(num < 1)
		return(NULL);

	list = list->next;  /* the first list never has data */

	for(; list && num > 1; num--)
	  {
		list = list->next;
	  }

	if(list)
	    return(list->object);
	else
	    return(NULL);
}

/* move the top object to the bottom of the list
 * this function is useful for reordering the list
 * so that a round robin ordering can occur
 */
PUBLIC void
XP_ListMoveTopToBottom (XP_List *list)
{
	XP_List *obj1;
	XP_List *prev_end;
	XP_List *next_obj;

	if(!list->next || list->next == list->prev)
		return;  /* no action required */

	/* move obj1 to the end */
	obj1     = list->next;
	next_obj = obj1->next;
	prev_end = list->prev;

	list->next     = next_obj;
	list->prev     = obj1;
	obj1->prev     = prev_end;
	obj1->next     = NULL;
	prev_end->next = obj1;

	if(next_obj)
		next_obj->prev = list;
}

/*
  Comment goes here
*/
PUBLIC int
XP_ListGetNumFromObject (XP_List *list, void * object)
{
	int count = 0;
	void * obj2;

	while((obj2 = XP_ListNextObject(list)) != NULL)
	  {
		count++;
		if(obj2 == object)
			return(count);
	  }

	return(0); /* not found */
}
		
/* returns the list node of the specified object if it was 
 * in the list
 */
PUBLIC XP_List *
XP_ListFindObject (XP_List *list, void * obj)
{
    if(!list)
        return(NULL);

    list = list->next;  /* the first list item never has data */

    while(list)
      {
		if(list->object == obj)
			return(list);
        list = list->next;
      }

    return(NULL);
}

/*
  Count the number of items in the list
  Should we cache this in the header?  maybe in header->object
*/
PUBLIC int 
XP_ListCount (XP_List *list)
{
  int count = 0;

  if (list)
    while ((list = list->next) != NULL)
      count++;

  return count;
}

/*
  Insert an object somewhere into the list.
  If insert_before is not NULL find the element whose 'object' is equal to
    insert_before and add a new element in front of that element.
*/
PUBLIC void
XP_ListInsertObject (XP_List *list, void *insert_before, void *object)
{
    XP_List *temp;
    XP_List *new_struct;

	XP_ASSERT(list);

	if (list) {
		if(NULL == insert_before) {
			XP_ListAddObjectToEnd(list, object);
			return;
		}

        /* find the prev pointer so we can add the forward pointer to
         * the new object
         */
        temp = list;

		/* walk down the list till we find the element we want */
        for(temp = list; temp; temp = temp->next)
        	if(temp->object == insert_before)
				break;

		/*
		  We got all the way though and didn't find the element we wanted
		    so just stick the new element on the end of the list
		*/
        if(!temp) {
            XP_ListAddObjectToEnd(list, object);
            return;
        }

        new_struct = XP_NEW(XP_List);
        if (!new_struct)
            return;

		/* set our values */
        new_struct->object = object;
		new_struct->next   = temp;
		new_struct->prev   = temp->prev;

		/* there is either a previous element or we are the new first element */
		if(temp->prev)
			temp->prev->next = new_struct;
		else
			list->next = new_struct;

		temp->prev = new_struct;

    }
}

/*
  Insert an object somewhere into the list.
  If insert_after is not NULL find the element whose 'object' is equal to
    insert_after and add a new element in back of that element.
*/
PUBLIC void
XP_ListInsertObjectAfter (XP_List *list, void *insert_after, void *object)
{
    XP_List *tmp; 
    XP_List *new_struct;

    if(list) {

		/* if no element to insert after just add to the end of the list */
        if(NULL == insert_after) {
            XP_ListAddObjectToEnd(list, object);
            return;
        }
            
            /* check that insert_after is a valid list entry */
		for(tmp = list; tmp; tmp = tmp->next)
			if(tmp->object == insert_after)
				break;

		/* the object we were asked to insert after is not actually in the
		   list.  Add the new item to the end
		 */
        if(!tmp) {
            XP_ListAddObjectToEnd(list, object);
            return;
        }

		/* creat a new list struct and insert it */
        new_struct = XP_NEW(XP_List);
        if (!new_struct)
            return;

        new_struct->object = object;
        new_struct->next = tmp->next;
        new_struct->prev = tmp;

		/* we either have a next element or we are the last one */
		if(tmp->next)
			tmp->next->prev = new_struct;
		else
			list->prev = new_struct;

		tmp->next = new_struct;

    }
}


/*
  Remove the element whose 'object' field is equal to remove_me
  Return TRUE if such an element was remove else FALSE
*/
PUBLIC Bool 
XP_ListRemoveObject (XP_List *list, void *remove_me)
{
	XP_List * tmp;

	/* check if list is empty */
	if(NULL == list)
		return(FALSE);

	/* walk down list looking for object we want */
	for(tmp = list; tmp; tmp = tmp->next)
		if(tmp->object == remove_me)
			break;

	/* element was not found */
	if(NULL == tmp)
		return(FALSE);

	/* we either have a prev or we were the first element */
	if(tmp->prev)
		tmp->prev->next = tmp->next;
	else
		list->next = tmp->next;

	/* we either have a next or we were the last element */
	if(tmp->next)
		tmp->next->prev = tmp->prev;
	else
		list->prev = tmp->prev;

	/* OK we have updated all of the pointers now free the element */
	XP_FREE (tmp);
	return TRUE; 

}

/*
  Remove the last element in the list and return it
*/
PUBLIC void * 
XP_ListRemoveEndObject (XP_List *list)
{
	XP_List * last;
	void    * data;

	/* check if bogus list */
	if(NULL == list)
		return(NULL);

	/* check if no last element (bug or empty list) */
	if(NULL == list->prev)
		return(NULL);

	last = list->prev;

	/* 
	  there is either a previous element or we were the first and
	  only element 
	 */  	
	if(last->prev)	
		last->prev->next = NULL;
	else
		list->next = NULL;

	/* always a new last element */
	list->prev = last->prev;

	/* remember the data pointer so we can return it */
	data = last->object;
    XP_FREE(last);
    return(data);

}

/*
  Remove the first element in the list and return its 'object'
*/
PUBLIC void * 
XP_ListRemoveTopObject (XP_List *list)
{
	XP_List * first;
	void    * data;

	/* check if bogus list */
	if(NULL == list)
		return(NULL);

	/* check if no first element (bug or empty list) */
	if(NULL == list->next)
		return(NULL);

	first = list->next;

	/*
	  There is either a next element or we were the last element too
	*/
	if(first->next)
		first->next->prev = NULL;
	else
		list->prev = NULL;

	/* new first element */
	list->next = first->next;

	/* remember the data pointer so we can return it */
	data = first->object;
    XP_FREE(first);
    return(data);
    
}

PUBLIC void * 
XP_ListPeekTopObject (XP_List *list)
{
  if (list && list->next) 
    {
      return list->next->object;
    } 
  else  /* Empty list */
    {
      return NULL;
    } 
}

#ifdef PROFILE
#pragma profile off
#endif

