#include "icalspanlist_cxx.h"
#include <exception>

/** @brief Construct an ICalSpanList from an icalset
    @param set     The icalset containing the VEVENTS
    @param start   Designated start of the spanlist
    @param end     Designated end of the spanlist
*/

ICalSpanList::ICalSpanList(icalset *set, icaltimetype start, icaltimetype end) throw(icalerrorenum)
{
  data = icalspanlist_new(set, start, end);
  if (!data) throw icalerrno;
};


/** @brief Constructor
    @param comp  A valid icalcomponent with a VFREEBUSY section
*/

ICalSpanList::ICalSpanList(icalcomponent *comp) throw(icalerrorenum)
{
  data = icalspanlist_from_vfreebusy(comp);
  if (!data) throw icalerrno;
}

/** @brief Constructor
    @param comp  A valid VComponent with a VFREEBUSY section
*/
ICalSpanList::ICalSpanList(VComponent &comp) throw(icalerrorenum)
{
  data = icalspanlist_from_vfreebusy((icalcomponent*) comp);
  if (!data) throw icalerrno;
}

/** Destructor */
ICalSpanList::~ICalSpanList() {
  if (data)
    icalspanlist_free(data);
}


/**
 * @brief Returns a VFREEBUSY component for the object.
 *
 * @see icalspanlist_as_vfreebusy()
 */

VComponent*
ICalSpanList::get_vfreebusy(const char *organizer, const char *attendee) throw(icalerrorenum)
{
  icalcomponent *comp;
  VComponent    *vcomp;

  comp = icalspanlist_as_vfreebusy(data, organizer, attendee);
  if (comp == 0) throw icalerrno;
    
  vcomp = new VComponent(comp);
  if (vcomp == 0) throw icalerrno;

  return vcomp;
}


/**
 * @brief Returns a summary of events over delta_t
 *
 * @param delta_t    Number of seconds to divide the spanlist time period
 *                   into.
 *
 * This method calculates the total number of events in each time slot
 * of delta_t seconds.
 *
 * @see icalspanlist_as_freebusy_matrix()
 */

std::vector<int> ICalSpanList::as_vector(int delta_t) throw(icalerrorenum)
{
  int *matrix;
  int i = 0;
  std::vector<int> event_vec;

  matrix = icalspanlist_as_freebusy_matrix(data, delta_t);

  if (!matrix) throw ICAL_USAGE_ERROR;

  while (matrix[i] != -1) {
    event_vec.push_back(matrix[i]); // Add item at end of vector
  }
  
  return(event_vec);
}
