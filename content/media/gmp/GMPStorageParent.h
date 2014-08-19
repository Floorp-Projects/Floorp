/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPStorageParent_h_
#define GMPStorageParent_h_

#include "mozilla/gmp/PGMPStorageParent.h"
#include "gmp-storage.h"
#include "nsTHashtable.h"
#include "nsDataHashtable.h"
#include "prio.h"

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPStorageParent : public PGMPStorageParent {
public:
  NS_INLINE_DECL_REFCOUNTING(GMPStorageParent)
  GMPStorageParent(const nsString& aOrigin, GMPParent* aPlugin);

  void Shutdown();

protected:
  virtual bool RecvOpen(const nsCString& aRecordName) MOZ_OVERRIDE;
  virtual bool RecvRead(const nsCString& aRecordName) MOZ_OVERRIDE;
  virtual bool RecvWrite(const nsCString& aRecordName,
                         const InfallibleTArray<uint8_t>& aBytes) MOZ_OVERRIDE;
  virtual bool RecvClose(const nsCString& aRecordName) MOZ_OVERRIDE;
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

private:
  ~GMPStorageParent() {}

  nsDataHashtable<nsCStringHashKey, PRFileDesc*> mFiles;
  const nsString mOrigin;
  nsRefPtr<GMPParent> mPlugin;
  bool mShutdown;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPStorageParent_h_
