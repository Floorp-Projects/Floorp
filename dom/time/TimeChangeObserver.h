/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_time_change_observer_h_
#define _mozilla_time_change_observer_h_

#include "nscore.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace time {

nsresult AddWindowListener(nsPIDOMWindowInner* aWindow);
nsresult RemoveWindowListener(nsPIDOMWindowInner* aWindow);

} // namespace time
} // namespace mozilla

#endif //_mozilla_time_change_observer_h_
