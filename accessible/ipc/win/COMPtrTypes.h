/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_COMPtrTypes_h
#define mozilla_a11y_COMPtrTypes_h

#include "mozilla/mscom/COMPtrHolder.h"

#include <oleacc.h>

namespace mozilla {
namespace a11y {

typedef mozilla::mscom::COMPtrHolder<IAccessible, IID_IAccessible> IAccessibleHolder;

class Accessible;

IAccessibleHolder
CreateHolderFromAccessible(Accessible* aAccToWrap);

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_COMPtrTypes_h
