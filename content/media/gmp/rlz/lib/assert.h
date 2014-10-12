/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_ASSERT_H_
#define FAKE_ASSERT_H_

#include <assert.h>

#define ASSERT_STRING(x) { assert(false); }
#define VERIFY(x) { assert(x); };

#endif
