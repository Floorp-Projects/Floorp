/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSKeyframesRule.h"

#include "mozilla/dom/CSSKeyframesRuleBinding.h"
#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/ServoBindings.h"

#include <limits>

namespace mozilla {
namespace dom {

// -------------------------------------------
// CSSKeyframeList
//

class CSSKeyframeList : public dom::CSSRuleList
{
public:
  CSSKeyframeList(already_AddRefed<RawServoKeyframesRule> aRawRule,
                  StyleSheet* aSheet,
                  CSSKeyframesRule* aParentRule)
    : mStyleSheet(aSheet)
    , mParentRule(aParentRule)
    , mRawRule(aRawRule)
  {
    mRules.SetCount(Servo_KeyframesRule_GetCount(mRawRule));
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CSSKeyframeList, dom::CSSRuleList)

  void DropSheetReference()
  {
    if (!mStyleSheet) {
      return;
    }
    mStyleSheet = nullptr;
    for (css::Rule* rule : mRules) {
      if (rule) {
        rule->DropSheetReference();
      }
    }
  }

  StyleSheet* GetParentObject() final { return mStyleSheet; }

  CSSKeyframeRule* GetRule(uint32_t aIndex) {
    if (!mRules[aIndex]) {
      uint32_t line = 0, column = 0;
      RefPtr<RawServoKeyframe> rule =
        Servo_KeyframesRule_GetKeyframeAt(mRawRule, aIndex,
                                          &line, &column).Consume();
      CSSKeyframeRule* ruleObj =
        new CSSKeyframeRule(rule.forget(), mStyleSheet, mParentRule,
                            line, column);
      mRules.ReplaceObjectAt(ruleObj, aIndex);
    }
    return static_cast<CSSKeyframeRule*>(mRules[aIndex]);
  }

  CSSKeyframeRule* IndexedGetter(uint32_t aIndex, bool& aFound) final
  {
    if (aIndex >= mRules.Length()) {
      aFound = false;
      return nullptr;
    }
    aFound = true;
    return GetRule(aIndex);
  }

  void AppendRule() {
    mRules.AppendObject(nullptr);
  }

  void RemoveRule(uint32_t aIndex) {
    mRules.RemoveObjectAt(aIndex);
  }

  uint32_t Length() final { return mRules.Length(); }

  void DropReferences()
  {
    if (!mStyleSheet && !mParentRule) {
      return;
    }
    mStyleSheet = nullptr;
    mParentRule = nullptr;
    for (css::Rule* rule : mRules) {
      if (rule) {
        rule->DropParentRuleReference();
        rule->DropSheetReference();
      }
    }
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t n = aMallocSizeOf(this);
    for (const css::Rule* rule : mRules) {
      n += rule ? rule->SizeOfIncludingThis(aMallocSizeOf) : 0;
    }
    return n;
  }

private:
  virtual ~CSSKeyframeList() {
    MOZ_ASSERT(!mParentRule, "Backpointer should have been cleared");
    MOZ_ASSERT(!mStyleSheet, "Backpointer should have been cleared");
    DropAllRules();
  }

  void DropAllRules()
  {
    DropReferences();
    mRules.Clear();
    mRawRule = nullptr;
  }

  // may be nullptr when the style sheet drops the reference to us.
  StyleSheet* mStyleSheet = nullptr;
  CSSKeyframesRule* mParentRule = nullptr;
  RefPtr<RawServoKeyframesRule> mRawRule;
  nsCOMArray<css::Rule> mRules;
};

// QueryInterface implementation for CSSKeyframeList
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSKeyframeList)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSRuleList)

NS_IMPL_ADDREF_INHERITED(CSSKeyframeList, dom::CSSRuleList)
NS_IMPL_RELEASE_INHERITED(CSSKeyframeList, dom::CSSRuleList)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSKeyframeList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSKeyframeList)
  tmp->DropAllRules();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(dom::CSSRuleList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSKeyframeList,
                                                  dom::CSSRuleList)
  for (css::Rule* rule : tmp->mRules) {
    if (rule) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRules[i]");
      cb.NoteXPCOMChild(rule);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// -------------------------------------------
// CSSKeyframesRule
//

CSSKeyframesRule::CSSKeyframesRule(RefPtr<RawServoKeyframesRule> aRawRule,
                                   StyleSheet* aSheet,
                                   css::Rule* aParentRule,
                                   uint32_t aLine,
                                   uint32_t aColumn)
  : css::Rule(aSheet, aParentRule, aLine, aColumn)
  , mRawRule(std::move(aRawRule))
{
}

CSSKeyframesRule::~CSSKeyframesRule()
{
  if (mKeyframeList) {
    mKeyframeList->DropReferences();
  }
}

NS_IMPL_ADDREF_INHERITED(CSSKeyframesRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSKeyframesRule, css::Rule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSKeyframesRule)
NS_INTERFACE_MAP_END_INHERITING(css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSKeyframesRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CSSKeyframesRule,
                                                css::Rule)
  if (tmp->mKeyframeList) {
    tmp->mKeyframeList->DropReferences();
    tmp->mKeyframeList = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSKeyframesRule, Rule)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mKeyframeList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

/* virtual */ bool
CSSKeyframesRule::IsCCLeaf() const
{
  // If we don't have rule list constructed, we are a leaf.
  return Rule::IsCCLeaf() && !mKeyframeList;
}

#ifdef DEBUG
/* virtual */ void
CSSKeyframesRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_KeyframesRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

/* virtual */ void
CSSKeyframesRule::DropSheetReference()
{
  if (mKeyframeList) {
    mKeyframeList->DropSheetReference();
  }
  css::Rule::DropSheetReference();
}

static const uint32_t kRuleNotFound = std::numeric_limits<uint32_t>::max();

uint32_t
CSSKeyframesRule::FindRuleIndexForKey(const nsAString& aKey)
{
  NS_ConvertUTF16toUTF8 key(aKey);
  return Servo_KeyframesRule_FindRule(mRawRule, &key);
}

template<typename Func>
void
CSSKeyframesRule::UpdateRule(Func aCallback)
{
  aCallback();
  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->RuleChanged(this);
  }
}

void
CSSKeyframesRule::GetName(nsAString& aName) const
{
  nsAtom* name = Servo_KeyframesRule_GetName(mRawRule);
  aName = nsDependentAtomString(name);
}

void
CSSKeyframesRule::SetName(const nsAString& aName)
{
  RefPtr<nsAtom> name = NS_Atomize(aName);
  nsAtom* oldName = Servo_KeyframesRule_GetName(mRawRule);
  if (name == oldName) {
    return;
  }

  UpdateRule([this, &name]() {
    Servo_KeyframesRule_SetName(mRawRule, name.forget().take());
  });
}

void
CSSKeyframesRule::AppendRule(const nsAString& aRule)
{
  StyleSheet* sheet = GetStyleSheet();
  if (!sheet) {
    // We cannot parse the rule if we don't have a stylesheet.
    return;
  }

  NS_ConvertUTF16toUTF8 rule(aRule);
  UpdateRule([this, sheet, &rule]() {
    bool parsedOk = Servo_KeyframesRule_AppendRule(
      mRawRule, sheet->RawContents(), &rule);
    if (parsedOk && mKeyframeList) {
      mKeyframeList->AppendRule();
    }
  });
}

void
CSSKeyframesRule::DeleteRule(const nsAString& aKey)
{
  auto index = FindRuleIndexForKey(aKey);
  if (index == kRuleNotFound) {
    return;
  }

  UpdateRule([this, index]() {
    Servo_KeyframesRule_DeleteRule(mRawRule, index);
    if (mKeyframeList) {
      mKeyframeList->RemoveRule(index);
    }
  });
}

/* virtual */ void
CSSKeyframesRule::GetCssText(nsAString& aCssText) const
{
  Servo_KeyframesRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ dom::CSSRuleList*
CSSKeyframesRule::CssRules()
{
  if (!mKeyframeList) {
    mKeyframeList = new CSSKeyframeList(do_AddRef(mRawRule), mSheet, this);
  }
  return mKeyframeList;
}

/* virtual */ dom::CSSKeyframeRule*
CSSKeyframesRule::FindRule(const nsAString& aKey)
{
  auto index = FindRuleIndexForKey(aKey);
  if (index != kRuleNotFound) {
    // Construct mKeyframeList but ignore the result.
    CssRules();
    return mKeyframeList->GetRule(index);
  }
  return nullptr;
}

/* virtual */ size_t
CSSKeyframesRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  if (mKeyframeList) {
    n += mKeyframeList->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

/* virtual */ JSObject*
CSSKeyframesRule::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CSSKeyframesRule_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
