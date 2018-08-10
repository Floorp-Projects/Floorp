/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_MEMORYPRESSUREOBSERVER_H
#define MOZILLA_LAYERS_MEMORYPRESSUREOBSERVER_H

#include "nsIObserver.h"

namespace mozilla {
namespace layers {

// A simple memory pressure observer implementation born out of the realization
// that almost all of our memory pressure observers do exactly the same thing.
//
// The intended way to use it is to have the class that nees to react on memory
// pressure inherit the MemoryPressureListener interface and own a strong
// reference to a MemoryPressureListener object.
// Call Unregister on the listener in the destructor of your class or whenever
// you do not which to receive the notification anymore, otherwise the listener
// will be held alive by the observer service (leak) and keep a dangling pointer
// to your class.

/// See nsIMemory.idl
enum class MemoryPressureReason {
    LOW_MEMORY,
    LOW_MEMORY_ONGOING,
    HEAP_MINIMIZE,
};

class MemoryPressureListener {
public:
  virtual void OnMemoryPressure(MemoryPressureReason aWhy) = 0;
};

class MemoryPressureObserver final : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Returns null if anything goes wrong.
  static already_AddRefed<MemoryPressureObserver>
  Create(MemoryPressureListener* aListener);

  void Unregister();

private:
  explicit MemoryPressureObserver(MemoryPressureListener* aListener);
  virtual ~MemoryPressureObserver();
  MemoryPressureListener* mListener;
};

} // namespace layers
} // namespace mozilla

#endif
