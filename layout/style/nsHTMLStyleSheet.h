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

#include "nsAutoPtr.h"
#include "nsColor.h"
#include "nsCOMPtr.h"
#include "nsIStyleRule.h"
#include "nsIStyleRuleProcessor.h"
#include "pldhash.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsString.h"

class nsIDocument;
class nsMappedAttributes;

class nsHTMLStyleSheet MOZ_FINAL : public nsIStyleRuleProcessor
{
public:
  nsHTMLStyleSheet(nsIDocument* aDocument);

  void SetOwningDocument(nsIDocument* aDocument);

  NS_DECL_ISUPPORTS

  // nsIStyleRuleProcessor API
  virtual void RulesMatching(ElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) MOZ_OVERRIDE;
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) MOZ_OVERRIDE;
#endif
  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) MOZ_OVERRIDE;
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
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
  nsHTMLStyleSheet(const nsHTMLStyleSheet& aCopy) MOZ_DELETE;
  nsHTMLStyleSheet& operator=(const nsHTMLStyleSheet& aCopy) MOZ_DELETE;

  ~nsHTMLStyleSheet();

  class HTMLColorRule;
  friend class HTMLColorRule;
  class HTMLColorRule MOZ_FINAL : public nsIStyleRule {
  public:
    HTMLColorRule() {}

    NS_DECL_ISUPPORTS

    // nsIStyleRule interface
    virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
  #endif

    nscolor             mColor;
  };

  // Implementation of SetLink/VisitedLink/ActiveLinkColor
  nsresult ImplLinkColorSetter(nsRefPtr<HTMLColorRule>& aRule, nscolor aColor);

  class GenericTableRule;
  friend class GenericTableRule;
  class GenericTableRule : public nsIStyleRule {
  public:
    GenericTableRule() {}
    virtual ~GenericTableRule() {}

    NS_DECL_ISUPPORTS

    // nsIStyleRule interface
    virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE = 0;
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
  #endif
  };

  // this rule handles <th> inheritance
  class TableTHRule;
  friend class TableTHRule;
  class TableTHRule MOZ_FINAL : public GenericTableRule {
  public:
    TableTHRule() {}

    virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
  };

  // Rule to handle quirk table colors
  class TableQuirkColorRule MOZ_FINAL : public GenericTableRule {
  public:
    TableQuirkColorRule() {}

    virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
  };

public: // for mLangRuleTable structures only

  // Rule to handle xml:lang attributes, of which we have exactly one
  // per language string, maintained in mLangRuleTable.
  class LangRule MOZ_FINAL : public nsIStyleRule {
  public:
    LangRule(const nsSubstring& aLang) : mLang(aLang) {}

    NS_DECL_ISUPPORTS

    // nsIStyleRule interface
    virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
  #endif

    nsString mLang;
  };

private:
  nsIDocument*            mDocument;
  nsRefPtr<HTMLColorRule> mLinkRule;
  nsRefPtr<HTMLColorRule> mVisitedRule;
  nsRefPtr<HTMLColorRule> mActiveRule;
  nsRefPtr<TableQuirkColorRule> mTableQuirkColorRule;
  nsRefPtr<TableTHRule>   mTableTHRule;

  PLDHashTable            mMappedAttrTable;
  PLDHashTable            mLangRuleTable;
};

#endif /* !defined(nsHTMLStyleSheet_h_) */
