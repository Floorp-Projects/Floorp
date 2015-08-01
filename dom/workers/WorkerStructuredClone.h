/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerStructuredClone_h
#define mozilla_dom_workers_WorkerStructuredClone_h

#include "Workers.h"
#include "mozilla/dom/PMessagePort.h"

class nsPIDOMWindow;

namespace mozilla {
namespace layers {
class Image;
}

namespace dom {

class MessagePortBase;

namespace workers {

// This class is implemented in WorkerPrivate.cpp
class WorkerStructuredCloneClosure final
{
private:
  WorkerStructuredCloneClosure(const WorkerStructuredCloneClosure&) = delete;
  WorkerStructuredCloneClosure & operator=(const WorkerStructuredCloneClosure&) = delete;

public:
  WorkerStructuredCloneClosure();
  ~WorkerStructuredCloneClosure();

  void Clear();

  // This can be null if the MessagePort is created in a worker.
  nsCOMPtr<nsPIDOMWindow> mParentWindow;

  nsTArray<nsCOMPtr<nsISupports>> mClonedObjects;

  // This is used for sharing the backend of ImageBitmaps.
  // The layers::Image object must be thread-safely reference-counted.
  // The layers::Image object will not be written ever via any ImageBitmap
  // instance, so no race condition will occur.
  nsTArray<nsRefPtr<layers::Image>> mClonedImages;

  // The transferred ports.
  nsTArray<nsRefPtr<MessagePortBase>> mMessagePorts;

  // Information for the transferring.
  nsTArray<MessagePortIdentifier> mMessagePortIdentifiers;

  // To avoid duplicates in the transferred ports.
  nsTArray<nsRefPtr<MessagePortBase>> mTransferredPorts;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_WorkerStructuredClone_h
