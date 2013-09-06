/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AtomList_h
#define mozilla_dom_AtomList_h

#include "jsapi.h"
#include "mozilla/dom/GeneratedAtomList.h"

namespace mozilla {
namespace dom {

template<class T>
T* GetAtomCache(JSContext* aCx)
{
  JSRuntime* rt = JS_GetRuntime(aCx);

  auto atomCache = static_cast<PerThreadAtomCache*>(JS_GetRuntimePrivate(rt));

  return static_cast<T*>(atomCache);
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AtomList_h
