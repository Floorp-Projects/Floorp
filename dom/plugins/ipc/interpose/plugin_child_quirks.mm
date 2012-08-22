/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <sys/sysctl.h>
#import "CoreFoundation/CoreFoundation.h"
#import "CoreServices/CoreServices.h"
#import "Carbon/Carbon.h"
#define int32_t int32_t
#define uint32_t uint32_t

#include "MacQuirks.h"

int static_init() {
  TriggerQuirks();
  return 0;
}
int run = static_init();
