/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSKeyframeRule.h"

#include "mozilla/DeclarationBlock.h"
#include "mozilla/dom/CSSKeyframeRuleBinding.h"
#include "nsDOMCSSDeclaration.h"

namespace mozilla::dom {

// -------------------------------------------
// CSSKeyframeDeclaration
//

class CSSKeyframeDeclaration : public nsDOMCSSDeclaration {
 public:
  explicit CSSKeyframeDeclaration(CSSKeyframeRule* aRule) : mRule(aRule) {
    mDecls =
        new DeclarationBlock(Servo_Keyframe_GetStyle(aRule->Raw()).Consume());
    mDecls->SetOwningRule(aRule);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(CSSKeyframeDeclaration,
                                                        nsICSSDeclaration)

  css::Rule* GetParentRule() final { return mRule; }

  void DropReference() {
    mRule = nullptr;
    mDecls->SetOwningRule(nullptr);
  }

  DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, DeclarationBlock** aCreated) final {
    if (aOperation != Operation::Read && mRule) {
      if (StyleSheet* sheet = mRule->GetStyleSheet()) {
        sheet->WillDirty();
      }
    }
    return mDecls;
  }
  nsresult SetCSSDeclaration(DeclarationBlock* aDecls,
                             MutationClosureData* aClosureData) final {
    if (!mRule) {
      return NS_OK;
    }
    mRule->UpdateRule([this, aDecls]() {
      if (mDecls != aDecls) {
        mDecls->SetOwningRule(nullptr);
        mDecls = aDecls;
        mDecls->SetOwningRule(mRule);
        Servo_Keyframe_SetStyle(mRule->Raw(), mDecls->Raw());
      }
    });
    return NS_OK;
  }
  ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal) const final {
    return GetParsingEnvironmentForRule(mRule, StyleCssRuleType::Keyframe);
  }
  Document* DocToUpdate() final { return nullptr; }

  nsINode* GetAssociatedNode() const final {
    return mRule ? mRule->GetAssociatedDocumentOrShadowRoot() : nullptr;
  }

  nsISupports* GetParentObject() const final {
    return mRule ? mRule->GetParentObject() : nullptr;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t n = aMallocSizeOf(this);
    // TODO we may want to add size of mDecls as well
    return n;
  }

  void SetRawAfterClone(RawServoKeyframe* aKeyframe) {
    mDecls->SetOwningRule(nullptr);
    mDecls = new DeclarationBlock(Servo_Keyframe_GetStyle(aKeyframe).Consume());
    mDecls->SetOwningRule(mRule);
  }

 private:
  virtual ~CSSKeyframeDeclaration() {
    MOZ_ASSERT(!mRule, "Backpointer should have been cleared");
  }

  CSSKeyframeRule* mRule;
  RefPtr<DeclarationBlock> mDecls;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(CSSKeyframeDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CSSKeyframeDeclaration)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(CSSKeyframeDeclaration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSKeyframeDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

// -------------------------------------------
// CSSKeyframeRule
//

CSSKeyframeRule::CSSKeyframeRule(already_AddRefed<RawServoKeyframe> aRaw,
                                 StyleSheet* aSheet, css::Rule* aParentRule,
                                 uint32_t aLine, uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn), mRaw(aRaw) {}

CSSKeyframeRule::~CSSKeyframeRule() {
  if (mDeclaration) {
    mDeclaration->DropReference();
  }
}

NS_IMPL_ADDREF_INHERITED(CSSKeyframeRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSKeyframeRule, css::Rule)

// QueryInterface implementation for nsCSSKeyframeRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSKeyframeRule)
NS_INTERFACE_MAP_END_INHERITING(css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSKeyframeRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CSSKeyframeRule, css::Rule)
  if (tmp->mDeclaration) {
    tmp->mDeclaration->DropReference();
    tmp->mDeclaration = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSKeyframeRule, css::Rule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeclaration)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool CSSKeyframeRule::IsCCLeaf() const {
  return Rule::IsCCLeaf() && !mDeclaration;
}

StyleCssRuleType CSSKeyframeRule::Type() const {
  return StyleCssRuleType::Keyframe;
}

void CSSKeyframeRule::SetRawAfterClone(RefPtr<RawServoKeyframe> aRaw) {
  mRaw = std::move(aRaw);

  if (mDeclaration) {
    mDeclaration->SetRawAfterClone(mRaw);
  }
}

#ifdef DEBUG
/* virtual */
void CSSKeyframeRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_Keyframe_Debug(mRaw, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

template <typename Func>
void CSSKeyframeRule::UpdateRule(Func aCallback) {
  if (IsReadOnly()) {
    return;
  }

  StyleSheet* sheet = GetStyleSheet();
  if (sheet) {
    sheet->WillDirty();
  }

  aCallback();

  if (sheet) {
    sheet->RuleChanged(this, StyleRuleChangeKind::Generic);
  }
}

void CSSKeyframeRule::GetKeyText(nsACString& aKeyText) {
  Servo_Keyframe_GetKeyText(mRaw, &aKeyText);
}

void CSSKeyframeRule::SetKeyText(const nsACString& aKeyText) {
  UpdateRule(
      [this, &aKeyText]() { Servo_Keyframe_SetKeyText(mRaw, &aKeyText); });
}

void CSSKeyframeRule::GetCssText(nsACString& aCssText) const {
  Servo_Keyframe_GetCssText(mRaw, &aCssText);
}

nsICSSDeclaration* CSSKeyframeRule::Style() {
  if (!mDeclaration) {
    mDeclaration = new CSSKeyframeDeclaration(this);
  }
  return mDeclaration;
}

size_t CSSKeyframeRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  if (mDeclaration) {
    n += mDeclaration->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

/* virtual */
JSObject* CSSKeyframeRule::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return CSSKeyframeRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
