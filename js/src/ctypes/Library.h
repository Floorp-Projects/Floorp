/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LIBRARY_H
#define LIBRARY_H

#include "jsapi.h"

struct PRLibrary;

namespace js {
namespace ctypes {

enum LibrarySlot {
  SLOT_LIBRARY = 0,
  LIBRARY_SLOTS
};

namespace Library
{
  JSBool Name(JSContext* cx, unsigned argc, jsval *vp);

  JSObject* Create(JSContext* cx, jsval path, JSCTypesCallbacks* callbacks);

  bool IsLibrary(JSObject* obj);
  PRLibrary* GetLibrary(JSObject* obj);

  JSBool Open(JSContext* cx, unsigned argc, jsval* vp);
}

}
}

#endif
