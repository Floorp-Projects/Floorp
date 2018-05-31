/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoKeyframeRule.h"

#include "mozilla/DeclarationBlock.h"
#include "nsDOMCSSDeclaration.h"
#include "mozAutoDocUpdate.h"

namespace mozilla {

// -------------------------------------------
// ServoKeyframeDeclaration
//

class ServoKeyframeDeclaration : public nsDOMCSSDeclaration
{
public:
  explicit ServoKeyframeDeclaration(ServoKeyframeRule* aRule)
    : mRule(aRule)
  {
    mDecls = new DeclarationBlock(
      Servo_Keyframe_GetStyle(aRule->Raw()).Consume());
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(
    ServoKeyframeDeclaration, nsICSSDeclaration)

  css::Rule* GetParentRule() final { return mRule; }

  void DropReference() {
    mRule = nullptr;
    mDecls->SetOwningRule(nullptr);
  }

  DeclarationBlock* GetCSSDeclaration(Operation aOperation) final
  {
    return mDecls;
  }
  nsresult SetCSSDeclaration(DeclarationBlock* aDecls) final
  {
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
      nsIPrincipal* aSubjectPrincipal) const final
  {
    return GetParsingEnvironmentForRule(mRule);
  }
  nsIDocument* DocToUpdate() final { return nullptr; }

  nsINode* GetParentObject() final
  {
    return mRule ? mRule->GetParentObject() : nullptr;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t n = aMallocSizeOf(this);
    // TODO we may want to add size of mDecls as well
    return n;
  }

private:
  virtual ~ServoKeyframeDeclaration() {
    MOZ_ASSERT(!mRule, "Backpointer should have been cleared");
  }

  ServoKeyframeRule* mRule;
  RefPtr<DeclarationBlock> mDecls;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(ServoKeyframeDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ServoKeyframeDeclaration)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(ServoKeyframeDeclaration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoKeyframeDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

// -------------------------------------------
// ServoKeyframeRule
//

ServoKeyframeRule::~ServoKeyframeRule()
{
  if (mDeclaration) {
    mDeclaration->DropReference();
  }
}

NS_IMPL_ADDREF_INHERITED(ServoKeyframeRule, dom::CSSKeyframeRule)
NS_IMPL_RELEASE_INHERITED(ServoKeyframeRule, dom::CSSKeyframeRule)

// QueryInterface implementation for nsCSSKeyframeRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoKeyframeRule)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSKeyframeRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoKeyframeRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ServoKeyframeRule,
                                                dom::CSSKeyframeRule)
  if (tmp->mDeclaration) {
    tmp->mDeclaration->DropReference();
    tmp->mDeclaration = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoKeyframeRule,
                                                  dom::CSSKeyframeRule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeclaration)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
ServoKeyframeRule::IsCCLeaf() const
{
  return Rule::IsCCLeaf() && !mDeclaration;
}

#ifdef DEBUG
/* virtual */ void
ServoKeyframeRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_Keyframe_Debug(mRaw, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

template<typename Func>
void
ServoKeyframeRule::UpdateRule(Func aCallback)
{
  aCallback();

  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->RuleChanged(this);
  }
}

void
ServoKeyframeRule::GetKeyText(nsAString& aKeyText)
{
  Servo_Keyframe_GetKeyText(mRaw, &aKeyText);
}

void
ServoKeyframeRule::SetKeyText(const nsAString& aKeyText)
{
  NS_ConvertUTF16toUTF8 keyText(aKeyText);
  UpdateRule([this, &keyText]() {
    Servo_Keyframe_SetKeyText(mRaw, &keyText);
  });
}

void
ServoKeyframeRule::GetCssText(nsAString& aCssText) const
{
  Servo_Keyframe_GetCssText(mRaw, &aCssText);
}

nsICSSDeclaration*
ServoKeyframeRule::Style()
{
  if (!mDeclaration) {
    mDeclaration = new ServoKeyframeDeclaration(this);
  }
  return mDeclaration;
}

size_t
ServoKeyframeRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  if (mDeclaration) {
    n += mDeclaration->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

} // namespace mozilla
