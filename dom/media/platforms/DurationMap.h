/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DurationMap_h
#define mozilla_DurationMap_h

#include "mozilla/Pair.h"
#include "nsTArray.h"

namespace mozilla {

class DurationMap
{
public:
  typedef Pair<int64_t, int64_t> DurationElement;

  DurationMap() : mMutex("DurationMap") { }

  // Insert Key and Duration pair at the end of our map.
  void Insert(int64_t aKey, int64_t aDuration)
  {
    MutexAutoLock lock(mMutex);
    mMap.AppendElement(MakePair(aKey, aDuration));
  }
  // Sets aDuration matching aKey and remove it from the map if found.
  // The element returned is the first one found.
  // Returns true if found, false otherwise.
  bool Find(int64_t aKey, int64_t& aDuration)
  {
    MutexAutoLock lock(mMutex);
    for (uint32_t i = 0; i < mMap.Length(); i++) {
      DurationElement& element = mMap[i];
      if (element.first() == aKey) {
        aDuration = element.second();
        mMap.RemoveElementAt(i);
        return true;
      }
    }
    return false;
  }
  // Remove all elements of the map.
  void Clear()
  {
    MutexAutoLock lock(mMutex);
    mMap.Clear();
  }

private:
  Mutex mMutex; // To protect mMap.
  AutoTArray<DurationElement, 16> mMap;
};

} // namespace mozilla

#endif // mozilla_DurationMap_h
