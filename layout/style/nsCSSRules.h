/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
#include "nsIDOMCSSConditionRule.h"
#include "nsIDOMCSSFontFeatureValuesRule.h"
#include "nsIDOMCSSGroupingRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSSupportsRule.h"
#include "nsTArray.h"

class nsMediaList;

namespace mozilla {

class ErrorResult;

namespace dom {
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

  // nsIDOMCSSConditionRule interface
  NS_DECL_NSIDOMCSSCONDITIONRULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey) override;

  // @media rule methods
  nsresult SetMedia(nsMediaList* aMedia);

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const override;
  using CSSMediaRule::SetConditionText;
  dom::MediaList* Media() override;

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

  // nsIDOMCSSConditionRule interface
  NS_DECL_NSIDOMCSSCONDITIONRULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                  nsMediaQueryResultCacheKey& aKey) override;

  bool UseForPresentation(nsPresContext* aPresContext);

  static bool UseForPresentation(nsIDocument* aDoc,
                                 nsIURI* aDocURI,
                                 const nsACString& aDocURISpec,
                                 const nsACString& aPattern,
                                 URLMatchingFunction aUrlMatchingFunction);

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
  void GetCssTextImpl(nsAString& aCssText) const override;
  using dom::CSSMozDocumentRule::SetConditionText;

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override MOZ_MUST_OVERRIDE;

protected:
  void AppendConditionText(nsAString& aOutput) const;

  nsAutoPtr<URL> mURLs; // linked list of |struct URL| above.
};

} // namespace css

} // namespace mozilla

class nsCSSFontFeatureValuesRule final : public mozilla::css::Rule,
                                         public nsIDOMCSSFontFeatureValuesRule
{
public:
  nsCSSFontFeatureValuesRule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber)
  {
  }

  nsCSSFontFeatureValuesRule(const nsCSSFontFeatureValuesRule& aCopy)
    // copy everything except our reference count
    : mozilla::css::Rule(aCopy),
      mFamilyList(aCopy.mFamilyList),
      mFeatureValues(aCopy.mFeatureValues)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  virtual bool IsCCLeaf() const override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  using Rule::GetType;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSFontFaceRule interface
  NS_DECL_NSIDOMCSSFONTFEATUREVALUESRULE

  // WebIDL interface
  uint16_t Type() const override;
  void GetCssTextImpl(nsAString& aCssText) const override;
  // The XPCOM GetFontFamily is fine
  void SetFontFamily(const nsAString& aFamily, mozilla::ErrorResult& aRv);
  // The XPCOM GetValueText is fine
  void SetValueText(const nsAString& aFamily, mozilla::ErrorResult& aRv);

  const mozilla::FontFamilyList& GetFamilyList() { return mFamilyList; }
  void SetFamilyList(const mozilla::FontFamilyList& aFamilyList);

  void AddValueList(int32_t aVariantAlternate,
                    nsTArray<gfxFontFeatureValueSet::ValueList>& aValueList);

  const nsTArray<gfxFontFeatureValueSet::FeatureValues>& GetFeatureValues()
  {
    return mFeatureValues;
  }

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~nsCSSFontFeatureValuesRule() {}

  mozilla::FontFamilyList mFamilyList;
  nsTArray<gfxFontFeatureValueSet::FeatureValues> mFeatureValues;
};

class nsCSSKeyframeRule;

class nsCSSKeyframeStyleDeclaration final : public nsDOMCSSDeclaration
{
public:
  explicit nsCSSKeyframeStyleDeclaration(nsCSSKeyframeRule *aRule);

  NS_IMETHOD GetParentRule(nsIDOMCSSRule **aParent) override;
  void DropReference() { mRule = nullptr; }
  virtual mozilla::DeclarationBlock* GetCSSDeclaration(Operation aOperation) override;
  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock* aDecl) override;
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv) override;
  nsDOMCSSDeclaration::ServoCSSParsingEnvironment GetServoCSSParsingEnvironment() const final;
  virtual nsIDocument* DocToUpdate() override;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsCSSKeyframeStyleDeclaration,
                                                         nsICSSDeclaration)

  virtual nsINode* GetParentObject() override;

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

  // nsIDOMCSSKeyframeRule interface
  NS_IMETHOD GetKeyText(nsAString& aKeyText) final;
  NS_IMETHOD SetKeyText(const nsAString& aKeyText) final;

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final;
  nsICSSDeclaration* Style() final;

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
  nsCSSKeyframesRule(const nsAString& aName,
                     uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::dom::CSSKeyframesRule(aLineNumber, aColumnNumber)
    , mName(aName)
  {
  }
private:
  nsCSSKeyframesRule(const nsCSSKeyframesRule& aCopy);
  ~nsCSSKeyframesRule();
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Rule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSKeyframesRule interface
  NS_IMETHOD GetName(nsAString& aName) final;
  NS_IMETHOD SetName(const nsAString& aName) final;
  NS_IMETHOD AppendRule(const nsAString& aRule) final;
  NS_IMETHOD DeleteRule(const nsAString& aKey) final;
  using nsIDOMCSSKeyframesRule::FindRule;

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final;
  mozilla::dom::CSSRuleList* CssRules() final { return GroupRule::CssRules(); }
  nsCSSKeyframeRule* FindRule(const nsAString& aKey) final;

  const nsString& GetName() { return mName; }

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  uint32_t FindRuleIndexForKey(const nsAString& aKey);

  nsString                                   mName;
};

class nsCSSPageRule;

class nsCSSPageStyleDeclaration final : public nsDOMCSSDeclaration
{
public:
  explicit nsCSSPageStyleDeclaration(nsCSSPageRule *aRule);

  NS_IMETHOD GetParentRule(nsIDOMCSSRule **aParent) override;
  void DropReference() { mRule = nullptr; }
  virtual mozilla::DeclarationBlock* GetCSSDeclaration(Operation aOperation) override;
  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock* aDecl) override;
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv) override;
  nsDOMCSSDeclaration::ServoCSSParsingEnvironment GetServoCSSParsingEnvironment() const final;
  virtual nsIDocument* DocToUpdate() override;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsCSSPageStyleDeclaration,
                                                         nsICSSDeclaration)

  virtual nsINode *GetParentObject() override;

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

  // nsIDOMCSSConditionRule interface
  NS_DECL_NSIDOMCSSCONDITIONRULE

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const override;
  using dom::CSSSupportsRule::SetConditionText;

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~CSSSupportsRule();

  bool mUseGroup;
  nsString mCondition;
};

} // namespace mozilla

#endif /* !defined(nsCSSRules_h_) */
