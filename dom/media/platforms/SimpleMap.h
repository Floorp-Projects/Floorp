/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SimpleMap_h
#define mozilla_SimpleMap_h

#include <utility>

#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"

namespace mozilla {

struct ThreadSafePolicy {
  struct PolicyLock {
    explicit PolicyLock(const char* aName) : mMutex(aName) {}
    Mutex mMutex MOZ_UNANNOTATED;
  };
  PolicyLock& mPolicyLock;
  explicit ThreadSafePolicy(PolicyLock& aPolicyLock)
      : mPolicyLock(aPolicyLock) {
    mPolicyLock.mMutex.Lock();
  }
  ~ThreadSafePolicy() { mPolicyLock.mMutex.Unlock(); }
};

struct NoOpPolicy {
  struct PolicyLock {
    explicit PolicyLock(const char*) {}
  };
  explicit NoOpPolicy(PolicyLock&) {}
  ~NoOpPolicy() = default;
};

// An map employing an array instead of a hash table to optimize performance,
// particularly beneficial when the number of expected items in the map is
// small.
template <typename K, typename V, typename Policy = NoOpPolicy>
class SimpleMap {
  using ElementType = std::pair<K, V>;
  using MapType = AutoTArray<ElementType, 16>;

 public:
  SimpleMap() : mLock("SimpleMap"){};

  // Check if aKey is in the map.
  bool Contains(const K& aKey) {
    struct Comparator {
      bool Equals(const ElementType& aElement, const K& aKey) const {
        return aElement.first == aKey;
      }
    };
    Policy guard(mLock);
    return mMap.Contains(aKey, Comparator());
  }
  // Insert Key and Value pair at the end of our map.
  void Insert(const K& aKey, const V& aValue) {
    Policy guard(mLock);
    mMap.AppendElement(std::make_pair(aKey, aValue));
  }
  // Sets aValue matching aKey and remove it from the map if found.
  // The element returned is the first one found.
  // Returns true if found, false otherwise.
  bool Find(const K& aKey, V& aValue) {
    if (Maybe<V> v = Take(aKey)) {
      aValue = v.extract();
      return true;
    }
    return false;
  }
  // Take the value matching aKey and remove it from the map if found.
  Maybe<V> Take(const K& aKey) {
    Policy guard(mLock);
    for (uint32_t i = 0; i < mMap.Length(); i++) {
      ElementType& element = mMap[i];
      if (element.first == aKey) {
        Maybe<V> value = Some(element.second);
        mMap.RemoveElementAt(i);
        return value;
      }
    }
    return Nothing();
  }
  // Remove all elements of the map.
  void Clear() {
    Policy guard(mLock);
    mMap.Clear();
  }
  // Iterate through all elements of the map and call the function F.
  template <typename F>
  void ForEach(F&& aCallback) {
    Policy guard(mLock);
    for (const auto& element : mMap) {
      aCallback(element.first, element.second);
    }
  }

 private:
  typename Policy::PolicyLock mLock;
  MapType mMap;
};

}  // namespace mozilla

#endif  // mozilla_SimpleMap_h
