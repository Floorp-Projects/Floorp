/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSStyleRule for stylo */

#include "mozilla/ServoStyleRule.h"

#include "mozilla/ServoBindings.h"

#include "nsDOMClassInfoID.h"

namespace mozilla {

// -- ServoStyleRule --------------------------------------------------

// QueryInterface implementation for ServoStyleRule
NS_INTERFACE_MAP_BEGIN(ServoStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, css::Rule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSStyleRule)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(ServoStyleRule)
NS_IMPL_RELEASE(ServoStyleRule)

already_AddRefed<css::Rule>
ServoStyleRule::Clone() const
{
  // Rule::Clone is only used when CSSStyleSheetInner is cloned in
  // preparation of being mutated. However, ServoStyleSheet never clones
  // anything, so this method should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be cloning ServoStyleRule");
  return nullptr;
}

size_t
ServoStyleRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

#ifdef DEBUG
void
ServoStyleRule::List(FILE* out, int32_t aIndent) const
{
  // TODO Implement this!
}
#endif

/* CSSRule implementation */

NS_IMETHODIMP
ServoStyleRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::STYLE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::GetCssText(nsAString& aCssText)
{
  Servo_StyleRule_GetCssText(mRawRule, &aCssText);
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::SetCssText(const nsAString& aCssText)
{
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return css::Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
ServoStyleRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  *aParentRule = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

css::Rule*
ServoStyleRule::GetCSSRule()
{
  return this;
}

/* CSSStyleRule implementation */

NS_IMETHODIMP
ServoStyleRule::GetSelectorText(nsAString& aSelectorText)
{
  Servo_StyleRule_GetSelectorText(mRawRule, &aSelectorText);
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::SetSelectorText(const nsAString& aSelectorText)
{
  // XXX We need to implement this... But Gecko doesn't have this either
  //     so it's probably okay to leave it unimplemented currently?
  //     See bug 37468 and mozilla::css::StyleRule::SetSelectorText.
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  *aStyle = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace mozilla
