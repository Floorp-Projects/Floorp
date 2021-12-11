/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_HeapSnapshotTempFileHelperParent_h
#define mozilla_devtools_HeapSnapshotTempFileHelperParent_h

#include "mozilla/devtools/PHeapSnapshotTempFileHelperParent.h"

namespace mozilla {
namespace devtools {

class HeapSnapshotTempFileHelperParent
    : public PHeapSnapshotTempFileHelperParent {
  friend class PHeapSnapshotTempFileHelperParent;

  explicit HeapSnapshotTempFileHelperParent() {}
  void ActorDestroy(ActorDestroyReason why) override {}
  mozilla::ipc::IPCResult RecvOpenHeapSnapshotTempFile(
      OpenHeapSnapshotTempFileResponse* outResponse);

 public:
  static inline PHeapSnapshotTempFileHelperParent* Create();
};

/* static */ inline PHeapSnapshotTempFileHelperParent*
HeapSnapshotTempFileHelperParent::Create() {
  return new HeapSnapshotTempFileHelperParent();
}

}  // namespace devtools
}  // namespace mozilla

#endif  // mozilla_devtools_HeapSnapshotTempFileHelperParent_h
