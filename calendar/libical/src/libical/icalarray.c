/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*-
  ======================================================================
  FILE: icalarray.c
  CREATOR: Damon Chaplin 07 March 2001
  
  $Id: icalarray.c,v 1.5 2002/06/28 09:30:15 acampi Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2001, Ximian, Inc.

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


 ======================================================================*/

/** @file icalarray.c
 *
 *  @brief An array of arbitrarily-sized elements which grows
 *  dynamically as elements are added. 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "icalarray.h"
#include "icalerror.h"


static void icalarray_expand		(icalarray	*array,
					 int		 space_needed);

/** @brief Constructor
 */

icalarray*
icalarray_new			(int		 element_size,
				 int		 increment_size)
{
    icalarray *array;

    array = (icalarray*) malloc (sizeof (icalarray));
    if (!array) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return NULL;
    }

    array->element_size = element_size;
    array->increment_size = increment_size;
    array->num_elements = 0;
    array->space_allocated = 0;
    array->data = NULL;

    return array;
}

/** @brief Destructor
 */

void
icalarray_free			(icalarray	*array)
{
    if (array->data)
	free (array->data);
    free (array);
}


void
icalarray_append		(icalarray	*array,
				 void		*element)
{
    if (array->num_elements >= array->space_allocated)
	icalarray_expand (array, 1);

    memcpy ((char *)(array->data) + ( array->num_elements * array->element_size ), element,
	    array->element_size);
    array->num_elements++;
}


void*
icalarray_element_at		(icalarray	*array,
				 int		 position)
{
    assert (position >= 0);
    assert (position < array->num_elements);

    return (char *)(array->data) + (position * array->element_size);
}


void
icalarray_remove_element_at	(icalarray	*array,
				 int		 position)
{
    void *dest;
    int elements_to_move;

    assert (position >= 0);
    assert (position < array->num_elements);

    dest = (char *)array->data + (position * array->element_size);
    elements_to_move = array->num_elements - position - 1;

    if (elements_to_move > 0)
	memmove (dest, (char *)dest + array->element_size,
		 elements_to_move * array->element_size);

    array->num_elements--;
}


void
icalarray_sort			(icalarray	*array,
				 int	       (*compare) (const void *,
							   const void *))
{
    qsort (array->data, array->num_elements, array->element_size, compare);
}


static void
icalarray_expand		(icalarray	*array,
				 int		 space_needed)
{
    int new_space_allocated;
    void *new_data;

    new_space_allocated = array->space_allocated + array->increment_size;

    if (space_needed > array->increment_size) 
	new_space_allocated += space_needed;

	/*
    new_data = realloc (array->data,
			new_space_allocated * array->element_size);
	*/
	new_data = malloc(new_space_allocated * array->element_size);
	memcpy(new_data,array->data,array->element_size*array->space_allocated);
	free(array->data);

    if (new_data) {
	array->data = new_data;
	array->space_allocated = new_space_allocated;
    } else {
	icalerror_set_errno(ICAL_ALLOCATION_ERROR);
    }
}


