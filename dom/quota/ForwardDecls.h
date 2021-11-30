/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_FORWARD_DECLS_H_
#define DOM_QUOTA_FORWARD_DECLS_H_

#include "mozilla/dom/quota/Config.h"

#ifndef QM_ERROR_STACKS_ENABLED
enum class nsresult : uint32_t;
#endif

namespace mozilla {

#ifdef QM_ERROR_STACKS_ENABLED
class QMResult;
#else
using QMResult = nsresult;
#endif

struct Ok;
template <typename V, typename E>
class Result;

using OkOrErr = Result<Ok, QMResult>;

}  // namespace mozilla

#endif  // DOM_QUOTA_FORWARD_DECLS_H_
