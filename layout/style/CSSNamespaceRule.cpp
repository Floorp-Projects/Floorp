/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSNamespaceRule.h"

#include "mozilla/ServoBindings.h"

using namespace mozilla::dom;

namespace mozilla {

CSSNamespaceRule::~CSSNamespaceRule()
{
}

#ifdef DEBUG
void
CSSNamespaceRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_NamespaceRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

nsAtom*
CSSNamespaceRule::GetPrefix() const
{
  return Servo_NamespaceRule_GetPrefix(mRawRule);
}

void
CSSNamespaceRule::GetURLSpec(nsString& aURLSpec) const
{
  nsAtom* atom = Servo_NamespaceRule_GetURI(mRawRule);
  atom->ToString(aURLSpec);
}

void
CSSNamespaceRule::GetCssText(nsAString& aCssText) const
{
  Servo_NamespaceRule_GetCssText(mRawRule, &aCssText);
}

size_t
CSSNamespaceRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

} // namespace mozilla
