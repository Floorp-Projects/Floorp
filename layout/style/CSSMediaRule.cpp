/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSMediaRule.h"

#include "mozilla/dom/CSSMediaRuleBinding.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

CSSMediaRule::CSSMediaRule(RefPtr<StyleMediaRule> aRawRule, StyleSheet* aSheet,
                           css::Rule* aParentRule, uint32_t aLine,
                           uint32_t aColumn)
    : ConditionRule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)) {}

CSSMediaRule::~CSSMediaRule() {
  if (mMediaList) {
    mMediaList->SetStyleSheet(nullptr);
  }
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(CSSMediaRule, css::ConditionRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSMediaRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CSSMediaRule,
                                                css::ConditionRule)
  if (tmp->mMediaList) {
    tmp->mMediaList->SetStyleSheet(nullptr);
    tmp->mMediaList = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSMediaRule,
                                                  css::ConditionRule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool CSSMediaRule::IsCCLeaf() const {
  return ConditionRule::IsCCLeaf() && !mMediaList;
}

/* virtual */
void CSSMediaRule::DropSheetReference() {
  if (mMediaList) {
    mMediaList->SetStyleSheet(nullptr);
  }
  ConditionRule::DropSheetReference();
}

void CSSMediaRule::SetRawAfterClone(RefPtr<StyleMediaRule> aRaw) {
  mRawRule = std::move(aRaw);
  if (mMediaList) {
    mMediaList->SetRawAfterClone(Servo_MediaRule_GetMedia(mRawRule).Consume());
    mMediaList->SetStyleSheet(nullptr);
    mMediaList->SetStyleSheet(GetStyleSheet());
  }
  css::ConditionRule::DidSetRawAfterClone();
}

already_AddRefed<StyleLockedCssRules> CSSMediaRule::GetOrCreateRawRules() {
  return Servo_MediaRule_GetRules(mRawRule).Consume();
}

StyleCssRuleType CSSMediaRule::Type() const { return StyleCssRuleType::Media; }

#ifdef DEBUG
/* virtual */
void CSSMediaRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_MediaRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

void CSSMediaRule::GetConditionText(nsACString& aConditionText) {
  Media()->GetMediaText(aConditionText);
}

/* virtual */
void CSSMediaRule::GetCssText(nsACString& aCssText) const {
  Servo_MediaRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ dom::MediaList* CSSMediaRule::Media() {
  if (!mMediaList) {
    mMediaList = new MediaList(Servo_MediaRule_GetMedia(mRawRule).Consume());
    mMediaList->SetStyleSheet(GetStyleSheet());
  }
  return mMediaList;
}

/* virtual */
size_t CSSMediaRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  // TODO Implement this!
  return aMallocSizeOf(this);
}

/* virtual */
JSObject* CSSMediaRule::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return CSSMediaRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
