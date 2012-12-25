/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A header for declaring various things that binding implementation headers
 * might need.  The idea is to make binding implementation headers safe to
 * include anywhere without running into include hell like we do with
 * BindingUtils.h
 */
#ifndef mozilla_dom_BindingDeclarations_h__
#define mozilla_dom_BindingDeclarations_h__

#include "nsStringGlue.h"
#include "jsapi.h"
#include "mozilla/Util.h"

namespace mozilla {
namespace dom {

struct MainThreadDictionaryBase
{
protected:
  JSContext* ParseJSON(const nsAString& aJSON,
                       mozilla::Maybe<JSAutoRequest>& aAr,
                       mozilla::Maybe<JSAutoCompartment>& aAc,
                       JS::Value& aVal);
};

struct EnumEntry {
  const char* value;
  size_t length;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BindingDeclarations_h__
