/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef a11yFilters_h_
#define a11yFilters_h_

class Accessible;

/**
 * Predefined filters used for nsAccIterator and nsAccCollector.
 */
namespace filters {

  /**
   * Return true if the traversed accessible complies with filter.
   */
  typedef bool (*FilterFuncPtr) (Accessible*);

  bool GetSelected(Accessible* aAccessible);
  bool GetSelectable(Accessible* aAccessible);
  bool GetRow(Accessible* aAccessible);
  bool GetCell(Accessible* aAccessible);
  bool GetEmbeddedObject(Accessible* aAccessible);
}

#endif
