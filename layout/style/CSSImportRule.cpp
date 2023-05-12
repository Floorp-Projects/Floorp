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

CSSImportRule::CSSImportRule(RefPtr<StyleLockedImportRule> aRawRule,
                             StyleSheet* aSheet, css::Rule* aParentRule,
                             uint32_t aLine, uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)) {
  const auto* sheet = Servo_ImportRule_GetSheet(mRawRule.get());
  mChildSheet = const_cast<StyleSheet*>(sheet);
  if (mChildSheet) {
    mChildSheet->AddReferencingRule(*this);
  }
}

CSSImportRule::~CSSImportRule() {
  if (mChildSheet) {
    mChildSheet->RemoveReferencingRule(*this);
  }
}

// QueryInterface implementation for CSSImportRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSImportRule)
NS_INTERFACE_MAP_END_INHERITING(css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSImportRule)

NS_IMPL_ADDREF_INHERITED(CSSImportRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSImportRule, css::Rule)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSImportRule, css::Rule)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildSheet");
  cb.NoteXPCOMChild(tmp->mChildSheet);
  MOZ_ASSERT_IF(tmp->mRawRule,
                Servo_ImportRule_GetSheet(tmp->mRawRule) == tmp->mChildSheet);
  // Note the child sheet twice, since the Servo rule also holds a strong
  // reference to it, but only if we're the "primary" rule reference.
  if (tmp->mChildSheet && tmp->mChildSheet->GetOwnerRule() == tmp) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRawRule.stylesheet");
    cb.NoteXPCOMChild(tmp->mChildSheet);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSImportRule)
  if (tmp->mChildSheet) {
    tmp->mChildSheet->RemoveReferencingRule(*tmp);
    tmp->mChildSheet = nullptr;
  }
  tmp->mRawRule = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(css::Rule)

#ifdef DEBUG
/* virtual */
void CSSImportRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_ImportRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSImportRule::Type() const {
  return StyleCssRuleType::Import;
}

void CSSImportRule::SetRawAfterClone(RefPtr<StyleLockedImportRule> aRaw) {
  mRawRule = std::move(aRaw);
  if (mChildSheet) {
    mChildSheet->RemoveReferencingRule(*this);
  }
  mChildSheet =
      const_cast<StyleSheet*>(Servo_ImportRule_GetSheet(mRawRule.get()));
  if (mChildSheet) {
    mChildSheet->AddReferencingRule(*this);
  }
}

StyleSheet* CSSImportRule::GetStyleSheetForBindings() {
  if (mChildSheet) {
    // FIXME(emilio): This is needed to make sure we don't expose shared sheets
    // to the OM (see wpt /css/cssom/cssimportrule-sheet-identity.html for
    // example).
    //
    // Perhaps instead we could create a clone of the stylesheet and keep it in
    // mChildSheet, without calling EnsureUniqueInner(), or something like that?
    if (StyleSheet* parent = GetParentStyleSheet()) {
      parent->EnsureUniqueInner();
    }
  }
  return mChildSheet;
}

dom::MediaList* CSSImportRule::GetMedia() {
  auto* sheet = GetStyleSheetForBindings();
  return sheet ? sheet->Media() : nullptr;
}

void CSSImportRule::DropSheetReference() {
  if (mChildSheet) {
    mChildSheet->RemoveFromParent();
  }
  Rule::DropSheetReference();
}

void CSSImportRule::GetHref(nsAString& aHref) const {
  Servo_ImportRule_GetHref(mRawRule, &aHref);
}

void CSSImportRule::GetLayerName(nsACString& aName) const {
  Servo_ImportRule_GetLayerName(mRawRule, &aName);
}

void CSSImportRule::GetSupportsText(nsACString& aSupportsText) const {
  Servo_ImportRule_GetSupportsText(mRawRule, &aSupportsText);
}

/* virtual */
void CSSImportRule::GetCssText(nsACString& aCssText) const {
  Servo_ImportRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */
size_t CSSImportRule::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  // TODO Implement this!
  return aMallocSizeOf(this);
}

bool CSSImportRule::IsCCLeaf() const {
  // We're not a leaf.
  return false;
}

/* virtual */
JSObject* CSSImportRule::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return CSSImportRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
