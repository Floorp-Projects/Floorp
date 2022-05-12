/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NonRefcountedDOMObject_h__
#define mozilla_dom_NonRefcountedDOMObject_h__

#include "nsISupportsImpl.h"

namespace mozilla::dom {

// Natives for DOM classes that aren't refcounted need to inherit from this
// class.
// If you're seeing objects of this class leak then natives for one of the DOM
// classes inheriting from it are leaking. If the native for that class has
// MOZ_COUNT_CTOR/DTOR in its constructor/destructor then it should show up in
// the leak log too.
class NonRefcountedDOMObject {
 protected:
  MOZ_COUNTED_DEFAULT_CTOR(NonRefcountedDOMObject)

  MOZ_COUNTED_DTOR(NonRefcountedDOMObject)

  NonRefcountedDOMObject(const NonRefcountedDOMObject& aOther)
      : NonRefcountedDOMObject() {}

  NonRefcountedDOMObject& operator=(const NonRefcountedDOMObject& aOther) {
    NonRefcountedDOMObject();
    return *this;
  }
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_NonRefcountedDOMObject_h__ */
