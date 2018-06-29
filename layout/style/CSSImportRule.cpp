/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSImportRule.h"

#include "mozilla/dom/CSSImportRuleBinding.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleSheet.h"

namespace mozilla {
namespace dom {

CSSImportRule::CSSImportRule(RefPtr<RawServoImportRule> aRawRule,
                             StyleSheet* aSheet,
                             css::Rule* aParentRule,
                             uint32_t aLine,
                             uint32_t aColumn)
  : css::Rule(aSheet, aParentRule, aLine, aColumn)
  , mRawRule(std::move(aRawRule))
{
  const auto* sheet = Servo_ImportRule_GetSheet(mRawRule.get());
  MOZ_ASSERT(sheet);
  mChildSheet = const_cast<StyleSheet*>(sheet);
  mChildSheet->SetOwnerRule(this);
}

CSSImportRule::~CSSImportRule()
{
  if (mChildSheet) {
    mChildSheet->SetOwnerRule(nullptr);
  }
}

// QueryInterface implementation for CSSImportRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSImportRule)
NS_INTERFACE_MAP_END_INHERITING(css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSImportRule)

NS_IMPL_ADDREF_INHERITED(CSSImportRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSImportRule, css::Rule)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSImportRule,
                                                  css::Rule)
  // Note the child sheet twice, since the Servo rule also holds a strong
  // reference to it.
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildSheet");
  cb.NoteXPCOMChild(tmp->mChildSheet);
  MOZ_ASSERT_IF(tmp->mRawRule,
                Servo_ImportRule_GetSheet(tmp->mRawRule) == tmp->mChildSheet);
  cb.NoteXPCOMChild(tmp->mChildSheet);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRawRule.stylesheet");
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSImportRule)
  if (tmp->mChildSheet) {
    tmp->mChildSheet->SetOwnerRule(nullptr);
    tmp->mChildSheet = nullptr;
  }
  tmp->mRawRule = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(css::Rule)

#ifdef DEBUG
/* virtual */ void
CSSImportRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_ImportRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

dom::MediaList*
CSSImportRule::GetMedia() const
{
  // When Bug 1326509 is fixed, we can assert mChildSheet instead.
  return mChildSheet ? mChildSheet->Media() : nullptr;
}

void
CSSImportRule::GetHref(nsAString& aHref) const
{
  Servo_ImportRule_GetHref(mRawRule, &aHref);
}

/* virtual */ void
CSSImportRule::GetCssText(nsAString& aCssText) const
{
  Servo_ImportRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ size_t
CSSImportRule::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

bool
CSSImportRule::IsCCLeaf() const
{
  // We're not a leaf.
  return false;
}

/* virtual */ JSObject*
CSSImportRule::WrapObject(JSContext* aCx,
                          JS::Handle<JSObject*> aGivenProto)
{
  return CSSImportRule_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
