/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rules in a CSS stylesheet other than style rules (e.g., @import rules) */

#ifndef nsCSSRules_h_
#define nsCSSRules_h_

#include "Declaration.h"
#include "StyleRule.h"
#include "gfxFontFeatures.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/SheetType.h"
#include "mozilla/css/GroupRule.h"
#include "mozilla/css/URLMatchingFunction.h"
#include "mozilla/dom/CSSFontFeatureValuesRule.h"
#include "mozilla/dom/CSSKeyframeRule.h"
#include "mozilla/dom/CSSKeyframesRule.h"
#include "mozilla/dom/CSSMediaRule.h"
#include "mozilla/dom/CSSPageRule.h"
#include "mozilla/dom/CSSSupportsRule.h"
#include "mozilla/dom/CSSMozDocumentRule.h"
#include "nsAutoPtr.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsDOMCSSDeclaration.h"
#include "nsTArray.h"

class nsMediaList;

namespace mozilla {

class ErrorResult;

namespace dom {
class DocGroup;
class MediaList;
}

namespace css {

class MediaRule final : public dom::CSSMediaRule
{
public:
  MediaRule(uint32_t aLineNumber, uint32_t aColumnNumber);
private:
  MediaRule(const MediaRule& aCopy);
  ~MediaRule();
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaRule, dom::CSSMediaRule)

  // Rule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual void SetStyleSheet(mozilla::StyleSheet* aSheet) override; //override GroupRule
  mozilla::CSSStyleSheet* GetStyleSheet() const
  {
    mozilla::StyleSheet* sheet = GroupRule::GetStyleSheet();
    return sheet ? sheet->AsGecko() : nullptr;
  }
  virtual already_AddRefed<Rule> Clone() const override;

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey) override;

  // @media rule methods
  nsresult SetMedia(nsMediaList* aMedia);

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final override;
  void GetConditionText(nsAString& aConditionText) final override;
  void SetConditionText(const nsAString& aConditionText,
                        ErrorResult& aRv) final override;
  dom::MediaList* Media() final override;

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override MOZ_MUST_OVERRIDE;

protected:
  void AppendConditionText(nsAString& aOutput) const;

  RefPtr<nsMediaList> mMedia;
};

class DocumentRule final : public dom::CSSMozDocumentRule
{
public:
  DocumentRule(uint32_t aLineNumber, uint32_t aColumnNumber);
private:
  DocumentRule(const DocumentRule& aCopy);
  ~DocumentRule();
public:

  NS_DECL_ISUPPORTS_INHERITED

  // Rule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual already_AddRefed<Rule> Clone() const override;

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                  nsMediaQueryResultCacheKey& aKey) override;

  bool UseForPresentation(nsPresContext* aPresContext);

  struct URL {
    URLMatchingFunction func;
    nsCString url;
    URL *next;

    URL() : next(nullptr) {}
    URL(const URL& aOther)
      : func(aOther.func)
      , url(aOther.url)
      , next(aOther.next ? new URL(*aOther.next) : nullptr)
    {
    }
    ~URL();
  };

  void SetURLs(URL *aURLs) { mURLs = aURLs; }

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final override;
  void GetConditionText(nsAString& aConditionText) final override;
  void SetConditionText(const nsAString& aConditionText,
                        ErrorResult& aRv) final override;

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override MOZ_MUST_OVERRIDE;

protected:
  void AppendConditionText(nsAString& aOutput) const;

  nsAutoPtr<URL> mURLs; // linked list of |struct URL| above.
};

} // namespace css

} // namespace mozilla

class nsCSSFontFeatureValuesRule final : public mozilla::dom::CSSFontFeatureValuesRule
{
public:
  nsCSSFontFeatureValuesRule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::dom::CSSFontFeatureValuesRule(aLineNumber, aColumnNumber)
  {
  }

  nsCSSFontFeatureValuesRule(const nsCSSFontFeatureValuesRule& aCopy)
    // copy everything except our reference count
    : mozilla::dom::CSSFontFeatureValuesRule(aCopy),
      mFamilyList(aCopy.mFamilyList),
      mFeatureValues(aCopy.mFeatureValues)
  {
  }

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final override;
#endif
  already_AddRefed<mozilla::css::Rule> Clone() const final override;

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final override;
  void GetFontFamily(nsAString& aFamily) final override;
  void SetFontFamily(const nsAString& aFamily, mozilla::ErrorResult& aRv) final override;
  void GetValueText(nsAString& aValueText) final override;
  void SetValueText(const nsAString& aValueText, mozilla::ErrorResult& aRv) final override;

  mozilla::SharedFontList* GetFamilyList() const { return mFamilyList; }
  void SetFamilyList(mozilla::SharedFontList* aFamilyList)
  {
    mFamilyList = aFamilyList;
  }

  void AddValueList(int32_t aVariantAlternate,
                    nsTArray<gfxFontFeatureValueSet::ValueList>& aValueList);

  const nsTArray<gfxFontFeatureValueSet::FeatureValues>& GetFeatureValues()
  {
    return mFeatureValues;
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final override;

protected:
  ~nsCSSFontFeatureValuesRule() {}

  RefPtr<mozilla::SharedFontList> mFamilyList;
  nsTArray<gfxFontFeatureValueSet::FeatureValues> mFeatureValues;
};

class nsCSSKeyframeRule;

class nsCSSKeyframeStyleDeclaration final : public nsDOMCSSDeclaration
{
public:
  explicit nsCSSKeyframeStyleDeclaration(nsCSSKeyframeRule *aRule);

  mozilla::css::Rule* GetParentRule() override;
  void DropReference() { mRule = nullptr; }
  virtual mozilla::DeclarationBlock* GetCSSDeclaration(Operation aOperation) override;
  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock* aDecl) override;
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv,
                                        nsIPrincipal* aSubjectPrincipal) override;
  nsDOMCSSDeclaration::ServoCSSParsingEnvironment
  GetServoCSSParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final override;
  virtual nsIDocument* DocToUpdate() override;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsCSSKeyframeStyleDeclaration,
                                                         nsICSSDeclaration)

  virtual nsINode* GetParentObject() override;
  virtual mozilla::dom::DocGroup* GetDocGroup() const override;

protected:
  virtual ~nsCSSKeyframeStyleDeclaration();

  // This reference is not reference-counted. The rule object tells us
  // when it's about to go away.
  nsCSSKeyframeRule* MOZ_NON_OWNING_REF mRule;
};

class nsCSSKeyframeRule final : public mozilla::dom::CSSKeyframeRule
{
public:
  // Steals the contents of aKeys, and takes the reference in Declaration
  nsCSSKeyframeRule(InfallibleTArray<float>&& aKeys,
                    already_AddRefed<mozilla::css::Declaration>&& aDeclaration,
                    uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::dom::CSSKeyframeRule(aLineNumber, aColumnNumber)
    , mKeys(mozilla::Move(aKeys))
    , mDeclaration(mozilla::Move(aDeclaration))
  {
    mDeclaration->SetOwningRule(this);
  }
private:
  nsCSSKeyframeRule(const nsCSSKeyframeRule& aCopy);
  ~nsCSSKeyframeRule();
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsCSSKeyframeRule,
                                           mozilla::dom::CSSKeyframeRule)
  virtual bool IsCCLeaf() const override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final override;
  void GetKeyText(nsAString& aKeyText) final override;
  void SetKeyText(const nsAString& aKeyText) final override;
  nsICSSDeclaration* Style() final override;

  const nsTArray<float>& GetKeys() const     { return mKeys; }
  mozilla::css::Declaration* Declaration()   { return mDeclaration; }

  void ChangeDeclaration(mozilla::css::Declaration* aDeclaration);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  void DoGetKeyText(nsAString &aKeyText) const;

private:
  nsTArray<float>                            mKeys;
  RefPtr<mozilla::css::Declaration>          mDeclaration;
  // lazily created when needed:
  RefPtr<nsCSSKeyframeStyleDeclaration>    mDOMDeclaration;
};

class nsCSSKeyframesRule final : public mozilla::dom::CSSKeyframesRule
{
public:
  nsCSSKeyframesRule(already_AddRefed<nsAtom> aName,
                     uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::dom::CSSKeyframesRule(aLineNumber, aColumnNumber)
    , mName(aName)
  {
  }
private:
  nsCSSKeyframesRule(const nsCSSKeyframesRule& aCopy);
  ~nsCSSKeyframesRule();
public:
  // Rule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final override;
  void GetName(nsAString& aName) const final override;
  void SetName(const nsAString& aName) final override;
  mozilla::dom::CSSRuleList* CssRules() final override { return GroupRule::CssRules(); }
  void AppendRule(const nsAString& aRule) final override;
  void DeleteRule(const nsAString& aKey) final override;
  nsCSSKeyframeRule* FindRule(const nsAString& aKey) final override;

  const nsAtom* GetName() const { return mName; }

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  uint32_t FindRuleIndexForKey(const nsAString& aKey);

  RefPtr<nsAtom> mName;
};

class nsCSSPageRule;

class nsCSSPageStyleDeclaration final : public nsDOMCSSDeclaration
{
public:
  explicit nsCSSPageStyleDeclaration(nsCSSPageRule *aRule);

  mozilla::css::Rule* GetParentRule() override;
  void DropReference() { mRule = nullptr; }
  virtual mozilla::DeclarationBlock* GetCSSDeclaration(Operation aOperation) override;
  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock* aDecl) override;
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv,
                                        nsIPrincipal* aSubjectPrincipal) override;
  nsDOMCSSDeclaration::ServoCSSParsingEnvironment
  GetServoCSSParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final override;
  virtual nsIDocument* DocToUpdate() override;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsCSSPageStyleDeclaration,
                                                         nsICSSDeclaration)

  virtual nsINode *GetParentObject() override;
  virtual mozilla::dom::DocGroup* GetDocGroup() const override;

protected:
  virtual ~nsCSSPageStyleDeclaration();

  // This reference is not reference-counted. The rule object tells us
  // when it's about to go away.
  nsCSSPageRule* MOZ_NON_OWNING_REF mRule;
};

class nsCSSPageRule final : public mozilla::dom::CSSPageRule
{
public:
  nsCSSPageRule(mozilla::css::Declaration* aDeclaration,
                uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::dom::CSSPageRule(aLineNumber, aColumnNumber)
    , mDeclaration(aDeclaration)
  {
    mDeclaration->SetOwningRule(this);
  }
private:
  nsCSSPageRule(const nsCSSPageRule& aCopy);
  ~nsCSSPageRule();
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsCSSPageRule, mozilla::dom::CSSPageRule)
  virtual bool IsCCLeaf() const override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // WebIDL interfaces
  virtual void GetCssTextImpl(nsAString& aCssText) const override;
  virtual nsICSSDeclaration* Style() override;

  mozilla::css::Declaration* Declaration()   { return mDeclaration; }

  void ChangeDeclaration(mozilla::css::Declaration* aDeclaration);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  RefPtr<mozilla::css::Declaration>     mDeclaration;
  // lazily created when needed:
  RefPtr<nsCSSPageStyleDeclaration>     mDOMDeclaration;
};

namespace mozilla {

class CSSSupportsRule final : public dom::CSSSupportsRule
{
public:
  CSSSupportsRule(bool aConditionMet, const nsString& aCondition,
                  uint32_t aLineNumber, uint32_t aColumnNumber);
  CSSSupportsRule(const CSSSupportsRule& aCopy);

  // Rule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                  nsMediaQueryResultCacheKey& aKey) override;

  NS_DECL_ISUPPORTS_INHERITED

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final override;
  void GetConditionText(nsAString& aConditionText) final override;
  void SetConditionText(const nsAString& aConditionText,
                        ErrorResult& aRv) final override;

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~CSSSupportsRule();

  bool mUseGroup;
  nsString mCondition;
};

} // namespace mozilla

#endif /* !defined(nsCSSRules_h_) */
