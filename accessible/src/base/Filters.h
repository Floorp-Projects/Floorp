/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_Filters_h__
#define mozilla_a11y_Filters_h__

#include <stdint.h>

/**
 * Predefined filters used for nsAccIterator and nsAccCollector.
 */
namespace mozilla {
namespace a11y {

class Accessible;

namespace filters {

enum EResult {
  eSkip = 0,
  eMatch = 1,
  eSkipSubtree = 2
};

/**
 * Return true if the traversed accessible complies with filter.
 */
typedef uint32_t (*FilterFuncPtr) (Accessible*);

/**
 * Matches selected/selectable accessibles in subtree.
 */
uint32_t GetSelected(Accessible* aAccessible);
uint32_t GetSelectable(Accessible* aAccessible);

/**
 * Matches row accessibles in subtree.
 */
uint32_t GetRow(Accessible* aAccessible);

/**
 * Matches cell accessibles in children.
 */
uint32_t GetCell(Accessible* aAccessible);

/**
 * Matches embedded objects in children.
 */
uint32_t GetEmbeddedObject(Accessible* aAccessible);

} // namespace filters
} // namespace a11y
} // namespace mozilla

#endif
