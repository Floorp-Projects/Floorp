/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalvalue.c
  CREATOR: eric 02 May 1999
  
  $Id: icalderivedvalue.c,v 1.5 2002/09/01 19:12:31 gray-john Exp $


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
#include <stdio.h> /* for sprintf */
#include <string.h> /* For memset, others */
#include <stddef.h> /* For offsetof() macro */
#include <errno.h>
#include <time.h> /* for mktime */
#include <stdlib.h> /* for atoi and atof */
#include <limits.h> /* for SHRT_MAX */         

struct icalvalue_impl*  icalvalue_new_impl(icalvalue_kind kind);

/* This map associates each of the value types with its string
   representation */
struct icalvalue_kind_map {
	icalvalue_kind kind;
	char name[20];
};

static struct icalvalue_kind_map value_map[28]={
    {ICAL_BOOLEAN_VALUE,"BOOLEAN"},
    {ICAL_UTCOFFSET_VALUE,"UTC-OFFSET"},
    {ICAL_XLICCLASS_VALUE,"X-LIC-CLASS"},
    {ICAL_RECUR_VALUE,"RECUR"},
    {ICAL_METHOD_VALUE,"METHOD"},
    {ICAL_CALADDRESS_VALUE,"CAL-ADDRESS"},
    {ICAL_PERIOD_VALUE,"PERIOD"},
    {ICAL_STATUS_VALUE,"STATUS"},
    {ICAL_BINARY_VALUE,"BINARY"},
    {ICAL_TEXT_VALUE,"TEXT"},
    {ICAL_DURATION_VALUE,"DURATION"},
    {ICAL_DATETIMEPERIOD_VALUE,"DATE-TIME-PERIOD"},
    {ICAL_INTEGER_VALUE,"INTEGER"},
    {ICAL_URI_VALUE,"URI"},
    {ICAL_TRIGGER_VALUE,"TRIGGER"},
    {ICAL_ATTACH_VALUE,"ATTACH"},
    {ICAL_CLASS_VALUE,"CLASS"},
    {ICAL_FLOAT_VALUE,"FLOAT"},
    {ICAL_QUERY_VALUE,"QUERY"},
    {ICAL_STRING_VALUE,"STRING"},
    {ICAL_TRANSP_VALUE,"TRANSP"},
    {ICAL_X_VALUE,"X"},
    {ICAL_DATETIME_VALUE,"DATE-TIME"},
    {ICAL_REQUESTSTATUS_VALUE,"REQUEST-STATUS"},
    {ICAL_GEO_VALUE,"GEO"},
    {ICAL_DATE_VALUE,"DATE"},
    {ICAL_ACTION_VALUE,"ACTION"},
    {ICAL_NO_VALUE,""}
};


icalvalue* icalvalue_new_boolean (int v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_BOOLEAN_VALUE);
   
   icalvalue_set_boolean((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_boolean(icalvalue* value, int v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_BOOLEAN_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_int = v; 

    icalvalue_reset_kind(impl);
}
int icalvalue_get_boolean(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_BOOLEAN_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_int;
}



icalvalue* icalvalue_new_utcoffset (int v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_UTCOFFSET_VALUE);
   
   icalvalue_set_utcoffset((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_utcoffset(icalvalue* value, int v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_UTCOFFSET_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_int = v; 

    icalvalue_reset_kind(impl);
}
int icalvalue_get_utcoffset(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_UTCOFFSET_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_int;
}



icalvalue* icalvalue_new_xlicclass (enum icalproperty_xlicclass v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_XLICCLASS_VALUE);
   
   icalvalue_set_xlicclass((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_xlicclass(icalvalue* value, enum icalproperty_xlicclass v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_XLICCLASS_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_enum = v; 

    icalvalue_reset_kind(impl);
}
enum icalproperty_xlicclass icalvalue_get_xlicclass(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_XLICCLASS_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_enum;
}



icalvalue* icalvalue_new_method (enum icalproperty_method v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_METHOD_VALUE);
   
   icalvalue_set_method((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_method(icalvalue* value, enum icalproperty_method v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_METHOD_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_enum = v; 

    icalvalue_reset_kind(impl);
}
enum icalproperty_method icalvalue_get_method(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_METHOD_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_enum;
}



icalvalue* icalvalue_new_caladdress (const char* v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_CALADDRESS_VALUE);
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_caladdress((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_caladdress(icalvalue* value, const char* v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_CALADDRESS_VALUE);
    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free((void*)impl->data.v_string);}


    impl->data.v_string = icalmemory_strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }
 

    icalvalue_reset_kind(impl);
}
const char* icalvalue_get_caladdress(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_CALADDRESS_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_string;
}



icalvalue* icalvalue_new_period (struct icalperiodtype v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_PERIOD_VALUE);
   
   icalvalue_set_period((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_period(icalvalue* value, struct icalperiodtype v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_PERIOD_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_period = v; 

    icalvalue_reset_kind(impl);
}
struct icalperiodtype icalvalue_get_period(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_PERIOD_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_period;
}



icalvalue* icalvalue_new_status (enum icalproperty_status v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_STATUS_VALUE);
   
   icalvalue_set_status((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_status(icalvalue* value, enum icalproperty_status v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_STATUS_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_enum = v; 

    icalvalue_reset_kind(impl);
}
enum icalproperty_status icalvalue_get_status(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_STATUS_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_enum;
}



icalvalue* icalvalue_new_binary (const char* v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_BINARY_VALUE);
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_binary((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_binary(icalvalue* value, const char* v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_BINARY_VALUE);
    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free((void*)impl->data.v_string);}


    impl->data.v_string = icalmemory_strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }
 

    icalvalue_reset_kind(impl);
}
const char* icalvalue_get_binary(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_BINARY_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_string;
}



icalvalue* icalvalue_new_text (const char* v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_TEXT_VALUE);
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_text((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_text(icalvalue* value, const char* v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_TEXT_VALUE);
    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free((void*)impl->data.v_string);}


    impl->data.v_string = icalmemory_strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }
 

    icalvalue_reset_kind(impl);
}
const char* icalvalue_get_text(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_TEXT_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_string;
}



icalvalue* icalvalue_new_duration (struct icaldurationtype v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DURATION_VALUE);
   
   icalvalue_set_duration((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_duration(icalvalue* value, struct icaldurationtype v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DURATION_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_duration = v; 

    icalvalue_reset_kind(impl);
}
struct icaldurationtype icalvalue_get_duration(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DURATION_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_duration;
}



icalvalue* icalvalue_new_integer (int v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_INTEGER_VALUE);
   
   icalvalue_set_integer((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_integer(icalvalue* value, int v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_INTEGER_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_int = v; 

    icalvalue_reset_kind(impl);
}
int icalvalue_get_integer(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_INTEGER_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_int;
}



icalvalue* icalvalue_new_uri (const char* v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_URI_VALUE);
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_uri((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_uri(icalvalue* value, const char* v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_URI_VALUE);
    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free((void*)impl->data.v_string);}


    impl->data.v_string = icalmemory_strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }
 

    icalvalue_reset_kind(impl);
}
const char* icalvalue_get_uri(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_URI_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_string;
}



icalvalue* icalvalue_new_class (enum icalproperty_class v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_CLASS_VALUE);
   
   icalvalue_set_class((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_class(icalvalue* value, enum icalproperty_class v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_CLASS_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_enum = v; 

    icalvalue_reset_kind(impl);
}
enum icalproperty_class icalvalue_get_class(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_CLASS_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_enum;
}



icalvalue* icalvalue_new_float (float v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_FLOAT_VALUE);
   
   icalvalue_set_float((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_float(icalvalue* value, float v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_FLOAT_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_float = v; 

    icalvalue_reset_kind(impl);
}
float icalvalue_get_float(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_FLOAT_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_float;
}



icalvalue* icalvalue_new_query (const char* v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_QUERY_VALUE);
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_query((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_query(icalvalue* value, const char* v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_QUERY_VALUE);
    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free((void*)impl->data.v_string);}


    impl->data.v_string = icalmemory_strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }
 

    icalvalue_reset_kind(impl);
}
const char* icalvalue_get_query(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_QUERY_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_string;
}



icalvalue* icalvalue_new_string (const char* v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_STRING_VALUE);
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_string((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_string(icalvalue* value, const char* v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_STRING_VALUE);
    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free((void*)impl->data.v_string);}


    impl->data.v_string = icalmemory_strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }
 

    icalvalue_reset_kind(impl);
}
const char* icalvalue_get_string(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_STRING_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_string;
}



icalvalue* icalvalue_new_transp (enum icalproperty_transp v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_TRANSP_VALUE);
   
   icalvalue_set_transp((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_transp(icalvalue* value, enum icalproperty_transp v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_TRANSP_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_enum = v; 

    icalvalue_reset_kind(impl);
}
enum icalproperty_transp icalvalue_get_transp(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_TRANSP_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_enum;
}



icalvalue* icalvalue_new_datetime (struct icaltimetype v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DATETIME_VALUE);
   
   icalvalue_set_datetime((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_datetime(icalvalue* value, struct icaltimetype v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DATETIME_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_time = v; 

    icalvalue_reset_kind(impl);
}
struct icaltimetype icalvalue_get_datetime(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DATETIME_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_time;
}



icalvalue* icalvalue_new_requeststatus (struct icalreqstattype v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_REQUESTSTATUS_VALUE);
   
   icalvalue_set_requeststatus((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_requeststatus(icalvalue* value, struct icalreqstattype v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_REQUESTSTATUS_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_requeststatus = v; 

    icalvalue_reset_kind(impl);
}
struct icalreqstattype icalvalue_get_requeststatus(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_REQUESTSTATUS_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_requeststatus;
}



icalvalue* icalvalue_new_geo (struct icalgeotype v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_GEO_VALUE);
   
   icalvalue_set_geo((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_geo(icalvalue* value, struct icalgeotype v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_GEO_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_geo = v; 

    icalvalue_reset_kind(impl);
}
struct icalgeotype icalvalue_get_geo(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_GEO_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_geo;
}



icalvalue* icalvalue_new_date (struct icaltimetype v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DATE_VALUE);
   
   icalvalue_set_date((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_date(icalvalue* value, struct icaltimetype v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DATE_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_time = v; 

    icalvalue_reset_kind(impl);
}
struct icaltimetype icalvalue_get_date(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DATE_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_time;
}



icalvalue* icalvalue_new_action (enum icalproperty_action v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_ACTION_VALUE);
   
   icalvalue_set_action((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_action(icalvalue* value, enum icalproperty_action v) {
    struct icalvalue_impl* impl; 
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_ACTION_VALUE);
    impl = (struct icalvalue_impl*)value;


    impl->data.v_enum = v; 

    icalvalue_reset_kind(impl);
}
enum icalproperty_action icalvalue_get_action(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_ACTION_VALUE);
    return ((struct icalvalue_impl*)value)->data.v_enum;
}


int icalvalue_kind_is_valid(const icalvalue_kind kind)
{
    int i = 0;
    do {
      if (value_map[i].kind == kind)
	return 1;
    } while (value_map[i++].kind != ICAL_NO_VALUE);

    return 0;
}  

const char* icalvalue_kind_to_string(const icalvalue_kind kind)
{
    int i;

    for (i=0; value_map[i].kind != ICAL_NO_VALUE; i++) {
	if (value_map[i].kind == kind) {
	    return value_map[i].name;
	}
    }

    return 0;
}

icalvalue_kind icalvalue_string_to_kind(const char* str)
{
    int i;

    for (i=0; value_map[i].kind != ICAL_NO_VALUE; i++) {
	if (strcmp(value_map[i].name,str) == 0) {
	    return value_map[i].kind;
	}
    }

    return  value_map[i].kind;

}

icalvalue* icalvalue_new_x (const char* v){
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_X_VALUE);
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_x((icalvalue*)impl,v);
   return (icalvalue*)impl;
}
void icalvalue_set_x(icalvalue* impl, const char* v) {
    icalerror_check_arg_rv( (impl!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    if(impl->x_value!=0) {free((void*)impl->x_value);}

    impl->x_value = icalmemory_strdup(v);

    if (impl->x_value == 0){
      errno = ENOMEM;
    }
 
 }
const char* icalvalue_get_x(const icalvalue* value) {

    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_X_VALUE);
    return value->x_value;
}

/* Recur is a special case, so it is not auto generated. */
icalvalue*
icalvalue_new_recur (struct icalrecurrencetype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_RECUR_VALUE);
    
   icalvalue_set_recur((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_recur(icalvalue* impl, struct icalrecurrencetype v)
{
    icalerror_check_arg_rv( (impl!=0),"value");
    icalerror_check_value_type(value, ICAL_RECUR_VALUE);

    if (impl->data.v_recur != 0){
	free(impl->data.v_recur);
	impl->data.v_recur = 0;
    }

    impl->data.v_recur = malloc(sizeof(struct icalrecurrencetype));

    if (impl->data.v_recur == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return;
    } else {
	memcpy(impl->data.v_recur, &v, sizeof(struct icalrecurrencetype));
    }
	       
}

struct icalrecurrencetype
icalvalue_get_recur(const icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_RECUR_VALUE);
  
    return *(value->data.v_recur);
}




icalvalue*
icalvalue_new_trigger (struct icaltriggertype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_TRIGGER_VALUE);
 
   icalvalue_set_trigger((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_trigger(icalvalue* value, struct icaltriggertype v)
{
    icalerror_check_arg_rv( (value!=0),"value");
    
   if(!icaltime_is_null_time(v.time)){
       icalvalue_set_datetime(value,v.time);
       value->kind = ICAL_DATETIME_VALUE;
   } else {
       icalvalue_set_duration(value,v.duration);
       value->kind = ICAL_DURATION_VALUE;
   }
}

struct icaltriggertype
icalvalue_get_trigger(const icalvalue* impl)
{
    struct icaltriggertype tr;

    icalerror_check_arg( (impl!=0),"value");
    icalerror_check_arg( (impl!=0),"value");

    if(impl->kind == ICAL_DATETIME_VALUE){
	 tr.duration = icaldurationtype_from_int(0);
	 tr.time = impl->data.v_time;
    } else if(impl->kind == ICAL_DURATION_VALUE){
	tr.time = icaltime_null_time();
	tr.duration = impl->data.v_duration;
    } else {
	tr.duration = icaldurationtype_from_int(0);
	tr.time = icaltime_null_time();
	icalerror_set_errno(ICAL_BADARG_ERROR);
    }

    return tr;
}

/* DATE-TIME-PERIOD is a special case, and is not auto generated */

icalvalue*
icalvalue_new_datetimeperiod (struct icaldatetimeperiodtype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DATETIMEPERIOD_VALUE);

   icalvalue_set_datetimeperiod(impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_datetimeperiod(icalvalue* impl, struct icaldatetimeperiodtype v)
{
    icalerror_check_arg_rv( (impl!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DATETIMEPERIOD_VALUE);

    if(!icaltime_is_null_time(v.time)){
	if(!icaltime_is_valid_time(v.time)){
	    icalerror_set_errno(ICAL_BADARG_ERROR);
	    return;
	}
	impl->kind = ICAL_DATETIME_VALUE;
	icalvalue_set_datetime(impl,v.time);
    } else if (!icalperiodtype_is_null_period(v.period)) {
	if(!icalperiodtype_is_valid_period(v.period)){
	    icalerror_set_errno(ICAL_BADARG_ERROR);
	    return;
	}
	impl->kind = ICAL_PERIOD_VALUE;
	icalvalue_set_period(impl,v.period);
    } else {
	icalerror_set_errno(ICAL_BADARG_ERROR);
    }
}

struct icaldatetimeperiodtype
icalvalue_get_datetimeperiod(const icalvalue* impl)
{
  struct icaldatetimeperiodtype dtp;
  
  icalerror_check_arg( (impl!=0),"value");
  icalerror_check_value_type(value, ICAL_DATETIMEPERIOD_VALUE);
  
  if(impl->kind == ICAL_DATETIME_VALUE){
      dtp.period = icalperiodtype_null_period();
      dtp.time = impl->data.v_time;
  } else if(impl->kind == ICAL_PERIOD_VALUE) {
      dtp.period = impl->data.v_period;
      dtp.time = icaltime_null_time();
  } else {
      dtp.period = icalperiodtype_null_period();
      dtp.time = icaltime_null_time();
      icalerror_set_errno(ICAL_BADARG_ERROR);
  }	

  return dtp;
}



icalvalue *
icalvalue_new_attach (icalattach *attach)
{
    struct icalvalue_impl *impl;

    icalerror_check_arg_rz ((attach != NULL), "attach");

    impl = icalvalue_new_impl (ICAL_ATTACH_VALUE);
    if (!impl) {
	errno = ENOMEM;
	return NULL;
    }

    icalvalue_set_attach ((icalvalue *) impl, attach);
    return (icalvalue *) impl;
}

void
icalvalue_set_attach (icalvalue *value, icalattach *attach)
{
    struct icalvalue_impl *impl;
 
    icalerror_check_arg_rv ((value != NULL), "value");
    icalerror_check_value_type (value, ICAL_ATTACH_VALUE);
    icalerror_check_arg_rv ((attach != NULL), "attach");
  
    impl = (struct icalvalue_impl *) value;
 
    icalattach_ref (attach);

    if (impl->data.v_attach)
	icalattach_unref (impl->data.v_attach);
  
    impl->data.v_attach = attach;
}

icalattach *
icalvalue_get_attach (const icalvalue *value)
{
    icalerror_check_arg_rz ((value != NULL), "value");
    icalerror_check_value_type (value, ICAL_ATTACH_VALUE);
    
    return value->data.v_attach;
}







/* The remaining interfaces are 'new', 'set' and 'get' for each of the value
   types */


/* Everything below this line is machine generated. Do not edit. */
