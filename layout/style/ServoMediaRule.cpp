/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSMediaRule for stylo */

#include "mozilla/ServoMediaRule.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/dom/MediaList.h"

using namespace mozilla::dom;

namespace mozilla {

ServoMediaRule::ServoMediaRule(RefPtr<RawServoMediaRule> aRawRule,
                               uint32_t aLine, uint32_t aColumn)
  : CSSMediaRule(Servo_MediaRule_GetRules(aRawRule).Consume(), aLine, aColumn)
  , mRawRule(std::move(aRawRule))
{
}

ServoMediaRule::~ServoMediaRule()
{
  if (mMediaList) {
    mMediaList->SetStyleSheet(nullptr);
  }
}

NS_IMPL_ADDREF_INHERITED(ServoMediaRule, CSSMediaRule)
NS_IMPL_RELEASE_INHERITED(ServoMediaRule, CSSMediaRule)

// QueryInterface implementation for MediaRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoMediaRule)
NS_INTERFACE_MAP_END_INHERITING(CSSMediaRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoMediaRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ServoMediaRule, CSSMediaRule)
  if (tmp->mMediaList) {
    tmp->mMediaList->SetStyleSheet(nullptr);
    tmp->mMediaList = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoMediaRule, CSSMediaRule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

/* virtual */ void
ServoMediaRule::SetStyleSheet(StyleSheet* aSheet)
{
  if (mMediaList) {
    mMediaList->SetStyleSheet(aSheet);
  }
  CSSMediaRule::SetStyleSheet(aSheet);
}

#ifdef DEBUG
/* virtual */ void
ServoMediaRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_MediaRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

void
ServoMediaRule::GetConditionText(nsAString& aConditionText)
{
  Media()->GetMediaText(aConditionText);
}

void
ServoMediaRule::SetConditionText(const nsAString& aConditionText,
                                 ErrorResult& aRv)
{
  Media()->SetMediaText(aConditionText);
}

/* virtual */ void
ServoMediaRule::GetCssText(nsAString& aCssText) const
{
  Servo_MediaRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ dom::MediaList*
ServoMediaRule::Media()
{
  if (!mMediaList) {
    mMediaList = new MediaList(Servo_MediaRule_GetMedia(mRawRule).Consume());
    mMediaList->SetStyleSheet(GetStyleSheet());
  }
  return mMediaList;
}

/* virtual */ size_t
ServoMediaRule::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

} // namespace mozilla
