/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef include_dom_media_mediacapabilities_BenchmarkStorageParent_h
#define include_dom_media_mediacapabilities_BenchmarkStorageParent_h

#include "mozilla/PBenchmarkStorageParent.h"

namespace mozilla {
class KeyValueStorage;

using mozilla::ipc::IPCResult;

class BenchmarkStorageParent : public PBenchmarkStorageParent {
  friend class PBenchmarkStorageParent;

 public:
  BenchmarkStorageParent();

  IPCResult RecvPut(const nsCString& aDbName, const nsCString& aKey,
                    const int32_t& aValue);

  IPCResult RecvGet(const nsCString& aDbName, const nsCString& aKey,
                    GetResolver&& aResolve);

  IPCResult RecvCheckVersion(const nsCString& aDbName, int32_t aVersion);

 private:
  RefPtr<KeyValueStorage> mStorage;
};

}  // namespace mozilla

#endif  // include_dom_media_mediacapabilities_BenchmarkStorageParent_h
