/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Rooting_h
#define gc_Rooting_h

#include "gc/Allocator.h"
#include "gc/Policy.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace js {

using FunctionVector = JS::GCVector<JSFunction*>;
using StringVector = JS::GCVector<JSString*>;

} /* namespace js */

#endif /* gc_Rooting_h */
