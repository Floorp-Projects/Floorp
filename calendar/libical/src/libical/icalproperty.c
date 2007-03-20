/* -*- Mode: C -*- */

/*======================================================================
  FILE: icalproperty.c
  CREATOR: eric 28 April 1999
  
  $Id: icalproperty.c,v 1.39 2004/10/27 11:57:26 acampi Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalproperty.c

======================================================================*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icalproperty.h"
#include "icalparameter.h"
#include "icalcomponent.h"
#include "pvl.h"
#include "icalenums.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalparser.h"

#include <string.h> /* For icalmemory_strdup, rindex */
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h> /* for printf */
#include <stdarg.h> /* for va_list, va_start, etc. */
                                               
#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

/* Private routines for icalproperty */
void icalvalue_set_parent(icalvalue* value,
			     icalproperty* property);
icalproperty* icalvalue_get_parent(icalvalue* value);

void icalparameter_set_parent(icalparameter* param,
			     icalproperty* property);
icalproperty* icalparameter_get_parent(icalparameter* value);


void icalproperty_set_x_name(icalproperty* prop, const char* name);

struct icalproperty_impl 
{
	char id[5];
	icalproperty_kind kind;
	char* x_name;
	pvl_list parameters;
	pvl_elem parameter_iterator;
	icalvalue* value;
	icalcomponent *parent;
};

void icalproperty_add_parameters(icalproperty* prop, va_list args)
{
    void* vp;

    while((vp = va_arg(args, void*)) != 0) {

	if (icalvalue_isa_value(vp) != 0 ){
	} else if (icalparameter_isa_parameter(vp) != 0 ){

	    icalproperty_add_parameter((icalproperty*)prop,
				       (icalparameter*)vp);
	} else {
	    icalerror_set_errno(ICAL_BADARG_ERROR);
	}

    }
}


icalproperty*
icalproperty_new_impl(icalproperty_kind kind)
{
    icalproperty* prop;

    if (!icalproperty_kind_is_valid(kind))
      return NULL;

    if ( ( prop = (icalproperty*) malloc(sizeof(icalproperty))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(prop->id,"prop");

    prop->kind = kind;
    prop->parameters = pvl_newlist();
    prop->parameter_iterator = 0;
    prop->value = 0;
    prop->x_name = 0;
    prop->parent = 0;

    return prop;
}


icalproperty*
icalproperty_new (icalproperty_kind kind)
{
    if (kind == ICAL_NO_PROPERTY){
        return 0;
    }

    return (icalproperty*)icalproperty_new_impl(kind);
}


icalproperty *
icalproperty_new_x_name(const char *name, const char *value) {

	icalproperty   *ret;

	if (name == NULL || value == NULL)
		return NULL;

	ret = icalproperty_new_x(value);
	if (ret == NULL)
		return NULL;

	icalproperty_set_x_name(ret, name);

	return ret;	
}

icalproperty*
icalproperty_new_clone(icalproperty* old)
{
    icalproperty *new = icalproperty_new_impl(old->kind);
    pvl_elem p;

    icalerror_check_arg_rz((old!=0),"old");
    icalerror_check_arg_rz((new!=0),"new");

    if (old->value !=0) {
	new->value = icalvalue_new_clone(old->value);
    }

    if (old->x_name != 0) {

	new->x_name = icalmemory_strdup(old->x_name);
	
	if (new->x_name == 0) {
	    icalproperty_free(new);
	    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	    return 0;
	}
    }

    for(p=pvl_head(old->parameters);p != 0; p = pvl_next(p)){
	icalparameter *param = icalparameter_new_clone(pvl_data(p));
	
	if (param == 0){
	    icalproperty_free(new);
	    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	    return 0;
	}

	pvl_push(new->parameters,param);
    
    } 

    return new;

}

icalproperty* icalproperty_new_from_string(const char* str)
{

    size_t buf_size = 1024;
    char* buf = icalmemory_new_buffer(buf_size);
    char* buf_ptr = buf;  
    icalproperty *prop;
    icalcomponent *comp;
    int errors  = 0;

#ifdef ICAL_UNIX_NEWLINE
    char newline[] = "\n";
#else
    char newline[] = "\r\n";
#endif

    icalerror_check_arg_rz( (str!=0),"str");

    /* Is this a HACK or a crafty reuse of code? */

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "BEGIN:VCALENDAR");
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, newline);
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, str);
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, newline);
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "END:VCALENDAR");
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, newline);

    comp = icalparser_parse_string(buf);

    if(comp == 0){
        icalerror_set_errno(ICAL_PARSE_ERROR);
        return 0;
    }

    errors = icalcomponent_count_errors(comp);

    prop = icalcomponent_get_first_property(comp,ICAL_ANY_PROPERTY);

    icalcomponent_remove_property(comp,prop);

    icalcomponent_free(comp);
    free(buf);

    if(errors > 0){
        icalproperty_free(prop);
        return 0;
    } else {
        return prop;
    }
    
}

void
icalproperty_free (icalproperty* p)
{
    icalparameter* param;
    
    icalerror_check_arg_rv((p!=0),"prop");

#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (p->parent ==0),"Tried to free a property that is still attached to a component. ");
    
#else
    if(p->parent !=0){
	return;
    }
#endif

    if (p->value != 0){
        icalvalue_set_parent(p->value,0);
	icalvalue_free(p->value);
    }
    
    while( (param = pvl_pop(p->parameters)) != 0){
	icalparameter_free(param);
    }
    
    pvl_free(p->parameters);
    
    if (p->x_name != 0) {
	free(p->x_name);
    }
    
    p->kind = ICAL_NO_PROPERTY;
    p->parameters = 0;
    p->parameter_iterator = 0;
    p->value = 0;
    p->x_name = 0;
    p->id[0] = 'X';
    
    free(p);

}


/* This returns where the start of the next line should be. chars_left does
   not include the trailing '\0'. */
#define MAX_LINE_LEN 75
/*#define MAX_LINE_LEN 120*/

static char*
get_next_line_start (char *line_start, int chars_left)
{
    char *pos;

    /* If we have 74 chars or less left, we can output all of them. 
       we return a pointer to the '\0' at the end of the string. */
    if (chars_left < MAX_LINE_LEN) {
        return line_start + chars_left;
    } 

    /* Now we jump to the last possible character of the line, and step back
       trying to find a ';' ':' or ' '. If we find one, we return the character
       after it. */
    pos = line_start + MAX_LINE_LEN - 2;
    while (pos > line_start) {
        if (*pos == ';' || *pos == ':' || *pos == ' ') {
	    return pos + 1;
	}
	pos--;
    }
    /* Now try to split on a UTF-8 boundary defined as a 7-bit
       value or as a byte with the two high-most bits set:
       11xxxxxx.  See http://czyborra.com/utf/ */

    pos = line_start + MAX_LINE_LEN - 1;
    while (pos > line_start) {
        /* plain ascii */
        if ((*pos & 128) == 0)
            return pos;

        /* utf8 escape byte */
        if ((*pos & 192) == 192)
            return pos;

        pos--;
    }

    /* Give up, just break at 74 chars (the 75th char is the space at
       the start of the line).  */

    return line_start + MAX_LINE_LEN - 1;
}


/** This splits the property into lines less than 75 octects long (as
 *  specified in RFC2445). It tries to split after a ';' if it can.
 *  It returns a tmp buffer.  NOTE: I'm not sure if it matters if we
 *  split a line in the middle of a UTF-8 character. It probably won't
 *  look nice in a text editor. 
 *  This will add the trailing newline as well
 */
static char*
fold_property_line (char *text)
{
    size_t buf_size;
    char *buf, *buf_ptr, *line_start, *next_line_start, *out_buf;
    int len, chars_left, first_line;
    char ch;

#ifdef ICAL_UNIX_NEWLINE
    char newline[] = "\n";
#else
    char newline[] = "\r\n";
#endif

    /* Start with a buffer twice the size of our property line, so we almost
       certainly won't overflow it. */
    len = strlen (text);
    buf_size = len * 2;
    buf = icalmemory_new_buffer (buf_size);
    buf_ptr = buf;

    /* Step through the text, finding each line to add to the output. */
    line_start = text;
    chars_left = len;
    first_line = 1;
    for (;;) {
        if (chars_left <= 0)
	    break;

        /* This returns the first character for the next line. */
        next_line_start = get_next_line_start (line_start, chars_left);

	/* If this isn't the first line, we need to output a newline and space
	   first. */
	if (!first_line) {
	    icalmemory_append_string (&buf, &buf_ptr, &buf_size, newline);
	    icalmemory_append_string (&buf, &buf_ptr, &buf_size, " ");
	}
	first_line = 0;

	/* This adds the line to our tmp buffer. We temporarily place a '\0'
	   in text, so we can copy the line in one go. */
	ch = *next_line_start;
	*next_line_start = '\0';
	icalmemory_append_string (&buf, &buf_ptr, &buf_size, line_start);
	*next_line_start = ch;

	/* Now we move on to the next line. */
	chars_left -= (next_line_start - line_start);
	line_start = next_line_start;
    }

    icalmemory_append_string (&buf, &buf_ptr, &buf_size, newline);

    /* Copy it to a temporary buffer, and then free it. */
    out_buf = icalmemory_tmp_buffer (strlen (buf) + 1);
    strcpy (out_buf, buf);
    icalmemory_free_buffer (buf);

    return out_buf;
}


/* Determine what VALUE parameter to include. The VALUE parameters
   are ignored in the normal parameter printing ( the block after
   this one, so we need to do it here */
static const char *
icalproperty_get_value_kind(icalproperty *prop)
{
	const char* kind_string = 0;

	icalparameter *orig_val_param
	    = icalproperty_get_first_parameter(prop,ICAL_VALUE_PARAMETER);

	icalvalue *value = icalproperty_get_value(prop);

	icalvalue_kind orig_kind = ICAL_NO_VALUE;

	icalvalue_kind this_kind = ICAL_NO_VALUE;

	icalvalue_kind default_kind 
	    =  icalproperty_kind_to_value_kind(prop->kind);

	if(orig_val_param){
	    orig_kind = (icalvalue_kind)icalparameter_get_value(orig_val_param);
	}

	if(value != 0){
	    this_kind = icalvalue_isa(value);
	}
	
	
	if(this_kind == default_kind &&
	   orig_kind != ICAL_NO_VALUE){
	    /* The kind is the default, so it does not need to be
               included, but do it anyway, since it was explicit in
               the property. But, use the default, not the one
               specified in the property */
	    
	    kind_string = icalvalue_kind_to_string(default_kind);

	} else if (this_kind != default_kind && this_kind !=  ICAL_NO_VALUE){
	    /* Not the default, so it must be specified */
	    kind_string = icalvalue_kind_to_string(this_kind);
	} else {
	    /* Don'tinclude the VALUE parameter at all */
	}

	return kind_string;
}

const char*
icalproperty_as_ical_string (icalproperty* prop)
{   
    icalparameter *param;

    /* Create new buffer that we can append names, parameters and a
       value to, and reallocate as needed. Later, this buffer will be
       copied to a icalmemory_tmp_buffer, which is managed internally
       by libical, so it can be given to the caller without fear of
       the caller forgetting to free it */

    const char* property_name = 0; 
    size_t buf_size = 1024;
    char* buf = icalmemory_new_buffer(buf_size);
    char* buf_ptr = buf;
    icalvalue* value;
    char *out_buf;
    const char* kind_string = 0;

#ifdef ICAL_UNIX_NEWLINE
    char newline[] = "\n";
#else
    char newline[] = "\r\n";
#endif


    
    icalerror_check_arg_rz( (prop!=0),"prop");


    /* Append property name */

    if (prop->kind == ICAL_X_PROPERTY && prop->x_name != 0){
	property_name = prop->x_name;
    } else {
	property_name = icalproperty_kind_to_string(prop->kind);
    }

    if (property_name == 0 ) {
	icalerror_warn("Got a property of an unknown kind.");
	icalmemory_free_buffer(buf);
	return 0;
	
    }

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, property_name);

    kind_string = icalproperty_get_value_kind(prop);
    if(kind_string!=0){
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, ";VALUE=");
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, kind_string);
    }

    /* Append parameters */
    for(param = icalproperty_get_first_parameter(prop,ICAL_ANY_PARAMETER);
	param != 0; 
	param = icalproperty_get_next_parameter(prop,ICAL_ANY_PARAMETER)) {

	icalparameter_kind kind = icalparameter_isa(param);
	kind_string = icalparameter_as_ical_string(param); 

	if(kind==ICAL_VALUE_PARAMETER){
	    continue;
	}

	if (kind_string == 0 ) {
	  icalerror_warn("Got a parameter of unknown kind for the following property");

	  icalerror_warn((property_name) ? property_name : "(NULL)");
	    continue;
	}

	icalmemory_append_string(&buf, &buf_ptr, &buf_size, ";");
    	icalmemory_append_string(&buf, &buf_ptr, &buf_size, kind_string);
    }    

    /* Append value */

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, ":");

    value = icalproperty_get_value(prop);

    if (value != 0){
	const char *str = icalvalue_as_ical_string(value);
	icalerror_assert((str !=0),"Could not get string representation of a value");
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, str);
    } else {
	icalmemory_append_string(&buf, &buf_ptr, &buf_size,"ERROR: No Value"); 
	
    }
    
    /* Now, copy the buffer to a tmp_buffer, which is safe to give to
       the caller without worring about de-allocating it. */

    /* We now use a function to fold the line properly every 75 characters.
       That function also adds the newline for us. */
    out_buf = fold_property_line (buf);

    icalmemory_free_buffer(buf);

    return out_buf;
}



icalproperty_kind
icalproperty_isa (icalproperty* p)
{
   if(p != 0){
       return p->kind;
   }

   return ICAL_NO_PROPERTY;
}

int
icalproperty_isa_property (void* property)
{
    icalproperty *impl = (icalproperty *) property;

    icalerror_check_arg_rz( (property!=0), "property");
    if (strcmp(impl->id,"prop") == 0) {
	return 1;
    } else {
	return 0;
    }
}


void
icalproperty_add_parameter (icalproperty* p,icalparameter* parameter)
{
   icalerror_check_arg_rv( (p!=0),"prop");
   icalerror_check_arg_rv( (parameter!=0),"parameter");
    
   pvl_push(p->parameters, parameter);

}

void
icalproperty_set_parameter (icalproperty* prop,icalparameter* parameter)
{
    icalparameter_kind kind;
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalerror_check_arg_rv( (parameter!=0),"parameter");

    kind = icalparameter_isa(parameter);
    if (kind != ICAL_X_PARAMETER)
      icalproperty_remove_parameter_by_kind(prop,kind);
    else
      icalproperty_remove_parameter_by_name(prop, 
					    icalparameter_get_xname(parameter));

    icalproperty_add_parameter(prop,parameter);
}

void icalproperty_set_parameter_from_string(icalproperty* prop,
                                            const char* name, const char* value)
{

    icalparameter_kind kind;
    icalparameter *param;

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalerror_check_arg_rv( (name!=0),"name");
    icalerror_check_arg_rv( (value!=0),"value");
    
    kind = icalparameter_string_to_kind(name);

    if(kind == ICAL_NO_PARAMETER){
        icalerror_set_errno(ICAL_BADARG_ERROR);
        return;
    }
    
    param  = icalparameter_new_from_value_string(kind,value);

    if (param == 0){
        icalerror_set_errno(ICAL_BADARG_ERROR);
        return;
    }

    if(kind == ICAL_X_PARAMETER){
	icalparameter_set_xname(param, name);
    }

    icalproperty_set_parameter(prop,param);

}

const char* icalproperty_get_parameter_as_string(icalproperty* prop,
                                                 const char* name)
{
    icalparameter_kind kind;
    icalparameter *param;
    char* str;
    char* pv;

    icalerror_check_arg_rz( (prop!=0),"prop");
    icalerror_check_arg_rz( (name!=0),"name");
    
    kind = icalparameter_string_to_kind(name);

    if(kind == ICAL_NO_PARAMETER){
        /* icalenum_string_to_parameter_kind will set icalerrno */
        return 0;
    }
    
    for(param = icalproperty_get_first_parameter(prop,kind); 
	    param != 0; 
	    param = icalproperty_get_next_parameter(prop,kind)) {
	    if (kind != ICAL_X_PARAMETER) {
		    break;
	    }

	    if (strcmp(icalparameter_get_xname(param),name)==0) {
		    break;
	    }		
    }

    if (param == 0){
        return 0;
    }


    str = icalparameter_as_ical_string(param);

    pv = strchr(str,'=');

    if(pv == 0){
        icalerror_set_errno(ICAL_INTERNAL_ERROR);
        return 0;
    }

    return pv+1;

}

/** @see icalproperty_remove_parameter_by_kind() 
 *
 *  @deprecated Please use icalproperty_remove_parameter_by_kind()
 *              instead.
 */

void
icalproperty_remove_parameter(icalproperty* prop, icalparameter_kind kind)
{
  icalproperty_remove_parameter_by_kind(prop, kind);
}


/** @brief Remove all parameters with the specified kind.
 *
 *  @param prop   A valid icalproperty.
 *  @param kind   The kind to remove (ex. ICAL_TZID_PARAMETER)
 *
 *  See icalproperty_remove_parameter_by_name() and
 *  icalproperty_remove_parameter_by_ref() for alternate ways of
 *  removing parameters
 */

void
icalproperty_remove_parameter_by_kind(icalproperty* prop, icalparameter_kind kind)
{
    pvl_elem p;     

    icalerror_check_arg_rv((prop!=0),"prop");
    
    for(p=pvl_head(prop->parameters);p != 0; p = pvl_next(p)){
	icalparameter* param = (icalparameter *)pvl_data (p);
        if (icalparameter_isa(param) == kind) {
            pvl_remove (prop->parameters, p);
	    icalparameter_free(param);
            break;
        }
    }                       
}


/** @brief Remove all parameters with the specified name.
 *
 *  @param prop   A valid icalproperty.
 *  @param name   The name of the parameter to remove
 *
 *  This function removes paramters with the given name.  The name
 *  corresponds to either a built-in name (TZID, etc.) or the name of
 *  an extended parameter (X-FOO)
 *
 *  See icalproperty_remove_parameter_by_kind() and
 *  icalproperty_remove_parameter_by_ref() for alternate ways of removing
 *  parameters
 */


void
icalproperty_remove_parameter_by_name(icalproperty* prop, const char *name)
{
    pvl_elem p;     

    icalerror_check_arg_rv((prop!=0),"prop");
    
    for(p=pvl_head(prop->parameters);p != 0; p = pvl_next(p)){
	icalparameter* param = (icalparameter *)pvl_data (p);
	const char * kind_string;

	if (icalparameter_isa(param) == ICAL_X_PARAMETER)
	  kind_string = icalparameter_get_xname(param);
	else
	  kind_string = icalparameter_kind_to_string(icalparameter_isa(param));

	if (!kind_string)
	  continue;

        if (0 == strcmp(kind_string, name)) {
            pvl_remove (prop->parameters, p);
            icalparameter_free(param);
            break;
        }
    }                       
}


/** @brief Remove the specified parameter reference from the property.
 *
 *  @param prop   A valid icalproperty.
 *  @param parameter   A reference to a specific icalparameter.
 *
 *  This function removes the specified parameter reference from the
 *  property.
 */

void
icalproperty_remove_parameter_by_ref(icalproperty* prop, icalparameter* parameter)
{
    pvl_elem p;
    icalparameter_kind kind;
    const char *name;

    icalerror_check_arg_rv((prop!=0),"prop");
    icalerror_check_arg_rv((parameter!=0),"parameter");

    kind = icalparameter_isa(parameter);
    name = icalparameter_get_xname(parameter);

    /*
     * FIXME If it's an X- parameter, also compare the names. It would be nice
     * to have a better abstraction like icalparameter_equals()
     */
    for(p=pvl_head(prop->parameters);p != 0; p = pvl_next(p)){
	icalparameter* p_param = (icalparameter *)pvl_data (p);
	if (icalparameter_isa(p_param) == kind &&
	    (kind != ICAL_X_PARAMETER ||
	    !strcmp(icalparameter_get_xname(p_param), name))) {
            pvl_remove (prop->parameters, p);
            icalparameter_free(p_param);
            break;
	} 
    }   
}


int
icalproperty_count_parameters (const icalproperty* prop)
{
    if(prop != 0){
	return pvl_count(prop->parameters);
    }

    icalerror_set_errno(ICAL_USAGE_ERROR);
    return -1;
}


icalparameter*
icalproperty_get_first_parameter(icalproperty* p, icalparameter_kind kind)
{
   icalerror_check_arg_rz( (p!=0),"prop");
   
   p->parameter_iterator = pvl_head(p->parameters);

   if (p->parameter_iterator == 0) {
       return 0;
   }

   for( p->parameter_iterator = pvl_head(p->parameters);
	p->parameter_iterator !=0;
	p->parameter_iterator = pvl_next(p->parameter_iterator)){

       icalparameter *param = (icalparameter*)pvl_data(p->parameter_iterator);

       if(icalparameter_isa(param) == kind || kind == ICAL_ANY_PARAMETER){
	   return param;
       }
   }

   return 0;
}


icalparameter*
icalproperty_get_next_parameter (icalproperty* p, icalparameter_kind kind)
{
    icalerror_check_arg_rz( (p!=0),"prop");
    
    if (p->parameter_iterator == 0) {
	return 0;
    }
    
    for( p->parameter_iterator = pvl_next(p->parameter_iterator);
	 p->parameter_iterator !=0;
	 p->parameter_iterator = pvl_next(p->parameter_iterator)){
	
	icalparameter *param = (icalparameter*)pvl_data(p->parameter_iterator);
	
	if(icalparameter_isa(param) == kind || kind == ICAL_ANY_PARAMETER){
	    return param;
	}
    }
    
    return 0;

}

icalparameter*
icalproperty_get_first_x_parameter(icalproperty* p, const char *name)
{
   icalerror_check_arg_rz( (p!=0),"prop");
   
   p->parameter_iterator = pvl_head(p->parameters);

   if (p->parameter_iterator == 0) {
       return 0;
   }

   for( p->parameter_iterator = pvl_head(p->parameters);
	p->parameter_iterator !=0;
	p->parameter_iterator = pvl_next(p->parameter_iterator)){

       icalparameter *param = (icalparameter*)pvl_data(p->parameter_iterator);

       if(icalparameter_isa(param) == ICAL_X_PARAMETER &&
	  !strcmp(icalparameter_get_xname(param), name)){
	   return param;
       }
   }

   return 0;
}


icalparameter*
icalproperty_get_next_x_parameter (icalproperty* p, const char *name)
{
    icalerror_check_arg_rz( (p!=0),"prop");
    
    if (p->parameter_iterator == 0) {
	return 0;
    }
    
    for( p->parameter_iterator = pvl_next(p->parameter_iterator);
	 p->parameter_iterator !=0;
	 p->parameter_iterator = pvl_next(p->parameter_iterator)){
	
	icalparameter *param = (icalparameter*)pvl_data(p->parameter_iterator);
	
	if(icalparameter_isa(param) == ICAL_X_PARAMETER &&
	  !strcmp(icalparameter_get_xname(param), name)){
	    return param;
	}
    }
    
    return 0;

}

void
icalproperty_set_value (icalproperty* p, icalvalue* value)
{
    icalerror_check_arg_rv((p !=0),"prop");
    icalerror_check_arg_rv((value !=0),"value");
    
    if (p->value != 0){
	icalvalue_set_parent(p->value,0);
	icalvalue_free(p->value);
	p->value = 0;
    }

    p->value = value;
    
    icalvalue_set_parent(value,p);
}


void icalproperty_set_value_from_string(icalproperty* prop,const char* str,
                                        const char* type)
{
    icalvalue *oval,*nval;
    icalvalue_kind kind = ICAL_NO_VALUE;

    icalerror_check_arg_rv( (prop!=0),"prop"); 
    icalerror_check_arg_rv( (str!=0),"str");
    icalerror_check_arg_rv( (type!=0),"type");
   
    if(strcmp(type,"NO")==0){
        /* Get the type from the value the property already has, if it exists */
        oval = icalproperty_get_value(prop);
        if(oval != 0){
            /* Use the existing value kind */
            kind  = icalvalue_isa(oval);
        } else {   
            /* Use the default kind for the property */
            kind = icalproperty_kind_to_value_kind(icalproperty_isa(prop));
        }
    } else {
        /* Use the given kind string */
        kind = icalvalue_string_to_kind(type);
    }

    if(kind == ICAL_NO_VALUE){
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return;
    }

    nval = icalvalue_new_from_string(kind, str);

    if(nval == 0){
        /* icalvalue_new_from_string sets errno */
        assert(icalerrno != ICAL_NO_ERROR);
        return;
    }

    icalproperty_set_value(prop,nval);


}

icalvalue*
icalproperty_get_value(const icalproperty* prop)
{
    icalerror_check_arg_rz( (prop!=0),"prop");
    
    return prop->value;
}

const char* icalproperty_get_value_as_string(const icalproperty* prop)
{
    icalvalue *value;
    
    icalerror_check_arg_rz( (prop!=0),"prop");

    value = prop->value; 

    return icalvalue_as_ical_string(value);
}


void icalproperty_set_x_name(icalproperty* prop, const char* name)
{
    icalerror_check_arg_rv( (name!=0),"name");
    icalerror_check_arg_rv( (prop!=0),"prop");

    if (prop->x_name != 0) {
        free(prop->x_name);
    }

    prop->x_name = icalmemory_strdup(name);

    if(prop->x_name == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
    }

}
                              
const char* icalproperty_get_x_name(icalproperty* prop){
    icalerror_check_arg_rz( (prop!=0),"prop");

    return prop->x_name;
}


const char* icalproperty_get_property_name(const icalproperty* prop)
{

    const char* property_name = 0;
    size_t buf_size = 256;
    char* buf = icalmemory_new_buffer(buf_size);
    char* buf_ptr = buf;  

    icalerror_check_arg_rz( (prop!=0),"prop");
 
    if (prop->kind == ICAL_X_PROPERTY && prop->x_name != 0){
        property_name = prop->x_name;
    } else {
        property_name = icalproperty_kind_to_string(prop->kind);
    }
 
    if (property_name == 0 ) {
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return 0;

    } else {
        /* _append_string will automatically grow the buffer if
           property_name is longer than the initial buffer size */
        icalmemory_append_string(&buf, &buf_ptr, &buf_size, property_name);
    }
 
    /* Add the buffer to the temporary buffer ring -- the caller will
       not have to free the memory. */
    icalmemory_add_tmp_buffer(buf);
 
    return buf;
}
                            



void icalproperty_set_parent(icalproperty* property,
			     icalcomponent* component)
{
    icalerror_check_arg_rv( (property!=0),"property");
    
    property->parent = component;
}

icalcomponent* icalproperty_get_parent(const icalproperty* property)
{
    icalerror_check_arg_rz( (property!=0),"property");

    return property->parent;
}
