/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoNamespaceRule.h"

#include "mozilla/ServoBindings.h"

using namespace mozilla::dom;

namespace mozilla {

ServoNamespaceRule::~ServoNamespaceRule()
{
}

NS_IMPL_ADDREF_INHERITED(ServoNamespaceRule, CSSNamespaceRule)
NS_IMPL_RELEASE_INHERITED(ServoNamespaceRule, CSSNamespaceRule)

NS_INTERFACE_MAP_BEGIN(ServoNamespaceRule)
NS_INTERFACE_MAP_END_INHERITING(CSSNamespaceRule)

#ifdef DEBUG
void
ServoNamespaceRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_NamespaceRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

already_AddRefed<css::Rule>
ServoNamespaceRule::Clone() const
{
  // Rule::Clone is only used when CSSStyleSheetInner is cloned in
  // preparation of being mutated. However, ServoStyleSheet never clones
  // anything, so this method should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be cloning ServoNamespaceRule");
  return nullptr;
}

nsIAtom*
ServoNamespaceRule::GetPrefix() const
{
  return Servo_NamespaceRule_GetPrefix(mRawRule);
}

void
ServoNamespaceRule::GetURLSpec(nsString& aURLSpec) const
{
  nsIAtom* atom = Servo_NamespaceRule_GetURI(mRawRule);
  atom->ToString(aURLSpec);
}

void
ServoNamespaceRule::GetCssTextImpl(nsAString& aCssText) const
{
  Servo_NamespaceRule_GetCssText(mRawRule, &aCssText);
}

size_t
ServoNamespaceRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

} // namespace mozilla
