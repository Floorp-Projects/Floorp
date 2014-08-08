/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Representation for dates. */

#ifndef mozilla_dom_Date_h
#define mozilla_dom_Date_h

#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class Date
{
public:
  // Not inlining much here to avoid the includes we'd need.
  Date();
  explicit Date(double aMilliseconds)
    : mMsecSinceEpoch(aMilliseconds)
  {}

  bool IsUndefined() const;
  double TimeStamp() const
  {
    return mMsecSinceEpoch;
  }
  void SetTimeStamp(double aMilliseconds)
  {
    mMsecSinceEpoch = aMilliseconds;
  }
  // Can return false if CheckedUnwrap fails.  This will NOT throw;
  // callers should do it as needed.
  bool SetTimeStamp(JSContext* aCx, JSObject* aObject);

  bool ToDateObject(JSContext* aCx, JS::MutableHandle<JS::Value> aRval) const;

private:
  double mMsecSinceEpoch;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Date_h
