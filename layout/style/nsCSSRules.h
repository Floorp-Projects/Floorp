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
#include "mozilla/dom/FontFace.h"
#include "nsAutoPtr.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsDOMCSSDeclaration.h"
#include "nsIDOMCSSConditionRule.h"
#include "nsIDOMCSSCounterStyleRule.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsIDOMCSSFontFeatureValuesRule.h"
#include "nsIDOMCSSGroupingRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSPageRule.h"
#include "nsIDOMCSSSupportsRule.h"
#include "nsIDOMCSSKeyframeRule.h"
#include "nsIDOMCSSKeyframesRule.h"
#include "nsTArray.h"

class nsMediaList;

namespace mozilla {

class ErrorResult;

namespace css {

class MediaRule final : public GroupRule,
                        public nsIDOMCSSMediaRule
{
public:
  MediaRule(uint32_t aLineNumber, uint32_t aColumnNumber);
private:
  MediaRule(const MediaRule& aCopy);
  ~MediaRule();
public:

  NS_DECL_ISUPPORTS_INHERITED

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
  virtual int32_t GetType() const override;
  virtual already_AddRefed<Rule> Clone() const override;
  virtual nsIDOMCSSRule* GetDOMRule() override
  {
    return this;
  }
  virtual nsIDOMCSSRule* GetExistingDOMRule() override
  {
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSGroupingRule interface
  NS_DECL_NSIDOMCSSGROUPINGRULE

  // nsIDOMCSSConditionRule interface
  NS_DECL_NSIDOMCSSCONDITIONRULE

  // nsIDOMCSSMediaRule interface
  NS_DECL_NSIDOMCSSMEDIARULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey) override;

  // @media rule methods
  nsresult SetMedia(nsMediaList* aMedia);
  
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override MOZ_MUST_OVERRIDE;

protected:
  void AppendConditionText(nsAString& aOutput);

  RefPtr<nsMediaList> mMedia;
};

class DocumentRule final : public GroupRule,
                           public nsIDOMCSSMozDocumentRule
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
  virtual int32_t GetType() const override;
  virtual already_AddRefed<Rule> Clone() const override;
  virtual nsIDOMCSSRule* GetDOMRule() override
  {
    return this;
  }
  virtual nsIDOMCSSRule* GetExistingDOMRule() override
  {
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSGroupingRule interface
  NS_DECL_NSIDOMCSSGROUPINGRULE

  // nsIDOMCSSConditionRule interface
  NS_DECL_NSIDOMCSSCONDITIONRULE

  // nsIDOMCSSMozDocumentRule interface
  NS_DECL_NSIDOMCSSMOZDOCUMENTRULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey) override;

  bool UseForPresentation(nsPresContext* aPresContext);

  enum Function {
    eURL,
    eURLPrefix,
    eDomain,
    eRegExp
  };

  struct URL {
    Function func;
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

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override MOZ_MUST_OVERRIDE;

protected:
  void AppendConditionText(nsAString& aOutput);

  nsAutoPtr<URL> mURLs; // linked list of |struct URL| above.
};

} // namespace css

struct CSSFontFaceDescriptors
{
#define CSS_FONT_DESC(name_, method_) nsCSSValue m##method_;
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC

  const nsCSSValue& Get(nsCSSFontDesc aFontDescID) const;
  nsCSSValue& Get(nsCSSFontDesc aFontDescID);

private:
  static nsCSSValue CSSFontFaceDescriptors::* const Fields[];
};

} // namespace mozilla

// A nsCSSFontFaceStyleDecl is always embedded in a nsCSSFontFaceRule.
class nsCSSFontFaceRule;
class nsCSSFontFaceStyleDecl final : public nsICSSDeclaration
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER
  NS_DECL_NSICSSDECLARATION
  virtual already_AddRefed<mozilla::dom::CSSValue>
  GetPropertyCSSValue(const nsAString& aProp, mozilla::ErrorResult& aRv)
    override;
  using nsICSSDeclaration::GetPropertyCSSValue;

  virtual nsINode *GetParentObject() override;
  virtual void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) override;

  nsresult GetPropertyValue(nsCSSFontDesc aFontDescID,
                            nsAString & aResult) const;

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~nsCSSFontFaceStyleDecl() {}

  friend class nsCSSFontFaceRule;

  inline nsCSSFontFaceRule* ContainingRule();
  inline const nsCSSFontFaceRule* ContainingRule() const;

  mozilla::CSSFontFaceDescriptors mDescriptors;

private:
  // NOT TO BE IMPLEMENTED
  // This object cannot be allocated on its own, only as part of
  // nsCSSFontFaceRule.
  void* operator new(size_t size) CPP_THROW_NEW;
};

class nsCSSFontFaceRule final : public mozilla::css::Rule,
                                public nsIDOMCSSFontFaceRule
{
public:
  nsCSSFontFaceRule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber) {}

  nsCSSFontFaceRule(const nsCSSFontFaceRule& aCopy)
    // copy everything except our reference count
    : mozilla::css::Rule(aCopy), mDecl(aCopy.mDecl) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsCSSFontFaceRule,
                                                         mozilla::css::Rule)

  // Rule methods
  DECL_STYLE_RULE_INHERIT
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSFontFaceRule interface
  NS_DECL_NSIDOMCSSFONTFACERULE

  void SetDesc(nsCSSFontDesc aDescID, nsCSSValue const & aValue);
  void GetDesc(nsCSSFontDesc aDescID, nsCSSValue & aValue);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  void GetDescriptors(mozilla::CSSFontFaceDescriptors& aDescriptors) const
    { aDescriptors = mDecl.mDescriptors; }

protected:
  ~nsCSSFontFaceRule() {}

  friend class nsCSSFontFaceStyleDecl;
  nsCSSFontFaceStyleDecl mDecl;
};

// nsFontFaceRuleContainer - used for associating sheet type with
// specific @font-face rules
struct nsFontFaceRuleContainer {
  RefPtr<nsCSSFontFaceRule> mRule;
  mozilla::SheetType mSheetType;
};

inline nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule()
{
  return reinterpret_cast<nsCSSFontFaceRule*>
    (reinterpret_cast<char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

inline const nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule() const
{
  return reinterpret_cast<const nsCSSFontFaceRule*>
    (reinterpret_cast<const char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

class nsCSSFontFeatureValuesRule final : public mozilla::css::Rule,
                                         public nsIDOMCSSFontFeatureValuesRule
{
public:
  nsCSSFontFeatureValuesRule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber) {}

  nsCSSFontFeatureValuesRule(const nsCSSFontFeatureValuesRule& aCopy)
    // copy everything except our reference count
    : mozilla::css::Rule(aCopy),
      mFamilyList(aCopy.mFamilyList),
      mFeatureValues(aCopy.mFeatureValues) {}

  NS_DECL_ISUPPORTS

  // Rule methods
  DECL_STYLE_RULE_INHERIT
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSFontFaceRule interface
  NS_DECL_NSIDOMCSSFONTFEATUREVALUESRULE

  const mozilla::FontFamilyList& GetFamilyList() { return mFamilyList; }
  void SetFamilyList(const mozilla::FontFamilyList& aFamilyList);

  void AddValueList(int32_t aVariantAlternate,
                    nsTArray<gfxFontFeatureValueSet::ValueList>& aValueList);

  const nsTArray<gfxFontFeatureValueSet::FeatureValues>& GetFeatureValues()
  {
    return mFeatureValues;
  }

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

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

class nsCSSKeyframeRule final : public mozilla::css::Rule,
                                public nsIDOMCSSKeyframeRule
{
public:
  // Steals the contents of aKeys, and takes the reference in Declaration
  nsCSSKeyframeRule(InfallibleTArray<float>&& aKeys,
                    already_AddRefed<mozilla::css::Declaration>&& aDeclaration,
                    uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber)
    , mKeys(mozilla::Move(aKeys))
    , mDeclaration(mozilla::Move(aDeclaration))
  {
    mDeclaration->SetOwningRule(this);
  }
private:
  nsCSSKeyframeRule(const nsCSSKeyframeRule& aCopy);
  ~nsCSSKeyframeRule();
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsCSSKeyframeRule, mozilla::css::Rule)

  // Rule methods
  DECL_STYLE_RULE_INHERIT
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSKeyframeRule interface
  NS_DECL_NSIDOMCSSKEYFRAMERULE

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

class nsCSSKeyframesRule final : public mozilla::css::GroupRule,
                                 public nsIDOMCSSKeyframesRule
{
public:
  nsCSSKeyframesRule(const nsSubstring& aName,
                     uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::GroupRule(aLineNumber, aColumnNumber)
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
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;
  virtual nsIDOMCSSRule* GetDOMRule() override
  {
    return this;
  }
  virtual nsIDOMCSSRule* GetExistingDOMRule() override
  {
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSKeyframesRule interface
  NS_DECL_NSIDOMCSSKEYFRAMESRULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey) override;

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

class nsCSSPageRule final : public mozilla::css::Rule,
                            public nsIDOMCSSPageRule
{
public:
  nsCSSPageRule(mozilla::css::Declaration* aDeclaration,
                uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber)
    , mDeclaration(aDeclaration)
  {
    mDeclaration->SetOwningRule(this);
  }
private:
  nsCSSPageRule(const nsCSSPageRule& aCopy);
  ~nsCSSPageRule();
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsCSSPageRule, nsIDOMCSSPageRule)

  // Rule methods
  DECL_STYLE_RULE_INHERIT
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSPageRule interface
  NS_DECL_NSIDOMCSSPAGERULE

  mozilla::css::Declaration* Declaration()   { return mDeclaration; }

  void ChangeDeclaration(mozilla::css::Declaration* aDeclaration);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;
private:
  RefPtr<mozilla::css::Declaration>     mDeclaration;
  // lazily created when needed:
  RefPtr<nsCSSPageStyleDeclaration>     mDOMDeclaration;
};

namespace mozilla {

class CSSSupportsRule : public css::GroupRule,
                        public nsIDOMCSSSupportsRule
{
public:
  CSSSupportsRule(bool aConditionMet, const nsString& aCondition,
                  uint32_t aLineNumber, uint32_t aColumnNumber);
  CSSSupportsRule(const CSSSupportsRule& aCopy);

  // Rule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                  nsMediaQueryResultCacheKey& aKey) override;
  virtual nsIDOMCSSRule* GetDOMRule() override
  {
    return this;
  }
  virtual nsIDOMCSSRule* GetExistingDOMRule() override
  {
    return this;
  }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSGroupingRule interface
  NS_DECL_NSIDOMCSSGROUPINGRULE

  // nsIDOMCSSConditionRule interface
  NS_DECL_NSIDOMCSSCONDITIONRULE

  // nsIDOMCSSSupportsRule interface
  NS_DECL_NSIDOMCSSSUPPORTSRULE

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~CSSSupportsRule();

  bool mUseGroup;
  nsString mCondition;
};

} // namespace mozilla

class nsCSSCounterStyleRule final : public mozilla::css::Rule,
                                    public nsIDOMCSSCounterStyleRule
{
public:
  explicit nsCSSCounterStyleRule(const nsAString& aName,
                                 uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber)
    , mName(aName)
    , mGeneration(0)
  {
  }

private:
  nsCSSCounterStyleRule(const nsCSSCounterStyleRule& aCopy);
  ~nsCSSCounterStyleRule();

public:
  NS_DECL_ISUPPORTS

  // Rule methods
  DECL_STYLE_RULE_INHERIT
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSCounterStyleRule
  NS_DECL_NSIDOMCSSCOUNTERSTYLERULE

  // This function is only used to check whether a non-empty value, which has
  // been accepted by parser, is valid for the given system and descriptor.
  static bool CheckDescValue(int32_t aSystem,
                             nsCSSCounterDesc aDescID,
                             const nsCSSValue& aValue);

  const nsString& GetName() const { return mName; }

  uint32_t GetGeneration() const { return mGeneration; }

  int32_t GetSystem() const;
  const nsCSSValue& GetSystemArgument() const;

  const nsCSSValue& GetDesc(nsCSSCounterDesc aDescID) const
  {
    MOZ_ASSERT(aDescID >= 0 && aDescID < eCSSCounterDesc_COUNT,
               "descriptor ID out of range");
    return mValues[aDescID];
  }

  void SetDesc(nsCSSCounterDesc aDescID, const nsCSSValue& aValue);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  typedef NS_STDCALL_FUNCPROTO(nsresult, Getter, nsCSSCounterStyleRule,
                               GetSymbols, (nsAString&));
  static const Getter kGetters[];

  nsresult GetDescriptor(nsCSSCounterDesc aDescID, nsAString& aValue);
  nsresult SetDescriptor(nsCSSCounterDesc aDescID, const nsAString& aValue);

  nsString   mName;
  nsCSSValue mValues[eCSSCounterDesc_COUNT];
  uint32_t   mGeneration;
};

#endif /* !defined(nsCSSRules_h_) */
