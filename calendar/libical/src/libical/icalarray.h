/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*======================================================================
 FILE: icalarray.h
 CREATOR: Damon Chaplin 07 March 2001


 $Id: icalarray.h,v 1.3 2002/10/09 19:50:46 acampi Exp $
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


#ifndef ICALARRAY_H
#define ICALARRAY_H

/** @file icalarray.h 
 *
 *  @brief An array of arbitrarily-sized elements which grows
 *  dynamically as elements are added. 
 */

typedef struct _icalarray icalarray;
struct _icalarray {
    unsigned int	 element_size;
    unsigned int	 increment_size;
    unsigned int	 num_elements;
    unsigned int	 space_allocated;
    void		*data;
};



icalarray *icalarray_new		(int		 element_size,
					 int		 increment_size);
void	   icalarray_free		(icalarray	*array);

void	   icalarray_append		(icalarray	*array,
					 void		*element);
void	   icalarray_remove_element_at	(icalarray	*array,
					 int		 position);

void	  *icalarray_element_at		(icalarray	*array,
					 int		 position);

void	   icalarray_sort		(icalarray	*array,
					 int	       (*compare) (const void *, const void *));


#endif /* ICALARRAY_H */
