/* -*- Mode: C -*-
  ======================================================================
  FILE: icalattach.c
  CREATOR: acampi 28 May 02
  
  $Id: icalattach.c,v 1.2 2002/10/09 19:54:59 acampi Exp $
  $Locker:  $
    

 (C) COPYRIGHT 2000, Andrea Campi

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icaltypes.c

 ======================================================================*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icaltypes.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalattachimpl.h"
#include <stdlib.h> /* for malloc and abs() */
#include <errno.h> /* for errno */
#include <string.h> /* for icalmemory_strdup */
#include <assert.h>

icalattach *
icalattach_new_from_url (const char *url)
{
    icalattach *attach;
    char *url_copy;

    icalerror_check_arg_rz ((url != NULL), "url");

    if ((attach = malloc (sizeof (icalattach))) == NULL) {
	errno = ENOMEM;
	return NULL;
    }

    if ((url_copy = strdup (url)) == NULL) {
	free (attach);
	errno = ENOMEM;
	return NULL;
    }

    attach->refcount = 1;
    attach->is_url = 1;
    attach->u.url.url = url_copy;

    return attach;
}

icalattach *
icalattach_new_from_data (unsigned char *data, icalattach_free_fn_t free_fn,
			  void *free_fn_data)
{
    icalattach *attach;

    icalerror_check_arg_rz ((data != NULL), "data");

    if ((attach = malloc (sizeof (icalattach))) == NULL) {
	errno = ENOMEM;
	return NULL;
    }

    attach->refcount = 1;
    attach->is_url = 0;
    attach->u.data.data = data;
    attach->u.data.free_fn = free_fn;
    attach->u.data.free_fn_data = free_fn_data;

    return attach;
}

void
icalattach_ref (icalattach *attach)
{
    icalerror_check_arg_rv ((attach != NULL), "attach");
    icalerror_check_arg_rv ((attach->refcount > 0), "attach->refcount > 0");

    attach->refcount++;
}

void
icalattach_unref (icalattach *attach)
{
    icalerror_check_arg_rv ((attach != NULL), "attach");
    icalerror_check_arg_rv ((attach->refcount > 0), "attach->refcount > 0");

    attach->refcount--;

    if (attach->refcount != 0)
	return;

    if (attach->is_url)
	free (attach->u.url.url);
    else if (attach->u.data.free_fn)
	(* attach->u.data.free_fn) (attach->u.data.data, attach->u.data.free_fn_data);

    free (attach);
}

int
icalattach_get_is_url (icalattach *attach)
{
    icalerror_check_arg_rz ((attach != NULL), "attach");

    return attach->is_url ? 1 : 0;
}

const char *
icalattach_get_url (icalattach *attach)
{
    icalerror_check_arg_rz ((attach != NULL), "attach");
    icalerror_check_arg_rz ((attach->is_url), "attach->is_url");

    return attach->u.url.url;
}

unsigned char *
icalattach_get_data (icalattach *attach)
{
    icalerror_check_arg_rz ((attach != NULL), "attach");
    icalerror_check_arg_rz ((!attach->is_url), "!attach->is_url");

    return attach->u.data.data;
}
