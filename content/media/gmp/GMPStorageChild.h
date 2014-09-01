/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPStorageChild_h_
#define GMPStorageChild_h_

#include "mozilla/gmp/PGMPStorageChild.h"
#include "gmp-storage.h"
#include "nsTHashtable.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace gmp {

class GMPChild;
class GMPStorageChild;

class GMPRecordImpl : public GMPRecord
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPRecordImpl)

  GMPRecordImpl(GMPStorageChild* aOwner,
                const nsCString& aName,
                GMPRecordClient* aClient);

  // GMPRecord.
  virtual GMPErr Open() MOZ_OVERRIDE;
  virtual GMPErr Read() MOZ_OVERRIDE;
  virtual GMPErr Write(const uint8_t* aData,
                       uint32_t aDataSize) MOZ_OVERRIDE;
  virtual GMPErr Close() MOZ_OVERRIDE;

  const nsCString& Name() const { return mName; }

  void OpenComplete(GMPErr aStatus);
  void ReadComplete(GMPErr aStatus, const uint8_t* aBytes, uint32_t aLength);
  void WriteComplete(GMPErr aStatus);

  void MarkClosed();

private:
  ~GMPRecordImpl() {}
  const nsCString mName;
  GMPRecordClient* const mClient;
  GMPStorageChild* const mOwner;
  bool mIsClosed;
};

class GMPStorageChild : public PGMPStorageChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPStorageChild)

  explicit GMPStorageChild(GMPChild* aPlugin);

  GMPErr CreateRecord(const nsCString& aRecordName,
                      GMPRecord** aOutRecord,
                      GMPRecordClient* aClient);

  GMPErr Open(GMPRecordImpl* aRecord);

  GMPErr Read(GMPRecordImpl* aRecord);

  GMPErr Write(GMPRecordImpl* aRecord,
               const uint8_t* aData,
               uint32_t aDataSize);

  GMPErr Close(GMPRecordImpl* aRecord);

protected:
  ~GMPStorageChild() {}

  // PGMPStorageChild
  virtual bool RecvOpenComplete(const nsCString& aRecordName,
                                const GMPErr& aStatus) MOZ_OVERRIDE;
  virtual bool RecvReadComplete(const nsCString& aRecordName,
                                const GMPErr& aStatus,
                                const InfallibleTArray<uint8_t>& aBytes) MOZ_OVERRIDE;
  virtual bool RecvWriteComplete(const nsCString& aRecordName,
                                 const GMPErr& aStatus) MOZ_OVERRIDE;
  virtual bool RecvShutdown() MOZ_OVERRIDE;

private:
  nsRefPtrHashtable<nsCStringHashKey, GMPRecordImpl> mRecords;
  GMPChild* mPlugin;
  bool mShutdown;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPStorageChild_h_
