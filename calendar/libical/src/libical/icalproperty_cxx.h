/* -*- Mode: C++ -*- */

/**
 * @file    icalproperty_cxx.h
 * @author  fnguyen (12/10/01)
 * @brief   Definition of C++ Wrapper for icalproperty.c
 *
 * (C) COPYRIGHT 2001, Critical Path
 */

#ifndef ICALPROPERTY_CXX_H
#define ICALPROPERTY_CXX_H

#include "ical.h"
#include "icptrholder.h"

typedef	char* string; // Will use the string library from STL

class ICalParameter;
class ICalValue;

class ICalProperty {
public:
	ICalProperty();
	ICalProperty(const ICalProperty&) throw(icalerrorenum);
	ICalProperty& operator=(const ICalProperty&) throw(icalerrorenum);
	~ICalProperty();

	ICalProperty(icalproperty* v);
	ICalProperty(string str);
	ICalProperty(icalproperty_kind kind);
	ICalProperty(icalproperty_kind kind, string str);

	operator icalproperty*() {return imp;}
        int operator==(ICalProperty& rhs);

	void detach() {
	    imp = NULL;
	}

public:
	string as_ical_string();
	icalproperty_kind isa();
	int isa_property(void* property);

	void add_parameter(ICalParameter& parameter);
	void set_parameter(ICalParameter& parameter);
	void set_parameter_from_string(string name, string val);
	string get_parameter_as_string(string name);
	void remove_parameter(icalparameter_kind kind);
	int count_parameters();

	/** Iterate through the parameters */
	ICalParameter* get_first_parameter(icalparameter_kind kind);
	ICalParameter* get_next_parameter(icalparameter_kind kind);

	/** Access the value of the property */
	void set_value(const ICalValue& val);
	void set_value_from_string(string val, string kind);

	ICalValue* get_value();
	string get_value_as_string();

	/** Return the name of the property -- the type name converted
	 *  to a string, or the value of get_x_name if the type is X
	 *  property
	*/
	string get_name();

public:
	/* Deal with X properties */
	static void set_x_name(ICalProperty &prop, string name);
	static string get_x_name(ICalProperty &prop);

	static icalvalue_kind icalparameter_value_to_value_kind(icalparameter_value val);

	/* Convert kinds to string and get default value type */
	static icalvalue_kind kind_to_value_kind(icalproperty_kind kind);
	static icalproperty_kind value_kind_to_kind(icalvalue_kind kind);
	static string kind_to_string(icalproperty_kind kind);
	static icalproperty_kind string_to_kind(string str);

	static icalproperty_method string_to_method(string str);
	static string method_to_string(icalproperty_method method);

	static string enum_to_string(int e);
	static int string_to_enum(string str);

	static string status_to_string(icalproperty_status);
	static icalproperty_status string_to_status(string str);

	static int enum_belongs_to_property(icalproperty_kind kind, int e);

public:
	/* ACTION */
	void set_action(enum icalproperty_action v);
	enum icalproperty_action get_action();

	/* ATTACH */
	void set_attach(icalattach *v);
	icalattach *get_attach();

	/* ATTENDEE */
	void set_attendee(string val);
	string get_attendee();

	/* CALSCALE */
	void set_calscale(string val);
	string get_calscale();

	/* CATEGORIES */
	void set_categories(string val);
	string get_categories();

	/* CLASS */
	void set_class(enum icalproperty_class val);
	enum icalproperty_class get_class();

	/* COMMENT */
	void set_comment(string val);
	string get_comment();

	/* COMPLETED */
	void set_completed(struct icaltimetype val);
	struct icaltimetype get_completed();

	/* CONTACT */
	void set_contact(string val);
	string get_contact();

	/* CREATED */
	void set_created(struct icaltimetype val);
	struct icaltimetype get_created();

	/* DESCRIPTION */
	void set_description(string val);
	string get_description();

	/* DTEND */
	void set_dtend(struct icaltimetype val);
	struct icaltimetype get_dtend();

	/* DTSTAMP */
	void set_dtstamp(struct icaltimetype val);
	struct icaltimetype get_dtstamp();

	/* DTSTART */
	void set_dtstart(struct icaltimetype val);
	struct icaltimetype get_dtstart();

	/* DUE */
	void set_due(struct icaltimetype val);
	struct icaltimetype get_due();

	/* DURATION */
	void set_duration(struct icaldurationtype val);
	struct icaldurationtype get_duration();

	/* EXDATE */
	void set_exdate(struct icaltimetype val);
	struct icaltimetype get_exdate();

	/* EXPAND */
	void set_expand(int val);
	int get_expand();

	/* EXRULE */
	void set_exrule(struct icalrecurrencetype val);
	struct icalrecurrencetype get_exrule();

	/* FREEBUSY */
	void set_freebusy(struct icalperiodtype val);
	struct icalperiodtype get_freebusy();

	/* GEO */
	void set_geo(struct icalgeotype val);
	struct icalgeotype get_geo();

	/* GRANT */
	void set_grant(string val);
	string get_grant();

	/* LAST-MODIFIED */
	void set_lastmodified(struct icaltimetype val);
	struct icaltimetype get_lastmodified();

	/* LOCATION */
	void set_location(string val);
	string get_location();

	/* MAXRESULTS */
	void set_maxresults(int val);
	int get_maxresults();

	/* MAXRESULTSSIZE */
	void set_maxresultsize(int val);
	int get_maxresultsize();

	/* METHOD */
	void set_method(enum icalproperty_method val);
	enum icalproperty_method get_method();

	/* OWNER */
	void set_owner(string val);
	string get_owner();

	/* ORGANIZER */
	void set_organizer(string val);
	string get_organizer();

	/* PERCENT-COMPLETE */
	void set_percentcomplete(int val);
	int get_percentcomplete();

	/* PRIORITY */
	void set_priority(int val);
	int get_priority();

	/* PRODID */
	void set_prodid(string val);
	string get_prodid();

	/* QUERY */
	void set_query(string val);
	string get_query();

	/* QUERYNAME */
	void set_queryname(string val);
	string get_queryname();

	/* RDATE */
	void set_rdate(struct icaldatetimeperiodtype val);
	struct icaldatetimeperiodtype get_rdate();

	/* RECURRENCE-ID */
	void set_recurrenceid(struct icaltimetype val);
	struct icaltimetype get_recurrenceid();

	/* RELATED-TO */
	void set_relatedto(string val);
	string get_relatedto();

        /* RELCALID */
        void set_relcalid(string val);
        string get_relcalid();

	/* REPEAT */
	void set_repeat(int val);
	int get_repeat();

	/* REQUEST-STATUS */
	void set_requeststatus(string val);
	string get_requeststatus();

	/* RESOURCES */
	void set_resources(string val);
	string get_resources();

	/* RRULE */
	void set_rrule(struct icalrecurrencetype val);
	struct icalrecurrencetype get_rrule();

	/* SCOPE */
	void set_scope(string val);
	string get_scope();

	/* SEQUENCE */
	void set_sequence(int val);
	int get_sequence();

	/* STATUS */
	void set_status(enum icalproperty_status val);
	enum icalproperty_status get_status();

	/* SUMMARY */
	void set_summary(string val);
	string get_summary();

	/* TARGET */
	void set_target(string val);
	string get_target();

	/* TRANSP */
	void set_transp(enum icalproperty_transp val);
	enum icalproperty_transp get_transp();

	/* TRIGGER */
	void set_trigger(struct icaltriggertype val);
	struct icaltriggertype get_trigger();

	/* TZID */
	void set_tzid(string val);
	string get_tzid();

	/* TZNAME */
	void set_tzname(string val);
	string get_tzname();

	/* TZOFFSETFROM */
	void set_tzoffsetfrom(int val);
	int get_tzoffsetfrom();

	/* TZOFFSETTO */
	void set_tzoffsetto(int val);
	int get_tzoffsetto();

	/* TZURL */
	void set_tzurl(string val);
	string get_tzurl();

	/* UID */
	void set_uid(string val);
	string get_uid();

	/* URL */
	void set_url(string val);
	string get_url();

	/* VERSION */
	void set_version(string val);
	string get_version();

	/* X */
	void set_x(string val);
	string get_x();

	/* X-LIC-CLUSTERCOUNT */
	void set_xlicclustercount(string val);
	string get_xlicclustercount();

	/* X-LIC-ERROR */
	void set_xlicerror(string val);
	string get_xlicerror();

	/* X-LIC-MIMECHARSET */
	void set_xlicmimecharset(string val);
	string get_xlicmimecharset();

	/* X-LIC-MIMECID */
	void set_xlicmimecid(string val);
	string get_xlicmimecid();

	/* X-LIC-MIMECONTENTTYPE */
	void set_xlicmimecontenttype(string val);
	string get_xlicmimecontenttype();

	/* X-LIC-MIMEENCODING */
	void set_xlicmimeencoding(string val);
	string get_xlicmimeencoding();

	/* X-LIC-MIMEFILENAME */
	void set_xlicmimefilename(string val);
	string get_xlicmimefilename();

	/* X-LIC-MIMEOPTINFO */
	void set_xlicmimeoptinfo(string val);
	string get_xlicmimeoptinfo();

private:
  icalproperty* imp; /**< The actual C based icalproperty */
};

typedef ICPointerHolder<ICalProperty> ICalPropertyTmpPtr;   /* see icptrholder.h for comments */

#endif /* ICalProperty_H */
