/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_permissiondelegateipcutils_h
#define mozilla_permissiondelegateipcutils_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/PermissionDelegateHandler.h"

namespace IPC {

template <>
struct ParamTraits<
    mozilla::PermissionDelegateHandler::DelegatedPermissionList> {
  typedef mozilla::PermissionDelegateHandler::DelegatedPermissionList paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    for (auto& permission : aParam.mPermissions) {
      WriteParam(aMsg, permission);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    for (auto& permission : aResult->mPermissions) {
      if (!ReadParam(aMsg, aIter, &permission)) {
        return false;
      }
    }

    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_permissiondelegateipcutils_h
