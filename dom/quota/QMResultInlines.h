/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QMRESULTINLINES_H_
#define DOM_QUOTA_QMRESULTINLINES_H_

#ifndef DOM_QUOTA_QMRESULT_H_
#  error Must include QMResult.h first
#endif

#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "nsError.h"

namespace mozilla {

inline Result<Ok, QMResult> ToResult(const QMResult& aValue) {
  if (NS_FAILED(aValue.NSResult())) {
    return Err(aValue);
  }
  return Ok();
}

inline Result<Ok, QMResult> ToResult(QMResult&& aValue) {
  if (NS_FAILED(aValue.NSResult())) {
    return Err(std::move(aValue));
  }
  return Ok();
}

}  // namespace mozilla

#endif
