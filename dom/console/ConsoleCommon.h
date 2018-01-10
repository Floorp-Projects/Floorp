/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ConsoleCommon_h
#define mozilla_dom_ConsoleCommon_h

#include "nsString.h"

namespace mozilla {
namespace dom {
namespace ConsoleCommon {

// This class is used to clear any exception at the end of this method.
class MOZ_RAII ClearException
{
public:
  explicit ClearException(JSContext* aCx)
    : mCx(aCx)
  {
  }

  ~ClearException()
  {
    JS_ClearPendingException(mCx);
  }

private:
  JSContext* mCx;
};

} // namespace ConsoleCommon
} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ConsoleCommon_h */
