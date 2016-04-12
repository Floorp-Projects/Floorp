/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_EmbeddedObjCollector_h__
#define mozilla_a11y_EmbeddedObjCollector_h__

#include "nsTArray.h"

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * Collect embedded objects. Provide quick access to accessible by index and
 * vice versa.
 */
class EmbeddedObjCollector final
{
public:
  ~EmbeddedObjCollector() { }

  /**
   * Return index of the given accessible within the collection.
   */
  int32_t GetIndexAt(Accessible* aAccessible);

  /**
   * Return accessible count within the collection.
   */
  uint32_t Count();

  /**
   * Return an accessible from the collection at the given index.
   */
  Accessible* GetAccessibleAt(uint32_t aIndex);

protected:
  /**
   * Ensure accessible at the given index is stored and return it.
   */
  Accessible* EnsureNGetObject(uint32_t aIndex);

  /**
   * Ensure index for the given accessible is stored and return it.
   */
  int32_t EnsureNGetIndex(Accessible* aAccessible);

  // Make sure it's used by Accessible class only.
  explicit EmbeddedObjCollector(Accessible* aRoot) :
    mRoot(aRoot), mRootChildIdx(0) {}

  /**
   * Append the object to collection.
   */
  void AppendObject(Accessible* aAccessible);

  friend class Accessible;

  Accessible* mRoot;
  uint32_t mRootChildIdx;
  nsTArray<Accessible*> mObjects;
};

} // namespace a11y
} // namespace mozilla

#endif
