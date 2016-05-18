/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An implementation of Rooted for RefPtr<T>.  This works by assuming that T has
 * a Trace() method defined on it which will trace whatever things inside the T
 * instance need tracing.
 *
 * This implementation has one serious drawback: operator= doesn't work right
 * because it's declared on Rooted directly and expects the type Rooted is
 * templated over.
 */

#ifndef mozilla_RootedRefPtr_h__
#define mozilla_RootedRefPtr_h__

#include "mozilla/RefPtr.h"
#include "js/GCPolicyAPI.h"
#include "js/RootingAPI.h"

namespace JS {
template<typename T>
struct GCPolicy<RefPtr<T>>
{
  static RefPtr<T> initial() {
    return RefPtr<T>();
  }

  static void trace(JSTracer* trc, RefPtr<T>* tp, const char* name)
  {
    if (*tp) {
      (*tp)->Trace(trc);
    }
  }
};
} // namespace JS

namespace js {
template<typename T>
struct RootedBase<RefPtr<T>>
{
  operator RefPtr<T>& () const
  {
    auto& self = *static_cast<const JS::Rooted<RefPtr<T>>*>(this);
    return self.get();
  }

  operator T*() const
  {
    auto& self = *static_cast<const JS::Rooted<RefPtr<T>>*>(this);
    return self.get();
  }
};
} // namespace js

#endif /* mozilla_RootedRefPtr_h__ */
