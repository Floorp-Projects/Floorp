/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Representation of CSSMozDocumentRule for stylo */

#ifndef mozilla_ServoMozDocumentRule_h
#define mozilla_ServoMozDocumentRule_h

#include "mozilla/dom/CSSMozDocumentRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {

class ServoDocumentRule final : public dom::CSSMozDocumentRule
{
public:
  ServoDocumentRule(RefPtr<RawServoDocumentRule> aRawRule,
                    uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED

  already_AddRefed<css::Rule> Clone() const override;
  bool UseForPresentation(nsPresContext* aPresContext,
                          nsMediaQueryResultCacheKey& aKey) final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  RawServoDocumentRule* Raw() const { return mRawRule; }

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final;
  void GetConditionText(nsAString& aConditionText) final;
  void SetConditionText(const nsAString& aConditionText,
                        ErrorResult& aRv) final;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override;

private:
  virtual ~ServoDocumentRule();

  RefPtr<RawServoDocumentRule> mRawRule;
};

} // namespace mozilla

#endif // mozilla_ServoDocumentRule_h
