/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_USERDATA_H_
#define MOZILLA_GFX_USERDATA_H_

#include <stdlib.h>
#include "mozilla/Assertions.h"

namespace mozilla {
namespace gfx {

struct UserDataKey {
  int unused;
};

/* this class is basically a clone of the user data concept from cairo */
class UserData
{
  typedef void (*destroyFunc)(void *data);
public:
  UserData() : count(0), entries(NULL) {}

  /* Attaches untyped userData associated with key. destroy is called on destruction */
  void Add(UserDataKey *key, void *userData, destroyFunc destroy)
  {
    // XXX we should really warn if user data with key has already been added,
    // since in that case Get() will return the old user data!

    // We could keep entries in a std::vector instead of managing it by hand
    // but that would propagate an stl dependency out which we'd rather not
    // do (see bug 666609). Plus, the entries array is expect to stay small
    // so doing a realloc everytime we add a new entry shouldn't be too costly
    entries = static_cast<Entry*>(realloc(entries, sizeof(Entry)*(count+1)));

    if (!entries) {
      MOZ_CRASH();
    }

    entries[count].key      = key;
    entries[count].userData = userData;
    entries[count].destroy  = destroy;

    count++;
  }

  /* Remove and return user data associated with key, without destroying it */
  void* Remove(UserDataKey *key)
  {
    for (int i=0; i<count; i++) {
      if (key == entries[i].key) {
        void *userData = entries[i].userData;
        // decrement before looping so entries[i+1] doesn't read past the end:
        --count;
        for (;i<count; i++) {
          entries[i] = entries[i+1];
        }
        return userData;
      }
    }
    return NULL;
  }

  /* Retrives the userData for the associated key */
  void *Get(UserDataKey *key)
  {
    for (int i=0; i<count; i++) {
      if (key == entries[i].key) {
        return entries[i].userData;
      }
    }
    return NULL;
  }

  ~UserData()
  {
    for (int i=0; i<count; i++) {
      entries[i].destroy(entries[i].userData);
    }
    free(entries);
  }

private:
  struct Entry {
    const UserDataKey *key;
    void *userData;
    destroyFunc destroy;
  };

  int count;
  Entry *entries;

};

}
}

#endif /* MOZILLA_GFX_USERDATA_H_ */
