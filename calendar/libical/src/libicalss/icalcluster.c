/* -*- Mode: C -*-
  ======================================================================
  FILE: icalcluster.c
  CREATOR: acampi 13 March 2002
  
  $Id: icalcluster.c,v 1.3 2002/06/28 10:15:39 acampi Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2002, Eric Busboom, http://www.softwarestudio.org

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


/**
 *
 * icalcluster is an utility class design to manage clusters of
 * icalcomponents on behalf of an implementation of icalset. This is
 * done in order to split out common behavior different classes might
 * need.
 * The definition of what exactly a cluster will contain depends on the
 * icalset subclass. At the basic level, an icluster is just a tuple,
 * with anything as key and an icalcomponent as value.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#if 0
#include <errno.h>
#include <sys/stat.h> /* for stat */
#ifndef WIN32
#include <unistd.h> /* for stat, getpid */
#else
#include <io.h>
#include <share.h>
#endif
#include <fcntl.h> /* for fcntl */
#endif

#include "icalcluster.h"
#include "icalclusterimpl.h"
#include "icalgauge.h"

#ifdef WIN32
#define snprintf	_snprintf
#define strcasecmp	stricmp
#endif


icalcluster * icalcluster_new_impl(void) {

	struct icalcluster_impl* impl;
  
	if ((impl = (struct icalcluster_impl*)malloc(
				sizeof(struct icalcluster_impl))) == 0) {
		icalerror_set_errno(ICAL_NEWFAILED_ERROR);
		return 0;
	}
  
	memset(impl, 0, sizeof(struct icalcluster_impl));
	strcpy(impl->id,ICALCLUSTER_ID);

	return impl;
}

/**
 * Create a cluster with a key/value pair.
 *
 * @todo Always do a deep copy.
 */

icalcluster * icalcluster_new(const char* key, icalcomponent *data) {
	struct icalcluster_impl *impl = icalcluster_new_impl();
	assert(impl->data == 0);

	impl->key	= strdup(key);
	impl->changed	= 0;
	impl->data	= 0;

	if (data != NULL) {
		if (icalcomponent_isa(data) != ICAL_XROOT_COMPONENT) {
			impl->data = icalcomponent_new(ICAL_XROOT_COMPONENT);
			icalcomponent_add_component(impl->data, data);
		} else {
			impl->data = icalcomponent_new_clone(data);
		}
	} else {
		impl->data = icalcomponent_new(ICAL_XROOT_COMPONENT);
	}

	return impl;
}

/**
 * Deep clone an icalcluster to a new one
 */

icalcluster *icalcluster_new_clone(const icalcluster *data) {
	struct icalcluster_impl *old = (struct icalcluster_impl *)data;
	struct icalcluster_impl *impl = icalcluster_new_impl();

	impl->key	= strdup(old->key);
	impl->data	= icalcomponent_new_clone(old->data);
	impl->changed	= 0;

	return impl;
}


void icalcluster_free(icalcluster *impl) {
	icalerror_check_arg_rv((impl!=0),"cluster");

	if (impl->key != 0){
		free(impl->key);
		impl->key = 0;
	}

	if (impl->data != 0){
		icalcomponent_free(impl->data);
		impl->data = 0;
	}

	free(impl);
}


const char *icalcluster_key(icalcluster *impl) {
	icalerror_check_arg_rz((impl!=0),"cluster");

	return impl->key;
}


int icalcluster_is_changed(icalcluster *impl) {
	icalerror_check_arg_rz((impl!=0),"cluster");

	return impl->changed;
}


void icalcluster_mark(icalcluster *impl) {
	icalerror_check_arg_rv((impl!=0),"cluster");

	impl->changed = 1;
}


void icalcluster_commit(icalcluster *impl) {
	icalerror_check_arg_rv((impl!=0),"cluster");

	impl->changed = 0;
}


icalcomponent *icalcluster_get_component(icalcluster *impl) {

	icalerror_check_arg_rz((impl!=0),"cluster");

	if (icalcomponent_isa(impl->data) != ICAL_XROOT_COMPONENT) {
		icalerror_warn("The top component is not an XROOT");
		fprintf(stderr, "%s\n", icalcomponent_as_ical_string(impl->data));
		abort();
	}

	return impl->data;
}


icalerrorenum icalcluster_add_component(icalcluster *impl, icalcomponent* child) {

	icalerror_check_arg_re((impl!=0),"cluster", ICAL_BADARG_ERROR);
	icalerror_check_arg_re((child!=0),"child",ICAL_BADARG_ERROR);

	icalcomponent_add_component(impl->data, child);
	icalcluster_mark(impl);

	return ICAL_NO_ERROR;
}


icalerrorenum icalcluster_remove_component(icalcluster *impl, icalcomponent* child) {

	icalerror_check_arg_re((impl!=0),"cluster",ICAL_BADARG_ERROR);
	icalerror_check_arg_re((child!=0),"child",ICAL_BADARG_ERROR);

	icalcomponent_remove_component(impl->data,child);
	icalcluster_mark(impl);

	return ICAL_NO_ERROR;
}


int icalcluster_count_components(icalcluster *impl, icalcomponent_kind kind) {

	icalerror_check_arg_re((impl!=0),"cluster",ICAL_BADARG_ERROR);

	return icalcomponent_count_components(impl->data, kind);
}


/** Iterate through components **/
icalcomponent *icalcluster_get_current_component(icalcluster* impl) {

	icalerror_check_arg_rz((impl!=0),"cluster");

	return icalcomponent_get_current_component(impl->data);
}


icalcomponent *icalcluster_get_first_component(icalcluster* impl) {

	icalerror_check_arg_rz((impl!=0),"cluster");

	return icalcomponent_get_first_component(impl->data,
						  ICAL_ANY_COMPONENT);
}


icalcomponent *icalcluster_get_next_component(icalcluster* impl) {

	icalerror_check_arg_rz((impl!=0),"cluster");
    
	return icalcomponent_get_next_component(impl->data,
					     ICAL_ANY_COMPONENT);
}
