/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing data from presentational
 * HTML attributes
 */

#ifndef nsHTMLStyleSheet_h_
#define nsHTMLStyleSheet_h_

#include "nsColor.h"
#include "nsCOMPtr.h"
#include "nsIStyleRule.h"
#include "nsIStyleRuleProcessor.h"
#include "PLDHashTable.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsString.h"

class nsIDocument;
class nsMappedAttributes;

class nsHTMLStyleSheet final : public nsIStyleRuleProcessor
{
public:
  explicit nsHTMLStyleSheet(nsIDocument* aDocument);

  void SetOwningDocument(nsIDocument* aDocument);

  NS_DECL_ISUPPORTS

  // nsIStyleRuleProcessor API
  virtual void RulesMatching(ElementRuleProcessorData* aData) override;
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) override;
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) override;
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) override;
#endif
  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) override;
  virtual nsRestyleHint HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData) override;
  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) override;
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                               mozilla::RestyleHintData& aRestyleHintDataResult) override;
  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) override;
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;
  size_t DOMSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  void Reset();
  nsresult SetLinkColor(nscolor aColor);
  nsresult SetActiveLinkColor(nscolor aColor);
  nsresult SetVisitedLinkColor(nscolor aColor);

  // Mapped Attribute management methods
  already_AddRefed<nsMappedAttributes>
    UniqueMappedAttributes(nsMappedAttributes* aMapped);
  void DropMappedAttributes(nsMappedAttributes* aMapped);

  nsIStyleRule* LangRuleFor(const nsString& aLanguage);

private: 
  nsHTMLStyleSheet(const nsHTMLStyleSheet& aCopy) = delete;
  nsHTMLStyleSheet& operator=(const nsHTMLStyleSheet& aCopy) = delete;

  ~nsHTMLStyleSheet() {}

  class HTMLColorRule;
  friend class HTMLColorRule;
  class HTMLColorRule final : public nsIStyleRule {
  private:
    ~HTMLColorRule() {}
  public:
    HTMLColorRule() {}

    NS_DECL_ISUPPORTS

    // nsIStyleRule interface
    virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
    virtual bool MightMapInheritedStyleData() override;
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
  #endif

    nscolor             mColor;
  };

  // Implementation of SetLink/VisitedLink/ActiveLinkColor
  nsresult ImplLinkColorSetter(RefPtr<HTMLColorRule>& aRule, nscolor aColor);

  class GenericTableRule;
  friend class GenericTableRule;
  class GenericTableRule : public nsIStyleRule {
  protected:
    virtual ~GenericTableRule() {}
  public:
    GenericTableRule() {}

    NS_DECL_ISUPPORTS

    // nsIStyleRule interface
    virtual void MapRuleInfoInto(nsRuleData* aRuleData) override = 0;
    virtual bool MightMapInheritedStyleData() override = 0;
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
  #endif
  };

  // this rule handles <th> inheritance
  class TableTHRule;
  friend class TableTHRule;
  class TableTHRule final : public GenericTableRule {
  public:
    TableTHRule() {}

    virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
    virtual bool MightMapInheritedStyleData() override;
  };

  // Rule to handle quirk table colors
  class TableQuirkColorRule final : public GenericTableRule {
  public:
    TableQuirkColorRule() {}

    virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
    virtual bool MightMapInheritedStyleData() override;
  };

public: // for mLangRuleTable structures only

  // Rule to handle xml:lang attributes, of which we have exactly one
  // per language string, maintained in mLangRuleTable.
  // We also create one extra rule for the "x-math" language string, used on
  // <math> elements.
  class LangRule final : public nsIStyleRule {
  private:
    ~LangRule() {}
  public:
    explicit LangRule(const nsSubstring& aLang) : mLang(aLang) {}

    NS_DECL_ISUPPORTS

    // nsIStyleRule interface
    virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
    virtual bool MightMapInheritedStyleData() override;
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
  #endif

    nsString mLang;
  };

private:
  nsIDocument*            mDocument;
  RefPtr<HTMLColorRule> mLinkRule;
  RefPtr<HTMLColorRule> mVisitedRule;
  RefPtr<HTMLColorRule> mActiveRule;
  RefPtr<TableQuirkColorRule> mTableQuirkColorRule;
  RefPtr<TableTHRule>   mTableTHRule;

  PLDHashTable            mMappedAttrTable;
  PLDHashTable            mLangRuleTable;
};

#endif /* !defined(nsHTMLStyleSheet_h_) */
