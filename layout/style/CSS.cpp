/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object holding utility CSS functions */

#include "CSS.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HighlightRegistry.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsStyleUtil.h"
#include "xpcpublic.h"

namespace mozilla::dom {

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

static Document* GetDocument(const GlobalObject& aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_DIAGNOSTIC_ASSERT(window, "CSS is only exposed to window globals");
  if (!window) {
    return nullptr;
  }
  return window->GetExtantDoc();
}

/* static */
HighlightRegistry* CSS::GetHighlights(const GlobalObject& aGlobal,
                                      ErrorResult& aRv) {
  Document* doc = GetDocument(aGlobal);
  if (!doc) {
    aRv.ThrowUnknownError("No document associated to this global?");
    return nullptr;
  }
  return &doc->HighlightRegistry();
}

/* static */
void CSS::RegisterProperty(const GlobalObject& aGlobal,
                           const PropertyDefinition& aDefinition,
                           ErrorResult& aRv) {
  Document* doc = GetDocument(aGlobal);
  if (!doc) {
    return aRv.ThrowUnknownError("No document associated to this global?");
  }
  doc->EnsureStyleSet().RegisterProperty(aDefinition, aRv);
}

}  // namespace mozilla::dom
