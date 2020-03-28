/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLifetimes_h_
#define DDLifetimes_h_

#include <type_traits>

#include "DDLifetime.h"
#include "DDLoggedTypeTraits.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"

namespace mozilla {

// Managed list of lifetimes.
class DDLifetimes {
 public:
  // DDLifetime for a given aObject, that exists at the aIndex time;
  // otherwise nullptr.
  DDLifetime* FindLifetime(const DDLogObject& aObject,
                           const DDMessageIndex& aIndex);

  // DDLifetime for a given aObject, that exists at the aIndex time;
  // otherwise nullptr.
  const DDLifetime* FindLifetime(const DDLogObject& aObject,
                                 const DDMessageIndex& aIndex) const;

  // Create a lifetime with the given object and construction index&time.
  DDLifetime& CreateLifetime(const DDLogObject& aObject, DDMessageIndex aIndex,
                             const DDTimeStamp& aConstructionTimeStamp);

  // Remove an existing lifetime (assumed to just have been found by
  // FindLifetime()).
  void RemoveLifetime(const DDLifetime* aLifetime);

  // Remove all lifetimes associated with the given HTMLMediaElement.
  void RemoveLifetimesFor(const dom::HTMLMediaElement* aMediaElement);

  // Remove all lifetimes.
  void Clear();

  // Visit all lifetimes associated with an HTMLMediaElement and run
  // `aF(const DDLifetime&)` on each one.
  // If aOnlyHTMLMediaElement is true, only run aF once of that element.
  template <typename F>
  void Visit(const dom::HTMLMediaElement* aMediaElement, F&& aF,
             bool aOnlyHTMLMediaElement = false) const {
    for (auto iter = mLifetimes.ConstIter(); !iter.Done(); iter.Next()) {
      for (const DDLifetime& lifetime : *iter.UserData()) {
        if (lifetime.mMediaElement == aMediaElement) {
          if (aOnlyHTMLMediaElement) {
            if (lifetime.mObject.Pointer() == aMediaElement &&
                lifetime.mObject.TypeName() ==
                    DDLoggedTypeTraits<dom::HTMLMediaElement>::Name()) {
              aF(lifetime);
              break;
            }
            continue;
          }
          static_assert(std::is_same_v<decltype(aF(lifetime)), void>, "");
          aF(lifetime);
        }
      }
    }
  }

  // Visit all lifetimes associated with an HTMLMediaElement and run
  // `aF(const DDLifetime&)` on each one.
  // If aF() returns false, the loop continues.
  // If aF() returns true, the loop stops, and true is returned immediately.
  // If all aF() calls have returned false, false is returned at the end.
  template <typename F>
  bool VisitBreakable(const dom::HTMLMediaElement* aMediaElement,
                      F&& aF) const {
    for (auto iter = mLifetimes.ConstIter(); !iter.Done(); iter.Next()) {
      for (const DDLifetime& lifetime : *iter.UserData()) {
        if (lifetime.mMediaElement == aMediaElement) {
          static_assert(std::is_same_v<decltype(aF(lifetime)), bool>, "");
          if (aF(lifetime)) {
            return true;
          }
        }
      }
    }
    return false;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

 private:
  // Hashtable key to use for each DDLogObject.
  class DDLogObjectHashKey : public PLDHashEntryHdr {
   public:
    typedef const DDLogObject& KeyType;
    typedef const DDLogObject* KeyTypePointer;

    explicit DDLogObjectHashKey(KeyTypePointer aKey) : mValue(*aKey) {}
    DDLogObjectHashKey(const DDLogObjectHashKey& aToCopy)
        : mValue(aToCopy.mValue) {}
    ~DDLogObjectHashKey() = default;

    KeyType GetKey() const { return mValue; }
    bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

    static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
    static PLDHashNumber HashKey(KeyTypePointer aKey) {
      return HashBytes(aKey, sizeof(DDLogObject));
    }
    enum { ALLOW_MEMMOVE = true };

   private:
    const DDLogObject mValue;
  };

  // Array of all DDLifetimes for a given DDLogObject; they should be
  // distinguished by their construction&destruction times.
  using LifetimesForObject = nsTArray<DDLifetime>;

  // For each DDLogObject, we store an array of all objects that have used this
  // pointer and type.
  nsClassHashtable<DDLogObjectHashKey, LifetimesForObject> mLifetimes;
};

}  // namespace mozilla

#endif  // DDLifetimes_h_
