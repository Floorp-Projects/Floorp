/* -*- Mode: C -*- */
/**
 @file icalset.h
 @author eric 28 November 1999

 Icalset is the "base class" for representations of a collection of
 iCal components. Derived classes (actually delegatees) include:
 
    icalfileset   Store components in a single file
    icaldirset    Store components in multiple files in a directory
    icalbdbset    Store components in a Berkeley DB File
    icalheapset   Store components on the heap
    icalmysqlset  Store components in a mysql database. 
**/

/*
 $Id: icalset.h,v 1.13 2002/09/26 22:25:12 lindner Exp $
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

#ifndef ICALSET_H
#define ICALSET_H

#include <limits.h> /* For PATH_MAX */
#include "ical.h"
#include "icalgauge.h"

#ifdef PATH_MAX
#define ICAL_PATH_MAX PATH_MAX
#else
#define ICAL_PATH_MAX 1024
#endif


typedef struct icalset_impl icalset;

typedef enum icalset_kind {
    ICAL_FILE_SET,
    ICAL_DIR_SET,
    ICAL_BDB_SET
} icalset_kind;

typedef struct icalsetiter
{
	icalcompiter iter;    /* icalcomponent_kind, pvl_elem iter */
	icalgauge* gauge;
        icalrecur_iterator* ritr; /*the last iterator*/
        icalcomponent* last_component; /*the pending recurring component to be processed  */
	const char* tzid; /* the calendar's timezone id */
} icalsetiter;

struct icalset_impl {
        icalset_kind kind;
	int size;
        char *dsn;
        icalset* (*init)(icalset* set, const char *dsn, void *options);
	void (*free)(icalset* set);
	const char* (*path)(icalset* set);
	void (*mark)(icalset* set);
	icalerrorenum (*commit)(icalset* set); 
	icalerrorenum (*add_component)(icalset* set, icalcomponent* comp);
	icalerrorenum (*remove_component)(icalset* set, icalcomponent* comp);
	int (*count_components)(icalset* set,
			     icalcomponent_kind kind);
	icalerrorenum (*select)(icalset* set, icalgauge* gauge);
	void (*clear)(icalset* set);
	icalcomponent* (*fetch)(icalset* set, const char* uid);
	icalcomponent* (*fetch_match)(icalset* set, icalcomponent *comp);
	int (*has_uid)(icalset* set, const char* uid);
	icalerrorenum (*modify)(icalset* set, icalcomponent *old,
				     icalcomponent *newc);
	icalcomponent* (*get_current_component)(icalset* set);	
	icalcomponent* (*get_first_component)(icalset* set);
	icalcomponent* (*get_next_component)(icalset* set);
	icalsetiter (*icalset_begin_component)(icalset* set,
					 icalcomponent_kind kind, icalgauge* gauge);
	icalcomponent* (*icalsetiter_to_next)(icalset* set, icalsetiter* i);
	icalcomponent* (*icalsetiter_to_prior)(icalset* set, icalsetiter* i);
};

/** @brief Register a new derived class */
int icalset_register_class(icalset *set);


/** @brief Generic icalset constructor
 *  
 *  @param kind     The type of icalset to create
 *  @param dsn      Data Source Name - usually a pathname or DB handle
 *  @param options  Any implementation specific options
 *
 *  @return         A valid icalset reference or NULL if error.
 * 
 *  This creates any of the icalset types available.
 */

icalset* icalset_new(icalset_kind kind, const char* dsn, void* options);

icalset* icalset_new_file(const char* path);
icalset* icalset_new_file_reader(const char* path);
icalset* icalset_new_file_writer(const char* path);

icalset* icalset_new_dir(const char* path);
icalset* icalset_new_file_reader(const char* path);
icalset* icalset_new_file_writer(const char* path);

void icalset_free(icalset* set);

const char* icalset_path(icalset* set);

/** Mark the cluster as changed, so it will be written to disk when it
    is freed. **/
void icalset_mark(icalset* set);

/** Write changes to disk immediately */
icalerrorenum icalset_commit(icalset* set); 

icalerrorenum icalset_add_component(icalset* set, icalcomponent* comp);
icalerrorenum icalset_remove_component(icalset* set, icalcomponent* comp);

int icalset_count_components(icalset* set,
			     icalcomponent_kind kind);

/** Restrict the component returned by icalset_first, _next to those
    that pass the gauge. */
icalerrorenum icalset_select(icalset* set, icalgauge* gauge);

/** Clears the gauge defined by icalset_select() */
void icalset_clear_select(icalset* set);

/** Get a component by uid */
icalcomponent* icalset_fetch(icalset* set, const char* uid);

int icalset_has_uid(icalset* set, const char* uid);
icalcomponent* icalset_fetch_match(icalset* set, icalcomponent *c);

/** Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icalset_modify(icalset* set, icalcomponent *oldc,
			       icalcomponent *newc);

/** Iterate through the components. If a guage has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icalset_get_current_component(icalset* set);
icalcomponent* icalset_get_first_component(icalset* set);
icalcomponent* icalset_get_next_component(icalset* set);

/** External Iterator with gauge - for thread safety */
extern icalsetiter icalsetiter_null;

icalsetiter icalset_begin_component(icalset* set,
				 icalcomponent_kind kind, icalgauge* gauge);

/** Default _next, _prior, _deref for subclasses that use single cluster */
icalcomponent* icalsetiter_next(icalsetiter* i);
icalcomponent* icalsetiter_prior(icalsetiter* i);
icalcomponent* icalsetiter_deref(icalsetiter* i);

/** for subclasses that use multiple clusters that require specialized cluster traversal */
icalcomponent* icalsetiter_to_next(icalset* set, icalsetiter* i);
icalcomponent* icalsetiter_to_prior(icalset* set, icalsetiter* i);

#endif /* !ICALSET_H */



