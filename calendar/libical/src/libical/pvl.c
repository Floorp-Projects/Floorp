/*======================================================================
 FILE: pvl.c
 CREATOR: eric November, 1995


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pvl.h"
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

/**
  struct pvl_list_t

  The list structure. This is the hanlde for the entire list 

  This type is also private. Use pvl_list instead 

  */

typedef struct pvl_list_t
{
	int MAGIC;		        /**< Magic Identifier */
	struct pvl_elem_t *head;	/**< Head of list */
	struct pvl_elem_t *tail;	/**< Tail of list */
	int count;			/**< Number of items in the list */
	struct pvl_elem_t *p;		/**< Pointer used for iterators */
} pvl_list_t;




/**
 * This global is incremented for each call to pvl_new_element(); it gives each
 * list a unique identifer 
 */

int pvl_elem_count = 0;
int pvl_list_count = 0;


/**
 * @brief Creates a new list, clears the pointers and assigns a magic number
 * 
 * @return  Pointer to the new list, 0 if there is no available memory. 
 */

pvl_list 
pvl_newlist()
{
    struct pvl_list_t *L;

    if ( ( L = (struct pvl_list_t*)malloc(sizeof(struct pvl_list_t))) == 0)
    {
	errno = ENOMEM;
	return 0;
    }

    L->MAGIC = pvl_list_count;
    pvl_list_count++;
    L->head = 0;
    L->tail = 0;
    L->count = 0;
    L->p = 0;

    return L;
}

void
pvl_free(pvl_list l)
{
   struct pvl_list_t *L = (struct pvl_list_t *)l;

   pvl_clear(l);

   free(L);
}

/**
 * @brief Creates a new list element, assigns a magic number, and assigns 
 * the next and previous pointers. 
 * 
 * Passing in the next and previous points may seem odd, but it allos the user
 * to set them while keeping the internal data hidden. In nearly all cases, 
 * the user is the pvl library itself. 
 *
 * @param d	The data item to be stored in the list
 * @param next  Pointer value to assign to the member "next"
 * @param prior Pointer value to assign to the member "prior"
 *
 * @return A pointer to the new element, 0 if there is no memory available. 
 */

pvl_elem 
pvl_new_element(void *d, pvl_elem next, pvl_elem prior)
{
    struct pvl_elem_t *E;

    if ( ( E = (struct pvl_elem_t*)malloc(sizeof(struct pvl_elem_t))) == 0)
    {
	errno = ENOMEM;
	return 0;
    }

    E->MAGIC = pvl_elem_count++;
    E->d = d;
    E->next = next;
    E->prior = prior;

    return (pvl_elem)E;
}

/**
 * @brief Add a new element to the from of the list
 *
 * @param L	The list to add the item to 
 * @param d	Pointer to the item to add
 */

void 
pvl_unshift(pvl_list L,void *d)
{
    struct pvl_elem_t *E = pvl_new_element(d,L->head,0);

    if (E->next != 0)
    {
	/* Link the head node to it */
	E->next->prior = E;
    }

    /* move the head */
    L->head = E;

    /* maybe move the tail */
  
    if (L->tail == 0)
    {
	L->tail = E;
    }

    L->count++;
}

/**
 * @brief Remove an element from the front of the list 
 *
 * @param L	The list to operate on
 *
 * @return the entry on the front of the list 
 */

void* 
pvl_shift(pvl_list L)
{
    if (L->head == 0)
    {
	return 0;
    }

    return pvl_remove(L,(void*)L->head);

}

/**
 * @brief Add a new item to the tail of the list
 *
 * @param L	The list to operate on
 * @param d	Pointer to the item to add
 *
 */

void 
pvl_push(pvl_list L,void *d)
{
    struct pvl_elem_t *E = pvl_new_element(d,0,L->tail);

    /* These are done in pvl_new_element
       E->next = 0;
       E->prior = L->tail;
    */

    if (L->tail != 0) 
    {
	L->tail->next = E;
    }

    if (L->head == 0) 
    {
	L->head = E;
    }
  
    L->tail = E;

    L->count++;

}

/**
 * @brief Remove an element from the tail of the list 
 *
 * @param L	The list to operate on
 */

void* 
pvl_pop(pvl_list L)
{
    if ( L->tail == 0)
    {
	return 0;
    }

    return pvl_remove(L,(void*) L->tail);;

}


/**
 * Add a new item to a list that is ordered by a comparison function. 
 * This routine assumes that the list is properly ordered. 
 *
 * @param L	The list to operate on
 * @param f	Pointer to a comparison function
 * @param d     Pointer to data to pass to the comparison function
 */

void 
pvl_insert_ordered(pvl_list L,pvl_comparef f,void *d)
{
    struct pvl_elem_t *P;

    L->count++;

    /* Empty list, add to head */

    if(L->head == 0)
    {
	pvl_unshift(L,d);
	return;
    }

    /* smaller than head, add to head */

    if ( ((*f)(d,L->head->d)) <= 0)
    { 
	pvl_unshift(L,d);
	return;
    }

    /* larger than tail, add to tail */
    if ( (*f)(d,L->tail->d) >= 0)
    { 
	pvl_push(L,d);
	return;
    }


    /* Search for the first element that is smaller, and add before it */
    
    for (P=L->head; P != 0; P = P->next)
    {
	if ( (*f)(P->d,d) >= 0)
	{
	    pvl_insert_before(L,P,d);
	    return;
	}
    }

    /* badness, choke */
#ifndef lint
    assert(0);
#endif
}

/**
 * @brief Add a new item after the referenced element. 
 * @param L	The list to operate on 
 * @param P	The list element to add the item after
 * @param d	Pointer to the item to add. 
 */

void 
pvl_insert_after(pvl_list L,pvl_elem P,void *d)
{
    struct pvl_elem_t *E = 0;

    L->count++;

    if (P == 0)
    {
	pvl_unshift(L,d);
	return;
    }

    if ( P == L->tail)
    {
	E = pvl_new_element(d,0,P);
	L->tail = E;
	E->prior->next = E;
    }
    else
    {
	E = pvl_new_element(d,P->next,P);
	E->next->prior  = E;
	E->prior->next = E;
    }
}

/**
 * @brief Add an item after a referenced item
 *
 * @param L	The list to operate on
 * @param P	The list element to add the item before
 * @param d	Pointer to the data to be added. 
 */

void 
pvl_insert_before(pvl_list L,pvl_elem P,void *d)
{
    struct pvl_elem_t *E = 0;

    L->count++;

    if (P == 0)
    {
	pvl_unshift(L,d);
	return;
    }

    if ( P == L->head)
    {
	E = pvl_new_element(d,P,0);
	E->next->prior = E;
	L->head = E;
    }
    else
    {
	E = pvl_new_element(d,P,P->prior);
	E->prior->next = E;
	E->next->prior = E;
    }
}

/**
 * @brief Remove the referenced item from the list.
 *
 * This routine will free the element, but not the data item that the
 * element contains. 
 *
 * @param L	The list to operate on
 * @param E	The element to remove. 
 */

void* 
pvl_remove(pvl_list L,pvl_elem E)
{
    void* data;

    if (E == L->head)
    {
	if (E->next != 0)
	{
	    E->next->prior = 0;
	    L->head = E->next;
	} else {
	    /* E Also points to tail -> only one element in list */
	    L->tail = 0;
	    L->head = 0;
	}
    }
    else if (E == L->tail)
    {
	if (E->prior != 0)
	{
	    E->prior->next = 0;
	    L->tail = E->prior;
	} else {
	    /* E points to the head, so it was the last element */
	    /* This case should be taken care of in the previous clause */
	    L->head = 0;
	    L->tail = 0;
	}
    }
    else
    {
	E->prior->next = E->next;
	E->next->prior = E->prior;
    }


    L->count--;

    data = E->d; 

    E->prior = 0;
    E->next = 0;
    E->d = 0;

    free(E);

    return data;

}

/**
 * @brief Return a pointer to data that satisfies a function.
 *
 * This routine will interate through the entire list and call the
 * find function for each item. It will break and return a pointer to the
 * data that causes the find function to return 1.
 *
 * @param l	The list to operate on
 * @param f	Pointer to the find function
 * @param v	Pointer to constant data to pass into the function
 * 
 * @return Pointer to the element that the find function found. 
 */

pvl_elem
pvl_find(pvl_list l,pvl_findf f,void* v)
{
    pvl_elem e;
    
    for (e=pvl_head(l); e!= 0; e = pvl_next(e))
    {
	if ( (*f)(((struct pvl_elem_t *)e)->d,v) == 1)
	{
	    /* Save this elem for a call to find_next */
	    ((struct pvl_list_t *)l)->p = e;
	    return e;
	}
    }
    
    return 0;

}

/**
 * @brief Like pvl_find(), but continues the search where the last find() or
 * find_next() left off.
 *
 * @param l	The list to operate on
 * @param f	Pointer to the find function
 * @param v	Pointer to constant data to pass into the function
 * 
 * @return Pointer to the element that the find function found. 
 */

pvl_elem
pvl_find_next(pvl_list l,pvl_findf f,void* v)
{
    
    pvl_elem e;
    
    for (e=pvl_head(l); e!= 0; e = pvl_next(e))
    {
	if ( (*f)(((struct pvl_elem_t *)e)->d,v) == 1)
	{
	    /* Save this elem for a call to find_next */
	    ((struct pvl_list_t *)l)->p = e;
	    return e;
	}
    }

    return 0;

}

/**
 * @brief Remove the all the elements in the list. The does not free
 * the data items the elements hold.
 */

void 
pvl_clear(pvl_list l)
{
    pvl_elem e = pvl_head(l);
    pvl_elem next;

    if (e == 0) {
	return;
    }

    while(e != 0)
    {
	next = pvl_next(e);
	pvl_remove(l,e);
	e = next;
    }
}


/**
 * @brief Returns the number of items in the list. 
 */

int 
pvl_count(pvl_list L)
{
    return L->count;
}


/**
 * @brief Returns a pointer to the given element 
 */

pvl_elem 
pvl_next(pvl_elem E)
{
    if (E == 0){
	return 0;
    }

    return (pvl_elem)E->next;
}


/**
 * @brief Returns a pointer to the element previous to the element given. 
 */

pvl_elem 
pvl_prior(pvl_elem E)
{
    return (pvl_elem)E->prior;
}


/**
 * @brief Returns a pointer to the first item in the list. 
 */

pvl_elem 
pvl_head(pvl_list L )
{
    return (pvl_elem)L->head;
}

/**
 * @brief Returns a pointer to the last item in the list. 
 */
pvl_elem 
pvl_tail(pvl_list L)
{
    return (pvl_elem)L->tail;
}

#ifndef PVL_USE_MACROS
void* 
pvl_data(pvl_elem E)
{
    if ( E == 0){
	return 0;
    }

    return E->d;
}
#endif

/**
 * @brief Call a function for every item in the list. 
 *
 * @param l	The list to operate on
 * @param f	Pointer to the function to call
 * @param v	Data to pass to the function on every iteration
 */

void 
pvl_apply(pvl_list l,pvl_applyf f, void *v)
{
    pvl_elem e;

    for (e=pvl_head(l); e!= 0; e = pvl_next(e))
    {
	(*f)(((struct pvl_elem_t *)e)->d,v);
    }

}
