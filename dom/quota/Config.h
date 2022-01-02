/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CONFIG_H_
#define DOM_QUOTA_CONFIG_H_

#ifdef DEBUG
#  define QM_LOG_ERROR_TO_CONSOLE_ENABLED
#endif

#define QM_LOG_ERROR_TO_BROWSER_CONSOLE_ENABLED

#if defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG)
#  define QM_LOG_ERROR_TO_TELEMETRY_ENABLED
#endif

#if defined(QM_LOG_ERROR_TO_CONSOLE_ENABLED) ||         \
    defined(QM_LOG_ERROR_TO_BROWSER_CONSOLE_ENABLED) || \
    defined(QM_LOG_ERROR_TO_TELEMETRY_ENABLED)
#  define QM_LOG_ERROR_ENABLED
#endif

#if defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG)
#  define QM_ERROR_STACKS_ENABLED
#endif

#define QM_SCOPED_LOG_EXTRA_INFO_ENABLED

#endif  // DOM_QUOTA_CONFIG_H_
