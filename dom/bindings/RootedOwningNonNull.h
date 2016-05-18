/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An implementation of Rooted for OwningNonNull<T>.  This works by assuming
 * that T has a Trace() method defined on it which will trace whatever things
 * inside the T instance need tracing.
 *
 * This implementation has one serious drawback: operator= doesn't work right
 * because it's declared on Rooted directly and expects the type Rooted is
 * templated over.
 */

#ifndef mozilla_RootedOwningNonNull_h__
#define mozilla_RootedOwningNonNull_h__

#include "mozilla/OwningNonNull.h"
#include "js/GCPolicyAPI.h"
#include "js/RootingAPI.h"

namespace JS {
template<typename T>
struct GCPolicy<mozilla::OwningNonNull<T>>
{
  typedef mozilla::OwningNonNull<T> OwningNonNull;

  static OwningNonNull initial()
  {
    return OwningNonNull();
  }

  static void trace(JSTracer* trc, OwningNonNull* tp,
                    const char* name)
  {
    // We have to be very careful here.  Normally, OwningNonNull can't be null.
    // But binding code can end up in a situation where it sets up a
    // Rooted<OwningNonNull> and then before it gets a chance to assign to it
    // (e.g. from the constructor of the thing being assigned) a GC happens.  So
    // we can land here when *tp stores a null pointer because it's not
    // initialized.
    //
    // So we need to check for that before jumping.
    if ((*tp).isInitialized()) {
      (*tp)->Trace(trc);
    }
  }
};
} // namespace JS

namespace js {
template<typename T>
struct RootedBase<mozilla::OwningNonNull<T>>
{
  typedef mozilla::OwningNonNull<T> OwningNonNull;

  operator OwningNonNull& () const
  {
    auto& self = *static_cast<const JS::Rooted<OwningNonNull>*>(this);
    return self.get();
  }

  operator T& () const
  {
    auto& self = *static_cast<const JS::Rooted<OwningNonNull>*>(this);
    return self.get();
  }
};
} // namespace js

#endif /* mozilla_RootedOwningNonNull_h__ */
