/* -*- Mode: C -*-
    ======================================================================
    FILE: icalspanlist.c
    CREATOR: ebusboom 23 aug 2000
  
    $Id: icalspanlist.c,v 1.13 2002/10/30 23:41:47 acampi Exp $
    $Locker:  $
    
    (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of either: 
    
    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html
    
    Or:
    
    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


    ======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ical.h"
#include "icalspanlist.h"

#include <stdlib.h> /* for free and malloc */
#include <string.h>

struct icalspanlist_impl {
  pvl_list spans;		/**< list of icaltime_span data **/
  struct icaltimetype start;    /**< start time of span **/
  struct icaltimetype end;	/**< end time of span **/
};

/** @brief Internal comparison function for two spans
 *
 *  @param  a   a spanlist.
 *  @param  b   another spanlist.
 *
 *  @return     -1, 0, 1 depending on the comparison of the start times.
 *
 * Used to insert spans into the tree in sorted order.
 */

static int compare_span(void* a, void* b)
{
    struct icaltime_span *span_a = (struct icaltime_span *)a ;
    struct icaltime_span *span_b = (struct icaltime_span *)b ;
    
    if(span_a->start == span_b->start){
	return 0;
    } else if(span_a->start < span_b->start){
	return -1;
    }  else { /*if(span_a->start > span->b.start)*/
	return 1;
    }
}


/** @brief callback function for collecting spanlists of a
 *         series of events.
 *
 *  @param   comp  A valid icalcomponent.
 *  @param   span  The span to insert into data.
 *  @param   data  The actual spanlist to insert into
 *
 *  This callback is used by icalcomponent_foreach_recurrence()
 *  to build up a spanlist.
 */

static void icalspanlist_new_callback(icalcomponent *comp,
					    struct icaltime_span *span,
					    void *data)
{
  icaltime_span *s;
  icalspanlist *sl = (icalspanlist*) data;

  if (span->is_busy == 0)
    return;

  if ((s=(icaltime_span *) malloc(sizeof(icaltime_span))) == 0) {
    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
    return;
  }

  /** copy span data into allocated memory.. **/
  *s = *span;
  pvl_insert_ordered(sl->spans, compare_span, (void*)s);
}
					    


/** @brief Make a free list from a set of VEVENT components.
 *
 *  @param set    A valid icalset containing VEVENTS
 *  @param start  The free list starts at this date/time
 *  @param end    The free list ends at this date/time
 *
 *  @return        A spanlist corresponding to the VEVENTS
 *
 * Given a set of components,  a start time and an end time
 * return a spanlist that contains the free/busy times.
 */

icalspanlist* icalspanlist_new(icalset *set, 
			       struct icaltimetype start,
			       struct icaltimetype end)
{
    struct icaltime_span range;
    pvl_elem itr;
    icalcomponent *c,*inner;
    icalcomponent_kind kind, inner_kind;
    icalspanlist *sl; 
    struct icaltime_span *freetime;

    if ( ( sl = (struct icalspanlist_impl*)
	   malloc(sizeof(struct icalspanlist_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    sl->spans =  pvl_newlist();
    sl->start =  start;
    sl->end   =  end;

    range.start = icaltime_as_timet(start);
    range.end = icaltime_as_timet(end);

    /* Get a list of spans of busy time from the events in the set
       and order the spans based on the start time */

   for(c = icalset_get_first_component(set);
	c != 0;
	c = icalset_get_next_component(set)){

       kind  = icalcomponent_isa(c);
       inner = icalcomponent_get_inner(c);

       if(!inner){
	   continue;
       }

       inner_kind = icalcomponent_isa(inner);

       if( kind != ICAL_VEVENT_COMPONENT &&
	   inner_kind != ICAL_VEVENT_COMPONENT){
	   continue;
       }
       
       icalerror_clear_errno();
       
       icalcomponent_foreach_recurrence(c, start, end, 
					icalspanlist_new_callback, 
					(void*)sl);
       
   }
    
    /* Now Fill in the free time spans. loop through the spans. if the
       start of the range is not within the span, create a free entry
       that runs from the start of the range to the start of the
       span. */

     for( itr = pvl_head(sl->spans);
	 itr != 0;
	 itr = pvl_next(itr))
    {
	struct icaltime_span *s = (struct icaltime_span*)pvl_data(itr);

	if ((freetime=(struct icaltime_span *)
	     malloc(sizeof(struct icaltime_span))) == 0){
	    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	    return 0;
	    }

	if(range.start < s->start){
	    freetime->start = range.start; 
	    freetime->end = s->start;
	    
	    freetime->is_busy = 0;


	    pvl_insert_ordered(sl->spans,compare_span,(void*)freetime);
	} else {
	    free(freetime);
	}
	
	range.start = s->end;
    }
     
     /* If the end of the range is null, then assume that everything
        after the last item in the calendar is open and add a span
        that indicates this */

     if( icaltime_is_null_time(end)){
	 struct icaltime_span* last_span;

	 last_span = (struct icaltime_span*)pvl_data(pvl_tail(sl->spans));

	 if (last_span != 0){

	     if ((freetime=(struct icaltime_span *)
		  malloc(sizeof(struct icaltime_span))) == 0){
		 icalerror_set_errno(ICAL_NEWFAILED_ERROR);
		 return 0;
	     }	
	
	     freetime->is_busy = 0;
	     freetime->start = last_span->end;
	     freetime->end = freetime->start;
	     pvl_insert_ordered(sl->spans,compare_span,(void*)freetime);
	 }
     }


     return sl;
}

/** @brief Destructor.
 *  @param s A valid icalspanlist
 *
 *  Free memory associated with the spanlist
 */

void icalspanlist_free(icalspanlist* s)
{
    struct icaltime_span *span;

    if (s == NULL)
      return;

    while( (span=pvl_pop(s->spans)) != 0){
	free(span);
    }
    
    pvl_free(s->spans);
    
    s->spans = 0;

    free(s);
}


/** @brief (Debug) print out spanlist to stdout.
 *  @param sl A valid icalspanlist.
 */

void icalspanlist_dump(icalspanlist* sl){
     int i = 0;
     pvl_elem itr;

     for( itr = pvl_head(sl->spans);
	 itr != 0;
	 itr = pvl_next(itr))
    {
	struct icaltime_span *s = (struct icaltime_span*)pvl_data(itr);
	
	printf("#%02d %d start: %s",++i,s->is_busy,ctime(&s->start));
	printf("      end  : %s",ctime(&s->end));

    }
}

icalcomponent* icalspanlist_make_free_list(icalspanlist* sl);
icalcomponent* icalspanlist_make_busy_list(icalspanlist* sl);


/** @brief Find next free time span in a spanlist.
 *
 *  @param  sl     The spanlist to search.
 *  @param  t      The time to start looking.
 *
 *  Given a spanlist and a time, find the next period of time
 *  that is free
 */

struct icalperiodtype icalspanlist_next_free_time(icalspanlist* sl,
						struct icaltimetype t)
{
     pvl_elem itr;
     struct icalperiodtype period;
     struct icaltime_span *s;

     time_t rangett= icaltime_as_timet(t);

     period.start = icaltime_null_time();
     period.end = icaltime_null_time();

     itr = pvl_head(sl->spans);
     s = (struct icaltime_span *)pvl_data(itr);

     if (s == 0){
	 /* No elements in span */
	 return period;
     }

     /* Is the reference time before the first span? If so, assume
        that the reference time is free */
     if(rangett <s->start ){
	 /* End of period is start of first span if span is busy, end
            of the span if it is free */
	 period.start =  t;

	 if (s->is_busy == 1){
	     period.end =  icaltime_from_timet(s->start,0);
	 } else {
	     period.end =  icaltime_from_timet(s->end,0);
	 }

	 return period;
     }

     /* Otherwise, find the first free span that contains the
        reference time. */
     for( itr = pvl_head(sl->spans);
	 itr != 0;
	 itr = pvl_next(itr))
    {
	s = (struct icaltime_span *)pvl_data(itr);

	if(s->is_busy == 0 && s->start >= rangett && 
	    ( rangett < s->end || s->end == s->start)){

	    if (rangett < s->start){
		period.start = icaltime_from_timet(s->start,0);
	    } else {
		period.start = icaltime_from_timet(rangett,0);
	    }
	    
	    period.end = icaltime_from_timet(s->end,0);

	    return period;
	}
			
    }

     period.start = icaltime_null_time();
     period.end = icaltime_null_time();

     return period;
}

struct icalperiodtype icalspanlist_next_busy_time(icalspanlist* sl,
						struct icaltimetype t);


/** @brief Returns an hour-by-hour array of free/busy times over a
 *         given period.
 *
 *  @param sl        A valid icalspanlist
 *  @param delta_t   The time slice to divide by, in seconds.  Default 3600.
 *  
 *  @return A pointer to an array of integers containing the number of
 *       busy events in each delta_t time period.  The final entry 
 *       contains the value -1.
 *
 *  This calculation is somewhat tricky.  This is due to the fact that
 *  the time range contains the start time, but does not contain the
 *  end time.  To perform a proper calculation we subtract one second
 *  off the end times to get a true containing time.
 * 
 *  Also note that if you supplying a spanlist that does not start or
 *  end on a time boundary divisible by delta_t you may get results
 *  that are not quite what you expect.
 */

int* icalspanlist_as_freebusy_matrix(icalspanlist* sl, int delta_t) {
  pvl_elem itr;
  int spanduration_secs;
  int *matrix;
  int matrix_slots;
  time_t sl_start, sl_end;

  icalerror_check_arg_rz( (sl!=0), "spanlist");

  if (!delta_t)
    delta_t = 3600;

  /** calculate the start and end time as time_t **/
  sl_start = icaltime_as_timet_with_zone(sl->start, icaltimezone_get_utc_timezone());
  sl_end   = icaltime_as_timet_with_zone(sl->end, icaltimezone_get_utc_timezone());


  /** insure that the time period falls on a time boundary divisable
      by delta_t */

  sl_start /= delta_t;
  sl_start *= delta_t;

  sl_end /= delta_t;
  sl_end *= delta_t;


  /** find the duration of this spanlist **/
  spanduration_secs = sl_end - sl_start;


  /** malloc our matrix, add one extra slot for a final -1 **/
  matrix_slots = spanduration_secs/delta_t + 1;

  matrix = (int*) malloc(sizeof(int) * matrix_slots);
  if (matrix == NULL) {
    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
    return NULL;
  }
  memset(matrix, 0, sizeof(int) * matrix_slots);
  matrix[matrix_slots-1] = -1;

  /* loop through each span and mark the slots in the array */

  for( itr = pvl_head(sl->spans);  itr != 0;  itr = pvl_next(itr)) {
    struct icaltime_span *s = (struct icaltime_span*)pvl_data(itr);
    
    if (s->is_busy == 1) {
      int offset_start = s->start/delta_t - sl_start/delta_t;
      int offset_end   = (s->end - 1) /delta_t  - sl_start/delta_t + 1;
      int i;
      
      if (offset_end >= matrix_slots)
	offset_end = matrix_slots - 1;

      i = offset_start;
      for (i=offset_start; i < offset_end; i++) {
	matrix[i]++;
      }
    }
  }
  return matrix;
}
    

/** @brief Return a VFREEBUSY component for the corresponding spanlist
 *
 *   @param sl         A valid icalspanlist, from icalspanlist_new()
 *   @param organizer  The organizer specified as MAILTO:user@domain
 *   @param attendee   The attendee specified as MAILTO:user@domain
 *
 *   @return            A valid icalcomponent or NULL.
 *
 * This function returns a VFREEBUSY component for the given spanlist.
 * The start time is mapped to DTSTART, the end time to DTEND.
 * Each busy span is represented as a separate FREEBUSY entry.
 * An attendee parameter is required, and organizer parameter is
 * optional.
 */

icalcomponent *icalspanlist_as_vfreebusy(icalspanlist* sl,
					 const char* organizer,
					 const char* attendee) {
  icalcomponent *comp;
  icalproperty *p;
  struct icaltimetype atime = icaltime_from_timet( time(0),0);
  pvl_elem itr;
  icaltimezone *utc_zone;
  icalparameter *param;

  if (!attendee) {
    icalerror_set_errno(ICAL_USAGE_ERROR);
    return 0;
  }

  utc_zone = icaltimezone_get_utc_timezone ();

  comp = icalcomponent_new_vfreebusy();
  
  icalcomponent_add_property(comp, icalproperty_new_dtstart(sl->start));
  icalcomponent_add_property(comp, icalproperty_new_dtend(sl->end));
  icalcomponent_add_property(comp, icalproperty_new_dtstamp(atime));

  if (organizer) {
    icalcomponent_add_property(comp, icalproperty_new_organizer(organizer));
  }
  icalcomponent_add_property(comp, icalproperty_new_attendee(attendee));

  /* now add the freebusy sections.. */

  for( itr = pvl_head(sl->spans);  itr != 0;  itr = pvl_next(itr)) {
    struct icalperiodtype period;
    struct icaltime_span *s = (struct icaltime_span*)pvl_data(itr);

    if (s->is_busy == 1) {

      period.start = icaltime_from_timet_with_zone (s->start, 0, utc_zone);
      period.end   = icaltime_from_timet_with_zone (s->end, 0, utc_zone);
      period.duration = icaldurationtype_null_duration();


      p = icalproperty_new_freebusy(period);
      param = icalparameter_new_fbtype(ICAL_FBTYPE_BUSY);
      icalproperty_add_parameter(p, param);

      icalcomponent_add_property(comp, p);
    }
    
  }

  return comp;
}


/** @brief Return a spanlist corresponding to the VFREEBUSY portion of
 *         an icalcomponent.
 *
 *   @param   c        A valid icalcomponent.
 *
 *   @return           A valid icalspanlist or NULL if no VFREEBUSY section.
 *
 */


icalspanlist *icalspanlist_from_vfreebusy(icalcomponent* comp)
{
  icalcomponent *inner;
  icalproperty  *prop;
  icalspanlist  *sl;

  icalerror_check_arg_rz((comp != NULL), "comp");
  
  inner = icalcomponent_get_inner(comp);
  if (!inner) return NULL;

  if ( ( sl = (icalspanlist*) malloc(sizeof(icalspanlist))) == 0) {
    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
    return 0;
  }
  sl->spans =  pvl_newlist();

  /* cycle through each FREEBUSY property, adding to the spanlist */
  for (prop = icalcomponent_get_first_property(inner, ICAL_FREEBUSY_PROPERTY);
       prop != NULL;
       prop = icalcomponent_get_next_property(inner, ICAL_FREEBUSY_PROPERTY)) {
    icaltime_span *s = (icaltime_span *) malloc(sizeof(icaltime_span));
    icalparameter *param;
    struct icalperiodtype period;
    icalparameter_fbtype fbtype;

    if (s == 0) {
      icalerror_set_errno(ICAL_NEWFAILED_ERROR);
      return 0;
    }
    
    param = icalproperty_get_first_parameter(prop, ICAL_FBTYPE_PARAMETER);
    fbtype = (param) ? icalparameter_get_fbtype(param) : ICAL_FBTYPE_BUSY;

    switch (fbtype) {
    case ICAL_FBTYPE_FREE:
    case ICAL_FBTYPE_NONE:
    case ICAL_FBTYPE_X:
      s->is_busy = 1;
    default:
      s->is_busy = 0;
    }
    
    period = icalproperty_get_freebusy(prop);
    s->start = icaltime_as_timet_with_zone(period.start, icaltimezone_get_utc_timezone());
    s->end = icaltime_as_timet_with_zone(period.end, icaltimezone_get_utc_timezone());
;
    pvl_insert_ordered(sl->spans, compare_span, (void*)s);
  }
  /** @todo calculate start/end limits.. fill in holes? **/
  return sl;
}
