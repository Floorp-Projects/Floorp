/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaAccessible_h_
#define mozilla_a11y_MsaaAccessible_h_

#include "ia2Accessible.h"
#include "ia2AccessibleComponent.h"
#include "ia2AccessibleHyperlink.h"
#include "ia2AccessibleValue.h"

namespace mozilla {
namespace a11y {

class MsaaAccessible : public ia2Accessible,
                       public ia2AccessibleComponent,
                       public ia2AccessibleHyperlink,
                       public ia2AccessibleValue {};

}  // namespace a11y
}  // namespace mozilla

#ifdef XP_WIN
// Undo the windows.h damage
#  undef GetMessage
#  undef CreateEvent
#  undef GetClassName
#  undef GetBinaryType
#  undef RemoveDirectory
#endif

#endif
