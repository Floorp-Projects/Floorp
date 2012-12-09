/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccCollector_h__
#define mozilla_a11y_AccCollector_h__

#include "Filters.h"

#include "nsTArray.h"

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * Collect accessible children complying with filter function. Provides quick
 * access to accessible by index.
 */
class AccCollector
{
public:
  AccCollector(Accessible* aRoot, filters::FilterFuncPtr aFilterFunc);
  virtual ~AccCollector();

  /**
   * Return accessible count within the collection.
   */
  uint32_t Count();

  /**
   * Return an accessible from the collection at the given index.
   */
  Accessible* GetAccessibleAt(uint32_t aIndex);

  /**
   * Return index of the given accessible within the collection.
   */
  virtual int32_t GetIndexAt(Accessible* aAccessible);

protected:
  /**
   * Ensure accessible at the given index is stored and return it.
   */
  Accessible* EnsureNGetObject(uint32_t aIndex);

  /**
   * Ensure index for the given accessible is stored and return it.
   */
  int32_t EnsureNGetIndex(Accessible* aAccessible);

  /**
   * Append the object to collection.
   */
  virtual void AppendObject(Accessible* aAccessible);

  filters::FilterFuncPtr mFilterFunc;
  Accessible* mRoot;
  uint32_t mRootChildIdx;

  nsTArray<Accessible*> mObjects;

private:
  AccCollector();
  AccCollector(const AccCollector&);
  AccCollector& operator =(const AccCollector&);
};

/**
 * Collect embedded objects. Provide quick access to accessible by index and
 * vice versa.
 */
class EmbeddedObjCollector : public AccCollector
{
public:
  virtual ~EmbeddedObjCollector() { }

public:
  virtual int32_t GetIndexAt(Accessible* aAccessible);

protected:
  // Make sure it's used by Accessible class only.
  EmbeddedObjCollector(Accessible* aRoot) :
    AccCollector(aRoot, filters::GetEmbeddedObject) { }

  virtual void AppendObject(Accessible* aAccessible);

  friend class Accessible;
};

} // namespace a11y
} // namespace mozilla

#endif
