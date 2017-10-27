/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIPresShellInlines_h
#define nsIPresShellInlines_h

#include "nsIDocument.h"

void
nsIPresShell::SetNeedLayoutFlush()
{
  mNeedLayoutFlush = true;
  if (nsIDocument* doc = mDocument->GetDisplayDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      shell->mNeedLayoutFlush = true;
    }
  }
}

void
nsIPresShell::SetNeedStyleFlush()
{
  mNeedStyleFlush = true;
  if (nsIDocument* doc = mDocument->GetDisplayDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      shell->mNeedStyleFlush = true;
    }
  }
}

void
nsIPresShell::EnsureStyleFlush()
{
  SetNeedStyleFlush();
  ObserveStyleFlushes();
}

void
nsIPresShell::SetNeedThrottledAnimationFlush()
{
  mNeedThrottledAnimationFlush = true;
  if (nsIDocument* doc = mDocument->GetDisplayDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      shell->mNeedThrottledAnimationFlush = true;
    }
  }
}

#endif // nsIPresShellInlines_h
