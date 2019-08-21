/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PresShellInlines_h
#define mozilla_PresShellInlines_h

#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"

namespace mozilla {

void PresShell::SetNeedLayoutFlush() {
  mNeedLayoutFlush = true;
  if (dom::Document* doc = mDocument->GetDisplayDocument()) {
    if (PresShell* presShell = doc->GetPresShell()) {
      presShell->mNeedLayoutFlush = true;
    }
  }

#ifdef MOZ_GECKO_PROFILER
  if (!mReflowCause) {
    mReflowCause = profiler_get_backtrace();
  }
#endif

  mReqsPerFlush[FlushKind::Layout]++;
}

void PresShell::SetNeedStyleFlush() {
  mNeedStyleFlush = true;
  if (dom::Document* doc = mDocument->GetDisplayDocument()) {
    if (PresShell* presShell = doc->GetPresShell()) {
      presShell->mNeedStyleFlush = true;
    }
  }

#ifdef MOZ_GECKO_PROFILER
  if (!mStyleCause) {
    mStyleCause = profiler_get_backtrace();
  }
#endif

  mReqsPerFlush[FlushKind::Style]++;
}

void PresShell::EnsureStyleFlush() {
  SetNeedStyleFlush();
  ObserveStyleFlushes();
}

void PresShell::SetNeedThrottledAnimationFlush() {
  mNeedThrottledAnimationFlush = true;
  if (dom::Document* doc = mDocument->GetDisplayDocument()) {
    if (PresShell* presShell = doc->GetPresShell()) {
      presShell->mNeedThrottledAnimationFlush = true;
    }
  }
}

ServoStyleSet* PresShell::StyleSet() const {
  return mDocument->StyleSetForPresShellOrMediaQueryEvaluation();
}

/* static */
inline void PresShell::EventHandler::OnPresShellDestroy(Document* aDocument) {
  if (sLastKeyDownEventTargetElement &&
      sLastKeyDownEventTargetElement->OwnerDoc() == aDocument) {
    sLastKeyDownEventTargetElement = nullptr;
  }
}

}  // namespace mozilla

#endif  // mozilla_PresShellInlines_h
