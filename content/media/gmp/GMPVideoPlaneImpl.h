/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoPlaneImpl_h_
#define GMPVideoPlaneImpl_h_

#include "gmp-video-plane.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace gmp {

class GMPVideoHostImpl;
class GMPPlaneData;

class GMPPlaneImpl : public GMPPlane
{
  friend struct IPC::ParamTraits<mozilla::gmp::GMPPlaneImpl>;
public:
  explicit GMPPlaneImpl(GMPVideoHostImpl* aHost);
  GMPPlaneImpl(const GMPPlaneData& aPlaneData, GMPVideoHostImpl* aHost);
  virtual ~GMPPlaneImpl();

  // This is called during a normal destroy sequence, which is
  // when a consumer is finished or during XPCOM shutdown.
  void DoneWithAPI();
  // This is called when something has gone wrong - specicifically,
  // a child process has crashed. Does not attempt to release Shmem,
  // as the Shmem has already been released.
  void ActorDestroyed();

  bool InitPlaneData(GMPPlaneData& aPlaneData);

  // GMPPlane
  virtual GMPErr CreateEmptyPlane(int32_t aAllocatedSize,
                                  int32_t aStride,
                                  int32_t aPlaneSize) MOZ_OVERRIDE;
  virtual GMPErr Copy(const GMPPlane& aPlane) MOZ_OVERRIDE;
  virtual GMPErr Copy(int32_t aSize,
                      int32_t aStride,
                      const uint8_t* aBuffer) MOZ_OVERRIDE;
  virtual void Swap(GMPPlane& aPlane) MOZ_OVERRIDE;
  virtual int32_t AllocatedSize() const MOZ_OVERRIDE;
  virtual void ResetSize() MOZ_OVERRIDE;
  virtual bool IsZeroSize() const MOZ_OVERRIDE;
  virtual int32_t Stride() const MOZ_OVERRIDE;
  virtual const uint8_t* Buffer() const MOZ_OVERRIDE;
  virtual uint8_t* Buffer() MOZ_OVERRIDE;
  virtual void Destroy() MOZ_OVERRIDE;

private:
  GMPErr MaybeResize(int32_t aNewSize);
  void DestroyBuffer();

  ipc::Shmem mBuffer;
  int32_t mSize;
  int32_t mStride;
  GMPVideoHostImpl* mHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoPlaneImpl_h_
