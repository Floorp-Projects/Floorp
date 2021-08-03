/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ShmemMessageUtils_h
#define mozilla_ipc_ShmemMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<Shmem> {
  typedef Shmem paramType;

  // NB: std::move(Shmem) is a /copy/.
  // It would be nice to make a Shmem a move-only class, but the ipdlh
  // generator can't handle those yet. One solution would be to use a dumb
  // struct/descriptor for ipdlh purposes. Arguably this dumb non-move-only
  // struct *is* Shmem.
  // Because Shmem is not move-only, when previous versions of this code
  // required Shmem&& here, it caused devs to assume that Shmem was move-aware,
  // when it is not. People would then cargo-cult std::move(shmem), even though
  // that's a copy. This can be a sign that people are getting the wrong idea
  // about how to use it, and when things happen. In particular,
  // `std::move(shmem)` leaves the strong references in `shmem` intact, and we
  // should avoid causing readers to believe otherwise.
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult);

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(L"(shmem segment)");
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_ShmemMessageUtils_h
