/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaldirset.h
 CREATOR: eric 28 November 1999


 $Id: icaldirset.h,v 1.6 2002/06/27 02:30:58 acampi Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

#ifndef ICALDIRSET_H
#define ICALDIRSET_H

#include "ical.h"
#include "icalset.h"
#include "icalcluster.h"
#include "icalgauge.h"

/* icaldirset Routines for storing, fetching, and searching for ical
 * objects in a database */

typedef struct icaldirset_impl icaldirset;

icalset* icaldirset_new(const char* path);

icalset* icaldirset_new_reader(const char* path);
icalset* icaldirset_new_writer(const char* path);


icalset* icaldirset_init(icalset* set, const char *dsn, void *options);
void icaldirset_free(icalset* set);

const char* icaldirset_path(icalset* set);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately*/
void icaldirset_mark(icalset* set);
icalerrorenum icaldirset_commit(icalset* set);

icalerrorenum icaldirset_add_component(icalset* store, icalcomponent* comp);
icalerrorenum icaldirset_remove_component(icalset* store, icalcomponent* comp);

int icaldirset_count_components(icalset* store,
			       icalcomponent_kind kind);

/* Restrict the component returned by icaldirset_first, _next to those
   that pass the gauge. _clear removes the gauge. */
icalerrorenum icaldirset_select(icalset* store, icalgauge* gauge);
void icaldirset_clear(icalset* store);

/* Get a component by uid */
icalcomponent* icaldirset_fetch(icalset* store, const char* uid);
int icaldirset_has_uid(icalset* store, const char* uid);
icalcomponent* icaldirset_fetch_match(icalset* set, icalcomponent *c);

/* Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icaldirset_modify(icalset* store, icalcomponent *oldc,
			       icalcomponent *newc);

/* Iterate through the components. If a gauge has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icaldirset_get_current_component(icalset* store);
icalcomponent* icaldirset_get_first_component(icalset* store);
icalcomponent* icaldirset_get_next_component(icalset* store);

/* External iterator for thread safety */
icalsetiter icaldirset_begin_component(icalset* set, icalcomponent_kind kind, icalgauge* gauge);
icalcomponent* icaldirsetiter_to_next(icalset* set, icalsetiter* i);
icalcomponent* icaldirsetiter_to_prior(icalset* set, icalsetiter* i);

typedef struct icaldirset_options {
  int flags;			/**< flags corresponding to the open() system call O_RDWR, etc. */
} icaldirset_options;

#endif /* !ICALDIRSET_H */



