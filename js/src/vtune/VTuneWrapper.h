/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vtunewrapper_h
#define vtunewrapper_h

#include "vtune/jitprofiling.h"

inline bool
IsVTuneProfilingActive()
{
    return iJIT_IsProfilingActive() == iJIT_SAMPLING_ON;
}

#endif // vtunewrapper_h
