/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SerializedStackHolder_h
#define mozilla_dom_SerializedStackHolder_h

#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla {
namespace dom {

// Information about a main or worker thread stack trace that can be accessed
// from either kind of thread. When a worker thread stack is serialized, the
// worker is held alive until this holder is destroyed.
class SerializedStackHolder {
  // Holds any encoded stack data.
  StructuredCloneHolder mHolder;

  // The worker associated with this stack, or null if this is a main thread
  // stack.
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;

  // Write aStack's data into mHolder.
  void WriteStack(JSContext* aCx, JS::HandleObject aStack);

 public:
  SerializedStackHolder();

  // Fill this holder with a main or worklet thread stack.
  void SerializeMainThreadOrWorkletStack(JSContext* aCx,
                                         JS::HandleObject aStack);

  // Fill this holder with a worker thread stack.
  void SerializeWorkerStack(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                            JS::HandleObject aStack);

  // Fill this holder with the current thread's current stack.
  void SerializeCurrentStack(JSContext* aCx);

  // Read back a saved frame stack. This must be called on the main thread.
  // This returns null on failure, and does not leave an exception on aCx.
  JSObject* ReadStack(JSContext* aCx);
};

// Construct a stack for the current thread, which may be consumed by the net
// monitor later on. This may be called on either the main or a worker thread.
//
// This always creates a stack, even if the net monitor isn't active for the
// associated window. The net monitor will only be active if the associated
// docshell or worker's WatchedByDevtools flag is set, so this should be checked
// before creating the stack.
UniquePtr<SerializedStackHolder> GetCurrentStackForNetMonitor(JSContext* aCx);

// If aStackHolder is non-null, this notifies the net monitor that aStackHolder
// is the stack from which aChannel originates. This must be called on the main
// thread. This call is synchronous, and aChannel and aStackHolder will not be
// used afterward. aChannel is an nsISupports object because this can be used
// with either nsIChannel or nsIWebSocketChannel.
void NotifyNetworkMonitorAlternateStack(
    nsISupports* aChannel, UniquePtr<SerializedStackHolder> aStackHolder);

// Read back the saved frame stack and store it in a string as JSON.
// This must be called on the main thread.
void ConvertSerializedStackToJSON(UniquePtr<SerializedStackHolder> aStackHolder,
                                  nsAString& aStackString);

// As above, notify the net monitor for a stack that has already been converted
// to JSON. This can be used with ConvertSerializedStackToJSON when multiple
// notifications might be needed for a single stack.
void NotifyNetworkMonitorAlternateStack(nsISupports* aChannel,
                                        const nsAString& aStackJSON);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SerializedStackHolder_h
