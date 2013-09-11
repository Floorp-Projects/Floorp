/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_storageprivilege_h__
#define mozilla_dom_quota_storageprivilege_h__

#include "mozilla/dom/quota/QuotaCommon.h"

BEGIN_QUOTA_NAMESPACE

enum StoragePrivilege {
  // Quota not tracked, persistence type is always "persistent".
  Chrome,

  // Quota tracked, persistence type can be either "persistent" or "temporary".
  // The permission "defaul-persistent-storage" is used to determine the
  // default persistence type.
  Content
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_storageprivilege_h__
