/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SerializedStructuredCloneBuffer.h"
#include "js/StructuredClone.h"

namespace IPC {

void ParamTraits<JSStructuredCloneData>::Write(MessageWriter* aWriter,
                                               const paramType& aParam) {
  MOZ_ASSERT(!(aParam.Size() % sizeof(uint64_t)));

  // We can only construct shared memory regions up to 4Gb in size, making that
  // the maximum possible JSStructuredCloneData size.
  mozilla::CheckedUint32 size = aParam.Size();
  if (!size.isValid()) {
    aWriter->FatalError("JSStructuredCloneData over 4Gb in size");
    return;
  }
  WriteParam(aWriter, size.value());

  MessageBufferWriter bufWriter(aWriter, size.value());
  aParam.ForEachDataChunk([&](const char* aData, size_t aSize) {
    return bufWriter.WriteBytes(aData, aSize);
  });
}

bool ParamTraits<JSStructuredCloneData>::Read(MessageReader* aReader,
                                              paramType* aResult) {
  uint32_t length = 0;
  if (!ReadParam(aReader, &length)) {
    aReader->FatalError("JSStructuredCloneData length read failed");
    return false;
  }
  MOZ_ASSERT(!(length % sizeof(uint64_t)));

  // Borrowing is not suitable to use for IPC to hand out data because we often
  // want to store the data somewhere for processing after IPC has released the
  // underlying buffers.
  //
  // This deserializer previously used a mechanism to transfer ownership over
  // the underlying buffers from IPC into the JSStructuredCloneData. This was
  // removed when support for serializing over shared memory was added, as the
  // benefit for avoiding copies was limited due to it only functioning for
  // buffers under 64k in size (as larger buffers would be serialized using
  // shared memory), and it added substantial complexity to the BufferList type
  // and the IPC serialization layer due to things like buffer alignment. This
  // can be revisited in the future if it turns out to be a noticable
  // performance regression. (bug 1783242)

  mozilla::BufferList<js::SystemAllocPolicy> buffers(0, 0, 4096);
  MessageBufferReader bufReader(aReader, length);
  uint32_t read = 0;
  while (read < length) {
    size_t bufLen;
    char* buf = buffers.AllocateBytes(length - read, &bufLen);
    if (!buf) {
      // Would be nice to allow actor to control behaviour here (bug 1784307)
      NS_ABORT_OOM(length - read);
      return false;
    }
    if (!bufReader.ReadBytesInto(buf, bufLen)) {
      aReader->FatalError("JSStructuredCloneData ReadBytesInto failed");
      return false;
    }
    read += bufLen;
  }

  MOZ_ASSERT(read == length);
  *aResult = JSStructuredCloneData(
      std::move(buffers), JS::StructuredCloneScope::DifferentProcess,
      OwnTransferablePolicy::IgnoreTransferablesIfAny);
  return true;
}

}  // namespace IPC
