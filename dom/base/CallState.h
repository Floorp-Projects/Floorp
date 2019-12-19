/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CallState_h
#define mozilla_CallState_h

namespace mozilla {

// An enum class to be used for returned value of callback functions.  If Stop
// is returned from the callback function, caller stops calling further
// children. If Continue is returned then caller will keep calling further
// children.
enum class CallState {
  Continue,
  Stop,
};

}  // namespace mozilla

#endif  // mozilla_CallSate_h
