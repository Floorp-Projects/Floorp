/* -*- Mode: C++ -*- */

/**
 * @file    icalparameter_cxx.h
 * @author  fnguyen (12/10/01)
 * @brief   Definition of C++ Wrapper for icalparameter.c
 *
 * (C) COPYRIGHT 2001, Critical Path
 */


#ifndef ICALPARAMETER_CXX_H
#define ICALPARAMETER_CXX_H

extern "C" {
#include "ical.h"
};

#include "icptrholder.h"

typedef	char* string; // Will use the string library from STL

class ICalParameter {
public:
	ICalParameter() throw(icalerrorenum);
	ICalParameter(const ICalParameter&) throw(icalerrorenum);
	ICalParameter& operator=(const ICalParameter&) throw(icalerrorenum);
	~ICalParameter();

	ICalParameter(icalparameter* v)  throw(icalerrorenum);

        // Create from string of form "PARAMNAME=VALUE"
	ICalParameter(string str) throw(icalerrorenum);

        // Create from just the value, the part after the "="
	ICalParameter(icalparameter_kind kind, string  str) throw(icalerrorenum); 
	ICalParameter(icalparameter_kind kind) throw(icalerrorenum);

	operator icalparameter*() { return imp; }

	void detach() {
	    imp = NULL;
	}

public:
	string as_ical_string() throw(icalerrorenum);
	bool is_valid();
	icalparameter_kind isa( );
	int isa_parameter(void* param);
	
public:
	/* Acess the name of an X parameer */
	static void set_xname (ICalParameter  &param, string  v);
	static string get_xname(ICalParameter  &param);
	static void set_xvalue (ICalParameter  &param, string  v);
	static string get_xvalue(ICalParameter  &param);

	/* Convert enumerations */
	static string kind_to_string(icalparameter_kind kind);
	static icalparameter_kind string_to_kind(string  str);

public:
	/* DELEGATED-FROM */
	string get_delegatedfrom();
	void set_delegatedfrom(string  v);

	/* RELATED */
	icalparameter_related get_related();
	void set_related(icalparameter_related v);

	/* SENT-BY */
	string get_sentby();
	void set_sentby(string  v);

	/* LANGUAGE */
	string get_language();
	void set_language(string  v);

	/* RELTYPE */
	icalparameter_reltype get_reltype();
	void set_reltype(icalparameter_reltype v);

	/* ENCODING */
	icalparameter_encoding get_encoding();
	void set_encoding(icalparameter_encoding v);

	/* ALTREP */
	string get_altrep();
	void set_altrep(string  v);

	/* FMTTYPE */
	string get_fmttype();
	void set_fmttype(string  v);

	/* FBTYPE */
	icalparameter_fbtype get_fbtype();
	void set_fbtype(icalparameter_fbtype v);

	/* RSVP */
	icalparameter_rsvp get_rsvp();
	void set_rsvp(icalparameter_rsvp v);

	/* RANGE */
	icalparameter_range get_range();
	void set_range(icalparameter_range v);

	/* DELEGATED-TO */
	string get_delegatedto();
	void set_delegatedto(string  v);

	/* CN */
	string get_cn();
	void set_cn(string  v);

	/* ROLE */
	icalparameter_role get_role();
	void set_role(icalparameter_role v);

	/* X-LIC-COMPARETYPE */
	icalparameter_xliccomparetype get_xliccomparetype();
	void set_xliccomparetype(icalparameter_xliccomparetype v);

	/* PARTSTAT */
	icalparameter_partstat get_partstat();
	void set_partstat(icalparameter_partstat v);

	/* X-LIC-ERRORTYPE */
	icalparameter_xlicerrortype get_xlicerrortype();
	void set_xlicerrortype(icalparameter_xlicerrortype v);

	/* MEMBER */
	string get_member();
	void set_member(string  v);

	/* X */
	string get_x();
	void set_x(string  v);

	/* CUTYPE */
	icalparameter_cutype get_cutype();
	void set_cutype(icalparameter_cutype v);

	/* TZID */
	string get_tzid();
	void set_tzid(string  v);

	/* VALUE */
	icalparameter_value get_value();
	void set_value(icalparameter_value v);

	/* DIR */
	string get_dir();
	void set_dir(string  v);

private:
	icalparameter* imp;
};

#endif 
