/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMQUOTACLIENTIMPL_H_
#define DOM_FS_PARENT_FILESYSTEMQUOTACLIENTIMPL_H_

#include "mozilla/dom/quota/Client.h"

namespace mozilla::dom::fs::usage {

Result<quota::UsageInfo, nsresult> GetUsageForOrigin(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata,
    const Atomic<bool>& aCanceled);

nsresult AboutToClearOrigins(
    const Nullable<quota::PersistenceType>& aPersistenceType,
    const quota::OriginScope& aOriginScope);

void StartIdleMaintenance();

void StopIdleMaintenance();

}  // namespace mozilla::dom::fs::usage

#endif  // DOM_FS_PARENT_FILESYSTEMQUOTACLIENTIMPL_H_
