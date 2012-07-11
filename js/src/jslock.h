/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef jslock_h__
#define jslock_h__

#include "jsapi.h"

#ifdef JS_THREADSAFE

# include "prlock.h"
# include "prcvar.h"
# include "prthread.h"
# include "prinit.h"

#else  /* JS_THREADSAFE */

typedef struct PRThread PRThread;
typedef struct PRCondVar PRCondVar;
typedef struct PRLock PRLock;

#endif /* JS_THREADSAFE */

#endif /* jslock_h___ */
