/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalbdbset_cxx.h
 CREATOR: dml 12/12/01
 (C) COPYRIGHT 2001, Critical Path
======================================================================*/

#ifndef ICALBDBSET_CXX_H
#define ICALBDBSET_CXX_H


extern "C" {
#include "ical.h"
#include "icalgauge.h"
}

#include "vcomponent.h"
#include <db_cxx.h>

typedef	char* string; // Will use the string library from STL

class ICalBDBSet {
public:

  ICalBDBSet();
  ICalBDBSet(const ICalBDBSet&);
  ICalBDBSet operator=(const ICalBDBSet &);
  ~ICalBDBSet();

  ICalBDBSet(const string& path, int flags);

public:

  void free();
  string path();

  icalerrorenum add_component(VComponent* child);
  icalerrorenum remove_component(VComponent* child);
  int count_components(icalcomponent_kind kind);

  // Restrict the component returned by icalbdbset_first, _next to those
  // that pass the gauge. _clear removes the gauge 
  icalerrorenum select(icalgauge *gauge);
  void clear();

  // Get and search for a component by uid 
  VComponent* fetch(string &uid);
  VComponent* fetch_match(icalcomponent *c);
  int has_uid(string &uid);

  // Iterate through components. If a guage has been defined, these
  // will skip over components that do not pass the gauge 
  VComponent* get_current_component();
  VComponent* get_first_component();
  VComponent* get_next_component();

  VComponent* get_component();

};

#endif
