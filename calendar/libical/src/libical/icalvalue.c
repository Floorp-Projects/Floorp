/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vi:set ts=4 sts=4 sw=4 expandtab : */
/*======================================================================
  FILE: icalvalue.c
  CREATOR: eric 02 May 1999
  
  $Id: icalvalue.c,v 1.40 2005/01/24 13:11:31 acampi Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvalue.c

  Contributions from:
     Graham Davison (g.m.davison@computer.org)


======================================================================*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icalerror.h"
#include "icalmemory.h"
#include "icalparser.h"
#include "icalenums.h"
#include "icalvalueimpl.h"

#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for snprintf */
#include <string.h> /* For memset, others */
#include <stddef.h> /* For offsetof() macro */
#include <errno.h>
#include <time.h> /* for mktime */
#include <stdlib.h> /* for atoi and atof */
#include <limits.h> /* for SHRT_MAX */         

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

#if _MAC_OS_
#include "icalmemory_strdup.h"
#endif

#define TMP_BUF_SIZE 1024

void print_datetime_to_string(char* str,  const struct icaltimetype *data);
void print_date_to_string(char* str,  const struct icaltimetype *data);
void print_time_to_string(char* str,  const struct icaltimetype *data);


struct icalvalue_impl*  icalvalue_new_impl(icalvalue_kind kind){

    struct icalvalue_impl* v;

    if (!icalvalue_kind_is_valid(kind))
      return NULL;

    if ( ( v = (struct icalvalue_impl*)
	   malloc(sizeof(struct icalvalue_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(v->id,"val");
    
    v->kind = kind;
    v->size = 0;
    v->parent = 0;
    v->x_value = 0;
    memset(&(v->data),0,sizeof(v->data));
    
    return v;

}



icalvalue*
icalvalue_new (icalvalue_kind kind)
{
    return (icalvalue*)icalvalue_new_impl(kind);
}

icalvalue* icalvalue_new_clone(const icalvalue* old) {
    struct icalvalue_impl* new;

    new = icalvalue_new_impl(old->kind);

    if (new == 0){
	return 0;
    }

    strcpy(new->id, old->id);
    new->kind = old->kind;
    new->size = old->size;

    switch (new->kind){
	case ICAL_ATTACH_VALUE: 
	case ICAL_BINARY_VALUE: 
	{
	    /* Hmm.  We just ref the attach value, which may not be the right
	     * thing to do.  We cannot quite copy the data, anyways, since we
	     * don't know how long it is.
	     */
	    new->data.v_attach = old->data.v_attach;
	    if (new->data.v_attach)
		icalattach_ref (new->data.v_attach);

	    break;
	}
	case ICAL_QUERY_VALUE:
	case ICAL_STRING_VALUE:
	case ICAL_TEXT_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_URI_VALUE:
	{
	    if (old->data.v_string != 0) { 
		new->data.v_string=icalmemory_strdup(old->data.v_string);

		if ( new->data.v_string == 0 ) {
		    return 0;
		}		    

	    }
	    break;
	}
	case ICAL_RECUR_VALUE:
	{
	    if(old->data.v_recur != 0){
		new->data.v_recur = malloc(sizeof(struct icalrecurrencetype));

		if(new->data.v_recur == 0){
		    return 0;
		}

		memcpy(	new->data.v_recur, old->data.v_recur,
			sizeof(struct icalrecurrencetype));	
	    }
	    break;
	}

	case ICAL_X_VALUE: 
	{
	    if (old->x_value != 0) {
		new->x_value=icalmemory_strdup(old->x_value);

		if (new->x_value == 0) {
		    return 0;
		}
	    }

	    break;
	}

	default:
	{
	    /* all of the other types are stored as values, not
               pointers, so we can just copy the whole structure. */

	    new->data = old->data;
	}
    }

    return new;
}

static char* icalmemory_strdup_and_dequote(const char* str)
{
    const char* p;
    char* out = (char*)malloc(sizeof(char) * strlen(str) +1);
    char* pout;

    if (out == 0){
	return 0;
    }

    pout = out;

    for (p = str; *p!=0; p++){
	
	if( *p == '\\')
	{
	    p++;
	    switch(*p){
		case 0:
		{
		    *pout = '\0';
		    break;

		}
		case 'n':
		case 'N':
		{
		    *pout = '\n';
		    break;
		}
		case 't':
		case 'T':
		{
		    *pout = '\t';
		    break;
		}
		case 'r':
		case 'R':
		{
		    *pout = '\r';
		    break;
		}
		case 'b':
		case 'B':
		{
		    *pout = '\b';
		    break;
		}
		case 'f':
		case 'F':
		{
		    *pout = '\f';
		    break;
		}
		case ';':
		case ',':
		case '"':
		case '\\':
		{
		    *pout = *p;
		    break;
		}
		default:
		{
		    *pout = ' ';
		}		
	    }
	} else {
	    *pout = *p;
	}

	pout++;
	
    }

    *pout = '\0';

    return out;
}

/*
 * FIXME
 *
 * This is a bad API, as it forces callers to specify their own X type.
 * This function should take care of this by itself.
 */
static
icalvalue* icalvalue_new_enum(icalvalue_kind kind, int x_type, const char* str)
{
    int e = icalproperty_kind_and_string_to_enum(kind, str);
    struct icalvalue_impl *value; 

    if(e != 0 && icalproperty_enum_belongs_to_property(
                   icalproperty_value_kind_to_kind(kind),e)) {
        
        value = icalvalue_new_impl(kind);
        value->data.v_enum = e;
    } else {
        /* Make it an X value */
        value = icalvalue_new_impl(kind);
        value->data.v_enum = x_type;
        icalvalue_set_x(value,str);
    }

    return value;
}


icalvalue* icalvalue_new_from_string_with_error(icalvalue_kind kind,const char* str,icalproperty** error)
{

    struct icalvalue_impl *value = 0;
    
    icalerror_check_arg_rz(str!=0,"str");

    if (error != 0){
	*error = 0;
    }

    switch (kind){
	
    case ICAL_ATTACH_VALUE:
	{
	    icalattach *attach;

	    attach = icalattach_new_from_url (str);
	    if (!attach)
		break;

	    value = icalvalue_new_attach (attach);
	    icalattach_unref (attach);
	    break;
	}

    case ICAL_BINARY_VALUE:
    case ICAL_BOOLEAN_VALUE:
        {
            /* HACK */
            value = 0;
            
	    if (error != 0){
		char temp[TMP_BUF_SIZE];
		snprintf(temp,sizeof(temp),"%s Values are not implemented",
                        icalvalue_kind_to_string(kind)); 
		*error = icalproperty_vanew_xlicerror( 
                                   temp, 
                                   icalparameter_new_xlicerrortype( 
                                        ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
                                   0); 
	    }
	    break;
	}
        

    case ICAL_TRANSP_VALUE:
        value = icalvalue_new_enum(kind, (int)ICAL_TRANSP_X,str);
        break;
    case ICAL_METHOD_VALUE:
        value = icalvalue_new_enum(kind, (int)ICAL_METHOD_X,str);
        break;
    case ICAL_STATUS_VALUE:
        value = icalvalue_new_enum(kind, (int)ICAL_STATUS_X,str);
        break;
    case ICAL_ACTION_VALUE:
        value = icalvalue_new_enum(kind, (int)ICAL_ACTION_X,str);
        break;

    case ICAL_QUERY_VALUE:
	    value = icalvalue_new_query(str);
	    break;

    case ICAL_CLASS_VALUE:
        value = icalvalue_new_enum(kind, (int)ICAL_CLASS_X,str);
        break;

    case ICAL_CMD_VALUE:
        value = icalvalue_new_enum(kind, ICAL_CMD_X,str);
        break;
    case ICAL_QUERYLEVEL_VALUE:
        value = icalvalue_new_enum(kind, ICAL_QUERYLEVEL_X,str);
        break;
    case ICAL_CARLEVEL_VALUE:
        value = icalvalue_new_enum(kind, ICAL_CARLEVEL_X,str);
        break;


    case ICAL_INTEGER_VALUE:
	    value = icalvalue_new_integer(atoi(str));
	    break;

    case ICAL_FLOAT_VALUE:
	    value = icalvalue_new_float((float)atof(str));
	    break;

    case ICAL_UTCOFFSET_VALUE:
	{
            int t,utcoffset, hours, minutes, seconds;
            /* treat the UTCOFSET string a a decimal number, disassemble its digits
               and reconstruct it as sections */
            t = strtol(str,0,10);
            /* add phantom seconds field */
            if(abs(t)<9999){t *= 100; }
            hours = (t/10000);
            minutes = (t-hours*10000)/100;
            seconds = (t-hours*10000-minutes*100);
            utcoffset = hours*3600+minutes*60+seconds;

	    value = icalvalue_new_utcoffset(utcoffset);

	    break;
	}
        
    case ICAL_TEXT_VALUE:
	{
	    char* dequoted_str = icalmemory_strdup_and_dequote(str);
	    value = icalvalue_new_text(dequoted_str);
	    free(dequoted_str);
	    break;
	}
        
    case ICAL_STRING_VALUE:
	    value = icalvalue_new_string(str);
	    break;
        
    case ICAL_CALADDRESS_VALUE:
	    value = icalvalue_new_caladdress(str);
	    break;
        
    case ICAL_URI_VALUE:
	    value = icalvalue_new_uri(str);
	    break;
        
    case ICAL_GEO_VALUE:
	    value = 0;
	    /* HACK */
            
	    if (error != 0){
		*error = icalproperty_vanew_xlicerror( 
		    "GEO Values are not implemented",
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    /*icalerror_warn("Parsing GEO properties is unimplmeneted");*/

	    break;
        
    case ICAL_RECUR_VALUE:
	{
	    struct icalrecurrencetype rt;
	    rt = icalrecurrencetype_from_string(str);
            if(rt.freq != ICAL_NO_RECURRENCE){
                value = icalvalue_new_recur(rt);
            }
	    break;
	}
        
    case ICAL_DATE_VALUE:
    case ICAL_DATETIME_VALUE:
	{
	    struct icaltimetype tt;
      
	    tt = icaltime_from_string(str);
            if(!icaltime_is_null_time(tt)){
                value = icalvalue_new_impl(kind);
                value->data.v_time = tt;

                icalvalue_reset_kind(value);
            }
	    break;
	}
        
    case ICAL_DATETIMEPERIOD_VALUE:
	{
	    struct icaltimetype tt;
            struct icalperiodtype p;
            tt = icaltime_from_string(str);

            if(!icaltime_is_null_time(tt)){
                value = icalvalue_new_datetime(tt);		
		break;
            }  
	    
            p = icalperiodtype_from_string(str);	    
	    if (!icalperiodtype_is_null_period(p)){
                value = icalvalue_new_period(p);
            }            

            break;
	}
        
    case ICAL_DURATION_VALUE:
	{
            struct icaldurationtype dur = icaldurationtype_from_string(str);
            
            if (!icaldurationtype_is_bad_duration(dur)) {    /* failed to parse */
                value = icalvalue_new_duration(dur);
            }
            
	    break;
	}
        
    case ICAL_PERIOD_VALUE:
	{
            struct icalperiodtype p;
            p = icalperiodtype_from_string(str);  
            
            if(!icalperiodtype_is_null_period(p)){
                value = icalvalue_new_period(p);
            }
            break; 
	}
	
    case ICAL_TRIGGER_VALUE:
	{
	    struct icaltriggertype tr = icaltriggertype_from_string(str);
            if (!icaltriggertype_is_bad_trigger(tr)) {
                value = icalvalue_new_trigger(tr);
            }
	    break;
	}
        
    case ICAL_REQUESTSTATUS_VALUE:
        {
            struct icalreqstattype rst = icalreqstattype_from_string(str);
            if(rst.code != ICAL_UNKNOWN_STATUS){
                value = icalvalue_new_requeststatus(rst);
            }
            break;

        }

    case ICAL_X_VALUE:
        {
            char* dequoted_str = icalmemory_strdup_and_dequote(str);
            value = icalvalue_new_x(dequoted_str);
            free(dequoted_str);
        }
        break;

    default:
        {
            if (error != 0 ){
		char temp[TMP_BUF_SIZE];
                
                snprintf(temp,TMP_BUF_SIZE,"Unknown type for \'%s\'",str);
			    
		*error = icalproperty_vanew_xlicerror( 
		    temp, 
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    icalerror_warn("icalvalue_new_from_string got an unknown value type");
            value=0;
	}
    }


    if (error != 0 && *error == 0 && value == 0){
	char temp[TMP_BUF_SIZE];
	
        snprintf(temp,TMP_BUF_SIZE,"Failed to parse value: \'%s\'",str);
	
	*error = icalproperty_vanew_xlicerror( 
	    temp, 
	    icalparameter_new_xlicerrortype( 
		ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
	    0); 
    }


    return value;

}

icalvalue* icalvalue_new_from_string(icalvalue_kind kind,const char* str)
{
    return icalvalue_new_from_string_with_error(kind,str,(icalproperty**)0);
}



void
icalvalue_free (icalvalue* v)
{
    icalerror_check_arg_rv((v != 0),"value");

#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (v->parent ==0),"This value is still attached to a property");
    
#else
    if(v->parent !=0){
	return;
    }
#endif

    if(v->x_value != 0){
        free(v->x_value);
    }

    switch (v->kind){
	case ICAL_BINARY_VALUE: 
	case ICAL_ATTACH_VALUE: {
	    if (v->data.v_attach) {
		icalattach_unref (v->data.v_attach);
		v->data.v_attach = NULL;
	    }

	    break;
	}
	case ICAL_TEXT_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_URI_VALUE:
	case ICAL_QUERY_VALUE:
	{
	    if (v->data.v_string != 0) { 
		free((void*)v->data.v_string);
		v->data.v_string = 0;
	    }
	    break;
	}
	case ICAL_RECUR_VALUE:
	{
	    if(v->data.v_recur != 0){
		free((void*)v->data.v_recur);
		v->data.v_recur = 0;
	    }
	    break;
	}

	default:
	{
	    /* Nothing to do */
	}
    }

    v->kind = ICAL_NO_VALUE;
    v->size = 0;
    v->parent = 0;
    memset(&(v->data),0,sizeof(v->data));
    v->id[0] = 'X';
    free(v);
}

int
icalvalue_is_valid (const icalvalue* value)
{
    if(value == 0){
	return 0;
    }
    
    return 1;
}

static char* icalvalue_binary_as_ical_string(const icalvalue* value) {

    const char* data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_binary(value);

    str = icalmemory_tmp_copy(
            "icalvalue_binary_as_ical_string is not implemented yet");

    return str;
}


#define MAX_INT_DIGITS 12 /* Enough for 2^32 + sign*/ 
    
static char* icalvalue_int_as_ical_string(const icalvalue* value) {
    int data;
    char* str = (char*)icalmemory_tmp_buffer(MAX_INT_DIGITS); 

    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_integer(value);
	
    snprintf(str,MAX_INT_DIGITS,"%d",data);

    return str;
}

static char* icalvalue_utcoffset_as_ical_string(const icalvalue* value)
{    
    int data,h,m,s;
    char sign;
    char* str = (char*)icalmemory_tmp_buffer(9);

    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_utcoffset(value);

    if (abs(data) == data){
	sign = '+';
    } else {
	sign = '-';
    }

    h = data/3600;
    m = (data - (h*3600))/ 60;
    s = (data - (h*3600) - (m*60));

    if (s > 0)
	snprintf(str,9,"%c%02d%02d%02d",sign,abs(h),abs(m),abs(s));
    else
	snprintf(str,9,"%c%02d%02d",sign,abs(h),abs(m));

    return str;
}

static char* icalvalue_string_as_ical_string(const icalvalue* value) {

    const char* data;
    char* str = 0;
    icalerror_check_arg_rz( (value!=0),"value");
    data = value->data.v_string;

    str = (char*)icalmemory_tmp_buffer(strlen(data)+1);   

    strcpy(str,data);

    return str;
}


static char* icalvalue_recur_as_ical_string(const icalvalue* value) 
{
    struct icalrecurrencetype *recur = value->data.v_recur;

    return icalrecurrencetype_as_string(recur);
}

 /* @todo This is not RFC2445 compliant.
 * The RFC only allows:
 * TSAFE-CHAR = %x20-21 / %x23-2B / %x2D-39 / %x3C-5B / %x5D-7E / NON-US-ASCII
 * As such, \t\r\b\f are not allowed, not even escaped
 */

static char* icalvalue_text_as_ical_string(const icalvalue* value) {
    char *str;
    char *str_p;
    char *rtrn;
    const char *p;
    size_t buf_sz;

    buf_sz = strlen(value->data.v_string)+1;

    str_p = str = (char*)icalmemory_new_buffer(buf_sz);

    if (str_p == 0){
      return 0;
    }

    for(p=value->data.v_string; *p!=0; p++){

	switch(*p){
	    case '\n': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\n");
		break;
	    }

	    case '\t': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\t");
		break;
	    }
	    case '\r': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\r");
		break;
	    }
	    case '\b': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\b");
		break;
	    }
	    case '\f': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\f");
		break;
	    }

	    case ';':
	    case ',':
	    case '"':
	    case '\\':{
		icalmemory_append_char(&str,&str_p,&buf_sz,'\\');
		icalmemory_append_char(&str,&str_p,&buf_sz,*p);
		break;
	    }

	    default: {
		icalmemory_append_char(&str,&str_p,&buf_sz,*p);
	    }
	}
    }

    /* Assume the last character is not a '\0' and add one. We could
       check *str_p != 0, but that would be an uninitialized memory
       read. */


    icalmemory_append_char(&str,&str_p,&buf_sz,'\0');

    rtrn = icalmemory_tmp_copy(str);

    icalmemory_free_buffer(str);

    return rtrn;
}


static char* 
icalvalue_attach_as_ical_string(const icalvalue* value) 
{
    icalattach *a;
    char * str;

    icalerror_check_arg_rz( (value!=0),"value");

    a = icalvalue_get_attach(value);

    if (icalattach_get_is_url (a)) {
	const char *url;

	url = icalattach_get_url (a);
	str = icalmemory_tmp_buffer (strlen (url) + 1);
	strcpy (str, url);
	return str;
    } else
	return icalvalue_binary_as_ical_string (value);
}


static char* icalvalue_duration_as_ical_string(const icalvalue* value) {

    struct icaldurationtype data;

    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_duration(value);

    return icaldurationtype_as_ical_string(data);
}

void print_time_to_string(char* str, const struct icaltimetype *data)
{
    char temp[20];

    if (icaltime_is_utc(*data)){
	snprintf(temp,sizeof(temp),"%02d%02d%02dZ",data->hour,data->minute,data->second);
    } else {
	snprintf(temp,sizeof(temp),"%02d%02d%02d",data->hour,data->minute,data->second);
    }   

    strcat(str,temp);
}

 
void print_date_to_string(char* str,  const struct icaltimetype *data)
{
    char temp[20];

    snprintf(temp,sizeof(temp),"%04d%02d%02d",data->year,data->month,data->day);

    strcat(str,temp);
}

static char* icalvalue_date_as_ical_string(const icalvalue* value) {

    struct icaltimetype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_date(value);

    str = (char*)icalmemory_tmp_buffer(9);
 
    str[0] = 0;
    print_date_to_string(str,&data);
   
    return str;
}

void print_datetime_to_string(char* str,  const struct icaltimetype *data)
{
    print_date_to_string(str,data);
    strcat(str,"T");
    print_time_to_string(str,data);
}

static const char* icalvalue_datetime_as_ical_string(const icalvalue* value) {
    
    struct icaltimetype data;
    char* str;
    icalvalue_kind kind = icalvalue_isa(value);    

    icalerror_check_arg_rz( (value!=0),"value");


    if( !(kind == ICAL_DATE_VALUE || kind == ICAL_DATETIME_VALUE ))
	{
	    icalerror_set_errno(ICAL_BADARG_ERROR);
	    return 0;
	}

    data = icalvalue_get_datetime(value);

    str = (char*)icalmemory_tmp_buffer(20);
 
    str[0] = 0;

    print_datetime_to_string(str,&data);
   
    return str;

}

#define MAX_FLOAT_DIGITS 40
    
static char* icalvalue_float_as_ical_string(const icalvalue* value) {

    float data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_float(value);

    str = (char*)icalmemory_tmp_buffer(MAX_FLOAT_DIGITS);

    snprintf(str,MAX_FLOAT_DIGITS,"%f",data);

    return str;
}

#define MAX_GEO_DIGITS 40
    
static char* icalvalue_geo_as_ical_string(const icalvalue* value) {

    struct icalgeotype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_geo(value);

    str = (char*)icalmemory_tmp_buffer(MAX_GEO_DIGITS);

    snprintf(str,MAX_GEO_DIGITS,"%f;%f",data.lat,data.lon);

    return str;
}

static const char* icalvalue_datetimeperiod_as_ical_string(const icalvalue* value) {
    struct icaldatetimeperiodtype dtp = icalvalue_get_datetimeperiod(value);

    icalerror_check_arg_rz( (value!=0),"value");

    if(!icaltime_is_null_time(dtp.time)){
	return icaltime_as_ical_string(dtp.time);
    } else {
	return icalperiodtype_as_ical_string(dtp.period);
    }
}

static const char* icalvalue_period_as_ical_string(const icalvalue* value) {
    struct icalperiodtype data;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_period(value);

    return icalperiodtype_as_ical_string(data);

}

static const char* icalvalue_trigger_as_ical_string(const icalvalue* value) {

    struct icaltriggertype data;

    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_trigger(value);

    if(!icaltime_is_null_time(data.time)){
	return icaltime_as_ical_string(data.time);
    } else {
	return icaldurationtype_as_ical_string(data.duration);
    }   

}

const char*
icalvalue_as_ical_string(const icalvalue* value)
{
    if(value == 0){
	return 0;
    }

    switch (value->kind){

    case ICAL_ATTACH_VALUE:
        return icalvalue_attach_as_ical_string(value);
        
    case ICAL_BINARY_VALUE:
        return icalvalue_binary_as_ical_string(value);
        
    case ICAL_BOOLEAN_VALUE:
    case ICAL_INTEGER_VALUE:
        return icalvalue_int_as_ical_string(value);                  
        
    case ICAL_UTCOFFSET_VALUE:
        return icalvalue_utcoffset_as_ical_string(value);                  
        
    case ICAL_TEXT_VALUE:
        return icalvalue_text_as_ical_string(value);
        
    case ICAL_QUERY_VALUE:
        return icalvalue_string_as_ical_string(value);
        
    case ICAL_STRING_VALUE:
    case ICAL_URI_VALUE:
    case ICAL_CALADDRESS_VALUE:
        return icalvalue_string_as_ical_string(value);
        
    case ICAL_DATE_VALUE:
        return icalvalue_date_as_ical_string(value);
    case ICAL_DATETIME_VALUE:
        return icalvalue_datetime_as_ical_string(value);
    case ICAL_DURATION_VALUE:
        return icalvalue_duration_as_ical_string(value);
        
    case ICAL_PERIOD_VALUE:
        return icalvalue_period_as_ical_string(value);
    case ICAL_DATETIMEPERIOD_VALUE:
        return icalvalue_datetimeperiod_as_ical_string(value);
        
    case ICAL_FLOAT_VALUE:
        return icalvalue_float_as_ical_string(value);
        
    case ICAL_GEO_VALUE:
        return icalvalue_geo_as_ical_string(value);
        
    case ICAL_RECUR_VALUE:
        return icalvalue_recur_as_ical_string(value);
        
    case ICAL_TRIGGER_VALUE:
        return icalvalue_trigger_as_ical_string(value);

    case ICAL_REQUESTSTATUS_VALUE:
        return icalreqstattype_as_string(value->data.v_requeststatus);
        
    case ICAL_ACTION_VALUE:
    case ICAL_CMD_VALUE:
    case ICAL_QUERYLEVEL_VALUE:
    case ICAL_CARLEVEL_VALUE:
    case ICAL_METHOD_VALUE:
    case ICAL_STATUS_VALUE:
    case ICAL_TRANSP_VALUE:
    case ICAL_CLASS_VALUE:
        if(value->x_value !=0){
            return icalmemory_tmp_copy(value->x_value);
        }

        return icalproperty_enum_to_string(value->data.v_enum);
        
    case ICAL_X_VALUE: 
	if (value->x_value != 0)
            return icalmemory_tmp_copy(value->x_value);

    /* FALLTHRU */

    case ICAL_NO_VALUE:
    default:
	{
	    return 0;
	}
    }
}


icalvalue_kind
icalvalue_isa (const icalvalue* value)
{
    if(value == 0){
	return ICAL_NO_VALUE;
    }

    return value->kind;
}


int
icalvalue_isa_value (void* value)
{
    struct icalvalue_impl *impl = (struct icalvalue_impl *)value;

    icalerror_check_arg_rz( (value!=0), "value");

    if (strcmp(impl->id,"val") == 0) {
	return 1;
    } else {
	return 0;
    }
}


static int icalvalue_is_time(const icalvalue* a) {
    icalvalue_kind kind = icalvalue_isa(a);

    if(kind == ICAL_DATETIME_VALUE ||
       kind == ICAL_DATE_VALUE ){
	return 1;
    }

    return 0;

}

/*
 * In case of error, this function returns 0. This is partly bogus, as 0 is
 * not part of the returned enum.
 * FIXME We should probably add an error value to the enum.
 */
icalparameter_xliccomparetype
icalvalue_compare(const icalvalue* a, const icalvalue *b)
{

    icalerror_check_arg_rz( (a!=0), "a");
    icalerror_check_arg_rz( (b!=0), "b");

    /* Not the same type; they can only be unequal */
    if( ! (icalvalue_is_time(a) && icalvalue_is_time(b)) &&
	icalvalue_isa(a) != icalvalue_isa(b)){
	return ICAL_XLICCOMPARETYPE_NOTEQUAL;
    }

    switch (icalvalue_isa(a)){

	case ICAL_ATTACH_VALUE:
	{
	    if (icalattach_get_is_url(a->data.v_attach) &&
	        icalattach_get_is_url(b->data.v_attach)) {
		if (strcasecmp(icalattach_get_url(a->data.v_attach),
			       icalattach_get_url(b->data.v_attach)) == 0)
		    return ICAL_XLICCOMPARETYPE_EQUAL;
		else
		    return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }
	    else {
	        if (a->data.v_attach == b->data.v_attach)
		    return ICAL_XLICCOMPARETYPE_EQUAL;
	        else
		    return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }
	}
        case ICAL_BINARY_VALUE:
	{
	    if (a->data.v_attach == b->data.v_attach)
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    else
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	}

	case ICAL_BOOLEAN_VALUE:
	{
	    if (icalvalue_get_boolean(a) == icalvalue_get_boolean(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }
	}

	case ICAL_FLOAT_VALUE:
	{
	    if (a->data.v_float > b->data.v_float){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (a->data.v_float < b->data.v_float){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}

	case ICAL_INTEGER_VALUE:
	case ICAL_UTCOFFSET_VALUE:
	{
	    if (a->data.v_int > b->data.v_int){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (a->data.v_int < b->data.v_int){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}

	case ICAL_DURATION_VALUE: 
	{
	    int dur_a = icaldurationtype_as_int(a->data.v_duration);
	    int dur_b = icaldurationtype_as_int(b->data.v_duration);

	    if (dur_a > dur_b){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (dur_a < dur_b){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}	    


	case ICAL_TEXT_VALUE:
	case ICAL_URI_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_TRIGGER_VALUE:
	case ICAL_DATE_VALUE:
	case ICAL_DATETIME_VALUE:
	case ICAL_DATETIMEPERIOD_VALUE:
	case ICAL_QUERY_VALUE:
	case ICAL_RECUR_VALUE:
	{
	    int r;

	    r =  strcmp(icalvalue_as_ical_string(a),
			  icalvalue_as_ical_string(b));

	    if (r > 0) { 	
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (r < 0){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }

		
	}

	case ICAL_METHOD_VALUE:
	{
	    if (icalvalue_get_method(a) == icalvalue_get_method(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }

	}

	case ICAL_STATUS_VALUE:
	{
	    if (icalvalue_get_status(a) == icalvalue_get_status(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }

	}

	case ICAL_TRANSP_VALUE:
	{
	    if (icalvalue_get_transp(a) == icalvalue_get_transp(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }
	}

	case ICAL_ACTION_VALUE:
        {
	    if (icalvalue_get_action(a) == icalvalue_get_action(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }
        }

	case ICAL_PERIOD_VALUE:
	case ICAL_GEO_VALUE:
	case ICAL_NO_VALUE:
	default:
	{
	    icalerror_warn("Comparison not implemented for value type");
	    return 0;
	}
    }   

}

/** Examine the value and possibly change the kind to agree with the
 *  value 
 */

void icalvalue_reset_kind(icalvalue* value)
{
    if( (value->kind==ICAL_DATETIME_VALUE || value->kind==ICAL_DATE_VALUE )&&
        !icaltime_is_null_time(value->data.v_time) ) {
        
        if(icaltime_is_date(value->data.v_time)){
            value->kind = ICAL_DATE_VALUE;
        } else {
            value->kind = ICAL_DATETIME_VALUE;
        }
    }
       
}

void icalvalue_set_parent(icalvalue* value,
			     icalproperty* property)
{
    value->parent = property;
}

icalproperty* icalvalue_get_parent(icalvalue* value)
{
    return value->parent;
}


int icalvalue_encode_ical_string(const char *szText, char *szEncText, int nMaxBufferLen)
{
    char   *ptr;
    icalvalue *value = 0;

    if ((szText == 0) || (szEncText == 0))
        return 0;
   
    value = icalvalue_new_from_string(ICAL_STRING_VALUE, szText);
    
    if (value == 0)
        return 0;
    
    ptr = icalvalue_text_as_ical_string(value);
    if (ptr == 0)
        return 0;
    
    if ((int)strlen(ptr) >= nMaxBufferLen)
        {
            icalvalue_free (value);
            return 0;
        }

    strcpy(szEncText, ptr);

    icalvalue_free ((icalvalue*)value);

    return 1;
}

/* The remaining interfaces are 'new', 'set' and 'get' for each of the value
   types */
