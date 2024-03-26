/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ExtensionKitUtils_h
#define mozilla_ipc_ExtensionKitUtils_h

#include <functional>
#include <xpc/xpc.h>
#include "mozilla/DarwinObjectPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/LaunchError.h"

namespace mozilla::ipc {

class BEProcessCapabilityGrantDeleter {
 public:
  void operator()(void* aGrant) const;
};

using UniqueBEProcessCapabilityGrant =
    mozilla::UniquePtr<void, BEProcessCapabilityGrantDeleter>;

class ExtensionKitProcess {
 public:
  enum class Kind {
    WebContent,
    Networking,
    Rendering,
  };

  // Called to start the process. The `aCompletion` function may be executed on
  // a background libdispatch thread.
  static void StartProcess(
      Kind aKind,
      const std::function<void(Result<ExtensionKitProcess, LaunchError>&&)>&
          aCompletion);

  // Get the kind of process being started.
  Kind GetKind() const { return mKind; }

  // Make an xpc_connection_t to this process. If an error is encountered,
  // `aError` will be populated with the error.
  //
  // Ownership over the newly created connection is returned to the caller.
  // The connection is returned in a suspended state, and must be resumed.
  DarwinObjectPtr<xpc_connection_t> MakeLibXPCConnection();

  UniqueBEProcessCapabilityGrant GrantForegroundCapability();

  // Invalidate the process, indicating that it should be cleaned up &
  // destroyed.
  void Invalidate();

  // Explicit copy constructors
  ExtensionKitProcess(const ExtensionKitProcess&);
  ExtensionKitProcess& operator=(const ExtensionKitProcess&);

  // Release the object when completed.
  ~ExtensionKitProcess();

 private:
  ExtensionKitProcess(Kind aKind, void* aProcessObject)
      : mKind(aKind), mProcessObject(aProcessObject) {}

  // Type tag for `mProcessObject`.
  Kind mKind;

  // This is one of `BEWebContentProcess`, `BENetworkingProcess` or
  // `BERenderingProcess`. It has been type erased to be usable from C++ code.
  void* mProcessObject;
};

}  // namespace mozilla::ipc

#endif  // mozilla_ipc_ExtensionKitUtils_h
