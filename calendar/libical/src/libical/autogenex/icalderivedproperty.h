/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedproperties.{c,h}
  CREATOR: eric 09 May 1999
  
  $Id: icalderivedproperty.h,v 1.5 2002/09/01 19:12:31 gray-john Exp $
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
 ======================================================================*/


#ifndef ICALDERIVEDPROPERTY_H
#define ICALDERIVEDPROPERTY_H

#include <time.h>
#include "icalparameter.h"
#include "icalderivedvalue.h"  
#include "icalrecur.h"

typedef struct icalproperty_impl icalproperty;

typedef enum icalproperty_kind {
    ICAL_ANY_PROPERTY = 0,
    ICAL_ACTION_PROPERTY, 
    ICAL_ALLOWCONFLICT_PROPERTY, 
    ICAL_ATTACH_PROPERTY, 
    ICAL_ATTENDEE_PROPERTY, 
    ICAL_CALID_PROPERTY, 
    ICAL_CALMASTER_PROPERTY, 
    ICAL_CALSCALE_PROPERTY, 
    ICAL_CARID_PROPERTY, 
    ICAL_CATEGORIES_PROPERTY, 
    ICAL_CLASS_PROPERTY, 
    ICAL_COMMENT_PROPERTY, 
    ICAL_COMPLETED_PROPERTY, 
    ICAL_CONTACT_PROPERTY, 
    ICAL_CREATED_PROPERTY, 
    ICAL_DECREED_PROPERTY, 
    ICAL_DEFAULTCHARSET_PROPERTY, 
    ICAL_DEFAULTLOCALE_PROPERTY, 
    ICAL_DEFAULTTZID_PROPERTY, 
    ICAL_DESCRIPTION_PROPERTY, 
    ICAL_DTEND_PROPERTY, 
    ICAL_DTSTAMP_PROPERTY, 
    ICAL_DTSTART_PROPERTY, 
    ICAL_DUE_PROPERTY, 
    ICAL_DURATION_PROPERTY, 
    ICAL_EXDATE_PROPERTY, 
    ICAL_EXPAND_PROPERTY, 
    ICAL_EXRULE_PROPERTY, 
    ICAL_FREEBUSY_PROPERTY, 
    ICAL_GEO_PROPERTY, 
    ICAL_LASTMODIFIED_PROPERTY, 
    ICAL_LOCATION_PROPERTY, 
    ICAL_MAXRESULTS_PROPERTY, 
    ICAL_MAXRESULTSSIZE_PROPERTY, 
    ICAL_METHOD_PROPERTY, 
    ICAL_ORGANIZER_PROPERTY, 
    ICAL_OWNER_PROPERTY, 
    ICAL_PERCENTCOMPLETE_PROPERTY, 
    ICAL_PRIORITY_PROPERTY, 
    ICAL_PRODID_PROPERTY, 
    ICAL_QUERY_PROPERTY, 
    ICAL_QUERYNAME_PROPERTY, 
    ICAL_RDATE_PROPERTY, 
    ICAL_RECURRENCEID_PROPERTY, 
    ICAL_RELATEDTO_PROPERTY, 
    ICAL_RELCALID_PROPERTY, 
    ICAL_REPEAT_PROPERTY, 
    ICAL_REQUESTSTATUS_PROPERTY, 
    ICAL_RESOURCES_PROPERTY, 
    ICAL_RRULE_PROPERTY, 
    ICAL_SCOPE_PROPERTY, 
    ICAL_SEQUENCE_PROPERTY, 
    ICAL_STATUS_PROPERTY, 
    ICAL_SUMMARY_PROPERTY, 
    ICAL_TARGET_PROPERTY, 
    ICAL_TRANSP_PROPERTY, 
    ICAL_TRIGGER_PROPERTY, 
    ICAL_TZID_PROPERTY, 
    ICAL_TZNAME_PROPERTY, 
    ICAL_TZOFFSETFROM_PROPERTY, 
    ICAL_TZOFFSETTO_PROPERTY, 
    ICAL_TZURL_PROPERTY, 
    ICAL_UID_PROPERTY, 
    ICAL_URL_PROPERTY, 
    ICAL_VERSION_PROPERTY, 
    ICAL_X_PROPERTY, 
    ICAL_XLICCLASS_PROPERTY, 
    ICAL_XLICCLUSTERCOUNT_PROPERTY, 
    ICAL_XLICERROR_PROPERTY, 
    ICAL_XLICMIMECHARSET_PROPERTY, 
    ICAL_XLICMIMECID_PROPERTY, 
    ICAL_XLICMIMECONTENTTYPE_PROPERTY, 
    ICAL_XLICMIMEENCODING_PROPERTY, 
    ICAL_XLICMIMEFILENAME_PROPERTY, 
    ICAL_XLICMIMEOPTINFO_PROPERTY, 
    ICAL_NO_PROPERTY
} icalproperty_kind;


/* ACTION */
icalproperty* icalproperty_new_action(enum icalproperty_action v);
void icalproperty_set_action(icalproperty* prop, enum icalproperty_action v);
enum icalproperty_action icalproperty_get_action(const icalproperty* prop);icalproperty* icalproperty_vanew_action(enum icalproperty_action v, ...);

/* ALLOW-CONFLICT */
icalproperty* icalproperty_new_allowconflict(const char* v);
void icalproperty_set_allowconflict(icalproperty* prop, const char* v);
const char* icalproperty_get_allowconflict(const icalproperty* prop);icalproperty* icalproperty_vanew_allowconflict(const char* v, ...);

/* ATTACH */
icalproperty* icalproperty_new_attach(icalattach * v);
void icalproperty_set_attach(icalproperty* prop, icalattach * v);
icalattach * icalproperty_get_attach(const icalproperty* prop);icalproperty* icalproperty_vanew_attach(icalattach * v, ...);

/* ATTENDEE */
icalproperty* icalproperty_new_attendee(const char* v);
void icalproperty_set_attendee(icalproperty* prop, const char* v);
const char* icalproperty_get_attendee(const icalproperty* prop);icalproperty* icalproperty_vanew_attendee(const char* v, ...);

/* CALID */
icalproperty* icalproperty_new_calid(const char* v);
void icalproperty_set_calid(icalproperty* prop, const char* v);
const char* icalproperty_get_calid(const icalproperty* prop);icalproperty* icalproperty_vanew_calid(const char* v, ...);

/* CALMASTER */
icalproperty* icalproperty_new_calmaster(const char* v);
void icalproperty_set_calmaster(icalproperty* prop, const char* v);
const char* icalproperty_get_calmaster(const icalproperty* prop);icalproperty* icalproperty_vanew_calmaster(const char* v, ...);

/* CALSCALE */
icalproperty* icalproperty_new_calscale(const char* v);
void icalproperty_set_calscale(icalproperty* prop, const char* v);
const char* icalproperty_get_calscale(const icalproperty* prop);icalproperty* icalproperty_vanew_calscale(const char* v, ...);

/* CARID */
icalproperty* icalproperty_new_carid(const char* v);
void icalproperty_set_carid(icalproperty* prop, const char* v);
const char* icalproperty_get_carid(const icalproperty* prop);icalproperty* icalproperty_vanew_carid(const char* v, ...);

/* CATEGORIES */
icalproperty* icalproperty_new_categories(const char* v);
void icalproperty_set_categories(icalproperty* prop, const char* v);
const char* icalproperty_get_categories(const icalproperty* prop);icalproperty* icalproperty_vanew_categories(const char* v, ...);

/* CLASS */
icalproperty* icalproperty_new_class(enum icalproperty_class v);
void icalproperty_set_class(icalproperty* prop, enum icalproperty_class v);
enum icalproperty_class icalproperty_get_class(const icalproperty* prop);icalproperty* icalproperty_vanew_class(enum icalproperty_class v, ...);

/* COMMENT */
icalproperty* icalproperty_new_comment(const char* v);
void icalproperty_set_comment(icalproperty* prop, const char* v);
const char* icalproperty_get_comment(const icalproperty* prop);icalproperty* icalproperty_vanew_comment(const char* v, ...);

/* COMPLETED */
icalproperty* icalproperty_new_completed(struct icaltimetype v);
void icalproperty_set_completed(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_completed(const icalproperty* prop);icalproperty* icalproperty_vanew_completed(struct icaltimetype v, ...);

/* CONTACT */
icalproperty* icalproperty_new_contact(const char* v);
void icalproperty_set_contact(icalproperty* prop, const char* v);
const char* icalproperty_get_contact(const icalproperty* prop);icalproperty* icalproperty_vanew_contact(const char* v, ...);

/* CREATED */
icalproperty* icalproperty_new_created(struct icaltimetype v);
void icalproperty_set_created(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_created(const icalproperty* prop);icalproperty* icalproperty_vanew_created(struct icaltimetype v, ...);

/* DECREED */
icalproperty* icalproperty_new_decreed(const char* v);
void icalproperty_set_decreed(icalproperty* prop, const char* v);
const char* icalproperty_get_decreed(const icalproperty* prop);icalproperty* icalproperty_vanew_decreed(const char* v, ...);

/* DEFAULT-CHARSET */
icalproperty* icalproperty_new_defaultcharset(const char* v);
void icalproperty_set_defaultcharset(icalproperty* prop, const char* v);
const char* icalproperty_get_defaultcharset(const icalproperty* prop);icalproperty* icalproperty_vanew_defaultcharset(const char* v, ...);

/* DEFAULT-LOCALE */
icalproperty* icalproperty_new_defaultlocale(const char* v);
void icalproperty_set_defaultlocale(icalproperty* prop, const char* v);
const char* icalproperty_get_defaultlocale(const icalproperty* prop);icalproperty* icalproperty_vanew_defaultlocale(const char* v, ...);

/* DEFAULT-TZID */
icalproperty* icalproperty_new_defaulttzid(const char* v);
void icalproperty_set_defaulttzid(icalproperty* prop, const char* v);
const char* icalproperty_get_defaulttzid(const icalproperty* prop);icalproperty* icalproperty_vanew_defaulttzid(const char* v, ...);

/* DESCRIPTION */
icalproperty* icalproperty_new_description(const char* v);
void icalproperty_set_description(icalproperty* prop, const char* v);
const char* icalproperty_get_description(const icalproperty* prop);icalproperty* icalproperty_vanew_description(const char* v, ...);

/* DTEND */
icalproperty* icalproperty_new_dtend(struct icaltimetype v);
void icalproperty_set_dtend(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtend(const icalproperty* prop);icalproperty* icalproperty_vanew_dtend(struct icaltimetype v, ...);

/* DTSTAMP */
icalproperty* icalproperty_new_dtstamp(struct icaltimetype v);
void icalproperty_set_dtstamp(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstamp(const icalproperty* prop);icalproperty* icalproperty_vanew_dtstamp(struct icaltimetype v, ...);

/* DTSTART */
icalproperty* icalproperty_new_dtstart(struct icaltimetype v);
void icalproperty_set_dtstart(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstart(const icalproperty* prop);icalproperty* icalproperty_vanew_dtstart(struct icaltimetype v, ...);

/* DUE */
icalproperty* icalproperty_new_due(struct icaltimetype v);
void icalproperty_set_due(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_due(const icalproperty* prop);icalproperty* icalproperty_vanew_due(struct icaltimetype v, ...);

/* DURATION */
icalproperty* icalproperty_new_duration(struct icaldurationtype v);
void icalproperty_set_duration(icalproperty* prop, struct icaldurationtype v);
struct icaldurationtype icalproperty_get_duration(const icalproperty* prop);icalproperty* icalproperty_vanew_duration(struct icaldurationtype v, ...);

/* EXDATE */
icalproperty* icalproperty_new_exdate(struct icaltimetype v);
void icalproperty_set_exdate(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_exdate(const icalproperty* prop);icalproperty* icalproperty_vanew_exdate(struct icaltimetype v, ...);

/* EXPAND */
icalproperty* icalproperty_new_expand(int v);
void icalproperty_set_expand(icalproperty* prop, int v);
int icalproperty_get_expand(const icalproperty* prop);icalproperty* icalproperty_vanew_expand(int v, ...);

/* EXRULE */
icalproperty* icalproperty_new_exrule(struct icalrecurrencetype v);
void icalproperty_set_exrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_exrule(const icalproperty* prop);icalproperty* icalproperty_vanew_exrule(struct icalrecurrencetype v, ...);

/* FREEBUSY */
icalproperty* icalproperty_new_freebusy(struct icalperiodtype v);
void icalproperty_set_freebusy(icalproperty* prop, struct icalperiodtype v);
struct icalperiodtype icalproperty_get_freebusy(const icalproperty* prop);icalproperty* icalproperty_vanew_freebusy(struct icalperiodtype v, ...);

/* GEO */
icalproperty* icalproperty_new_geo(struct icalgeotype v);
void icalproperty_set_geo(icalproperty* prop, struct icalgeotype v);
struct icalgeotype icalproperty_get_geo(const icalproperty* prop);icalproperty* icalproperty_vanew_geo(struct icalgeotype v, ...);

/* LAST-MODIFIED */
icalproperty* icalproperty_new_lastmodified(struct icaltimetype v);
void icalproperty_set_lastmodified(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_lastmodified(const icalproperty* prop);icalproperty* icalproperty_vanew_lastmodified(struct icaltimetype v, ...);

/* LOCATION */
icalproperty* icalproperty_new_location(const char* v);
void icalproperty_set_location(icalproperty* prop, const char* v);
const char* icalproperty_get_location(const icalproperty* prop);icalproperty* icalproperty_vanew_location(const char* v, ...);

/* MAXRESULTS */
icalproperty* icalproperty_new_maxresults(int v);
void icalproperty_set_maxresults(icalproperty* prop, int v);
int icalproperty_get_maxresults(const icalproperty* prop);icalproperty* icalproperty_vanew_maxresults(int v, ...);

/* MAXRESULTSSIZE */
icalproperty* icalproperty_new_maxresultssize(int v);
void icalproperty_set_maxresultssize(icalproperty* prop, int v);
int icalproperty_get_maxresultssize(const icalproperty* prop);icalproperty* icalproperty_vanew_maxresultssize(int v, ...);

/* METHOD */
icalproperty* icalproperty_new_method(enum icalproperty_method v);
void icalproperty_set_method(icalproperty* prop, enum icalproperty_method v);
enum icalproperty_method icalproperty_get_method(const icalproperty* prop);icalproperty* icalproperty_vanew_method(enum icalproperty_method v, ...);

/* ORGANIZER */
icalproperty* icalproperty_new_organizer(const char* v);
void icalproperty_set_organizer(icalproperty* prop, const char* v);
const char* icalproperty_get_organizer(const icalproperty* prop);icalproperty* icalproperty_vanew_organizer(const char* v, ...);

/* OWNER */
icalproperty* icalproperty_new_owner(const char* v);
void icalproperty_set_owner(icalproperty* prop, const char* v);
const char* icalproperty_get_owner(const icalproperty* prop);icalproperty* icalproperty_vanew_owner(const char* v, ...);

/* PERCENT-COMPLETE */
icalproperty* icalproperty_new_percentcomplete(int v);
void icalproperty_set_percentcomplete(icalproperty* prop, int v);
int icalproperty_get_percentcomplete(const icalproperty* prop);icalproperty* icalproperty_vanew_percentcomplete(int v, ...);

/* PRIORITY */
icalproperty* icalproperty_new_priority(int v);
void icalproperty_set_priority(icalproperty* prop, int v);
int icalproperty_get_priority(const icalproperty* prop);icalproperty* icalproperty_vanew_priority(int v, ...);

/* PRODID */
icalproperty* icalproperty_new_prodid(const char* v);
void icalproperty_set_prodid(icalproperty* prop, const char* v);
const char* icalproperty_get_prodid(const icalproperty* prop);icalproperty* icalproperty_vanew_prodid(const char* v, ...);

/* QUERY */
icalproperty* icalproperty_new_query(const char* v);
void icalproperty_set_query(icalproperty* prop, const char* v);
const char* icalproperty_get_query(const icalproperty* prop);icalproperty* icalproperty_vanew_query(const char* v, ...);

/* QUERYNAME */
icalproperty* icalproperty_new_queryname(const char* v);
void icalproperty_set_queryname(icalproperty* prop, const char* v);
const char* icalproperty_get_queryname(const icalproperty* prop);icalproperty* icalproperty_vanew_queryname(const char* v, ...);

/* RDATE */
icalproperty* icalproperty_new_rdate(struct icaldatetimeperiodtype v);
void icalproperty_set_rdate(icalproperty* prop, struct icaldatetimeperiodtype v);
struct icaldatetimeperiodtype icalproperty_get_rdate(const icalproperty* prop);icalproperty* icalproperty_vanew_rdate(struct icaldatetimeperiodtype v, ...);

/* RECURRENCE-ID */
icalproperty* icalproperty_new_recurrenceid(struct icaltimetype v);
void icalproperty_set_recurrenceid(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_recurrenceid(const icalproperty* prop);icalproperty* icalproperty_vanew_recurrenceid(struct icaltimetype v, ...);

/* RELATED-TO */
icalproperty* icalproperty_new_relatedto(const char* v);
void icalproperty_set_relatedto(icalproperty* prop, const char* v);
const char* icalproperty_get_relatedto(const icalproperty* prop);icalproperty* icalproperty_vanew_relatedto(const char* v, ...);

/* RELCALID */
icalproperty* icalproperty_new_relcalid(const char* v);
void icalproperty_set_relcalid(icalproperty* prop, const char* v);
const char* icalproperty_get_relcalid(const icalproperty* prop);icalproperty* icalproperty_vanew_relcalid(const char* v, ...);

/* REPEAT */
icalproperty* icalproperty_new_repeat(int v);
void icalproperty_set_repeat(icalproperty* prop, int v);
int icalproperty_get_repeat(const icalproperty* prop);icalproperty* icalproperty_vanew_repeat(int v, ...);

/* REQUEST-STATUS */
icalproperty* icalproperty_new_requeststatus(struct icalreqstattype v);
void icalproperty_set_requeststatus(icalproperty* prop, struct icalreqstattype v);
struct icalreqstattype icalproperty_get_requeststatus(const icalproperty* prop);icalproperty* icalproperty_vanew_requeststatus(struct icalreqstattype v, ...);

/* RESOURCES */
icalproperty* icalproperty_new_resources(const char* v);
void icalproperty_set_resources(icalproperty* prop, const char* v);
const char* icalproperty_get_resources(const icalproperty* prop);icalproperty* icalproperty_vanew_resources(const char* v, ...);

/* RRULE */
icalproperty* icalproperty_new_rrule(struct icalrecurrencetype v);
void icalproperty_set_rrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_rrule(const icalproperty* prop);icalproperty* icalproperty_vanew_rrule(struct icalrecurrencetype v, ...);

/* SCOPE */
icalproperty* icalproperty_new_scope(const char* v);
void icalproperty_set_scope(icalproperty* prop, const char* v);
const char* icalproperty_get_scope(const icalproperty* prop);icalproperty* icalproperty_vanew_scope(const char* v, ...);

/* SEQUENCE */
icalproperty* icalproperty_new_sequence(int v);
void icalproperty_set_sequence(icalproperty* prop, int v);
int icalproperty_get_sequence(const icalproperty* prop);icalproperty* icalproperty_vanew_sequence(int v, ...);

/* STATUS */
icalproperty* icalproperty_new_status(enum icalproperty_status v);
void icalproperty_set_status(icalproperty* prop, enum icalproperty_status v);
enum icalproperty_status icalproperty_get_status(const icalproperty* prop);icalproperty* icalproperty_vanew_status(enum icalproperty_status v, ...);

/* SUMMARY */
icalproperty* icalproperty_new_summary(const char* v);
void icalproperty_set_summary(icalproperty* prop, const char* v);
const char* icalproperty_get_summary(const icalproperty* prop);icalproperty* icalproperty_vanew_summary(const char* v, ...);

/* TARGET */
icalproperty* icalproperty_new_target(const char* v);
void icalproperty_set_target(icalproperty* prop, const char* v);
const char* icalproperty_get_target(const icalproperty* prop);icalproperty* icalproperty_vanew_target(const char* v, ...);

/* TRANSP */
icalproperty* icalproperty_new_transp(enum icalproperty_transp v);
void icalproperty_set_transp(icalproperty* prop, enum icalproperty_transp v);
enum icalproperty_transp icalproperty_get_transp(const icalproperty* prop);icalproperty* icalproperty_vanew_transp(enum icalproperty_transp v, ...);

/* TRIGGER */
icalproperty* icalproperty_new_trigger(struct icaltriggertype v);
void icalproperty_set_trigger(icalproperty* prop, struct icaltriggertype v);
struct icaltriggertype icalproperty_get_trigger(const icalproperty* prop);icalproperty* icalproperty_vanew_trigger(struct icaltriggertype v, ...);

/* TZID */
icalproperty* icalproperty_new_tzid(const char* v);
void icalproperty_set_tzid(icalproperty* prop, const char* v);
const char* icalproperty_get_tzid(const icalproperty* prop);icalproperty* icalproperty_vanew_tzid(const char* v, ...);

/* TZNAME */
icalproperty* icalproperty_new_tzname(const char* v);
void icalproperty_set_tzname(icalproperty* prop, const char* v);
const char* icalproperty_get_tzname(const icalproperty* prop);icalproperty* icalproperty_vanew_tzname(const char* v, ...);

/* TZOFFSETFROM */
icalproperty* icalproperty_new_tzoffsetfrom(int v);
void icalproperty_set_tzoffsetfrom(icalproperty* prop, int v);
int icalproperty_get_tzoffsetfrom(const icalproperty* prop);icalproperty* icalproperty_vanew_tzoffsetfrom(int v, ...);

/* TZOFFSETTO */
icalproperty* icalproperty_new_tzoffsetto(int v);
void icalproperty_set_tzoffsetto(icalproperty* prop, int v);
int icalproperty_get_tzoffsetto(const icalproperty* prop);icalproperty* icalproperty_vanew_tzoffsetto(int v, ...);

/* TZURL */
icalproperty* icalproperty_new_tzurl(const char* v);
void icalproperty_set_tzurl(icalproperty* prop, const char* v);
const char* icalproperty_get_tzurl(const icalproperty* prop);icalproperty* icalproperty_vanew_tzurl(const char* v, ...);

/* UID */
icalproperty* icalproperty_new_uid(const char* v);
void icalproperty_set_uid(icalproperty* prop, const char* v);
const char* icalproperty_get_uid(const icalproperty* prop);icalproperty* icalproperty_vanew_uid(const char* v, ...);

/* URL */
icalproperty* icalproperty_new_url(const char* v);
void icalproperty_set_url(icalproperty* prop, const char* v);
const char* icalproperty_get_url(const icalproperty* prop);icalproperty* icalproperty_vanew_url(const char* v, ...);

/* VERSION */
icalproperty* icalproperty_new_version(const char* v);
void icalproperty_set_version(icalproperty* prop, const char* v);
const char* icalproperty_get_version(const icalproperty* prop);icalproperty* icalproperty_vanew_version(const char* v, ...);

/* X */
icalproperty* icalproperty_new_x(const char* v);
void icalproperty_set_x(icalproperty* prop, const char* v);
const char* icalproperty_get_x(const icalproperty* prop);icalproperty* icalproperty_vanew_x(const char* v, ...);

/* X-LIC-CLASS */
icalproperty* icalproperty_new_xlicclass(enum icalproperty_xlicclass v);
void icalproperty_set_xlicclass(icalproperty* prop, enum icalproperty_xlicclass v);
enum icalproperty_xlicclass icalproperty_get_xlicclass(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicclass(enum icalproperty_xlicclass v, ...);

/* X-LIC-CLUSTERCOUNT */
icalproperty* icalproperty_new_xlicclustercount(const char* v);
void icalproperty_set_xlicclustercount(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicclustercount(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicclustercount(const char* v, ...);

/* X-LIC-ERROR */
icalproperty* icalproperty_new_xlicerror(const char* v);
void icalproperty_set_xlicerror(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicerror(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicerror(const char* v, ...);

/* X-LIC-MIMECHARSET */
icalproperty* icalproperty_new_xlicmimecharset(const char* v);
void icalproperty_set_xlicmimecharset(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecharset(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimecharset(const char* v, ...);

/* X-LIC-MIMECID */
icalproperty* icalproperty_new_xlicmimecid(const char* v);
void icalproperty_set_xlicmimecid(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecid(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimecid(const char* v, ...);

/* X-LIC-MIMECONTENTTYPE */
icalproperty* icalproperty_new_xlicmimecontenttype(const char* v);
void icalproperty_set_xlicmimecontenttype(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecontenttype(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimecontenttype(const char* v, ...);

/* X-LIC-MIMEENCODING */
icalproperty* icalproperty_new_xlicmimeencoding(const char* v);
void icalproperty_set_xlicmimeencoding(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimeencoding(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimeencoding(const char* v, ...);

/* X-LIC-MIMEFILENAME */
icalproperty* icalproperty_new_xlicmimefilename(const char* v);
void icalproperty_set_xlicmimefilename(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimefilename(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimefilename(const char* v, ...);

/* X-LIC-MIMEOPTINFO */
icalproperty* icalproperty_new_xlicmimeoptinfo(const char* v);
void icalproperty_set_xlicmimeoptinfo(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimeoptinfo(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimeoptinfo(const char* v, ...);


#endif /*ICALPROPERTY_H*/
