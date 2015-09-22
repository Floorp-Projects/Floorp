/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_HeapSnapshotTempFileHelperChild_h
#define mozilla_devtools_HeapSnapshotTempFileHelperChild_h

#include "mozilla/devtools/PHeapSnapshotTempFileHelperChild.h"

namespace mozilla {
namespace devtools {

class HeapSnapshotTempFileHelperChild : public PHeapSnapshotTempFileHelperChild
{
    explicit HeapSnapshotTempFileHelperChild() { }

public:
    static inline PHeapSnapshotTempFileHelperChild* Create();
};

/* static */ inline PHeapSnapshotTempFileHelperChild*
HeapSnapshotTempFileHelperChild::Create()
{
    return new HeapSnapshotTempFileHelperChild();
}

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_HeapSnapshotTempFileHelperChild_h
