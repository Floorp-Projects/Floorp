/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparam.h
  CREATOR: eric 20 March 1999


  $Id: icalderivedparameter.h,v 1.5 2002/09/01 19:12:31 gray-john Exp $
  $Locker:  $

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalparam.h

  ======================================================================*/

#ifndef ICALDERIVEDPARAMETER_H
#define ICALDERIVEDPARAMETER_H


typedef struct icalparameter_impl icalparameter;

const char* icalparameter_enum_to_string(int e);
int icalparameter_string_to_enum(const char* str); 

typedef enum icalparameter_kind {
    ICAL_ANY_PARAMETER = 0,
    ICAL_ALTREP_PARAMETER, 
    ICAL_CN_PARAMETER, 
    ICAL_CUTYPE_PARAMETER, 
    ICAL_DELEGATEDFROM_PARAMETER, 
    ICAL_DELEGATEDTO_PARAMETER, 
    ICAL_DIR_PARAMETER, 
    ICAL_ENCODING_PARAMETER, 
    ICAL_FBTYPE_PARAMETER, 
    ICAL_FMTTYPE_PARAMETER, 
    ICAL_LANGUAGE_PARAMETER, 
    ICAL_MEMBER_PARAMETER, 
    ICAL_PARTSTAT_PARAMETER, 
    ICAL_RANGE_PARAMETER, 
    ICAL_RELATED_PARAMETER, 
    ICAL_RELTYPE_PARAMETER, 
    ICAL_ROLE_PARAMETER, 
    ICAL_RSVP_PARAMETER, 
    ICAL_SENTBY_PARAMETER, 
    ICAL_TZID_PARAMETER, 
    ICAL_VALUE_PARAMETER, 
    ICAL_X_PARAMETER, 
    ICAL_XLICCOMPARETYPE_PARAMETER, 
    ICAL_XLICERRORTYPE_PARAMETER, 
    ICAL_NO_PARAMETER
} icalparameter_kind;

#define ICALPARAMETER_FIRST_ENUM 20000

typedef enum icalparameter_cutype {
    ICAL_CUTYPE_X = 20000,
    ICAL_CUTYPE_INDIVIDUAL = 20001,
    ICAL_CUTYPE_GROUP = 20002,
    ICAL_CUTYPE_RESOURCE = 20003,
    ICAL_CUTYPE_ROOM = 20004,
    ICAL_CUTYPE_UNKNOWN = 20005,
    ICAL_CUTYPE_NONE = 20006
} icalparameter_cutype;

typedef enum icalparameter_encoding {
    ICAL_ENCODING_X = 20007,
    ICAL_ENCODING_8BIT = 20008,
    ICAL_ENCODING_BASE64 = 20009,
    ICAL_ENCODING_NONE = 20010
} icalparameter_encoding;

typedef enum icalparameter_fbtype {
    ICAL_FBTYPE_X = 20011,
    ICAL_FBTYPE_FREE = 20012,
    ICAL_FBTYPE_BUSY = 20013,
    ICAL_FBTYPE_BUSYUNAVAILABLE = 20014,
    ICAL_FBTYPE_BUSYTENTATIVE = 20015,
    ICAL_FBTYPE_NONE = 20016
} icalparameter_fbtype;

typedef enum icalparameter_partstat {
    ICAL_PARTSTAT_X = 20017,
    ICAL_PARTSTAT_NEEDSACTION = 20018,
    ICAL_PARTSTAT_ACCEPTED = 20019,
    ICAL_PARTSTAT_DECLINED = 20020,
    ICAL_PARTSTAT_TENTATIVE = 20021,
    ICAL_PARTSTAT_DELEGATED = 20022,
    ICAL_PARTSTAT_COMPLETED = 20023,
    ICAL_PARTSTAT_INPROCESS = 20024,
    ICAL_PARTSTAT_NONE = 20025
} icalparameter_partstat;

typedef enum icalparameter_range {
    ICAL_RANGE_X = 20026,
    ICAL_RANGE_THISANDPRIOR = 20027,
    ICAL_RANGE_THISANDFUTURE = 20028,
    ICAL_RANGE_NONE = 20029
} icalparameter_range;

typedef enum icalparameter_related {
    ICAL_RELATED_X = 20030,
    ICAL_RELATED_START = 20031,
    ICAL_RELATED_END = 20032,
    ICAL_RELATED_NONE = 20033
} icalparameter_related;

typedef enum icalparameter_reltype {
    ICAL_RELTYPE_X = 20034,
    ICAL_RELTYPE_PARENT = 20035,
    ICAL_RELTYPE_CHILD = 20036,
    ICAL_RELTYPE_SIBLING = 20037,
    ICAL_RELTYPE_NONE = 20038
} icalparameter_reltype;

typedef enum icalparameter_role {
    ICAL_ROLE_X = 20039,
    ICAL_ROLE_CHAIR = 20040,
    ICAL_ROLE_REQPARTICIPANT = 20041,
    ICAL_ROLE_OPTPARTICIPANT = 20042,
    ICAL_ROLE_NONPARTICIPANT = 20043,
    ICAL_ROLE_NONE = 20044
} icalparameter_role;

typedef enum icalparameter_rsvp {
    ICAL_RSVP_X = 20045,
    ICAL_RSVP_TRUE = 20046,
    ICAL_RSVP_FALSE = 20047,
    ICAL_RSVP_NONE = 20048
} icalparameter_rsvp;

typedef enum icalparameter_value {
    ICAL_VALUE_X = 20049,
    ICAL_VALUE_BINARY = 20050,
    ICAL_VALUE_BOOLEAN = 20051,
    ICAL_VALUE_DATE = 20052,
    ICAL_VALUE_DURATION = 20053,
    ICAL_VALUE_FLOAT = 20054,
    ICAL_VALUE_INTEGER = 20055,
    ICAL_VALUE_PERIOD = 20056,
    ICAL_VALUE_RECUR = 20057,
    ICAL_VALUE_TEXT = 20058,
    ICAL_VALUE_URI = 20059,
    ICAL_VALUE_ERROR = 20060,
    ICAL_VALUE_DATETIME = 20061,
    ICAL_VALUE_UTCOFFSET = 20062,
    ICAL_VALUE_CALADDRESS = 20063,
    ICAL_VALUE_NONE = 20064
} icalparameter_value;

typedef enum icalparameter_xliccomparetype {
    ICAL_XLICCOMPARETYPE_X = 20065,
    ICAL_XLICCOMPARETYPE_EQUAL = 20066,
    ICAL_XLICCOMPARETYPE_NOTEQUAL = 20067,
    ICAL_XLICCOMPARETYPE_LESS = 20068,
    ICAL_XLICCOMPARETYPE_GREATER = 20069,
    ICAL_XLICCOMPARETYPE_LESSEQUAL = 20070,
    ICAL_XLICCOMPARETYPE_GREATEREQUAL = 20071,
    ICAL_XLICCOMPARETYPE_REGEX = 20072,
    ICAL_XLICCOMPARETYPE_ISNULL = 20073,
    ICAL_XLICCOMPARETYPE_ISNOTNULL = 20074,
    ICAL_XLICCOMPARETYPE_NONE = 20075
} icalparameter_xliccomparetype;

typedef enum icalparameter_xlicerrortype {
    ICAL_XLICERRORTYPE_X = 20076,
    ICAL_XLICERRORTYPE_COMPONENTPARSEERROR = 20077,
    ICAL_XLICERRORTYPE_PROPERTYPARSEERROR = 20078,
    ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR = 20079,
    ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR = 20080,
    ICAL_XLICERRORTYPE_VALUEPARSEERROR = 20081,
    ICAL_XLICERRORTYPE_INVALIDITIP = 20082,
    ICAL_XLICERRORTYPE_UNKNOWNVCALPROPERROR = 20083,
    ICAL_XLICERRORTYPE_MIMEPARSEERROR = 20084,
    ICAL_XLICERRORTYPE_VCALPROPPARSEERROR = 20085,
    ICAL_XLICERRORTYPE_NONE = 20086
} icalparameter_xlicerrortype;

#define ICALPARAMETER_LAST_ENUM 20087

/* DELEGATED-FROM */
icalparameter* icalparameter_new_delegatedfrom(const char* v);
const char* icalparameter_get_delegatedfrom(const icalparameter* value);
void icalparameter_set_delegatedfrom(icalparameter* value, const char* v);

/* RELATED */
icalparameter* icalparameter_new_related(icalparameter_related v);
icalparameter_related icalparameter_get_related(const icalparameter* value);
void icalparameter_set_related(icalparameter* value, icalparameter_related v);

/* SENT-BY */
icalparameter* icalparameter_new_sentby(const char* v);
const char* icalparameter_get_sentby(const icalparameter* value);
void icalparameter_set_sentby(icalparameter* value, const char* v);

/* LANGUAGE */
icalparameter* icalparameter_new_language(const char* v);
const char* icalparameter_get_language(const icalparameter* value);
void icalparameter_set_language(icalparameter* value, const char* v);

/* RELTYPE */
icalparameter* icalparameter_new_reltype(icalparameter_reltype v);
icalparameter_reltype icalparameter_get_reltype(const icalparameter* value);
void icalparameter_set_reltype(icalparameter* value, icalparameter_reltype v);

/* ENCODING */
icalparameter* icalparameter_new_encoding(icalparameter_encoding v);
icalparameter_encoding icalparameter_get_encoding(const icalparameter* value);
void icalparameter_set_encoding(icalparameter* value, icalparameter_encoding v);

/* ALTREP */
icalparameter* icalparameter_new_altrep(const char* v);
const char* icalparameter_get_altrep(const icalparameter* value);
void icalparameter_set_altrep(icalparameter* value, const char* v);

/* FMTTYPE */
icalparameter* icalparameter_new_fmttype(const char* v);
const char* icalparameter_get_fmttype(const icalparameter* value);
void icalparameter_set_fmttype(icalparameter* value, const char* v);

/* FBTYPE */
icalparameter* icalparameter_new_fbtype(icalparameter_fbtype v);
icalparameter_fbtype icalparameter_get_fbtype(const icalparameter* value);
void icalparameter_set_fbtype(icalparameter* value, icalparameter_fbtype v);

/* RSVP */
icalparameter* icalparameter_new_rsvp(icalparameter_rsvp v);
icalparameter_rsvp icalparameter_get_rsvp(const icalparameter* value);
void icalparameter_set_rsvp(icalparameter* value, icalparameter_rsvp v);

/* RANGE */
icalparameter* icalparameter_new_range(icalparameter_range v);
icalparameter_range icalparameter_get_range(const icalparameter* value);
void icalparameter_set_range(icalparameter* value, icalparameter_range v);

/* DELEGATED-TO */
icalparameter* icalparameter_new_delegatedto(const char* v);
const char* icalparameter_get_delegatedto(const icalparameter* value);
void icalparameter_set_delegatedto(icalparameter* value, const char* v);

/* CN */
icalparameter* icalparameter_new_cn(const char* v);
const char* icalparameter_get_cn(const icalparameter* value);
void icalparameter_set_cn(icalparameter* value, const char* v);

/* ROLE */
icalparameter* icalparameter_new_role(icalparameter_role v);
icalparameter_role icalparameter_get_role(const icalparameter* value);
void icalparameter_set_role(icalparameter* value, icalparameter_role v);

/* X-LIC-COMPARETYPE */
icalparameter* icalparameter_new_xliccomparetype(icalparameter_xliccomparetype v);
icalparameter_xliccomparetype icalparameter_get_xliccomparetype(const icalparameter* value);
void icalparameter_set_xliccomparetype(icalparameter* value, icalparameter_xliccomparetype v);

/* PARTSTAT */
icalparameter* icalparameter_new_partstat(icalparameter_partstat v);
icalparameter_partstat icalparameter_get_partstat(const icalparameter* value);
void icalparameter_set_partstat(icalparameter* value, icalparameter_partstat v);

/* X-LIC-ERRORTYPE */
icalparameter* icalparameter_new_xlicerrortype(icalparameter_xlicerrortype v);
icalparameter_xlicerrortype icalparameter_get_xlicerrortype(const icalparameter* value);
void icalparameter_set_xlicerrortype(icalparameter* value, icalparameter_xlicerrortype v);

/* MEMBER */
icalparameter* icalparameter_new_member(const char* v);
const char* icalparameter_get_member(const icalparameter* value);
void icalparameter_set_member(icalparameter* value, const char* v);

/* X */
icalparameter* icalparameter_new_x(const char* v);
const char* icalparameter_get_x(const icalparameter* value);
void icalparameter_set_x(icalparameter* value, const char* v);

/* CUTYPE */
icalparameter* icalparameter_new_cutype(icalparameter_cutype v);
icalparameter_cutype icalparameter_get_cutype(const icalparameter* value);
void icalparameter_set_cutype(icalparameter* value, icalparameter_cutype v);

/* TZID */
icalparameter* icalparameter_new_tzid(const char* v);
const char* icalparameter_get_tzid(const icalparameter* value);
void icalparameter_set_tzid(icalparameter* value, const char* v);

/* VALUE */
icalparameter* icalparameter_new_value(icalparameter_value v);
icalparameter_value icalparameter_get_value(const icalparameter* value);
void icalparameter_set_value(icalparameter* value, icalparameter_value v);

/* DIR */
icalparameter* icalparameter_new_dir(const char* v);
const char* icalparameter_get_dir(const icalparameter* value);
void icalparameter_set_dir(icalparameter* value, const char* v);

#endif /*ICALPARAMETER_H*/

/* Everything below this line is machine generated. Do not edit. */
