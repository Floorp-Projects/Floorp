/* -*- Mode: C++ -*- */

/**
 * @file    icalparameter_cxx.cpp
 * @author  fnguyen (12/10/01)
 * @brief   Implementation of C++ Wrapper for icalparameter.c
 *
 * (C) COPYRIGHT 2001, Critical Path
 */

#ifndef ICALPARAMETER_CXX_H
#include "icalparameter_cxx.h"
#endif

#ifndef ICALVALUE_CXX_H
#include "icalvalue_cxx.h"
#endif

typedef	char* string; // Will use the string library from STL

ICalParameter::ICalParameter() throw(icalerrorenum) : imp(icalparameter_new(ICAL_ANY_PARAMETER)){
}

ICalParameter::ICalParameter(const ICalParameter& v) throw(icalerrorenum) {
	imp = icalparameter_new_clone(v.imp);
	if (!imp) throw icalerrno;
}

ICalParameter& ICalParameter::operator=(const ICalParameter& v)  throw(icalerrorenum) {
	if (this == &v) return *this;

	if (imp != NULL)
	{
		icalparameter_free(imp);
		imp = icalparameter_new_clone(v.imp);
		if (!imp) throw icalerrno;
	}
    
	return *this;
}

ICalParameter::~ICalParameter(){
	if (imp != NULL) icalparameter_free(imp);
}

ICalParameter::ICalParameter(icalparameter* v) throw(icalerrorenum) : imp(v){
}

/// Create from string of form "PARAMNAME=VALUE"
ICalParameter::ICalParameter(string str) throw(icalerrorenum) { 
	imp = icalparameter_new_from_string(str);
	if (!imp) throw icalerrno;
}

/// Create from just the value, the part after the "="
ICalParameter::ICalParameter(icalparameter_kind kind, string  str) throw(icalerrorenum) { 
	imp = icalparameter_new_from_value_string(kind, str);
	if (!imp) throw icalerrno;
}

ICalParameter::ICalParameter(icalparameter_kind kind) throw(icalerrorenum) {
	imp = icalparameter_new(kind);
	if (!imp) throw icalerrno;
}

string ICalParameter::as_ical_string() throw(icalerrorenum) {
  char *str = icalparameter_as_ical_string(imp);

  if (!str) throw icalerrno;

  return str;
}

bool ICalParameter::is_valid(){
	if (imp == NULL) return false;
	return (icalparameter_isa_parameter((void*)imp) ? true : false);
}
icalparameter_kind ICalParameter::isa(){
	return icalparameter_isa(imp);
}
int ICalParameter::isa_parameter(void* param){
	return icalparameter_isa_parameter(param);
}

/* Acess the name of an X parameer */
void ICalParameter::set_xname(ICalParameter &param, string  v){
	icalparameter_set_xname(param, v);
}
string ICalParameter::get_xname(ICalParameter &param){
	return (string)icalparameter_get_xname(param);
}
void ICalParameter::set_xvalue (ICalParameter &param, string  v){
	icalparameter_set_xvalue(param, v);
}
string ICalParameter::get_xvalue(ICalParameter &param){
	return (string)icalparameter_get_xvalue(param);
}

/* Convert enumerations */
string ICalParameter::kind_to_string(icalparameter_kind kind){
	return (string)icalparameter_kind_to_string(kind);
}
icalparameter_kind ICalParameter::string_to_kind(string  str){
	return icalparameter_string_to_kind(str);
}

/* DELEGATED-FROM */
string ICalParameter::get_delegatedfrom(){
	return (string)icalparameter_get_delegatedfrom(imp);
}
void ICalParameter::set_delegatedfrom(string  v){
	icalparameter_set_delegatedfrom(imp, v);
}

/* RELATED */
icalparameter_related ICalParameter::get_related(){
	return icalparameter_get_related(imp);
}
void ICalParameter::set_related(icalparameter_related v){
	icalparameter_set_related(imp, v);
}

/* SENT-BY */
string ICalParameter::get_sentby(){
	return (string)icalparameter_get_sentby(imp);
}
void ICalParameter::set_sentby(string  v){
	icalparameter_set_sentby(imp, v);
}

/* LANGUAGE */
string ICalParameter::get_language(){
	return (string)icalparameter_get_language(imp);
}
void ICalParameter::set_language(string  v){
	icalparameter_set_language(imp, v);
}

/* RELTYPE */
icalparameter_reltype ICalParameter::get_reltype(){
	return icalparameter_get_reltype(imp);
}
void ICalParameter::set_reltype(icalparameter_reltype v){
	icalparameter_set_reltype(imp, v);
}

/* ENCODING */
icalparameter_encoding ICalParameter::get_encoding(){
	return icalparameter_get_encoding(imp);
}
void ICalParameter::set_encoding(icalparameter_encoding v){
	icalparameter_set_encoding(imp, v);
}

/* ALTREP */
string ICalParameter::get_altrep(){
	return (string)icalparameter_get_language(imp);
}
void ICalParameter::set_altrep(string  v){
	icalparameter_set_altrep(imp, v);
}

/* FMTTYPE */
string ICalParameter::get_fmttype(){
	return (string)icalparameter_get_fmttype(imp);
}
void ICalParameter::set_fmttype(string  v){
	icalparameter_set_fmttype(imp, v);
}

/* FBTYPE */
icalparameter_fbtype ICalParameter::get_fbtype(){
	return icalparameter_get_fbtype(imp);
}
void ICalParameter::set_fbtype(icalparameter_fbtype v){
	icalparameter_set_fbtype(imp, v);
}

/* RSVP */
icalparameter_rsvp ICalParameter::get_rsvp(){
	return icalparameter_get_rsvp(imp);
}
void ICalParameter::set_rsvp(icalparameter_rsvp v){
	icalparameter_set_rsvp(imp, v);
}

/* RANGE */
icalparameter_range ICalParameter::get_range(){
	return icalparameter_get_range(imp);
}
void ICalParameter::set_range(icalparameter_range v){
	icalparameter_set_range(imp, v);
}

/* DELEGATED-TO */
string ICalParameter::get_delegatedto(){
	return (string)icalparameter_get_delegatedto(imp);
}
void ICalParameter::set_delegatedto(string  v){
	icalparameter_set_delegatedto(imp, v);
}

/* CN */
string ICalParameter::get_cn(){
	return (string)icalparameter_get_cn(imp);
}
void ICalParameter::set_cn(string  v){
	icalparameter_set_cn(imp, v);
}

/* ROLE */
icalparameter_role ICalParameter::get_role(){
	return icalparameter_get_role(imp);
}
void ICalParameter::set_role(icalparameter_role v){
	icalparameter_set_role(imp, v);
}

/* X-LIC-COMPARETYPE */
icalparameter_xliccomparetype ICalParameter::get_xliccomparetype(){
	return icalparameter_get_xliccomparetype(imp);
}
void ICalParameter::set_xliccomparetype(icalparameter_xliccomparetype v){
	icalparameter_set_xliccomparetype(imp, v);
}

/* PARTSTAT */
icalparameter_partstat ICalParameter::get_partstat(){
	return icalparameter_get_partstat(imp);
}
void ICalParameter::set_partstat(icalparameter_partstat v){
	icalparameter_set_partstat(imp, v);
}

/* X-LIC-ERRORTYPE */
icalparameter_xlicerrortype ICalParameter::get_xlicerrortype(){
	return icalparameter_get_xlicerrortype(imp);
}
void ICalParameter::set_xlicerrortype(icalparameter_xlicerrortype v){
	icalparameter_set_xlicerrortype(imp, v);
}

/* MEMBER */
string ICalParameter::get_member(){
	return (string)icalparameter_get_member(imp);
}
void ICalParameter::set_member(string  v){
	icalparameter_set_member(imp, v);
}

/* X */
string ICalParameter::get_x(){
	return (string)icalparameter_get_x(imp);
}
void ICalParameter::set_x(string  v){
	icalparameter_set_x(imp, v);
}

/* CUTYPE */
icalparameter_cutype ICalParameter::get_cutype(){
	return icalparameter_get_cutype(imp);
}
void ICalParameter::set_cutype(icalparameter_cutype v){
	icalparameter_set_cutype(imp, v);
}

/* TZID */
string ICalParameter::get_tzid(){
	return (string)icalparameter_get_tzid(imp);
}
void ICalParameter::set_tzid(string  v){
	icalparameter_set_tzid(imp, v);
}

/* VALUE */
icalparameter_value ICalParameter::get_value(){
	return icalparameter_get_value(imp);
}
void ICalParameter::set_value(icalparameter_value v){
	icalparameter_set_value(imp, v);
}

/* DIR */
string ICalParameter::get_dir(){
	return (string)icalparameter_get_dir(imp);
}
void ICalParameter::set_dir(string  v){
	icalparameter_set_dir(imp, v);
}
