/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TemporalFields_h
#define builtin_temporal_TemporalFields_h

#include "jstypes.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

namespace js::temporal {

[[nodiscard]] bool SortTemporalFieldNames(
    JSContext* cx, JS::StackGCVector<JS::PropertyKey>& fieldNames);

} /* namespace js::temporal */

#endif /* builtin_temporal_TemporalFields_h */
