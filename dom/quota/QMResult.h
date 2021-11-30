/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QMRESULT_H_
#define DOM_QUOTA_QMRESULT_H_

#include "ErrorList.h"
#include "mozilla/dom/quota/Config.h"
#include "mozilla/dom/quota/ForwardDecls.h"

namespace mozilla {

#ifdef QM_ERROR_STACKS_ENABLED
// A wrapped nsresult, primarily intended for use along with mozilla::Result
// and QM_TRY macros. The wrapper contains stack id and frame id which are
// reported in LogError besides the error result itself.
//
// XXX Document the general situation more, bug 1709777.
class QMResult {
  uint64_t mStackId;
  uint32_t mFrameId;
  nsresult mNSResult;

 public:
  QMResult() : QMResult(NS_OK) {}

  explicit QMResult(nsresult aNSResult);

  uint64_t StackId() const { return mStackId; }

  uint32_t FrameId() const { return mFrameId; }

  nsresult NSResult() const { return mNSResult; }

  /**
   * Propagate the result.
   *
   * This is used by GenericErrorResult<QMResult> to create a propagated
   * result.
   */
  QMResult Propagate() const {
    return QMResult{mStackId, mFrameId + 1, mNSResult};
  }

 private:
  QMResult(uint64_t aStackId, uint32_t aFrameId, nsresult aNSResult)
      : mStackId(aStackId), mFrameId(aFrameId), mNSResult(aNSResult) {}
};
#endif

inline QMResult ToQMResult(nsresult aValue) { return QMResult(aValue); }

}  // namespace mozilla

#endif
