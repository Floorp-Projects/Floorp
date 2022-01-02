/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef include_dom_media_mediacapabilities_BenchmarkStorageChild_h
#define include_dom_media_mediacapabilities_BenchmarkStorageChild_h

#include "mozilla/PBenchmarkStorageChild.h"

namespace mozilla {

class BenchmarkStorageChild : public PBenchmarkStorageChild {
 public:
  /* Singleton class to avoid recreating the protocol every time we need access
   * to the storage. */
  static PBenchmarkStorageChild* Instance();

  ~BenchmarkStorageChild();

 private:
  BenchmarkStorageChild() = default;
};

}  // namespace mozilla

#endif  // include_dom_media_mediacapabilities_BenchmarkStorageChild_h
