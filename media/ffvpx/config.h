/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_FFVPX_CONFIG_H
#define MOZ_FFVPX_CONFIG_H
#if defined(XP_WIN)
#include "config_win.h"
#elif defined(XP_DARWIN)
#if defined(HAVE_64BIT_BUILD)
#include "config_darwin64.h"
#else
#include "config_darwin32.h"
#endif
#elif defined(XP_UNIX)
#if defined(HAVE_64BIT_BUILD)
#include "config_unix64.h"
#else
#include "config_unix32.h"
#endif
#endif
#include "config_common.h"
#endif // MOZ_FFVPX_CONFIG_H
