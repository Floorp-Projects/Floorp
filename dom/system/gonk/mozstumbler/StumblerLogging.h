/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef STUMBLERLOGGING_H
#define STUMBLERLOGGING_H

#include "mozilla/Logging.h"

PRLogModuleInfo* GetLog();

#define STUMBLER_DBG(arg, ...)  MOZ_LOG(GetLog(), mozilla::LogLevel::Debug, ("STUMBLER - %s: " arg, __func__, ##__VA_ARGS__))
#define STUMBLER_LOG(arg, ...)  MOZ_LOG(GetLog(), mozilla::LogLevel::Info, ("STUMBLER - %s: " arg, __func__, ##__VA_ARGS__))
#define STUMBLER_ERR(arg, ...)  MOZ_LOG(GetLog(), mozilla::LogLevel::Error, ("STUMBLER -%s: " arg, __func__, ##__VA_ARGS__))

#endif
