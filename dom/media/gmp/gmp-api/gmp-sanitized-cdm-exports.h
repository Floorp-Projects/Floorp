/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// This file exposes symbols from the CDM headers + undefines macros which
// X11 defines and which clash with the CDM headers.
//
// Ideally we can prune where X11 headers are included so that we don't need
// this, but this band aid is useful until then.

#ifndef DOM_MEDIA_GMP_GMP_API_GMP_SANITIZED_CDM_EXPORTS_H_
#define DOM_MEDIA_GMP_GMP_API_GMP_SANITIZED_CDM_EXPORTS_H_

// If Status is defined undef it so we don't break the CDM headers.
#ifdef Status
#  undef Status
#endif  // Status
#include "content_decryption_module.h"

#endif  // DOM_MEDIA_GMP_GMP_API_GMP_SANITIZED_CDM_EXPORTS_H_
