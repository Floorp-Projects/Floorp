/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DEBUGONLYMACRO_H_
#define DOM_QUOTA_DEBUGONLYMACRO_H_

#include "mozilla/dom/quota/RemoveParen.h"

#ifdef DEBUG
#  define DEBUGONLY(expr) MOZ_REMOVE_PAREN(expr)
#else
#  define DEBUGONLY(expr)

#endif

#endif  // DOM_QUOTA_DEBUGONLYMACRO_H_
