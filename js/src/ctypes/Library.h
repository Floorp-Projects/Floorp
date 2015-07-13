/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ctypes_Library_h
#define ctypes_Library_h

#include "js/TypeDecls.h"

struct JSCTypesCallbacks;
struct PRLibrary;

namespace js {
namespace ctypes {

enum LibrarySlot {
  SLOT_LIBRARY = 0,
  LIBRARY_SLOTS
};

namespace Library
{
  bool Name(JSContext* cx, unsigned argc, JS::Value* vp);

  JSObject* Create(JSContext* cx, JS::Value path, const JSCTypesCallbacks* callbacks);

  bool IsLibrary(JSObject* obj);
  PRLibrary* GetLibrary(JSObject* obj);

  bool Open(JSContext* cx, unsigned argc, JS::Value* vp);
} // namespace Library

} // namespace ctypes
} // namespace js

#endif /* ctypes_Library_h */
