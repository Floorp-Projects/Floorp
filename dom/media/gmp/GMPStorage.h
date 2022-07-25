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

namespace mozilla::gmp {

class GMPStorage {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPStorage)

  virtual GMPErr Open(const nsACString& aRecordName) = 0;
  virtual bool IsOpen(const nsACString& aRecordName) const = 0;
  virtual GMPErr Read(const nsACString& aRecordName,
                      nsTArray<uint8_t>& aOutBytes) = 0;
  virtual GMPErr Write(const nsACString& aRecordName,
                       const nsTArray<uint8_t>& aBytes) = 0;
  virtual void Close(const nsACString& aRecordName) = 0;

 protected:
  virtual ~GMPStorage() = default;
};

already_AddRefed<GMPStorage> CreateGMPMemoryStorage();
already_AddRefed<GMPStorage> CreateGMPDiskStorage(const nsACString& aNodeId,
                                                  const nsAString& aGMPName);

}  // namespace mozilla::gmp

#endif
