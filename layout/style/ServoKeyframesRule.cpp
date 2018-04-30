/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoKeyframesRule.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoKeyframeRule.h"

#include <limits>

namespace mozilla {

// -------------------------------------------
// ServoKeyframeList
//

class ServoKeyframeList : public dom::CSSRuleList
{
public:
  explicit ServoKeyframeList(already_AddRefed<RawServoKeyframesRule> aRawRule)
    : mRawRule(aRawRule)
  {
    mRules.SetCount(Servo_KeyframesRule_GetCount(mRawRule));
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServoKeyframeList, dom::CSSRuleList)

  void SetParentRule(ServoKeyframesRule* aParentRule)
  {
    mParentRule = aParentRule;
    for (css::Rule* rule : mRules) {
      if (rule) {
        rule->SetParentRule(aParentRule);
      }
    }
  }
  void SetStyleSheet(StyleSheet* aSheet)
  {
    mStyleSheet = aSheet;
    for (css::Rule* rule : mRules) {
      if (rule) {
        rule->SetStyleSheet(aSheet);
      }
    }
  }

  StyleSheet* GetParentObject() final { return mStyleSheet; }

  ServoKeyframeRule* GetRule(uint32_t aIndex) {
    if (!mRules[aIndex]) {
      uint32_t line = 0, column = 0;
      RefPtr<RawServoKeyframe> rule =
        Servo_KeyframesRule_GetKeyframeAt(mRawRule, aIndex,
                                          &line, &column).Consume();
      ServoKeyframeRule* ruleObj =
        new ServoKeyframeRule(rule.forget(), line, column);
      mRules.ReplaceObjectAt(ruleObj, aIndex);
      ruleObj->SetStyleSheet(mStyleSheet);
      ruleObj->SetParentRule(mParentRule);
    }
    return static_cast<ServoKeyframeRule*>(mRules[aIndex]);
  }

  ServoKeyframeRule* IndexedGetter(uint32_t aIndex, bool& aFound) final
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

  void DropReference()
  {
    mStyleSheet = nullptr;
    mParentRule = nullptr;
    for (css::Rule* rule : mRules) {
      if (rule) {
        rule->SetStyleSheet(nullptr);
        rule->SetParentRule(nullptr);
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
  virtual ~ServoKeyframeList() {
    MOZ_ASSERT(!mParentRule, "Backpointer should have been cleared");
    MOZ_ASSERT(!mStyleSheet, "Backpointer should have been cleared");
    DropAllRules();
  }

  void DropAllRules()
  {
    if (mParentRule || mStyleSheet) {
      DropReference();
    }
    mRules.Clear();
    mRawRule = nullptr;
  }

  // may be nullptr when the style sheet drops the reference to us.
  StyleSheet* mStyleSheet = nullptr;
  ServoKeyframesRule* mParentRule = nullptr;
  RefPtr<RawServoKeyframesRule> mRawRule;
  nsCOMArray<css::Rule> mRules;
};

// QueryInterface implementation for ServoKeyframeList
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoKeyframeList)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSRuleList)

NS_IMPL_ADDREF_INHERITED(ServoKeyframeList, dom::CSSRuleList)
NS_IMPL_RELEASE_INHERITED(ServoKeyframeList, dom::CSSRuleList)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoKeyframeList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ServoKeyframeList)
  tmp->DropAllRules();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(dom::CSSRuleList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoKeyframeList,
                                                  dom::CSSRuleList)
  for (css::Rule* rule : tmp->mRules) {
    if (rule) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRules[i]");
      cb.NoteXPCOMChild(rule);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// -------------------------------------------
// ServoKeyframesRule
//

ServoKeyframesRule::ServoKeyframesRule(RefPtr<RawServoKeyframesRule> aRawRule,
                                       uint32_t aLine, uint32_t aColumn)
  : dom::CSSKeyframesRule(aLine, aColumn)
  , mRawRule(Move(aRawRule))
{
}

ServoKeyframesRule::~ServoKeyframesRule()
{
  if (mKeyframeList) {
    mKeyframeList->DropReference();
  }
}

NS_IMPL_ADDREF_INHERITED(ServoKeyframesRule, dom::CSSKeyframesRule)
NS_IMPL_RELEASE_INHERITED(ServoKeyframesRule, dom::CSSKeyframesRule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoKeyframesRule)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSKeyframesRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoKeyframesRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ServoKeyframesRule,
                                                dom::CSSKeyframesRule)
  if (tmp->mKeyframeList) {
    tmp->mKeyframeList->DropReference();
    tmp->mKeyframeList = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoKeyframesRule, Rule)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mKeyframeList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

/* virtual */ bool
ServoKeyframesRule::IsCCLeaf() const
{
  // If we don't have rule list constructed, we are a leaf.
  return Rule::IsCCLeaf() && !mKeyframeList;
}

#ifdef DEBUG
/* virtual */ void
ServoKeyframesRule::List(FILE* out, int32_t aIndent) const
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
ServoKeyframesRule::SetStyleSheet(StyleSheet* aSheet)
{
  if (mKeyframeList) {
    mKeyframeList->SetStyleSheet(aSheet);
  }
  dom::CSSKeyframesRule::SetStyleSheet(aSheet);
}

static const uint32_t kRuleNotFound = std::numeric_limits<uint32_t>::max();

uint32_t
ServoKeyframesRule::FindRuleIndexForKey(const nsAString& aKey)
{
  NS_ConvertUTF16toUTF8 key(aKey);
  return Servo_KeyframesRule_FindRule(mRawRule, &key);
}

template<typename Func>
void
ServoKeyframesRule::UpdateRule(Func aCallback)
{
  nsIDocument* doc = GetDocument();
  MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

  aCallback();

  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->RuleChanged(this);
  }
}

void
ServoKeyframesRule::GetName(nsAString& aName) const
{
  nsAtom* name = Servo_KeyframesRule_GetName(mRawRule);
  aName = nsDependentAtomString(name);
}

void
ServoKeyframesRule::SetName(const nsAString& aName)
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
ServoKeyframesRule::AppendRule(const nsAString& aRule)
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
ServoKeyframesRule::DeleteRule(const nsAString& aKey)
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
ServoKeyframesRule::GetCssText(nsAString& aCssText) const
{
  Servo_KeyframesRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ dom::CSSRuleList*
ServoKeyframesRule::CssRules()
{
  if (!mKeyframeList) {
    mKeyframeList = new ServoKeyframeList(do_AddRef(mRawRule));
    mKeyframeList->SetParentRule(this);
    if (StyleSheet* sheet = GetStyleSheet()) {
      mKeyframeList->SetStyleSheet(sheet);
    }
  }
  return mKeyframeList;
}

/* virtual */ dom::CSSKeyframeRule*
ServoKeyframesRule::FindRule(const nsAString& aKey)
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
ServoKeyframesRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  if (mKeyframeList) {
    n += mKeyframeList->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

} // namespace mozilla
