/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalfileset.h
 CREATOR: eric 23 December 1999


 $Id: icalfileset.h,v 1.13 2002/12/05 13:45:20 acampi Exp $
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

#ifndef ICALFILESET_H
#define ICALFILESET_H

#include "ical.h"
#include "icalset.h"
#include "icalcluster.h"
#include "icalgauge.h"
#include <sys/types.h> /* For open() flags and mode */
#include <sys/stat.h> /* For open() flags and mode */
#include <fcntl.h> /* For open() flags and mode */

#ifdef WIN32
#define mode_t int
#endif

typedef struct icalfileset_impl icalfileset;

icalset* icalfileset_new(const char* path);
icalset* icalfileset_new_reader(const char* path);
icalset* icalfileset_new_writer(const char* path);

icalset* icalfileset_init(icalset *set, const char *dsn, void* options);

icalfileset* icalfileset_new_from_cluster(const char* path, icalcluster *cluster);

icalcluster* icalfileset_produce_icalcluster(const char *path);

void icalfileset_free(icalset* cluster);

const char* icalfileset_path(icalset* cluster);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately. */
void icalfileset_mark(icalset* set);
icalerrorenum icalfileset_commit(icalset* set); 

icalerrorenum icalfileset_add_component(icalset* set,
					icalcomponent* child);

icalerrorenum icalfileset_remove_component(icalset* set,
					   icalcomponent* child);

int icalfileset_count_components(icalset* set,
				 icalcomponent_kind kind);

/**
 * Restrict the component returned by icalfileset_first, _next to those
 * that pass the gauge. _clear removes the gauge 
 */
icalerrorenum icalfileset_select(icalset* set, icalgauge* gauge);

/** clear the gauge **/
void icalfileset_clear(icalset* set);

/** Get and search for a component by uid **/
icalcomponent* icalfileset_fetch(icalset* set, const char* uid);
int icalfileset_has_uid(icalset* set, const char* uid);
icalcomponent* icalfileset_fetch_match(icalset* set, icalcomponent *c);


/**
 *  Modify components according to the MODIFY method of CAP. Works on the
 *  currently selected components. 
 */
icalerrorenum icalfileset_modify(icalset* set, 
				 icalcomponent *oldcomp,
			       icalcomponent *newcomp);

/* Iterate through components. If a gauge has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icalfileset_get_current_component (icalset* cluster);
icalcomponent* icalfileset_get_first_component(icalset* cluster);
icalcomponent* icalfileset_get_next_component(icalset* cluster);

/* External iterator for thread safety */
icalsetiter icalfileset_begin_component(icalset* set, icalcomponent_kind kind, icalgauge* gauge);
icalcomponent * icalfilesetiter_to_next(icalset* set, icalsetiter *iter);
icalcomponent* icalfileset_form_a_matched_recurrence_component(icalsetiter* itr);

/** Return a reference to the internal component. You probably should
   not be using this. */

icalcomponent* icalfileset_get_component(icalset* cluster);

/** 
 * @brief options for opening an icalfileset.
 *
 * These options should be passed to the icalset_new() function
 */

typedef struct icalfileset_options {
  int          flags;		/**< flags for open() O_RDONLY, etc  */
  mode_t       mode;		/**< file mode */
  icalcluster  *cluster;	/**< use this cluster to initialize data */
} icalfileset_options;

extern icalfileset_options icalfileset_options_default;

#endif /* !ICALFILESET_H */



