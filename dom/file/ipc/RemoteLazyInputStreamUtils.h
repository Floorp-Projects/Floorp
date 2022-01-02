/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RemoteLazyInputStreamUtils_h
#define mozilla_RemoteLazyInputStreamUtils_h

#include <cstdint>

/*
 * RemoteLazyInputStream was previously part of the IPCBlob world.
 * See IPCBlobUtils.h to know how to use it. As a follow up, the documentation
 * will be partially moved here too.
 */

class nsIInputStream;
enum class nsresult : uint32_t;

namespace mozilla {

class RemoteLazyStream;

namespace ipc {
class IPCStream;
class PBackgroundChild;
class PBackgroundParent;
}  // namespace ipc

namespace dom {
class ContentChild;
class ContentParent;
}  // namespace dom

class RemoteLazyInputStreamUtils final {
 public:
  static nsresult SerializeInputStream(nsIInputStream* aInputStream,
                                       uint64_t aSize,
                                       RemoteLazyStream& aOutStream,
                                       dom::ContentParent* aManager);

  static nsresult SerializeInputStream(
      nsIInputStream* aInputStream, uint64_t aSize,
      RemoteLazyStream& aOutStream, mozilla::ipc::PBackgroundParent* aManager);

  static nsresult SerializeInputStream(nsIInputStream* aInputStream,
                                       uint64_t aSize,
                                       RemoteLazyStream& aOutStream,
                                       dom::ContentChild* aManager);

  static nsresult SerializeInputStream(
      nsIInputStream* aInputStream, uint64_t aSize,
      RemoteLazyStream& aOutStream, mozilla::ipc::PBackgroundChild* aManager);
};

}  // namespace mozilla

#endif  // mozilla_RemoteLazyInputStreamUtils_h
