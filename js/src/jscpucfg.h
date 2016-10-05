/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscpucfg_h
#define jscpucfg_h

#include "mozilla/EndianUtils.h"

#ifndef JS_STACK_GROWTH_DIRECTION
# ifdef __hppa
#  define JS_STACK_GROWTH_DIRECTION (1)
# else
#  define JS_STACK_GROWTH_DIRECTION (-1)
# endif
#endif

#endif /* jscpucfg_h */
