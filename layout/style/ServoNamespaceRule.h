/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoNamespaceRule_h
#define mozilla_ServoNamespaceRule_h

#include "mozilla/ServoBindingTypes.h"
#include "mozilla/dom/CSSNamespaceRule.h"

namespace mozilla {

class ServoNamespaceRule : public dom::CSSNamespaceRule
{
public:
  ServoNamespaceRule(already_AddRefed<RawServoNamespaceRule> aRule,
                     uint32_t aLine, uint32_t aColumn)
    : CSSNamespaceRule(aLine, aColumn)
    , mRawRule(std::move(aRule))
  {
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ServoNamespaceRule,
                                       dom::CSSNamespaceRule)

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  nsAtom* GetPrefix() const final;
  void GetURLSpec(nsString& aURLSpec) const final;

  // WebIDL interface
  void GetCssText(nsAString& aCssText) const final;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

private:
  ~ServoNamespaceRule();

  RefPtr<RawServoNamespaceRule> mRawRule;
};

} // namespace mozilla

#endif // mozilla_ServoNamespaceRule_h
