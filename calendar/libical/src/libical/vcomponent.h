/* -*- Mode: C++ -*- */

/**
 * @file    vcomponent.h
 * @author  fnguyen (12/10/01)
 * @brief   C++ classes for the icalcomponent wrapper (VToDo VEvent, etc..).
 *
 * (C) COPYRIGHT 2001, Critical Path
 */

#ifndef VCOMPONENT_H
#define VCOMPONENT_H

#include "ical.h"
#include "icptrholder.h"

typedef	char* string; // Will use the string library from STL

class ICalProperty;

/**
 * @class VComponent
 * @brief A class wrapping the libical icalcomponent functions
 *
 * @exception icalerrorenum   Any errors generated in libical are
 *                            propogated via this exception type.
 */

class VComponent {
public:
	VComponent()                             throw (icalerrorenum);
	VComponent(const VComponent&)            throw (icalerrorenum);
	VComponent& operator=(const VComponent&) throw (icalerrorenum);
	virtual ~VComponent();

	VComponent(icalcomponent* v)        throw (icalerrorenum);
	VComponent(string str)              throw (icalerrorenum);
	VComponent(icalcomponent_kind kind) throw (icalerrorenum);;

	operator icalcomponent* () { return imp; }

	void new_from_string(string str);

	// detach imp to this object. use with caution. it would cause
	// memory leak if you are not careful.
	void detach() {
	    imp = NULL;
	}

public:
	string as_ical_string() throw (icalerrorenum);
	bool is_valid();
	icalcomponent_kind isa();
	int isa_component(void* component);

	/// Working with properties
	void add_property(ICalProperty* property);
	void remove_property(ICalProperty* property);
	int count_properties(icalproperty_kind kind);

	// Iterate through the properties
	ICalProperty* get_current_property();
	ICalProperty* get_first_property(icalproperty_kind kind);
	ICalProperty* get_next_property(icalproperty_kind kind);

	// Working with components

	/**
	 *  Return the first VEVENT, VTODO or VJOURNAL sub-component if
	 *   it is one of those types 
	 */

	VComponent* get_inner();

	void add_component(VComponent* child);
	void remove_component(VComponent* child);
	int count_components(icalcomponent_kind kind);

	/**
	 * Iteration Routines. There are two forms of iterators,
	 * internal and external. The internal ones came first, and
	 * are almost completely sufficient, but they fail badly when
	 * you want to construct a loop that removes components from
	 * the container.
	*/

	/// Iterate through components
	VComponent* get_current_component();
	VComponent* get_first_component(icalcomponent_kind kind);
	VComponent* get_next_component(icalcomponent_kind kind);

	/// Using external iterators
	icalcompiter begin_component(icalcomponent_kind kind);
	icalcompiter end_component(icalcomponent_kind kind);
	VComponent* next(icalcompiter* i);
	VComponent* prev(icalcompiter* i);
	VComponent* current(icalcompiter* i);

	/// Working with embedded error properties
	int count_errors();

	/// Remove all X-LIC-ERROR properties
	void strip_errors();

	/// Convert some X-LIC-ERROR properties into RETURN-STATUS properties
	void convert_errors();

	/// Kind conversion routines 
	static icalcomponent_kind string_to_kind(string str);
	static string kind_to_string(icalcomponent_kind kind);

public:
	struct icaltimetype get_dtstart();
	void set_dtstart(struct icaltimetype v);

	/** For the icalcomponent routines only, dtend and duration
	 *  are tied together. If you call the set routine for one and
	 *  the other exists, the routine will calculate the change to
	 *  the other. That is, if there is a DTEND and you call
	 *  set_duration, the routine will modify DTEND to be the sum
	 *  of DTSTART and the duration. If you call a get routine for
	 *  one and the other exists, the routine will calculate the
	 *  return value. If you call a set routine and neither
	 *  exists, the routine will create the apropriate property
	*/

	struct icaltimetype get_dtend();
	void set_dtend(struct icaltimetype v);

        struct icaltimetype get_due();
        void set_due(struct icaltimetype v);

	struct icaldurationtype get_duration();
	void set_duration(struct icaldurationtype v);

	icalproperty_method get_method();
	void set_method(icalproperty_method method);

	struct icaltimetype get_dtstamp();
	void set_dtstamp(struct icaltimetype v);

	string get_summary();
	void set_summary(string v);

        string get_location();
        void set_location(string v);

        string get_description();
        void set_description(string v);

	string get_comment();
	void set_comment(string v);

	string get_uid();
	void set_uid(string v);

	string get_relcalid();
	void set_relcalid(string v);

	struct icaltimetype get_recurrenceid();
	void set_recurrenceid(struct icaltimetype v);

        int get_sequence();
        void set_sequence(int v);

        int get_status();
        void set_status(enum icalproperty_status v);


public:
	/**
	 * For VCOMPONENT: Return a reference to the first VEVENT,
	 * VTODO, or VJOURNAL
	*/
	VComponent* get_first_real_component();

	/** 
	 * For VEVENT, VTODO, VJOURNAL and VTIMEZONE: report the
	 * start and end times of an event in UTC 
	 */
	virtual struct icaltime_span get_span();

        int recurrence_is_excluded(struct icaltimetype *dtstart,
                                   struct icaltimetype *recurtime);


public:
	/**
	   helper functions for adding/removing/updating property and
	   sub-components */

	/// Note: the VComponent kind have to be the same

	bool remove(VComponent&, bool ignoreValue);
	bool update(VComponent&, bool removeMissing);
	bool add(VComponent&);

private:
	/* Internal operations. They are private, and you should not be using them. */
	VComponent* get_parent();
	void set_parent(VComponent *parent);

        char* quote_ical_string(char* str);

private:
	icalcomponent* imp;
};

class VCalendar : public VComponent {
public:
	VCalendar();
	VCalendar(const VCalendar&);
	VCalendar& operator=(const VCalendar&);
	~VCalendar();

	VCalendar(icalcomponent* v);
	VCalendar(string str);
};


class VEvent : public VComponent {
public:
	VEvent();
	VEvent(const VEvent&);
	VEvent& operator=(const VEvent&);
	~VEvent();

	VEvent(icalcomponent* v);
	VEvent(string str);
};

class VToDo : public VComponent {
public:
	VToDo();
	VToDo(const VToDo&);
	VToDo& operator=(const VToDo&);
	~VToDo();

	VToDo(icalcomponent* v);
	VToDo(string str);
};

class VAgenda : public VComponent {
public:
        VAgenda();
        VAgenda(const VAgenda&);
        VAgenda& operator=(const VAgenda&);
        ~VAgenda();

	VAgenda(icalcomponent* v);
        VAgenda(string str);
};

class VQuery : public VComponent {
public:
	VQuery();
	VQuery(const VQuery&);
	VQuery& operator=(const VQuery&);
	~VQuery();

	VQuery(icalcomponent* v);
	VQuery(string str);
};

class VJournal : public VComponent {
public:
	VJournal();
	VJournal(const VJournal&);
	VJournal& operator=(const VJournal&);
	~VJournal();

	VJournal(icalcomponent* v);
	VJournal(string str);
};

class VAlarm : public VComponent {
public:
	VAlarm();
	VAlarm(const VAlarm&);
	VAlarm& operator=(const VAlarm&);
	~VAlarm();

	VAlarm(icalcomponent* v);
	VAlarm(string str);

	/**
	 *  compute the absolute trigger time for this alarm. trigger
	 *  may be related to the containing component c.  the result
	 *  is populated in tr->time
	 */
	icalrequeststatus getTriggerTime(VComponent &c, struct icaltriggertype *tr);
};

class VFreeBusy : public VComponent {
public:
	VFreeBusy();
	VFreeBusy(const VFreeBusy&);
	VFreeBusy& operator=(const VFreeBusy&);
	~VFreeBusy();

	VFreeBusy(icalcomponent* v);
	VFreeBusy(string str);
};

class VTimezone : public VComponent {
public:
	VTimezone();
	VTimezone(const VTimezone&);
	VTimezone& operator=(const VTimezone&);
	~VTimezone();

	VTimezone(icalcomponent* v);
	VTimezone(string str);
};

class XStandard : public VComponent {
public:
	XStandard();
	XStandard(const XStandard&);
	XStandard& operator=(const XStandard&);
	~XStandard();

	XStandard(icalcomponent* v);
	XStandard(string str);
};

class XDaylight : public VComponent {
public:
	XDaylight();
	XDaylight(const XDaylight&);
	XDaylight& operator=(const XDaylight&);
	~XDaylight();

	XDaylight(icalcomponent* v);
	XDaylight(string str);
};

typedef ICPointerHolder<VComponent> VComponentTmpPtr; /* see icptrholder.h for comments */

#endif /* VComponent_H */
