/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedparameters.{c,h}
  CREATOR: eric 09 May 1999
  
  $Id: icalderivedparameter.c,v 1.5 2002/09/01 19:12:31 gray-john Exp $
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
#include "icalparameterimpl.h"

#include "icalproperty.h"
#include "icalerror.h"
#include "icalmemory.h"

#include <stdlib.h> /* for malloc() */
#include <errno.h>
#include <string.h> /* for memset() */

icalvalue_kind icalparameter_value_to_value_kind(icalparameter_value value);

struct icalparameter_impl* icalparameter_new_impl(icalparameter_kind kind);

/* This map associates each of the parameters with the string
   representation of the paramter's name */
struct icalparameter_kind_map {
    icalparameter_kind kind;
    char *name;
    
};

/* This map associates the enumerations for the VALUE parameter with
   the kinds of VALUEs. */

struct icalparameter_value_kind_map {
    icalparameter_value value; 
    icalvalue_kind kind; 
};

/* This map associates the parameter enumerations with a specific parameter and the string representation of the enumeration */

struct icalparameter_map {
    icalparameter_kind kind;
    int enumeration;
    const char* str;
};



static struct icalparameter_value_kind_map value_kind_map[15] = {
    {ICAL_VALUE_BINARY,ICAL_BINARY_VALUE},
    {ICAL_VALUE_BOOLEAN,ICAL_BOOLEAN_VALUE},
    {ICAL_VALUE_DATE,ICAL_DATE_VALUE},
    {ICAL_VALUE_DURATION,ICAL_DURATION_VALUE},
    {ICAL_VALUE_FLOAT,ICAL_FLOAT_VALUE},
    {ICAL_VALUE_INTEGER,ICAL_INTEGER_VALUE},
    {ICAL_VALUE_PERIOD,ICAL_PERIOD_VALUE},
    {ICAL_VALUE_RECUR,ICAL_RECUR_VALUE},
    {ICAL_VALUE_TEXT,ICAL_TEXT_VALUE},
    {ICAL_VALUE_URI,ICAL_URI_VALUE},
    {ICAL_VALUE_DATETIME,ICAL_DATETIME_VALUE},
    {ICAL_VALUE_UTCOFFSET,ICAL_UTCOFFSET_VALUE},
    {ICAL_VALUE_CALADDRESS,ICAL_CALADDRESS_VALUE},
    {ICAL_VALUE_X,ICAL_X_VALUE},
    {ICAL_VALUE_NONE,ICAL_NO_VALUE}
};

static struct icalparameter_kind_map parameter_map[24] = { 
    {ICAL_ALTREP_PARAMETER,"ALTREP"},
    {ICAL_CN_PARAMETER,"CN"},
    {ICAL_CUTYPE_PARAMETER,"CUTYPE"},
    {ICAL_DELEGATEDFROM_PARAMETER,"DELEGATED-FROM"},
    {ICAL_DELEGATEDTO_PARAMETER,"DELEGATED-TO"},
    {ICAL_DIR_PARAMETER,"DIR"},
    {ICAL_ENCODING_PARAMETER,"ENCODING"},
    {ICAL_FBTYPE_PARAMETER,"FBTYPE"},
    {ICAL_FMTTYPE_PARAMETER,"FMTTYPE"},
    {ICAL_LANGUAGE_PARAMETER,"LANGUAGE"},
    {ICAL_MEMBER_PARAMETER,"MEMBER"},
    {ICAL_PARTSTAT_PARAMETER,"PARTSTAT"},
    {ICAL_RANGE_PARAMETER,"RANGE"},
    {ICAL_RELATED_PARAMETER,"RELATED"},
    {ICAL_RELTYPE_PARAMETER,"RELTYPE"},
    {ICAL_ROLE_PARAMETER,"ROLE"},
    {ICAL_RSVP_PARAMETER,"RSVP"},
    {ICAL_SENTBY_PARAMETER,"SENT-BY"},
    {ICAL_TZID_PARAMETER,"TZID"},
    {ICAL_VALUE_PARAMETER,"VALUE"},
    {ICAL_X_PARAMETER,"X"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,"X-LIC-COMPARETYPE"},
    {ICAL_XLICERRORTYPE_PARAMETER,"X-LIC-ERRORTYPE"},
    { ICAL_NO_PARAMETER, ""}
};

static struct icalparameter_map icalparameter_map[] = {
{ICAL_ANY_PARAMETER,0,""},
    {ICAL_CUTYPE_PARAMETER,ICAL_CUTYPE_INDIVIDUAL,"INDIVIDUAL"},
    {ICAL_CUTYPE_PARAMETER,ICAL_CUTYPE_GROUP,"GROUP"},
    {ICAL_CUTYPE_PARAMETER,ICAL_CUTYPE_RESOURCE,"RESOURCE"},
    {ICAL_CUTYPE_PARAMETER,ICAL_CUTYPE_ROOM,"ROOM"},
    {ICAL_CUTYPE_PARAMETER,ICAL_CUTYPE_UNKNOWN,"UNKNOWN"},
    {ICAL_ENCODING_PARAMETER,ICAL_ENCODING_8BIT,"8BIT"},
    {ICAL_ENCODING_PARAMETER,ICAL_ENCODING_BASE64,"BASE64"},
    {ICAL_FBTYPE_PARAMETER,ICAL_FBTYPE_FREE,"FREE"},
    {ICAL_FBTYPE_PARAMETER,ICAL_FBTYPE_BUSY,"BUSY"},
    {ICAL_FBTYPE_PARAMETER,ICAL_FBTYPE_BUSYUNAVAILABLE,"BUSYUNAVAILABLE"},
    {ICAL_FBTYPE_PARAMETER,ICAL_FBTYPE_BUSYTENTATIVE,"BUSYTENTATIVE"},
    {ICAL_PARTSTAT_PARAMETER,ICAL_PARTSTAT_NEEDSACTION,"NEEDS-ACTION"},
    {ICAL_PARTSTAT_PARAMETER,ICAL_PARTSTAT_ACCEPTED,"ACCEPTED"},
    {ICAL_PARTSTAT_PARAMETER,ICAL_PARTSTAT_DECLINED,"DECLINED"},
    {ICAL_PARTSTAT_PARAMETER,ICAL_PARTSTAT_TENTATIVE,"TENTATIVE"},
    {ICAL_PARTSTAT_PARAMETER,ICAL_PARTSTAT_DELEGATED,"DELEGATED"},
    {ICAL_PARTSTAT_PARAMETER,ICAL_PARTSTAT_COMPLETED,"COMPLETED"},
    {ICAL_PARTSTAT_PARAMETER,ICAL_PARTSTAT_INPROCESS,"INPROCESS"},
    {ICAL_RANGE_PARAMETER,ICAL_RANGE_THISANDPRIOR,"THISANDPRIOR"},
    {ICAL_RANGE_PARAMETER,ICAL_RANGE_THISANDFUTURE,"THISANDFUTURE"},
    {ICAL_RELATED_PARAMETER,ICAL_RELATED_START,"START"},
    {ICAL_RELATED_PARAMETER,ICAL_RELATED_END,"END"},
    {ICAL_RELTYPE_PARAMETER,ICAL_RELTYPE_PARENT,"PARENT"},
    {ICAL_RELTYPE_PARAMETER,ICAL_RELTYPE_CHILD,"CHILD"},
    {ICAL_RELTYPE_PARAMETER,ICAL_RELTYPE_SIBLING,"SIBLING"},
    {ICAL_ROLE_PARAMETER,ICAL_ROLE_CHAIR,"CHAIR"},
    {ICAL_ROLE_PARAMETER,ICAL_ROLE_REQPARTICIPANT,"REQ-PARTICIPANT"},
    {ICAL_ROLE_PARAMETER,ICAL_ROLE_OPTPARTICIPANT,"OPT-PARTICIPANT"},
    {ICAL_ROLE_PARAMETER,ICAL_ROLE_NONPARTICIPANT,"NON-PARTICIPANT"},
    {ICAL_RSVP_PARAMETER,ICAL_RSVP_TRUE,"TRUE"},
    {ICAL_RSVP_PARAMETER,ICAL_RSVP_FALSE,"FALSE"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_BINARY,"BINARY"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_BOOLEAN,"BOOLEAN"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_DATE,"DATE"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_DURATION,"DURATION"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_FLOAT,"FLOAT"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_INTEGER,"INTEGER"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_PERIOD,"PERIOD"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_RECUR,"RECUR"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_TEXT,"TEXT"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_URI,"URI"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_ERROR,"ERROR"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_DATETIME,"DATE-TIME"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_UTCOFFSET,"UTC-OFFSET"},
    {ICAL_VALUE_PARAMETER,ICAL_VALUE_CALADDRESS,"CAL-ADDRESS"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_EQUAL,"EQUAL"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_NOTEQUAL,"NOTEQUAL"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_LESS,"LESS"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_GREATER,"GREATER"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_LESSEQUAL,"LESSEQUAL"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_GREATEREQUAL,"GREATEREQUAL"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_REGEX,"REGEX"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_ISNULL,"ISNULL"},
    {ICAL_XLICCOMPARETYPE_PARAMETER,ICAL_XLICCOMPARETYPE_ISNOTNULL,"ISNOTNULL"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_COMPONENTPARSEERROR,"COMPONENT-PARSE-ERROR"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_PROPERTYPARSEERROR,"PROPERTY-PARSE-ERROR"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR,"PARAMETER-NAME-PARSE-ERROR"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR,"PARAMETER-VALUE-PARSE-ERROR"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_VALUEPARSEERROR,"VALUE-PARSE-ERROR"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_INVALIDITIP,"INVALID-ITIP"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_UNKNOWNVCALPROPERROR,"UNKNOWN-VCAL-PROP-ERROR"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_MIMEPARSEERROR,"MIME-PARSE-ERROR"},
    {ICAL_XLICERRORTYPE_PARAMETER,ICAL_XLICERRORTYPE_VCALPROPPARSEERROR,"VCAL-PROP-PARSE-ERROR"},
    {ICAL_NO_PARAMETER,0,""}};

/* DELEGATED-FROM */
icalparameter* icalparameter_new_delegatedfrom(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_DELEGATEDFROM_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_delegatedfrom((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_delegatedfrom(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_delegatedfrom(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* RELATED */
icalparameter* icalparameter_new_related(icalparameter_related v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_RELATED_X,"v");
    icalerror_check_arg_rz(v < ICAL_RELATED_NONE,"v");
   impl = icalparameter_new_impl(ICAL_RELATED_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_related((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_related icalparameter_get_related(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");

return (icalparameter_related)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_related(icalparameter* param, icalparameter_related v)
{
   icalerror_check_arg_rv(v >= ICAL_RELATED_X,"v");
    icalerror_check_arg_rv(v < ICAL_RELATED_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* SENT-BY */
icalparameter* icalparameter_new_sentby(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_SENTBY_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_sentby((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_sentby(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_sentby(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* LANGUAGE */
icalparameter* icalparameter_new_language(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_LANGUAGE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_language((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_language(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_language(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* RELTYPE */
icalparameter* icalparameter_new_reltype(icalparameter_reltype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_RELTYPE_X,"v");
    icalerror_check_arg_rz(v < ICAL_RELTYPE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_RELTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_reltype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_reltype icalparameter_get_reltype(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_RELTYPE_X;
        }

return (icalparameter_reltype)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_reltype(icalparameter* param, icalparameter_reltype v)
{
   icalerror_check_arg_rv(v >= ICAL_RELTYPE_X,"v");
    icalerror_check_arg_rv(v < ICAL_RELTYPE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* ENCODING */
icalparameter* icalparameter_new_encoding(icalparameter_encoding v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_ENCODING_X,"v");
    icalerror_check_arg_rz(v < ICAL_ENCODING_NONE,"v");
   impl = icalparameter_new_impl(ICAL_ENCODING_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_encoding((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_encoding icalparameter_get_encoding(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_ENCODING_X;
        }

return (icalparameter_encoding)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_encoding(icalparameter* param, icalparameter_encoding v)
{
   icalerror_check_arg_rv(v >= ICAL_ENCODING_X,"v");
    icalerror_check_arg_rv(v < ICAL_ENCODING_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* ALTREP */
icalparameter* icalparameter_new_altrep(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_ALTREP_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_altrep((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_altrep(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_altrep(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* FMTTYPE */
icalparameter* icalparameter_new_fmttype(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_FMTTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_fmttype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_fmttype(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_fmttype(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* FBTYPE */
icalparameter* icalparameter_new_fbtype(icalparameter_fbtype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_FBTYPE_X,"v");
    icalerror_check_arg_rz(v < ICAL_FBTYPE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_FBTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_fbtype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_fbtype icalparameter_get_fbtype(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_FBTYPE_X;
        }

return (icalparameter_fbtype)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_fbtype(icalparameter* param, icalparameter_fbtype v)
{
   icalerror_check_arg_rv(v >= ICAL_FBTYPE_X,"v");
    icalerror_check_arg_rv(v < ICAL_FBTYPE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* RSVP */
icalparameter* icalparameter_new_rsvp(icalparameter_rsvp v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_RSVP_X,"v");
    icalerror_check_arg_rz(v < ICAL_RSVP_NONE,"v");
   impl = icalparameter_new_impl(ICAL_RSVP_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_rsvp((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_rsvp icalparameter_get_rsvp(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");

return (icalparameter_rsvp)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_rsvp(icalparameter* param, icalparameter_rsvp v)
{
   icalerror_check_arg_rv(v >= ICAL_RSVP_X,"v");
    icalerror_check_arg_rv(v < ICAL_RSVP_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* RANGE */
icalparameter* icalparameter_new_range(icalparameter_range v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_RANGE_X,"v");
    icalerror_check_arg_rz(v < ICAL_RANGE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_RANGE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_range((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_range icalparameter_get_range(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");

return (icalparameter_range)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_range(icalparameter* param, icalparameter_range v)
{
   icalerror_check_arg_rv(v >= ICAL_RANGE_X,"v");
    icalerror_check_arg_rv(v < ICAL_RANGE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* DELEGATED-TO */
icalparameter* icalparameter_new_delegatedto(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_DELEGATEDTO_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_delegatedto((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_delegatedto(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_delegatedto(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* CN */
icalparameter* icalparameter_new_cn(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_CN_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_cn((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_cn(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_cn(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* ROLE */
icalparameter* icalparameter_new_role(icalparameter_role v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_ROLE_X,"v");
    icalerror_check_arg_rz(v < ICAL_ROLE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_ROLE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_role((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_role icalparameter_get_role(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_ROLE_X;
        }

return (icalparameter_role)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_role(icalparameter* param, icalparameter_role v)
{
   icalerror_check_arg_rv(v >= ICAL_ROLE_X,"v");
    icalerror_check_arg_rv(v < ICAL_ROLE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* X-LIC-COMPARETYPE */
icalparameter* icalparameter_new_xliccomparetype(icalparameter_xliccomparetype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_XLICCOMPARETYPE_X,"v");
    icalerror_check_arg_rz(v < ICAL_XLICCOMPARETYPE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_XLICCOMPARETYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_xliccomparetype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_xliccomparetype icalparameter_get_xliccomparetype(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");

return (icalparameter_xliccomparetype)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_xliccomparetype(icalparameter* param, icalparameter_xliccomparetype v)
{
   icalerror_check_arg_rv(v >= ICAL_XLICCOMPARETYPE_X,"v");
    icalerror_check_arg_rv(v < ICAL_XLICCOMPARETYPE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* PARTSTAT */
icalparameter* icalparameter_new_partstat(icalparameter_partstat v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_PARTSTAT_X,"v");
    icalerror_check_arg_rz(v < ICAL_PARTSTAT_NONE,"v");
   impl = icalparameter_new_impl(ICAL_PARTSTAT_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_partstat((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_partstat icalparameter_get_partstat(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_PARTSTAT_X;
        }

return (icalparameter_partstat)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_partstat(icalparameter* param, icalparameter_partstat v)
{
   icalerror_check_arg_rv(v >= ICAL_PARTSTAT_X,"v");
    icalerror_check_arg_rv(v < ICAL_PARTSTAT_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* X-LIC-ERRORTYPE */
icalparameter* icalparameter_new_xlicerrortype(icalparameter_xlicerrortype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_XLICERRORTYPE_X,"v");
    icalerror_check_arg_rz(v < ICAL_XLICERRORTYPE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_XLICERRORTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_xlicerrortype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_xlicerrortype icalparameter_get_xlicerrortype(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");

return (icalparameter_xlicerrortype)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_xlicerrortype(icalparameter* param, icalparameter_xlicerrortype v)
{
   icalerror_check_arg_rv(v >= ICAL_XLICERRORTYPE_X,"v");
    icalerror_check_arg_rv(v < ICAL_XLICERRORTYPE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* MEMBER */
icalparameter* icalparameter_new_member(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_MEMBER_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_member((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_member(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_member(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* X */
icalparameter* icalparameter_new_x(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_X_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_x((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_x(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_x(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* CUTYPE */
icalparameter* icalparameter_new_cutype(icalparameter_cutype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_CUTYPE_X,"v");
    icalerror_check_arg_rz(v < ICAL_CUTYPE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_CUTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_cutype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_cutype icalparameter_get_cutype(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_CUTYPE_X;
        }

return (icalparameter_cutype)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_cutype(icalparameter* param, icalparameter_cutype v)
{
   icalerror_check_arg_rv(v >= ICAL_CUTYPE_X,"v");
    icalerror_check_arg_rv(v < ICAL_CUTYPE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* TZID */
icalparameter* icalparameter_new_tzid(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_TZID_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_tzid((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_tzid(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_tzid(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}

/* VALUE */
icalparameter* icalparameter_new_value(icalparameter_value v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz(v >= ICAL_VALUE_X,"v");
    icalerror_check_arg_rz(v < ICAL_VALUE_NONE,"v");
   impl = icalparameter_new_impl(ICAL_VALUE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_value((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_value icalparameter_get_value(const icalparameter* param)
{
   icalerror_clear_errno();
icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_VALUE_X;
        }

return (icalparameter_value)((struct icalparameter_impl*)param)->data;
}

void icalparameter_set_value(icalparameter* param, icalparameter_value v)
{
   icalerror_check_arg_rv(v >= ICAL_VALUE_X,"v");
    icalerror_check_arg_rv(v < ICAL_VALUE_NONE,"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data = (int)v;
}

/* DIR */
icalparameter* icalparameter_new_dir(const char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_DIR_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_dir((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

const char* icalparameter_get_dir(const icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return (const char*)((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_dir(icalparameter* param, const char* v)
{
   icalerror_check_arg_rv( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);
}


const char* icalparameter_kind_to_string(icalparameter_kind kind)
{
    int i;

    for (i=0; parameter_map[i].kind != ICAL_NO_PARAMETER; i++) {
	if (parameter_map[i].kind == kind) {
	    return parameter_map[i].name;
	}
    }

    return 0;

}

icalparameter_kind icalparameter_string_to_kind(const char* string)
{
    int i;

    if (string ==0 ) { 
	return ICAL_NO_PARAMETER;
    }

    for (i=0; parameter_map[i].kind  != ICAL_NO_PARAMETER; i++) {

	if (strcmp(parameter_map[i].name, string) == 0) {
	    return parameter_map[i].kind;
	}
    }

    if(strncmp(string,"X-",2)==0){
	return ICAL_X_PARAMETER;
    }

    return ICAL_NO_PARAMETER;
}


icalvalue_kind icalparameter_value_to_value_kind(icalparameter_value value)
{
    int i;

    for (i=0; value_kind_map[i].kind  != ICAL_NO_VALUE; i++) {

	if (value_kind_map[i].value == value) {
	    return value_kind_map[i].kind;
	}
    }

    return ICAL_NO_VALUE;
}


const char* icalparameter_enum_to_string(int e) 
{
    int i;

    icalerror_check_arg_rz(e >= ICALPARAMETER_FIRST_ENUM,"e");
    icalerror_check_arg_rz(e <= ICALPARAMETER_LAST_ENUM,"e");

    for (i=0; icalparameter_map[i].kind != ICAL_NO_PARAMETER; i++){
        if(e == icalparameter_map[i].enumeration){
            return icalparameter_map[i].str;
        }
    }

    return 0;
}

int icalparameter_string_to_enum(const char* str)
{
    int i;

    icalerror_check_arg_rz(str != 0,"str");

    for (i=0; icalparameter_map[i].kind != ICAL_NO_PARAMETER; i++){
        if(strcmp(str,icalparameter_map[i].str) == 0) {
            return icalparameter_map[i].enumeration;
        }
    }

    return 0;
}

icalparameter* icalparameter_new_from_value_string(icalparameter_kind kind,const  char* val)
{

    struct icalparameter_impl* param=0;
    int found_kind = 0;
    int i;

    icalerror_check_arg_rz((val!=0),"val");

    /* Search through the parameter map to find a matching kind */

    param = icalparameter_new_impl(kind);

    for (i=0; icalparameter_map[i].kind != ICAL_NO_PARAMETER; i++){
        if(kind == icalparameter_map[i].kind) {
            found_kind = 1;
            if(strcmp(val,icalparameter_map[i].str) == 0) {

                param->data = (int)icalparameter_map[i].enumeration;
                return param;
            }
        }
    }
    
    if(found_kind == 1){
        /* The kind was in the parameter map, but the string did not
           match, so assume that it is an alternate value, like an
           X-value.*/
        
        icalparameter_set_xvalue(param, val);

    } else {
 
        /* If the kind was not found, then it must be a string type */
        
        ((struct icalparameter_impl*)param)->string = icalmemory_strdup(val);

    }

   return param;
}




/* Everything below this line is machine generated. Do not edit. */
