/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalvalue.h
  CREATOR: eric 20 March 1999


  $Id: icalderivedvalue.h,v 1.5 2002/09/01 19:12:31 gray-john Exp $
  $Locker:  $

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvalue.h

  ======================================================================*/

#ifndef ICALDERIVEDVALUE_H
#define ICALDERIVEDVALUE_H

#include "icaltypes.h"
#include "icalrecur.h"
#include "icaltime.h"
#include "icalduration.h"
#include "icalperiod.h"
#include "icalattach.h"
     
typedef struct icalvalue_impl icalvalue;



void icalvalue_set_x(icalvalue* value, const char* v);
icalvalue* icalvalue_new_x(const char* v);
const char* icalvalue_get_x(const icalvalue* value);

icalvalue* icalvalue_new_recur (struct icalrecurrencetype v);
void icalvalue_set_recur(icalvalue* value, struct icalrecurrencetype v);
struct icalrecurrencetype icalvalue_get_recur(const icalvalue* value);

icalvalue* icalvalue_new_trigger (struct icaltriggertype v);
void icalvalue_set_trigger(icalvalue* value, struct icaltriggertype v);
struct icaltriggertype icalvalue_get_trigger(const icalvalue* value);

icalvalue* icalvalue_new_datetimeperiod (struct icaldatetimeperiodtype v);
void icalvalue_set_datetimeperiod(icalvalue* value, struct icaldatetimeperiodtype v);
struct icaldatetimeperiodtype icalvalue_get_datetimeperiod(const icalvalue* value);

icalvalue *icalvalue_new_attach (icalattach *attach);
void icalvalue_set_attach (icalvalue *value, icalattach *attach);
icalattach *icalvalue_get_attach (const icalvalue *value);

void icalvalue_reset_kind(icalvalue* value);

typedef enum icalvalue_kind {
   ICAL_ANY_VALUE=5000,
    ICAL_BOOLEAN_VALUE=5001,
    ICAL_UTCOFFSET_VALUE=5002,
    ICAL_XLICCLASS_VALUE=5003,
    ICAL_RECUR_VALUE=5004,
    ICAL_METHOD_VALUE=5005,
    ICAL_CALADDRESS_VALUE=5006,
    ICAL_PERIOD_VALUE=5007,
    ICAL_STATUS_VALUE=5008,
    ICAL_BINARY_VALUE=5009,
    ICAL_TEXT_VALUE=5010,
    ICAL_DURATION_VALUE=5011,
    ICAL_DATETIMEPERIOD_VALUE=5012,
    ICAL_INTEGER_VALUE=5013,
    ICAL_URI_VALUE=5014,
    ICAL_TRIGGER_VALUE=5015,
    ICAL_ATTACH_VALUE=5016,
    ICAL_CLASS_VALUE=5017,
    ICAL_FLOAT_VALUE=5018,
    ICAL_QUERY_VALUE=5019,
    ICAL_STRING_VALUE=5020,
    ICAL_TRANSP_VALUE=5021,
    ICAL_X_VALUE=5022,
    ICAL_DATETIME_VALUE=5023,
    ICAL_REQUESTSTATUS_VALUE=5024,
    ICAL_GEO_VALUE=5025,
    ICAL_DATE_VALUE=5026,
    ICAL_ACTION_VALUE=5027,
   ICAL_NO_VALUE=5028
} icalvalue_kind ;

#define ICALPROPERTY_FIRST_ENUM 10000

typedef enum icalproperty_action {
    ICAL_ACTION_X = 10000,
    ICAL_ACTION_AUDIO = 10001,
    ICAL_ACTION_DISPLAY = 10002,
    ICAL_ACTION_EMAIL = 10003,
    ICAL_ACTION_PROCEDURE = 10004,
    ICAL_ACTION_NONE = 10005
} icalproperty_action;

typedef enum icalproperty_class {
    ICAL_CLASS_X = 10006,
    ICAL_CLASS_PUBLIC = 10007,
    ICAL_CLASS_PRIVATE = 10008,
    ICAL_CLASS_CONFIDENTIAL = 10009,
    ICAL_CLASS_NONE = 10010
} icalproperty_class;

typedef enum icalproperty_method {
    ICAL_METHOD_X = 10011,
    ICAL_METHOD_PUBLISH = 10012,
    ICAL_METHOD_REQUEST = 10013,
    ICAL_METHOD_REPLY = 10014,
    ICAL_METHOD_ADD = 10015,
    ICAL_METHOD_CANCEL = 10016,
    ICAL_METHOD_REFRESH = 10017,
    ICAL_METHOD_COUNTER = 10018,
    ICAL_METHOD_DECLINECOUNTER = 10019,
    ICAL_METHOD_CREATE = 10020,
    ICAL_METHOD_READ = 10021,
    ICAL_METHOD_RESPONSE = 10022,
    ICAL_METHOD_MOVE = 10023,
    ICAL_METHOD_MODIFY = 10024,
    ICAL_METHOD_GENERATEUID = 10025,
    ICAL_METHOD_DELETE = 10026,
    ICAL_METHOD_NONE = 10027
} icalproperty_method;

typedef enum icalproperty_status {
    ICAL_STATUS_X = 10028,
    ICAL_STATUS_TENTATIVE = 10029,
    ICAL_STATUS_CONFIRMED = 10030,
    ICAL_STATUS_COMPLETED = 10031,
    ICAL_STATUS_NEEDSACTION = 10032,
    ICAL_STATUS_CANCELLED = 10033,
    ICAL_STATUS_INPROCESS = 10034,
    ICAL_STATUS_DRAFT = 10035,
    ICAL_STATUS_FINAL = 10036,
    ICAL_STATUS_NONE = 10037
} icalproperty_status;

typedef enum icalproperty_transp {
    ICAL_TRANSP_X = 10038,
    ICAL_TRANSP_OPAQUE = 10039,
    ICAL_TRANSP_OPAQUENOCONFLICT = 10040,
    ICAL_TRANSP_TRANSPARENT = 10041,
    ICAL_TRANSP_TRANSPARENTNOCONFLICT = 10042,
    ICAL_TRANSP_NONE = 10043
} icalproperty_transp;

typedef enum icalproperty_xlicclass {
    ICAL_XLICCLASS_X = 10044,
    ICAL_XLICCLASS_PUBLISHNEW = 10045,
    ICAL_XLICCLASS_PUBLISHUPDATE = 10046,
    ICAL_XLICCLASS_PUBLISHFREEBUSY = 10047,
    ICAL_XLICCLASS_REQUESTNEW = 10048,
    ICAL_XLICCLASS_REQUESTUPDATE = 10049,
    ICAL_XLICCLASS_REQUESTRESCHEDULE = 10050,
    ICAL_XLICCLASS_REQUESTDELEGATE = 10051,
    ICAL_XLICCLASS_REQUESTNEWORGANIZER = 10052,
    ICAL_XLICCLASS_REQUESTFORWARD = 10053,
    ICAL_XLICCLASS_REQUESTSTATUS = 10054,
    ICAL_XLICCLASS_REQUESTFREEBUSY = 10055,
    ICAL_XLICCLASS_REPLYACCEPT = 10056,
    ICAL_XLICCLASS_REPLYDECLINE = 10057,
    ICAL_XLICCLASS_REPLYDELEGATE = 10058,
    ICAL_XLICCLASS_REPLYCRASHERACCEPT = 10059,
    ICAL_XLICCLASS_REPLYCRASHERDECLINE = 10060,
    ICAL_XLICCLASS_ADDINSTANCE = 10061,
    ICAL_XLICCLASS_CANCELEVENT = 10062,
    ICAL_XLICCLASS_CANCELINSTANCE = 10063,
    ICAL_XLICCLASS_CANCELALL = 10064,
    ICAL_XLICCLASS_REFRESH = 10065,
    ICAL_XLICCLASS_COUNTER = 10066,
    ICAL_XLICCLASS_DECLINECOUNTER = 10067,
    ICAL_XLICCLASS_MALFORMED = 10068,
    ICAL_XLICCLASS_OBSOLETE = 10069,
    ICAL_XLICCLASS_MISSEQUENCED = 10070,
    ICAL_XLICCLASS_UNKNOWN = 10071,
    ICAL_XLICCLASS_NONE = 10072
} icalproperty_xlicclass;

#define ICALPROPERTY_LAST_ENUM 10073


 /* BOOLEAN */ 
icalvalue* icalvalue_new_boolean(int v); 
int icalvalue_get_boolean(const icalvalue* value); 
void icalvalue_set_boolean(icalvalue* value, int v);


 /* UTC-OFFSET */ 
icalvalue* icalvalue_new_utcoffset(int v); 
int icalvalue_get_utcoffset(const icalvalue* value); 
void icalvalue_set_utcoffset(icalvalue* value, int v);


 /* X-LIC-CLASS */ 
icalvalue* icalvalue_new_xlicclass(enum icalproperty_xlicclass v); 
enum icalproperty_xlicclass icalvalue_get_xlicclass(const icalvalue* value); 
void icalvalue_set_xlicclass(icalvalue* value, enum icalproperty_xlicclass v);


 /* METHOD */ 
icalvalue* icalvalue_new_method(enum icalproperty_method v); 
enum icalproperty_method icalvalue_get_method(const icalvalue* value); 
void icalvalue_set_method(icalvalue* value, enum icalproperty_method v);


 /* CAL-ADDRESS */ 
icalvalue* icalvalue_new_caladdress(const char* v); 
const char* icalvalue_get_caladdress(const icalvalue* value); 
void icalvalue_set_caladdress(icalvalue* value, const char* v);


 /* PERIOD */ 
icalvalue* icalvalue_new_period(struct icalperiodtype v); 
struct icalperiodtype icalvalue_get_period(const icalvalue* value); 
void icalvalue_set_period(icalvalue* value, struct icalperiodtype v);


 /* STATUS */ 
icalvalue* icalvalue_new_status(enum icalproperty_status v); 
enum icalproperty_status icalvalue_get_status(const icalvalue* value); 
void icalvalue_set_status(icalvalue* value, enum icalproperty_status v);


 /* BINARY */ 
icalvalue* icalvalue_new_binary(const char* v); 
const char* icalvalue_get_binary(const icalvalue* value); 
void icalvalue_set_binary(icalvalue* value, const char* v);


 /* TEXT */ 
icalvalue* icalvalue_new_text(const char* v); 
const char* icalvalue_get_text(const icalvalue* value); 
void icalvalue_set_text(icalvalue* value, const char* v);


 /* DURATION */ 
icalvalue* icalvalue_new_duration(struct icaldurationtype v); 
struct icaldurationtype icalvalue_get_duration(const icalvalue* value); 
void icalvalue_set_duration(icalvalue* value, struct icaldurationtype v);


 /* INTEGER */ 
icalvalue* icalvalue_new_integer(int v); 
int icalvalue_get_integer(const icalvalue* value); 
void icalvalue_set_integer(icalvalue* value, int v);


 /* URI */ 
icalvalue* icalvalue_new_uri(const char* v); 
const char* icalvalue_get_uri(const icalvalue* value); 
void icalvalue_set_uri(icalvalue* value, const char* v);


 /* CLASS */ 
icalvalue* icalvalue_new_class(enum icalproperty_class v); 
enum icalproperty_class icalvalue_get_class(const icalvalue* value); 
void icalvalue_set_class(icalvalue* value, enum icalproperty_class v);


 /* FLOAT */ 
icalvalue* icalvalue_new_float(float v); 
float icalvalue_get_float(const icalvalue* value); 
void icalvalue_set_float(icalvalue* value, float v);


 /* QUERY */ 
icalvalue* icalvalue_new_query(const char* v); 
const char* icalvalue_get_query(const icalvalue* value); 
void icalvalue_set_query(icalvalue* value, const char* v);


 /* STRING */ 
icalvalue* icalvalue_new_string(const char* v); 
const char* icalvalue_get_string(const icalvalue* value); 
void icalvalue_set_string(icalvalue* value, const char* v);


 /* TRANSP */ 
icalvalue* icalvalue_new_transp(enum icalproperty_transp v); 
enum icalproperty_transp icalvalue_get_transp(const icalvalue* value); 
void icalvalue_set_transp(icalvalue* value, enum icalproperty_transp v);


 /* DATE-TIME */ 
icalvalue* icalvalue_new_datetime(struct icaltimetype v); 
struct icaltimetype icalvalue_get_datetime(const icalvalue* value); 
void icalvalue_set_datetime(icalvalue* value, struct icaltimetype v);


 /* REQUEST-STATUS */ 
icalvalue* icalvalue_new_requeststatus(struct icalreqstattype v); 
struct icalreqstattype icalvalue_get_requeststatus(const icalvalue* value); 
void icalvalue_set_requeststatus(icalvalue* value, struct icalreqstattype v);


 /* GEO */ 
icalvalue* icalvalue_new_geo(struct icalgeotype v); 
struct icalgeotype icalvalue_get_geo(const icalvalue* value); 
void icalvalue_set_geo(icalvalue* value, struct icalgeotype v);


 /* DATE */ 
icalvalue* icalvalue_new_date(struct icaltimetype v); 
struct icaltimetype icalvalue_get_date(const icalvalue* value); 
void icalvalue_set_date(icalvalue* value, struct icaltimetype v);


 /* ACTION */ 
icalvalue* icalvalue_new_action(enum icalproperty_action v); 
enum icalproperty_action icalvalue_get_action(const icalvalue* value); 
void icalvalue_set_action(icalvalue* value, enum icalproperty_action v);

#endif /*ICALVALUE_H*/
