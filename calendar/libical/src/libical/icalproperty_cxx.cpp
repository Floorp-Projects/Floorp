/* -*- Mode: C++ -*- */

/**
 * @file    icalproperty_cxx.cpp
 * @author  fnguyen (12/10/01)
 * @brief   Implementation of C++ Wrapper for icalproperty.c
 *
 * (C) COPYRIGHT 2001, Critical Path
 */


#ifndef ICALPROPERTY_CXX_H
#include "icalproperty_cxx.h"
#endif

#ifndef ICALPARAMETER_CXX_H
#include "icalparameter_cxx.h"
#endif

#ifndef ICALVALUE_CXX_H
#include "icalvalue_cxx.h"
#endif

ICalProperty::ICalProperty() : imp(icalproperty_new(ICAL_ANY_PROPERTY)){
}

ICalProperty::ICalProperty(const ICalProperty& v) throw(icalerrorenum){
	imp = icalproperty_new_clone(v.imp);
	if (!imp) throw icalerrno;
}

ICalProperty& ICalProperty::operator=(const ICalProperty& v) throw(icalerrorenum) {
	if (this == &v) return *this;

	if (imp != NULL)
	{
		icalproperty_free(imp);
		imp = icalproperty_new_clone(v.imp);
		if (!imp) throw icalerrno;
	}

	return *this;
}
ICalProperty::~ICalProperty(){
	if (imp != NULL) icalproperty_free(imp);
}

ICalProperty::ICalProperty(icalproperty* v) : imp(v) {
}

ICalProperty::ICalProperty(string str){
	imp = icalproperty_new_from_string(str);
}

ICalProperty::ICalProperty(icalproperty_kind kind){
	imp = icalproperty_new(kind);
}

string ICalProperty::as_ical_string(){
	return (string)icalproperty_as_ical_string(imp);
}
icalproperty_kind ICalProperty::isa(){
	return icalproperty_isa(imp);
}
int ICalProperty::isa_property(void* property){
	return icalproperty_isa_property(property);
}

int ICalProperty::operator==(ICalProperty& rhs){
	icalparameter_xliccomparetype  result;
	ICalValue*  thisPropValue = this->get_value();
	ICalValue*  rhsPropValue  = rhs.get_value();
	result = icalvalue_compare((icalvalue*)*thisPropValue, (icalvalue*)*rhsPropValue);
	return (result ==  ICAL_XLICCOMPARETYPE_EQUAL);
}

void ICalProperty::add_parameter(ICalParameter& parameter){
	icalproperty_add_parameter(imp, parameter);
}
void ICalProperty::set_parameter(ICalParameter &parameter){
	icalproperty_set_parameter(imp, parameter);
}
void ICalProperty::set_parameter_from_string(string name, string val){
	icalproperty_set_parameter_from_string(imp, name, val);
}
string ICalProperty::get_parameter_as_string(string name){
	return (string)icalproperty_get_parameter_as_string(imp, name);
}
void ICalProperty::remove_parameter(icalparameter_kind kind){
	icalproperty_remove_parameter(imp, kind);
}
int ICalProperty::count_parameters(){
	return icalproperty_count_parameters(imp);
}

/** Iterate through the parameters */
ICalParameter* ICalProperty::get_first_parameter(icalparameter_kind kind){
	icalparameter* p = icalproperty_get_first_parameter(imp, kind);
	return (p != NULL)? new ICalParameter(p): NULL;
}
ICalParameter* ICalProperty::get_next_parameter(icalparameter_kind kind){
	icalparameter* p = icalproperty_get_next_parameter(imp, kind);
	return (p != NULL)? new ICalParameter(p): NULL;
}

/** Access the value of the property */
void ICalProperty::set_value(const ICalValue& val){
	icalproperty_set_value(imp, (ICalValue&)val);
}
void ICalProperty::set_value_from_string(string val, string kind){
	icalproperty_set_value_from_string(imp, val, kind);
}

ICalValue* ICalProperty::get_value(){
	return new ICalValue(icalproperty_get_value(imp));
}
string ICalProperty::get_value_as_string(){
	return (string)icalproperty_get_value_as_string(imp);
}

/** Return the name of the property -- the type name converted to a
 *  string, or the value of get_x_name if the type is X property
 */
string ICalProperty::get_name(){
	return (string)icalproperty_get_property_name(imp);
}

/* Deal with X properties */
void ICalProperty::set_x_name(ICalProperty &prop, string name){
	icalproperty_set_x_name(prop, name);
}
string ICalProperty::get_x_name(ICalProperty &prop){
	return (string)icalproperty_get_x_name(prop);
}

icalvalue_kind ICalProperty::icalparameter_value_to_value_kind(icalparameter_value val){
	return icalparameter_value_to_value_kind(val);
}

/* Convert kinds to string and get default value type */
icalvalue_kind ICalProperty::kind_to_value_kind(icalproperty_kind kind){
	return icalproperty_kind_to_value_kind(kind);
}
icalproperty_kind ICalProperty::value_kind_to_kind(icalvalue_kind kind){
	return icalproperty_value_kind_to_kind(kind);
}
string ICalProperty::kind_to_string(icalproperty_kind kind){
	return (string)icalproperty_kind_to_string(kind);
}
icalproperty_kind ICalProperty::string_to_kind(string str){
	return icalproperty_string_to_kind(str);
}

string ICalProperty::method_to_string(icalproperty_method method){
	return (string)icalproperty_method_to_string(method);
}
icalproperty_method ICalProperty::string_to_method(string str){
	return icalproperty_string_to_method(str);
}

string ICalProperty::enum_to_string(int e){
	return (string)icalproperty_enum_to_string(e);
}
int ICalProperty::string_to_enum(string str){
	return icalproperty_string_to_enum(str);
}

string ICalProperty::status_to_string(icalproperty_status s){
	return (string)icalproperty_status_to_string(s);
}
icalproperty_status ICalProperty::string_to_status(string str){
	return icalproperty_string_to_status(str);
}

int ICalProperty::enum_belongs_to_property(icalproperty_kind kind, int e){
	return icalproperty_enum_belongs_to_property(kind, e);
}

/* ACTION */
void ICalProperty::set_action(enum icalproperty_action val){
	icalproperty_set_action(imp, val);
}
enum icalproperty_action ICalProperty::get_action(){
	return icalproperty_get_action(imp);
}

/* ATTACH */
void ICalProperty::set_attach(icalattach *val){
	icalproperty_set_attach(imp, val);
}
icalattach * ICalProperty::get_attach(){
	return icalproperty_get_attach(imp);
}

/* ATTENDEE */
void ICalProperty::set_attendee(string val){
	icalproperty_set_attendee(imp, val);
}
string ICalProperty::get_attendee(){
	return (string)icalproperty_get_attendee(imp);
}

/* CALSCALE */
void ICalProperty::set_calscale(string val){
	icalproperty_set_calscale(imp, val);
}
string ICalProperty::get_calscale(){
	return (string)icalproperty_get_calscale(imp);
}

/* CATEGORIES */
void ICalProperty::set_categories(string val){
	icalproperty_set_categories(imp, val);
}
string ICalProperty::get_categories(){
	return (string)icalproperty_get_categories(imp);
}

/* CLASS */
void ICalProperty::set_class(enum icalproperty_class val){
	icalproperty_set_class(imp, val);
}
enum icalproperty_class ICalProperty::get_class(){
	return (enum icalproperty_class)icalproperty_get_class(imp);
}

/* COMMENT */
void ICalProperty::set_comment(string val){
	icalproperty_set_comment(imp, val);
}
string ICalProperty::get_comment(){
	return (string)icalproperty_get_comment(imp);
}

/* COMPLETED */
void ICalProperty::set_completed(struct icaltimetype val){
	icalproperty_set_completed(imp, val);
}
struct icaltimetype ICalProperty::get_completed(){
	return icalproperty_get_completed(imp);
}

/* CONTACT */
void ICalProperty::set_contact(string val){
	icalproperty_set_contact(imp, val);
}
string ICalProperty::get_contact(){
	return (string)icalproperty_get_contact(imp);
}

/* CREATED */
void ICalProperty::set_created(struct icaltimetype val){
	icalproperty_set_created(imp, val);
}
struct icaltimetype ICalProperty::get_created(){
	return icalproperty_get_created(imp);
}

/* DESCRIPTION */
void ICalProperty::set_description(string val){
	icalproperty_set_description(imp, val);
}
string ICalProperty::get_description(){
	return (string)icalproperty_get_description(imp);
}

/* DTEND */
void ICalProperty::set_dtend(struct icaltimetype val){
	icalproperty_set_dtend(imp, val);
}
struct icaltimetype ICalProperty::get_dtend(){
	return icalproperty_get_dtend(imp);
}

/* DTSTAMP */
void ICalProperty::set_dtstamp(struct icaltimetype val){
	icalproperty_set_dtstamp(imp, val);
}
struct icaltimetype ICalProperty::get_dtstamp(){
	return icalproperty_get_dtstamp(imp);
}

/* DTSTART */
void ICalProperty::set_dtstart(struct icaltimetype val){
	icalproperty_set_dtstart(imp, val);
}
struct icaltimetype ICalProperty::get_dtstart(){
	return icalproperty_get_dtstart(imp);
}

/* DUE */
void ICalProperty::set_due(struct icaltimetype val){
	icalproperty_set_due(imp, val);
}
struct icaltimetype ICalProperty::get_due(){
	return icalproperty_get_due(imp);
}

/* DURATION */
void ICalProperty::set_duration(struct icaldurationtype val){
	icalproperty_set_duration(imp, val);
}
struct icaldurationtype ICalProperty::get_duration(){
	return icalproperty_get_duration(imp);
}

/* EXDATE */
void ICalProperty::set_exdate(struct icaltimetype val){
	icalproperty_set_exdate(imp, val);
}
struct icaltimetype ICalProperty::get_exdate(){
	return icalproperty_get_exdate(imp);
}

/* EXPAND */
void ICalProperty::set_expand(int val){
        icalproperty_set_expand(imp, val);
}
int ICalProperty::get_expand(){
        return icalproperty_get_expand(imp);
}

/* EXRULE */
void ICalProperty::set_exrule(struct icalrecurrencetype val){
	icalproperty_set_exrule(imp, val);
}
struct icalrecurrencetype ICalProperty::get_exrule(){
	return icalproperty_get_exrule(imp);
}

/* FREEBUSY */
void ICalProperty::set_freebusy(struct icalperiodtype val){
	icalproperty_set_freebusy(imp, val);
}
struct icalperiodtype ICalProperty::get_freebusy(){
	return icalproperty_get_freebusy(imp);
}

/* GEO */
void ICalProperty::set_geo(struct icalgeotype val){
	icalproperty_set_geo(imp, val);
}
struct icalgeotype ICalProperty::get_geo(){
	return icalproperty_get_geo(imp);
}

/* LAST-MODIFIED */
void ICalProperty::set_lastmodified(struct icaltimetype val){
	icalproperty_set_lastmodified(imp, val);
}
struct icaltimetype ICalProperty::get_lastmodified(){
	return icalproperty_get_lastmodified(imp);
}

/* LOCATION */
void ICalProperty::set_location(string val){
	icalproperty_set_location(imp, val);
}
string ICalProperty::get_location(){
	return (string)icalproperty_get_location(imp);
}

/* MAXRESULTS */
void ICalProperty::set_maxresults(int val){
	icalproperty_set_maxresults(imp, val);
}
int ICalProperty::get_maxresults(){
	return icalproperty_get_maxresults(imp);
}

/* MAXRESULTSSIZE */
void ICalProperty::set_maxresultsize(int val){
	icalproperty_set_maxresultssize(imp, val);
}
int ICalProperty::get_maxresultsize(){
	return icalproperty_get_maxresultssize(imp);
}

/* METHOD */
void ICalProperty::set_method(enum icalproperty_method val){
	icalproperty_set_method(imp, val);
}
enum icalproperty_method ICalProperty::get_method(){
	return icalproperty_get_method(imp);
}

/* ORGANIZER */
void ICalProperty::set_organizer(string val){
	icalproperty_set_organizer(imp, val);
}
string ICalProperty::get_organizer(){
	return (string)icalproperty_get_organizer(imp);
}

/* OWNER */
void ICalProperty::set_owner(string val){
	icalproperty_set_owner(imp, val);
}
string ICalProperty::get_owner(){
	return (string)icalproperty_get_owner(imp);
}

/* PERCENT-COMPLETE */
void ICalProperty::set_percentcomplete(int val){
	icalproperty_set_percentcomplete(imp, val);
}
int ICalProperty::get_percentcomplete(){
	return icalproperty_get_percentcomplete(imp);
}

/* PRIORITY */
void ICalProperty::set_priority(int val){
	icalproperty_set_priority(imp, val);
}
int ICalProperty::get_priority(){
	return icalproperty_get_priority(imp);
}

/* PRODID */
void ICalProperty::set_prodid(string val){
	icalproperty_set_prodid(imp, val);
}
string ICalProperty::get_prodid(){
	return (string)icalproperty_get_prodid(imp);
}

/* QUERY */
void ICalProperty::set_query(string val){
	icalproperty_set_query(imp, val);
}
string ICalProperty::get_query(){
	return (string)icalproperty_get_query(imp);
}

/* QUERYNAME */
void ICalProperty::set_queryname(string val){
	icalproperty_set_queryname(imp, val);
}
string ICalProperty::get_queryname(){
	return (string)icalproperty_get_queryname(imp);
}

/* RDATE */
void ICalProperty::set_rdate(struct icaldatetimeperiodtype val){
	icalproperty_set_rdate(imp, val);
}
struct icaldatetimeperiodtype ICalProperty::get_rdate(){
	return icalproperty_get_rdate(imp);
}

/* RECURRENCE-ID */
void ICalProperty::set_recurrenceid(struct icaltimetype val){
	icalproperty_set_recurrenceid(imp, val);
}
struct icaltimetype ICalProperty::get_recurrenceid(){
	return icalproperty_get_recurrenceid(imp);
}

/* RELATED-TO */
void ICalProperty::set_relatedto(string val){
	icalproperty_set_relatedto(imp, val);
}
string ICalProperty::get_relatedto(){
	return (string)icalproperty_get_relatedto(imp);
}

/* RELCALID */
void ICalProperty::set_relcalid(string val){
        icalproperty_set_relcalid(imp, val);
}
string ICalProperty::get_relcalid(){
        return (string)icalproperty_get_relcalid(imp);
}

/* REPEAT */
void ICalProperty::set_repeat(int val){
	icalproperty_set_repeat(imp, val);
}
int ICalProperty::get_repeat(){
	return icalproperty_get_repeat(imp);
}

/* REQUEST-STATUS */
void ICalProperty::set_requeststatus(string val){
  icalreqstattype v;
  
  v = icalreqstattype_from_string((char*)val);

  icalproperty_set_requeststatus(imp, v);
}

string ICalProperty::get_requeststatus(){
  icalreqstattype v;
  v =   icalproperty_get_requeststatus(imp);
  return (string)(icalreqstattype_as_string(v));
}

/* RESOURCES */
void ICalProperty::set_resources(string val){
	icalproperty_set_resources(imp, val);
}
string ICalProperty::get_resources(){
	return (string)icalproperty_get_resources(imp);
}

/* RRULE */
void ICalProperty::set_rrule(struct icalrecurrencetype val){
	icalproperty_set_rrule(imp, val);
}
struct icalrecurrencetype ICalProperty::get_rrule(){
	return icalproperty_get_rrule(imp);
}

/* SCOPE */
void ICalProperty::set_scope(string val){
	icalproperty_set_scope(imp, val);
}
string ICalProperty::get_scope(){
	return (string)icalproperty_get_scope(imp);
}

/* SEQUENCE */
void ICalProperty::set_sequence(int val){
	icalproperty_set_sequence(imp, val);
}
int ICalProperty::get_sequence(){
	return icalproperty_get_sequence(imp);
}

/* STATUS */
void ICalProperty::set_status(enum icalproperty_status val){
	icalproperty_set_status(imp, val);
}
enum icalproperty_status ICalProperty::get_status(){
	return icalproperty_get_status(imp);
}

/* SUMMARY */
void ICalProperty::set_summary(string val){
	icalproperty_set_summary(imp, val);
}
string ICalProperty::get_summary(){
	return (string)icalproperty_get_summary(imp);
}

/* TARGET */
void ICalProperty::set_target(string val){
	icalproperty_set_target(imp, val);
}
string ICalProperty::get_target(){
	return (string)icalproperty_get_target(imp);
}

/* TRANSP */
void ICalProperty::set_transp(enum icalproperty_transp val){
	icalproperty_set_transp(imp, val);
}
enum icalproperty_transp ICalProperty::get_transp(){
	return icalproperty_get_transp(imp);
}

/* TRIGGER */
void ICalProperty::set_trigger(struct icaltriggertype val){
	icalproperty_set_trigger(imp, val);
}
struct icaltriggertype ICalProperty::get_trigger(){
	return icalproperty_get_trigger(imp);
}

/* TZID */
void ICalProperty::set_tzid(string val){
	icalproperty_set_tzid(imp, val);
}
string ICalProperty::get_tzid(){
	return (string)icalproperty_get_tzid(imp);
}

/* TZNAME */
void ICalProperty::set_tzname(string val){
	icalproperty_set_tzname(imp, val);
}
string ICalProperty::get_tzname(){
	return (string)icalproperty_get_tzname(imp);
}

/* TZOFFSETFROM */
void ICalProperty::set_tzoffsetfrom(int val){
	icalproperty_set_tzoffsetfrom(imp, val);
}
int ICalProperty::get_tzoffsetfrom(){
	return icalproperty_get_tzoffsetfrom(imp);
}

/* TZOFFSETTO */
void ICalProperty::set_tzoffsetto(int val){
	icalproperty_set_tzoffsetto(imp, val);
}
int ICalProperty::get_tzoffsetto(){
	return icalproperty_get_tzoffsetto(imp);
}

/* TZURL */
void ICalProperty::set_tzurl(string val){
	icalproperty_set_tzurl(imp, val);
}
string ICalProperty::get_tzurl(){
	return (string)icalproperty_get_tzurl(imp);
}

/* UID */
void ICalProperty::set_uid(string val){
	icalproperty_set_uid(imp, val);
}
string ICalProperty::get_uid(){
	return (string)icalproperty_get_uid(imp);
}

/* URL */
void ICalProperty::set_url(string val){
	icalproperty_set_url(imp, val);
}
string ICalProperty::get_url(){
	return (string)icalproperty_get_url(imp);
}

/* VERSION */
void ICalProperty::set_version(string val){
	icalproperty_set_version(imp, val);
}
string ICalProperty::get_version(){
	return (string)icalproperty_get_version(imp);
}

/* X */
void ICalProperty::set_x(string val){
	icalproperty_set_x(imp, val);
}
string ICalProperty::get_x(){
	return (string)icalproperty_get_x(imp);
}

/* X-LIC-CLUSTERCOUNT */
void ICalProperty::set_xlicclustercount(string val){
	icalproperty_set_xlicclustercount(imp, val);
}
string ICalProperty::get_xlicclustercount(){
	return (string)icalproperty_get_xlicclustercount(imp);
}

/* X-LIC-ERROR */
void ICalProperty::set_xlicerror(string val){
	icalproperty_set_xlicerror(imp, val);
}
string ICalProperty::get_xlicerror(){
	return (string)icalproperty_get_xlicerror(imp);
}

/* X-LIC-MIMECHARSET */
void ICalProperty::set_xlicmimecharset(string val){
	icalproperty_set_xlicmimecharset(imp, val);
}
string ICalProperty::get_xlicmimecharset(){
	return (string)icalproperty_get_xlicmimecharset(imp);
}

/* X-LIC-MIMECID */
void ICalProperty::set_xlicmimecid(string val){
	icalproperty_set_xlicmimecid(imp, val);
}
string ICalProperty::get_xlicmimecid(){
	return (string)icalproperty_get_xlicmimecid(imp);
}

/* X-LIC-MIMECONTENTTYPE */
void ICalProperty::set_xlicmimecontenttype(string val){
	icalproperty_set_xlicmimecontenttype(imp, val);
}
string ICalProperty::get_xlicmimecontenttype(){
	return (string)icalproperty_get_xlicmimecontenttype(imp);
}

/* X-LIC-MIMEENCODING */
void ICalProperty::set_xlicmimeencoding(string val){
	icalproperty_set_xlicmimeencoding(imp, val);
}
string ICalProperty::get_xlicmimeencoding(){
	return (string)icalproperty_get_xlicmimeencoding(imp);
}

/* X-LIC-MIMEFILENAME */
void ICalProperty::set_xlicmimefilename(string val){
	icalproperty_set_xlicmimefilename(imp, val);
}
string ICalProperty::get_xlicmimefilename(){
	return (string)icalproperty_get_xlicmimefilename(imp);
}

/* X-LIC-MIMEOPTINFO */
void ICalProperty::set_xlicmimeoptinfo(string val){
	icalproperty_set_xlicmimeoptinfo(imp, val);
}
string ICalProperty::get_xlicmimeoptinfo(){
	return (string)icalproperty_get_xlicmimeoptinfo(imp);
}
