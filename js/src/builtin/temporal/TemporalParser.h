/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TemporalParser_h
#define builtin_temporal_TemporalParser_h

#include <stdint.h>

#include "js/TypeDecls.h"

class JSLinearString;

namespace js::temporal {

/**
 * ParseTimeZoneOffsetString ( isoString )
 */
bool ParseTimeZoneOffsetString(JSContext* cx, JS::Handle<JSString*> str,
                               int64_t* result);

/**
 * ParseTemporalCalendarString ( isoString )
 */
JSLinearString* ParseTemporalCalendarString(JSContext* cx,
                                            JS::Handle<JSString*> str);

} /* namespace js::temporal */

#endif /* builtin_temporal_TemporalParser_h */
