/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_GonkNativeHandleUtils_h
#define IPC_GonkNativeHandleUtils_h

#include "ipc/IPCMessageUtils.h"

#include "GonkNativeHandle.h"

namespace IPC {

#ifdef MOZ_WIDGET_GONK
template <>
struct ParamTraits<mozilla::layers::GonkNativeHandle> {
  typedef mozilla::layers::GonkNativeHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
};
#else
template <>
struct ParamTraits<mozilla::layers::GonkNativeHandle> {
  typedef mozilla::layers::GonkNativeHandle paramType;
  static void Write(Message*, const paramType&) {}
  static bool Read(const Message*, void**, paramType*) { return false; }
};
#endif

} // namespace IPC

#endif // IPC_GonkNativeHandleUtils_h
