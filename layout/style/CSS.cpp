/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object holding utility CSS functions */

#include "CSS.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ServoBindings.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/Document.h"
#include "nsStyleUtil.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

/* static */
bool CSS::Supports(const GlobalObject&, const nsACString& aProperty,
                   const nsACString& aValue) {
  return Servo_CSSSupports2(&aProperty, &aValue);
}

/* static */
bool CSS::Supports(const GlobalObject&, const nsACString& aCondition) {
  return Servo_CSSSupports(&aCondition, /* ua = */ false, /* chrome = */ false,
                           /* quirks = */ false);
}

/* static */
void CSS::Escape(const GlobalObject&, const nsAString& aIdent,
                 nsAString& aReturn) {
  nsStyleUtil::AppendEscapedCSSIdent(aIdent, aReturn);
}

}  // namespace dom
}  // namespace mozilla
