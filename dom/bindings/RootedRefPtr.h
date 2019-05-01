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
template <typename T>
struct GCPolicy<RefPtr<T>> {
  static RefPtr<T> initial() { return RefPtr<T>(); }

  static void trace(JSTracer* trc, RefPtr<T>* tp, const char* name) {
    if (*tp) {
      (*tp)->Trace(trc);
    }
  }

  static bool isValid(const RefPtr<T>& v) {
    return !v || GCPolicy<T>::isValid(*v.get());
  }
};
}  // namespace JS

namespace js {
template <typename T, typename Wrapper>
struct WrappedPtrOperations<RefPtr<T>, Wrapper> {
  operator T*() const { return static_cast<const Wrapper*>(this)->get(); }
};
}  // namespace js

#endif /* mozilla_RootedRefPtr_h__ */
