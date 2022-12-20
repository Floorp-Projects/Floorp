/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSValidatorUtils
#define mozilla_dom_JSValidatorUtils
#include "ErrorList.h"
#include "nsStringFwd.h"

namespace mozilla::ipc {
class IProtocol;
class Shmem;
}  // namespace mozilla::ipc

using namespace mozilla::ipc;

namespace mozilla::dom {
class JSValidatorUtils final {
 public:
  static nsresult CopyCStringToShmem(IProtocol* aActor,
                                     const nsCString& aCString, Shmem& aMem);
};
};  // namespace mozilla::dom
#endif
