/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalset.c
 CREATOR: eric 17 Jul 2000


 Icalset is the "base class" for representations of a collection of
 iCal components. Derived classes (actually delegates) include:
 
    icalfileset   Store components in a single file
    icaldirset    Store components in multiple files in a directory
    icalheapset   Store components on the heap
    icalmysqlset  Store components in a mysql database. 

 $Id: icalset.c,v 1.16 2002/07/21 17:00:26 lindner Exp $
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

#include "ical.h"
#include "icalset.h"
#include "icalfileset.h"
#include "icalfilesetimpl.h"
#include "icaldirset.h"
#include "icaldirsetimpl.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef WITH_BDB4
#include "icalbdbset.h"
#include "icalbdbsetimpl.h"
#endif

/* #define _DLOPEN_TEST */
#ifdef _DLOPEN_TEST
#include <sys/types.h>
#include <dlfcn.h>
#include <dirent.h>
#endif

static icalset icalset_dirset_init = {
    ICAL_DIR_SET,
    sizeof(icaldirset),
    NULL,
    icaldirset_init,
    icaldirset_free,
    icaldirset_path,
    icaldirset_mark,
    icaldirset_commit,
    icaldirset_add_component,
    icaldirset_remove_component,
    icaldirset_count_components,
    icaldirset_select,
    icaldirset_clear,
    icaldirset_fetch,
    icaldirset_fetch_match,
    icaldirset_has_uid,
    icaldirset_modify,
    icaldirset_get_current_component,
    icaldirset_get_first_component,
    icaldirset_get_next_component,
    icaldirset_begin_component,
    icaldirsetiter_to_next,
    icaldirsetiter_to_prior
};


static icalset icalset_fileset_init = {
    ICAL_FILE_SET,
    sizeof(icalfileset),
    NULL,
    icalfileset_init,
    icalfileset_free,
    icalfileset_path,
    icalfileset_mark,
    icalfileset_commit,
    icalfileset_add_component,
    icalfileset_remove_component,
    icalfileset_count_components,
    icalfileset_select,
    icalfileset_clear,
    icalfileset_fetch,
    icalfileset_fetch_match,
    icalfileset_has_uid,
    icalfileset_modify,
    icalfileset_get_current_component,
    icalfileset_get_first_component,
    icalfileset_get_next_component,
    icalfileset_begin_component,
    icalfilesetiter_to_next,
    NULL
};

#ifdef WITH_BDB4
static icalset icalset_bdbset_init = {
    ICAL_BDB_SET,
    sizeof(icalbdbset),
    NULL,
    icalbdbset_init,
    icalbdbset_free,
    icalbdbset_path,
    icalbdbset_mark,
    icalbdbset_commit,
    icalbdbset_add_component,
    icalbdbset_remove_component,
    icalbdbset_count_components,
    icalbdbset_select,
    icalbdbset_clear,
    icalbdbset_fetch,
    icalbdbset_fetch_match,
    icalbdbset_has_uid,
    icalbdbset_modify,
    icalbdbset_get_current_component,
    icalbdbset_get_first_component,
    icalbdbset_get_next_component,
    icalbdbset_begin_component,
    icalbdbsetiter_to_next,
    NULL 
};
#endif

#ifdef _DLOPEN_TEST
static int	icalset_init_done = 0;
static pvl_list icalset_kinds = 0;

typedef icalset *(*fptr)(void);

/**
 * Try to load the file and register any icalset found within.
 */
static int load(const char *file) {

        void           *modh;
        fptr            inith;
	icalset	       *icalset_init_ptr;

        if ((modh = dlopen(file, RTLD_NOW)) == 0) {
                perror("dlopen");
                return 0;
        }

        if ((inith = (fptr)dlsym(modh, "InitModule")) == 0) {
                perror("dlsym");
                return 0;
        }

        while ((icalset_init_ptr = ((inith)())) != 0) {
		pvl_push(icalset_kinds, &icalset_init_ptr);
        }

        return 1;
}

/**
 * Look in the given directory for files called mod_*.o and try to
 * load them.
 */
int icalset_loaddir(const char *path) {
        DIR             *d;
        struct dirent   *dp;
        char            buf[PATH_MAX],
                       *bufptr;
        int             tot = 0;

        strcpy(buf, path);
        bufptr = buf + strlen(buf);

        if (*(bufptr-1) != '/')
                *bufptr++ = '/';

        if ((d = opendir(path)) == 0) {
                perror("opendir");
                return 0;
        }

        while ((dp = readdir(d)) != 0) {
                if (strncmp(dp->d_name, "mod_", 4)) continue;

                strcpy(bufptr, dp->d_name);

                load(buf);
                tot++;
        }
        (void)closedir(d);

        return 1;
}

int icalset_register_class(icalset *set);

static void icalset_init(void) {
    assert(icalset_kinds == 0);
    icalset_kinds = pvl_newlist();

    pvl_push(icalset_kinds, &icalset_fileset_init);
    pvl_push(icalset_kinds, &icalset_dirset_init);
#ifdef WITH_BDB4
    pvl_push(icalset_kinds, &icalset_bdb4set_init);
#endif

#ifdef EXT_PATH
    icalset_loaddir(EXT_PATH);
#endif

    icalset_init_done++;
}

int icalset_register_class(icalset *set) {

    if (!icalset_init_done)
	icalset_init();

    pvl_push(icalset_kinds, set);
    return 1;
}

#endif

icalset* icalset_new(icalset_kind kind, const char* dsn, void* options) {
  icalset *data = NULL;
  icalset *ret = NULL;

#ifdef _DLOPEN_TEST
    pvl_elem	e;
    icalset    *impl;

    if (!icalset_init_done)
	icalset_init();

    for(e = pvl_head(icalset_kinds); e!=0; e = pvl_next(e)) {
	impl = (icalset*)pvl_data(e);
	if (impl->kind == kind)
	    break;
    }
    if (e == 0) {
	icalerror_set_errno(ICAL_UNIMPLEMENTED_ERROR);
	return(NULL);
    }

    data = (icalset*)malloc(impl->size);
    if (data == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	errno = ENOMEM;
	return 0;
    }

    /* The first member of the derived class must be an icalset. */
    memset(data,0,impl->size);
    /* *data = *impl; */
    memcpy(data, impl, sizeof(icalset));

    data->dsn     = strdup(dsn);
#else
  switch(kind) {
  case ICAL_FILE_SET:
    data = (icalset*) malloc(sizeof(icalfileset));
    if (data == 0) {
      	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	errno = ENOMEM;
	return 0;
    }
    memset(data,0,sizeof(icalfileset));
    *data = icalset_fileset_init;
    break;
  case ICAL_DIR_SET:
    data = (icalset*) malloc(sizeof(icaldirset));
    if (data == 0) {
      	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	errno = ENOMEM;
	return 0;
    }
    memset(data,0,sizeof(icaldirset));
    *data = icalset_dirset_init;
    break;
#ifdef WITH_BDB4
  case ICAL_BDB_SET:
    data = (icalset*) malloc(sizeof(icalbdbset));
    if (data == 0) {
      	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	errno = ENOMEM;
	return 0;
    }
    memset(data,0,sizeof(icalbdbset));
    *data = icalset_bdbset_init;
    break;
#endif
    
  default:
    icalerror_set_errno(ICAL_UNIMPLEMENTED_ERROR);
    /** unimplemented **/
    return(NULL);
  }

  if ( data == 0) {
    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
    return 0;
  }
  data->kind    = kind;
  data->dsn     = strdup(dsn);
#endif

  /** call the implementation specific initializer **/
  if ((ret = data->init(data, dsn, options)) == NULL)
    icalset_free(data);

  return ret;
}

icalset* icalset_new_file(const char* path)
{
  return icalset_new(ICAL_FILE_SET, path, NULL);
}

icalset* icalset_new_file_writer(const char* path)
{
  return icalfileset_new_writer(path);
}

icalset* icalset_new_file_reader(const char* path)
{
  return icalfileset_new_reader(path);
}


icalset* icalset_new_dir(const char* path)
{
  return icalset_new(ICAL_DIR_SET, path, NULL);
}

icalset* icalset_new_dir_writer(const char* path)
{
  return icaldirset_new_writer(path);
}

icalset* icalset_new_dir_reader(const char* path)
{
  return icaldirset_new_reader(path);
}



/* Functions for built-in methods */

/**
 *  free memory associated with this icalset
 *  automatically calls the implementation specific free routine
 */

void icalset_free(icalset* set)
{
  if (set->free)
    set->free(set);

  if (set->dsn)
    free(set->dsn);

  free(set);
}


const char* icalset_path(icalset* set) {
    return set->path(set);
}

void icalset_mark(icalset* set) {
    set->mark(set);
}

icalerrorenum icalset_commit(icalset* set) {
    return set->commit(set);
}

icalerrorenum icalset_add_component(icalset* set, icalcomponent* comp) {
    return set->add_component(set,comp);
}

icalerrorenum icalset_remove_component(icalset* set, icalcomponent* comp) {
    return set->remove_component(set,comp);
}

int icalset_count_components(icalset* set,icalcomponent_kind kind) {
    return set->count_components(set,kind);
}

icalerrorenum icalset_select(icalset* set, icalgauge* gauge) {
    return set->select(set, gauge);
}

void icalset_clear(icalset* set) {
    set->clear(set);
}

icalcomponent* icalset_fetch(icalset* set, const char* uid) {
    return set->fetch(set, uid);
}

icalcomponent* icalset_fetch_match(icalset* set, icalcomponent *comp) {
    return set->fetch_match(set, comp);
}

int icalset_has_uid(icalset* set, const char* uid) {
    return set->has_uid(set, uid);
}

icalerrorenum icalset_modify(icalset* set, icalcomponent *old,
			       icalcomponent *new) {
    return set->modify(set, old, new);
}

icalcomponent* icalset_get_current_component(icalset* set) {
    return set->get_current_component(set);
}

icalcomponent* icalset_get_first_component(icalset* set) {
    return set->get_first_component(set);
}

icalcomponent* icalset_get_next_component(icalset* set) {
    return set->get_next_component(set);
}

icalsetiter icalsetiter_null = {{ICAL_NO_COMPONENT, 0}, 0};

icalsetiter icalset_begin_component(icalset* set,
                                 icalcomponent_kind kind, icalgauge* gauge) {
    return set->icalset_begin_component(set, kind, gauge);
}

icalcomponent* icalsetiter_next(icalsetiter* itr) {

    icalcomponent* c = 0;
    icalerror_check_arg_rz( (itr != NULL), "i");

    do  {
	c = icalcompiter_next(&(itr->iter));
	if(c != 0 && (itr->gauge == 0 ||
			icalgauge_compare(itr->gauge, c) == 1)){
	    return c;
	}
    } while (c != 0);

    return 0;
}

icalcomponent* icalsetiter_prior(icalsetiter* i) {

    icalcomponent* c = 0;
    icalerror_check_arg_rz( (i != NULL), "i" );

    do  {
	c = icalcompiter_prior(&(i->iter));
	if(c != 0 && (i->gauge == 0 ||
			icalgauge_compare(i->gauge, c) == 1)){
	    return c;
	}
    } while (c != 0);

    return 0;
}

icalcomponent* icalsetiter_deref(icalsetiter* i) {
    icalerror_check_arg_rz( (i != NULL), "i" );
    return (icalcompiter_deref(&(i->iter)));
}

/* for subclasses that use multiple clusters that require specialized cluster traversal */
icalcomponent* icalsetiter_to_next(icalset* set, icalsetiter* i)
{
    return set->icalsetiter_to_next(set, i);
}

icalcomponent* icalsetiter_to_prior(icalset* set, icalsetiter* i)
{
    return set->icalsetiter_to_prior(set, i);
}
