/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_RefCounted_h
#define js_RefCounted_h

#include "mozilla/RefCounted.h"

#include "js/Utility.h"

namespace js {

// Replacement for mozilla::RefCounted and mozilla::external::AtomicRefCounted
// that default to JS::DeletePolicy.

template <typename T, typename D = JS::DeletePolicy<T>>
using RefCounted = mozilla::RefCounted<T, D>;

template <typename T, typename D = JS::DeletePolicy<T>>
using AtomicRefCounted = mozilla::external::AtomicRefCounted<T, D>;

} // namespace js

#endif /* js_RefCounted_h */
