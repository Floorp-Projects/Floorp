/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_USERDATA_H_
#define MOZILLA_GFX_USERDATA_H_

#include <stdlib.h>
#include "Types.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace gfx {

struct UserDataKey {
  int unused;
};

/* this class is basically a clone of the user data concept from cairo */
class UserData {
 public:
  typedef void (*DestroyFunc)(void* data);

  UserData() : count(0), entries(nullptr) {}

  /* Attaches untyped userData associated with key. destroy is called on
   * destruction */
  void Add(UserDataKey* key, void* userData, DestroyFunc destroy) {
    for (int i = 0; i < count; i++) {
      if (key == entries[i].key) {
        if (entries[i].destroy) {
          entries[i].destroy(entries[i].userData);
        }
        entries[i].userData = userData;
        entries[i].destroy = destroy;
        return;
      }
    }

    // We could keep entries in a std::vector instead of managing it by hand
    // but that would propagate an stl dependency out which we'd rather not
    // do (see bug 666609). Plus, the entries array is expect to stay small
    // so doing a realloc everytime we add a new entry shouldn't be too costly
    entries =
        static_cast<Entry*>(realloc(entries, sizeof(Entry) * (count + 1)));

    if (!entries) {
      MOZ_CRASH("GFX: UserData::Add");
    }

    entries[count].key = key;
    entries[count].userData = userData;
    entries[count].destroy = destroy;

    count++;
  }

  /* Remove and return user data associated with key, without destroying it */
  void* Remove(UserDataKey* key) {
    for (int i = 0; i < count; i++) {
      if (key == entries[i].key) {
        void* userData = entries[i].userData;
        // decrement before looping so entries[i+1] doesn't read past the end:
        --count;
        for (; i < count; i++) {
          entries[i] = entries[i + 1];
        }
        return userData;
      }
    }
    return nullptr;
  }

  /* Remove and destroy a given key */
  void RemoveAndDestroy(UserDataKey* key) {
    for (int i = 0; i < count; i++) {
      if (key == entries[i].key) {
        if (entries[i].destroy) {
          entries[i].destroy(entries[i].userData);
        }
        // decrement before looping so entries[i+1] doesn't read past the end:
        --count;
        for (; i < count; i++) {
          entries[i] = entries[i + 1];
        }
      }
    }
  }

  /* Retrives the userData for the associated key */
  void* Get(UserDataKey* key) const {
    for (int i = 0; i < count; i++) {
      if (key == entries[i].key) {
        return entries[i].userData;
      }
    }
    return nullptr;
  }

  bool Has(UserDataKey* key) {
    for (int i = 0; i < count; i++) {
      if (key == entries[i].key) {
        return true;
      }
    }
    return false;
  }

  void Destroy() {
    if (!entries) {
      return;
    }
    for (int i = 0; i < count; i++) {
      if (entries[i].destroy) {
        entries[i].destroy(entries[i].userData);
      }
    }
    free(entries);
    entries = nullptr;
    count = 0;
  }

  ~UserData() { Destroy(); }

 private:
  struct Entry {
    const UserDataKey* key;
    void* userData;
    DestroyFunc destroy;
  };

  int count;
  Entry* entries;
};

class ThreadSafeUserData {
 protected:
  struct LockedUserData : public UserData {
    Mutex mLock;

    LockedUserData() : mLock("LockedUserData::mLock") {}
  };

 public:
  ~ThreadSafeUserData() {
    if (LockedUserData* userData = mUserData.exchange(nullptr)) {
      {
        MutexAutoLock lock(userData->mLock);
        userData->Destroy();
      }
      delete userData;
    }
  }

  void Add(UserDataKey* key, void* value, UserData::DestroyFunc destroy) {
    LockedUserData* userData = GetUserData();
    MutexAutoLock lock(userData->mLock);
    userData->Add(key, value, destroy);
  }

  void* Remove(UserDataKey* key) {
    LockedUserData* userData = GetUserData();
    MutexAutoLock lock(userData->mLock);
    return userData->Remove(key);
  }

  void RemoveAndDestroy(UserDataKey* key) {
    LockedUserData* userData = GetUserData();
    MutexAutoLock lock(userData->mLock);
    userData->RemoveAndDestroy(key);
  }

  void* Get(UserDataKey* key) const {
    LockedUserData* userData = GetUserData();
    MutexAutoLock lock(userData->mLock);
    return userData->Get(key);
  }

  bool Has(UserDataKey* key) {
    LockedUserData* userData = GetUserData();
    MutexAutoLock lock(userData->mLock);
    return userData->Has(key);
  }

 private:
  LockedUserData* GetUserData() const {
    LockedUserData* userData = mUserData;
    if (!userData) {
      userData = new LockedUserData;
      if (!mUserData.compareExchange(nullptr, userData)) {
        delete userData;
        userData = mUserData;
        MOZ_ASSERT(userData);
      }
    }
    return userData;
  }

  // The Mutex class is quite large. For small, frequent classes (ScaledFont,
  // SourceSurface, etc.) this can add a lot of memory overhead, especially if
  // UserData is only infrequently used. To avoid this, we only allocate the
  // LockedUserData if it is actually used. If unused, it only adds a single
  // pointer as overhead.
  mutable Atomic<LockedUserData*> mUserData;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_USERDATA_H_ */
