/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerCloneData_h__
#define mozilla_dom_ServiceWorkerCloneData_h__

#include "mozilla/dom/ipc/StructuredCloneData.h"

namespace mozilla {
namespace dom {

// Helper class used to pack structured clone data so that it can be
// passed across thread and process boundaries.  Currently the raw
// StructuredCloneData and StructureCloneHolder APIs both make it
// difficult to meet this needs directly.  This helper class improves
// the situation by:
//
//  1. Provides a ref-counted version of StructuredCloneData.  We need
//     StructuredCloneData so we can serialize/deserialize across IPC.
//     The move constructor problems in StructuredCloneData (bug 1462676),
//     though, makes it hard to pass it around.  Passing a ref-counted
//     pointer addresses this problem.
//  2. Normally StructuredCloneData runs into problems if you try to move
//     it across thread boundaries because it releases its SharedJSAllocatedData
//     on the wrong thread.  This helper will correctly proxy release the
//     shared data on the correct thread.
//
// This helper class should really just be used to serialize on one thread
// and then move the reference across thread/process boundries to the
// target worker thread.  This class is not intended to support simultaneous
// read/write operations from different threads at the same time.
class ServiceWorkerCloneData final : public ipc::StructuredCloneData
{
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  ~ServiceWorkerCloneData();

public:
  ServiceWorkerCloneData();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ServiceWorkerCloneData)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ServiceWorkerCloneData_h__
