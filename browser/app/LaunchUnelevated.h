/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LaunchUnelevated_h
#define mozilla_LaunchUnelevated_h

#include "mozilla/Maybe.h"

namespace mozilla {

mozilla::Maybe<bool> IsElevated();
bool LaunchUnelevated(int aArgc, wchar_t* aArgv[]);

} // namespace mozilla

#endif // mozilla_LaunchUnelevated_h

