/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_ActorsParent_h
#define mozilla_dom_quota_ActorsParent_h

#include "mozilla/dom/quota/Config.h"

namespace mozilla::dom::quota {

void InitializeQuotaManager();

void InitializeScopedLogExtraInfo();

bool RecvShutdownQuotaManager();

}  // namespace mozilla::dom::quota

#endif  // mozilla_dom_quota_ActorsParent_h
