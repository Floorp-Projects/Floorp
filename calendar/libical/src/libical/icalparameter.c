/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedparameters.{c,h}
  CREATOR: eric 09 May 1999
  
  $Id: icalparameter.c,v 1.12 2004/03/17 19:04:53 acampi Exp $
  $Locker:  $
    

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalderivedparameters.{c,h}

  Contributions from:
     Graham Davison (g.m.davison@computer.org)

 ======================================================================*/
/*#line 29 "icalparameter.c.in"*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "icalparameter.h"
#include "icalproperty.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalparameterimpl.h"

#include <stdlib.h> /* for malloc() */
#include <errno.h>
#include <string.h> /* for memset() */

/* In icalderivedparameter */
icalparameter* icalparameter_new_from_value_string(icalparameter_kind kind,const  char* val);


struct icalparameter_impl* icalparameter_new_impl(icalparameter_kind kind)
{
    struct icalparameter_impl* v;

    if ( ( v = (struct icalparameter_impl*)
	   malloc(sizeof(struct icalparameter_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(v->id,"para");

    v->kind = kind;
    v->size = 0;
    v->string = 0;
    v->x_name = 0;
    v->parent = 0;
    v->data = 0;

    return v;
}

icalparameter*
icalparameter_new (icalparameter_kind kind)
{
    struct icalparameter_impl* v = icalparameter_new_impl(kind);

    return (icalparameter*) v;

}

icalparameter *
icalparameter_new_x_name(const char *name, const char *value) {

	icalparameter  *ret;

	if (name == NULL || value == NULL)
		return NULL;

	ret = icalparameter_new_x(value);
	if (ret == NULL)
		return NULL;

	icalparameter_set_xname(ret, name);

	return ret;	
}

void
icalparameter_free (icalparameter* param)
{

/*  HACK. This always triggers, even when parameter is non-zero
    icalerror_check_arg_rv((parameter==0),"parameter");*/


#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (param->parent ==0),"Tried to free a parameter that is still attached to a component. ");
    
#else
    if(param->parent !=0){
	return;
    }
#endif

    
    if (param->string != 0){
	free ((void*)param->string);
    }
    
    if (param->x_name != 0){
	free ((void*)param->x_name);
    }
    
    memset(param,0,sizeof(param));

    param->parent = 0;
    param->id[0] = 'X';
    free(param);
}



icalparameter* 
icalparameter_new_clone(icalparameter* old)
{
    struct icalparameter_impl *new;

    new = icalparameter_new_impl(old->kind);

    icalerror_check_arg_rz((old!=0),"param");

    if (new == 0){
	return 0;
    }

    memcpy(new,old,sizeof(struct icalparameter_impl));

    if (old->string != 0){
	new->string = icalmemory_strdup(old->string);
	if (new->string == 0){
	    icalparameter_free(new);
	    return 0;
	}
    }

    if (old->x_name != 0){
	new->x_name = icalmemory_strdup(old->x_name);
	if (new->x_name == 0){
	    icalparameter_free(new);
	    return 0;
	}
    }

    return new;
}

icalparameter* icalparameter_new_from_string(const char *str)
{
    char* eq;
    char* cpy;
    icalparameter_kind kind;
    icalparameter *param;

    icalerror_check_arg_rz(str != 0,"str");

    cpy = icalmemory_strdup(str);

    if (cpy == 0){
        icalerror_set_errno(ICAL_NEWFAILED_ERROR);
        return 0;
    }

    eq = strchr(cpy,'=');

    if(eq == 0){
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return 0;
    }

    *eq = '\0';

    eq++;

    kind = icalparameter_string_to_kind(cpy);

    if(kind == ICAL_NO_PARAMETER){
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return 0;
    }

    param = icalparameter_new_from_value_string(kind,eq);

    if(kind == ICAL_X_PARAMETER){
        icalparameter_set_xname(param,cpy);
    }

    free(cpy);

    return param;
    
}

/**
 * Return a string representation of the parameter according to RFC2445.
 *
 * param	= param-name "=" param-value
 * param-name	= iana-token / x-token
 * param-value	= paramtext /quoted-string
 * paramtext	= *SAFE-SHARE
 * quoted-string= DQUOTE *QSAFE-CHARE DQUOTE
 * QSAFE-CHAR	= any character except CTLs and DQUOTE
 * SAFE-CHAR	= any character except CTLs, DQUOTE. ";", ":", ","
 */
char*
icalparameter_as_ical_string (icalparameter* param)
{
    size_t buf_size = 1024;
    char* buf; 
    char* buf_ptr;
    char *out_buf;
    const char *kind_string;

    icalerror_check_arg_rz( (param!=0), "parameter");

    /* Create new buffer that we can append names, parameters and a
       value to, and reallocate as needed. Later, this buffer will be
       copied to a icalmemory_tmp_buffer, which is managed internally
       by libical, so it can be given to the caller without fear of
       the caller forgetting to free it */

    buf = icalmemory_new_buffer(buf_size);
    buf_ptr = buf;

    if(param->kind == ICAL_X_PARAMETER) {

	icalmemory_append_string(&buf, &buf_ptr, &buf_size, 
				 icalparameter_get_xname(param));

    } else {

	kind_string = icalparameter_kind_to_string(param->kind);
	
	if (param->kind == ICAL_NO_PARAMETER || 
	    param->kind == ICAL_ANY_PARAMETER || 
	    kind_string == 0)
	{
	    icalerror_set_errno(ICAL_BADARG_ERROR);
	    return 0;
	}
	
	
	/* Put the parameter name into the string */
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, kind_string);

    }

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "=");

    if(param->string !=0){
        int qm = 0;

	/* Encapsulate the property in quotes if necessary */
	if (strpbrk(param->string, ";:,") != 0) {
		icalmemory_append_char (&buf, &buf_ptr, &buf_size, '"');
		qm = 1;
	}
        icalmemory_append_string(&buf, &buf_ptr, &buf_size, param->string); 
	if (qm == 1) {
		icalmemory_append_char (&buf, &buf_ptr, &buf_size, '"');
	}
    } else if (param->data != 0){
        const char* str = icalparameter_enum_to_string(param->data);
        icalmemory_append_string(&buf, &buf_ptr, &buf_size, str); 
    } else {
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return 0;
    }

    /* Now, copy the buffer to a tmp_buffer, which is safe to give to
       the caller without worring about de-allocating it. */
    
    out_buf = icalmemory_tmp_copy(buf);
    icalmemory_free_buffer(buf);

    return out_buf;

}


int
icalparameter_is_valid (icalparameter* parameter);


icalparameter_kind
icalparameter_isa (icalparameter* parameter)
{
    if(parameter == 0){
	return ICAL_NO_PARAMETER;
    }

    return parameter->kind;
}


int
icalparameter_isa_parameter (void* parameter)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl *)parameter;

    if (parameter == 0){
	return 0;
    }

    if (strcmp(impl->id,"para") == 0) {
	return 1;
    } else {
	return 0;
    }
}


void
icalparameter_set_xname (icalparameter* param, const char* v)
{
    icalerror_check_arg_rv( (param!=0),"param");
    icalerror_check_arg_rv( (v!=0),"v");

    if (param->x_name != 0){
	free((void*)param->x_name);
    }

    param->x_name = icalmemory_strdup(v);

    if (param->x_name == 0){
	errno = ENOMEM;
    }

}

const char*
icalparameter_get_xname (icalparameter* param)
{
    icalerror_check_arg_rz( (param!=0),"param");

    return param->x_name;
}

void
icalparameter_set_xvalue (icalparameter* param, const char* v)
{
    icalerror_check_arg_rv( (param!=0),"param");
    icalerror_check_arg_rv( (v!=0),"v");

    if (param->string != 0){
	free((void*)param->string);
    }

    param->string = icalmemory_strdup(v);

    if (param->string == 0){
	errno = ENOMEM;
    }

}

const char*
icalparameter_get_xvalue (icalparameter* param)
{
    icalerror_check_arg_rz( (param!=0),"param");

    return param->string;
}

void icalparameter_set_parent(icalparameter* param,
			     icalproperty* property)
{
    icalerror_check_arg_rv( (param!=0),"param");

    param->parent = property;
}

icalproperty* icalparameter_get_parent(icalparameter* param)
{
    icalerror_check_arg_rz( (param!=0),"param");

    return param->parent;
}


/* Everything below this line is machine generated. Do not edit. */
/* ALTREP */
