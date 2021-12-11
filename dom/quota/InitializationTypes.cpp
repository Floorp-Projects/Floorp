/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InitializationTypes.h"

namespace mozilla::dom::quota {

//  static
nsLiteralCString StringGenerator::GetString(
    const Initialization aInitialization) {
  switch (aInitialization) {
    case Initialization::Storage:
      return "Storage"_ns;
    case Initialization::TemporaryStorage:
      return "TemporaryStorage"_ns;
    case Initialization::DefaultRepository:
      return "DefaultRepository"_ns;
    case Initialization::TemporaryRepository:
      return "TemporaryRepository"_ns;
    case Initialization::UpgradeStorageFrom0_0To1_0:
      return "UpgradeStorageFrom0_0To1_0"_ns;
    case Initialization::UpgradeStorageFrom1_0To2_0:
      return "UpgradeStorageFrom1_0To2_0"_ns;
    case Initialization::UpgradeStorageFrom2_0To2_1:
      return "UpgradeStorageFrom2_0To2_1"_ns;
    case Initialization::UpgradeStorageFrom2_1To2_2:
      return "UpgradeStorageFrom2_1To2_2"_ns;
    case Initialization::UpgradeStorageFrom2_2To2_3:
      return "UpgradeStorageFrom2_2To2_3"_ns;
    case Initialization::UpgradeFromIndexedDBDirectory:
      return "UpgradeFromIndexedDBDirectory"_ns;
    case Initialization::UpgradeFromPersistentStorageDirectory:
      return "UpgradeFromPersistentStorageDirectory"_ns;

    default:
      MOZ_CRASH("Bad initialization value!");
  }
}

// static
nsLiteralCString StringGenerator::GetString(
    const OriginInitialization aOriginInitialization) {
  switch (aOriginInitialization) {
    case OriginInitialization::PersistentOrigin:
      return "PersistentOrigin"_ns;
    case OriginInitialization::TemporaryOrigin:
      return "TemporaryOrigin"_ns;

    default:
      MOZ_CRASH("Bad origin initialization value!");
  }
}

}  // namespace mozilla::dom::quota
