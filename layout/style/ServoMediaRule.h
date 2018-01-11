/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSMediaRule for stylo */

#ifndef mozilla_ServoMediaRule_h
#define mozilla_ServoMediaRule_h

#include "mozilla/dom/CSSMediaRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {

class ServoMediaList;

class ServoMediaRule final : public dom::CSSMediaRule
{
public:
  ServoMediaRule(RefPtr<RawServoMediaRule> aRawRule,
                 uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServoMediaRule, dom::CSSMediaRule)

  already_AddRefed<css::Rule> Clone() const override;
  bool UseForPresentation(nsPresContext* aPresContext,
                          nsMediaQueryResultCacheKey& aKey) final;
  void SetStyleSheet(StyleSheet* aSheet) override;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  RawServoMediaRule* Raw() const { return mRawRule; }

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final;
  void GetConditionText(nsAString& aConditionText) final;
  void SetConditionText(const nsAString& aConditionText,
                        ErrorResult& aRv) final;
  dom::MediaList* Media() final;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override;

private:
  virtual ~ServoMediaRule();

  RefPtr<RawServoMediaRule> mRawRule;
  RefPtr<ServoMediaList> mMediaList;
};

} // namespace mozilla

#endif // mozilla_ServoMediaRule_h
