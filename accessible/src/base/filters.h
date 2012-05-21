/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef a11yFilters_h_
#define a11yFilters_h_

class nsAccessible;

/**
 * Predefined filters used for nsAccIterator and nsAccCollector.
 */
namespace filters {

  /**
   * Return true if the traversed accessible complies with filter.
   */
  typedef bool (*FilterFuncPtr) (nsAccessible*);

  bool GetSelected(nsAccessible* aAccessible);
  bool GetSelectable(nsAccessible* aAccessible);
  bool GetRow(nsAccessible* aAccessible);
  bool GetCell(nsAccessible* aAccessible);
  bool GetEmbeddedObject(nsAccessible* aAccessible);
}

#endif
