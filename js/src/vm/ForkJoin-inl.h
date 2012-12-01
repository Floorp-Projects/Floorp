/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ForkJoin_inl_h__
#define ForkJoin_inl_h__

namespace js {

inline ForkJoinSlice *
ForkJoinSlice::current()
{
#ifdef JS_THREADSAFE_ION
    return (ForkJoinSlice*) PR_GetThreadPrivate(ThreadPrivateIndex);
#else
    return NULL;
#endif
}

// True if this thread is currently executing a parallel operation across
// multiple threads.
static inline bool
InParallelSection()
{
#ifdef JS_THREADSAFE
    return ForkJoinSlice::current() != NULL;
#else
    return false;
#endif
}

} // namespace js

#endif // ForkJoin_inl_h__
