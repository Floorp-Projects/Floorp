/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HalImpl_h
#define mozilla_HalImpl_h

#ifdef MOZ_UNIFIED_BUILD
#error Cannot use HalImpl.h in unified builds.
#endif

#define MOZ_HAL_NAMESPACE hal_impl
#undef mozilla_Hal_h
#undef mozilla_HalInternal_h
#include "Hal.h"
#include "HalInternal.h"
#undef MOZ_HAL_NAMESPACE

#endif // mozilla_HalImpl_h
