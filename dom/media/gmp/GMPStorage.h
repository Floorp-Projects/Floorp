/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPStorage_h_
#define GMPStorage_h_

#include "gmp-storage.h"
#include "mozilla/AlreadyAddRefed.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace gmp {

class GMPStorage {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPStorage)

  virtual GMPErr Open(const nsCString& aRecordName) = 0;
  virtual bool IsOpen(const nsCString& aRecordName) const = 0;
  virtual GMPErr Read(const nsCString& aRecordName,
                      nsTArray<uint8_t>& aOutBytes) = 0;
  virtual GMPErr Write(const nsCString& aRecordName,
                       const nsTArray<uint8_t>& aBytes) = 0;
  virtual GMPErr GetRecordNames(nsTArray<nsCString>& aOutRecordNames) const = 0;
  virtual void Close(const nsCString& aRecordName) = 0;
protected:
  virtual ~GMPStorage() {}
};

already_AddRefed<GMPStorage> CreateGMPMemoryStorage();
already_AddRefed<GMPStorage> CreateGMPDiskStorage(const nsCString& aNodeId,
                                                  const nsString& aGMPName);

} // namespace gmp
} // namespace mozilla

#endif