/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotacommon_h__
#define mozilla_dom_quota_quotacommon_h__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

#define BEGIN_QUOTA_NAMESPACE \
  namespace mozilla { namespace dom { namespace quota {
#define END_QUOTA_NAMESPACE \
  } /* namespace quota */ } /* namespace dom */ } /* namespace mozilla */
#define USING_QUOTA_NAMESPACE \
  using namespace mozilla::dom::quota;

BEGIN_QUOTA_NAMESPACE

#ifdef DEBUG
void
AssertIsOnIOThread();
#else
inline void
AssertIsOnIOThread()
{ }
#endif

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_quotacommon_h__
