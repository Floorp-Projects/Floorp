// vim:set ts=4 sts=2 sw=2 et cin:
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxBaseSharedMemorySurface.h"
#include "cairo.h"

const cairo_user_data_key_t SHM_KEY = {0};
