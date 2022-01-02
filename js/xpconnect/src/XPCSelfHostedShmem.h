/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcselfhostedshmem_h___
#define xpcselfhostedshmem_h___

#include "base/shared_memory.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Span.h"
#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIThread.h"

namespace xpc {

// This class is a singleton which holds a file-mapping of the Self Hosted
// Stencil XDR, to be shared by the parent process with all the child processes.
//
// It is either initialized by the parent process by monitoring when the Self
// hosted stencil is produced, or by the content process by reading a shared
// memory-mapped file.
class SelfHostedShmem final : public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  // NOTE: This type is identical to JS::SelfHostedCache, but we repeat it to
  // avoid including JS engine API in ipc code.
  using ContentType = mozilla::Span<const uint8_t>;

  static SelfHostedShmem& GetSingleton();

  // Initialize this singleton with the content of the Self-hosted Stencil XDR.
  // This will be used to initialize the shared memory which would hold a copy.
  //
  // This function is not thread-safe and should be call at most once and from
  // the main thread.
  void InitFromParent(ContentType aXdr);

  // Initialize this singleton with the content coming from the parent process,
  // using a file handle which maps to the memory pages of the parent process.
  //
  // This function is not thread-safe and should be call at most once and from
  // the main thread.
  [[nodiscard]] bool InitFromChild(base::SharedMemoryHandle aHandle,
                                   size_t aLen);

  // Return a span over the read-only XDR content of the self-hosted stencil.
  ContentType Content() const;

  // Return the file handle which is under which the content is mapped.
  const mozilla::UniqueFileHandle& Handle() const;

  // Register this class to the memory reporter service.
  void InitMemoryReporter();

  // Unregister this class from the memory reporter service, and delete the
  // memory associated with the shared memory. As the memory is borrowed by the
  // JS engine, this function should be called after JS_Shutdown.
  //
  // NOTE: This is not using the Observer service which would call Shutdown
  // while JS code might still be running during shutdown process.
  static void Shutdown();

 private:
  SelfHostedShmem() = default;
  ~SelfHostedShmem() = default;

  static mozilla::StaticRefPtr<SelfHostedShmem> sSelfHostedXdr;

  // read-only file Handle used to transfer from the parent process to content
  // processes.
  mozilla::UniqueFileHandle mHandle;

  // Shared memory used by JS runtime initialization.
  mozilla::UniquePtr<base::SharedMemory> mMem;

  // Length of the content within the shared memory.
  size_t mLen = 0;
};

}  // namespace xpc

#endif  // !xpcselfhostedshmem_h___
