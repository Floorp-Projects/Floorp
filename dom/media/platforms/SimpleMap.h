/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SimpleMap_h
#define mozilla_SimpleMap_h

#include "mozilla/Pair.h"
#include "nsTArray.h"

namespace mozilla {

template<typename T>
class SimpleMap
{
public:
  typedef Pair<int64_t, T> Element;

  SimpleMap() : mMutex("SimpleMap") { }

  // Insert Key and Value pair at the end of our map.
  void Insert(int64_t aKey, const T& aValue)
  {
    MutexAutoLock lock(mMutex);
    mMap.AppendElement(MakePair(aKey, aValue));
  }
  // Sets aValue matching aKey and remove it from the map if found.
  // The element returned is the first one found.
  // Returns true if found, false otherwise.
  bool Find(int64_t aKey, T& aValue)
  {
    MutexAutoLock lock(mMutex);
    for (uint32_t i = 0; i < mMap.Length(); i++) {
      Element& element = mMap[i];
      if (element.first() == aKey) {
        aValue = element.second();
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
  AutoTArray<Element, 16> mMap;
};

} // namespace mozilla

#endif // mozilla_SimpleMap_h
