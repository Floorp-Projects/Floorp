/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPresContextInlines_h
#define nsPresContextInlines_h

#include "mozilla/dom/Document.h"
#include "mozilla/PresShell.h"
#include "nsCSSFrameConstructor.h"

inline mozilla::ServoStyleSet* nsPresContext::StyleSet() const {
  return mDocument->StyleSetForPresShell();
}

inline nsCSSFrameConstructor* nsPresContext::FrameConstructor() const {
  return PresShell()->FrameConstructor();
}

#endif  // #ifndef nsPresContextInlines_h
