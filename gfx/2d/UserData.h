/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (Sub) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Muizelaar <jmuizelaar@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_GFX_USERDATA_H_
#define MOZILLA_GFX_USERDATA_H_

#include <stdlib.h>
#include "mozilla/mozalloc.h"

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
    entries = static_cast<Entry*>(moz_xrealloc(entries, sizeof(Entry)*(count+1)));

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
