/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "mozilla/unused.h"
#include "nsXULAppAPI.h"

#include "FenceUtilsGonk.h"

using namespace android;
using namespace mozilla::layers;

using base::FileDescriptor;

namespace IPC {

void
ParamTraits<FenceHandle>::Write(Message* aMsg,
                                const paramType& aParam)
{
#if ANDROID_VERSION >= 19
  sp<Fence> flattenable = aParam.mFence;
#else
  Flattenable *flattenable = aParam.mFence.get();
#endif
  size_t nbytes = flattenable->getFlattenedSize();
  size_t nfds = flattenable->getFdCount();
  char data[nbytes];
  int fds[nfds];

#if ANDROID_VERSION >= 19
  // Make a copy of "data" and "fds" for flatten() to avoid casting problem
  void *pdata = (void *)data;
  int *pfds = fds;

  flattenable->flatten(pdata, nbytes, pfds, nfds);

  // In Kitkat, flatten() will change the value of nbytes and nfds, which dues
  // to multiple parcelable object consumption. The actual size and fd count
  // which returned by getFlattenedSize() and getFdCount() are not changed.
  // So we change nbytes and nfds back by call corresponding calls.
  nbytes = flattenable->getFlattenedSize();
  nfds = flattenable->getFdCount();
#else
  flattenable->flatten(data, nbytes, fds, nfds);
#endif
  aMsg->WriteSize(nbytes);
  aMsg->WriteSize(nfds);
  aMsg->WriteBytes(data, nbytes);
  for (size_t n = 0; n < nfds; ++n) {
    // Dupe the fd to make sure the fence life time and set auto-close
    // for cross-process. If IPC is cross-thread, the other side will use
    // the duped fd directly.
    aMsg->WriteFileDescriptor(FileDescriptor(dup(fds[n]), true));
  }
}

bool
ParamTraits<FenceHandle>::Read(const Message* aMsg,
                               void** aIter, paramType* aResult)
{
  size_t nbytes;
  size_t nfds;
  const char* data;

  if (!aMsg->ReadSize(aIter, &nbytes) ||
      !aMsg->ReadSize(aIter, &nfds) ||
      !aMsg->ReadBytes(aIter, &data, nbytes)) {
    return false;
  }

  // Check if nfds is correct.
  // aMsg->num_fds() could include fds of another ParamTraits<>s.
  if (nfds > aMsg->num_fds()) {
    return false;
  }
  int fds[nfds];

  for (size_t n = 0; n < nfds; ++n) {
    FileDescriptor fd;
    if (!aMsg->ReadFileDescriptor(aIter, &fd)) {
      return false;
    }
    // If the GraphicBuffer was shared cross-process, SCM_RIGHTS does
    // the right thing and dup's the fd.  If it's shared cross-thread,
    // SCM_RIGHTS doesn't dup the fd.  That's surprising, but we just
    // deal with it here.  NB: only the "default" (master) process can
    // alloc gralloc buffers.
    fds[n] = fd.fd;
  }

  sp<Fence> buffer(new Fence());
#if ANDROID_VERSION >= 19
  // Make a copy of "data" and "fds" for unflatten() to avoid casting problem
  void const *pdata = (void const *)data;
  int const *pfds = fds;

  if (NO_ERROR == buffer->unflatten(pdata, nbytes, pfds, nfds)) {
#else
  Flattenable *flattenable = buffer.get();

  if (NO_ERROR == flattenable->unflatten(data, nbytes, fds, nfds)) {
#endif
    aResult->mFence = buffer;
    return true;
  }
  return false;
}

} // namespace IPC

namespace mozilla {
namespace layers {

FenceHandle::FenceHandle()
  : mFence(android::Fence::NO_FENCE)
{
}

FenceHandle::FenceHandle(const sp<Fence>& aFence)
  : mFence(aFence)
{
}

void
FenceHandle::Merge(const FenceHandle& aFenceHandle)
{
  if (!aFenceHandle.IsValid()) {
    return;
  }

  if (!IsValid()) {
    mFence = aFenceHandle.mFence;
  } else {
    android::sp<android::Fence> mergedFence = android::Fence::merge(
                  android::String8::format("FenceHandle"),
                  mFence, aFenceHandle.mFence);
    if (!mergedFence.get()) {
      // synchronization is broken, the best we can do is hope fences
      // signal in order so the new fence will act like a union.
      // This error handling is same as android::ConsumerBase does.
      mFence = aFenceHandle.mFence;
      return;
    }
    mFence = mergedFence;
  }
}

} // namespace layers
} // namespace mozilla
