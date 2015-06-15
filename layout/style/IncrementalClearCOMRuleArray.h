/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IncrementalClearCOMRuleArray_h
#define mozilla_IncrementalClearCOMRuleArray_h

#include "nsCOMArray.h"

namespace mozilla {
namespace css {
class Rule;
} // namespace css

class IncrementalClearCOMRuleArray : public nsCOMArray<css::Rule>
{
public:
  IncrementalClearCOMRuleArray() {}
  ~IncrementalClearCOMRuleArray() { Clear(); }

  void Clear();
};

} // namespace mozilla

#endif /* !defined(mozilla_IncrementalClearCOMRuleArray_h) */
