/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NonRefcountedDOMObject_h__
#define mozilla_dom_NonRefcountedDOMObject_h__

#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

// Natives for DOM classes with 'owned' as the value for nativeOwnership in
// Bindings.conf need to inherit from this class.
// If you're seeing objects of this class leak then natives for one of the DOM
// classes with 'owned' as the value for nativeOwnership in Bindings.conf is
// leaking. If the native for that class has MOZ_COUNT_CTOR/DTOR in its
// constructor/destructor then it should show up in the leak log too.
class NonRefcountedDOMObject
{
protected:
  NonRefcountedDOMObject()
  {
    MOZ_COUNT_CTOR(NonRefcountedDOMObject);
  }
  ~NonRefcountedDOMObject()
  {
    MOZ_COUNT_DTOR(NonRefcountedDOMObject);
  }
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_NonRefcountedDOMObject_h__ */
