/* -*- Mode: C++ -*- */

/**
 * @file     icalspanlist_cxx.h
 * @author   Critical Path
 * @brief    C++ class wrapping the icalspanlist data structure
 *
 * This class wraps the icalspanlist routines in libicalss
 *
 * Errors within libicalss are propagated via exceptions of type
 * icalerrorenum.  See icalerror.h for the complete list of exceptions
 * that might be thrown.
 */

#ifndef ICALSPANLIST_CXX_H
#define ICALSPANLIST_CXX_H

#include "ical.h"
#include "icalss.h"
#include "vcomponent.h"
#include <vector>		/* For as_matrix.. */

class ICalSpanList {
 public:
  /** Construct an ICalSpanList from an icalset */
  ICalSpanList(icalset *set, icaltimetype start, icaltimetype end) throw(icalerrorenum);

  /** Construct an ICalSpanList from the VFREEBUSY chunk of a icalcomponent */
  ICalSpanList(icalcomponent *comp) throw(icalerrorenum);

  /** Construct an ICalSpanList from the VFREEBUSY chunk of a vcomponent */
  ICalSpanList(VComponent &comp) throw(icalerrorenum);

  /** Destructor */
  ~ICalSpanList();

  /** Return a VFREEBUSY icalcomponent */
  VComponent* get_vfreebusy(const char *organizer, const char *attendee) throw(icalerrorenum);

  /** Return the base data when casting */
  operator icalspanlist*()    {return data;}

  /** Return a vector of the number of events over delta t */
  std::vector<int> as_vector(int delta_t) throw(icalerrorenum);

  /** Dump the spanlist to stdout */
  void dump()                 {icalspanlist_dump(data);}

 private:
  icalspanlist *data;
};

#endif
