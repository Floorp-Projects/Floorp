/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPStorageParent_h_
#define GMPStorageParent_h_

#include "mozilla/gmp/PGMPStorageParent.h"
#include "GMPStorage.h"

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPStorageParent : public PGMPStorageParent {
public:
  NS_INLINE_DECL_REFCOUNTING(GMPStorageParent)
  GMPStorageParent(const nsCString& aNodeId,
                   GMPParent* aPlugin);

  nsresult Init();
  void Shutdown();

protected:
  mozilla::ipc::IPCResult RecvOpen(const nsCString& aRecordName) override;
  mozilla::ipc::IPCResult RecvRead(const nsCString& aRecordName) override;
  mozilla::ipc::IPCResult RecvWrite(const nsCString& aRecordName,
                                    InfallibleTArray<uint8_t>&& aBytes) override;
  mozilla::ipc::IPCResult RecvGetRecordNames() override;
  mozilla::ipc::IPCResult RecvClose(const nsCString& aRecordName) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  ~GMPStorageParent() {}

  RefPtr<GMPStorage> mStorage;

  const nsCString mNodeId;
  RefPtr<GMPParent> mPlugin;
  // True after Shutdown(), or if Init() has not completed successfully.
  bool mShutdown;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPStorageParent_h_
