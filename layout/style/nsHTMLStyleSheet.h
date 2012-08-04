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

#include "mozilla/Attributes.h"

#include "nsIStyleSheet.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "pldhash.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "mozilla/Attributes.h"

class nsMappedAttributes;

class nsHTMLStyleSheet MOZ_FINAL : public nsIStyleSheet,
                                   public nsIStyleRuleProcessor
{
public:
  nsHTMLStyleSheet(void);
  nsresult Init();

  NS_DECL_ISUPPORTS

  // nsIStyleSheet api
  virtual nsIURI* GetSheetURI() const;
  virtual nsIURI* GetBaseURI() const;
  virtual void GetTitle(nsString& aTitle) const;
  virtual void GetType(nsString& aType) const;
  virtual bool HasRules() const;
  virtual bool IsApplicable() const;
  virtual void SetEnabled(bool aEnabled);
  virtual bool IsComplete() const;
  virtual void SetComplete();
  virtual nsIStyleSheet* GetParentSheet() const;  // will be null
  virtual nsIDocument* GetOwningDocument() const;
  virtual void SetOwningDocument(nsIDocument* aDocumemt);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsIStyleRuleProcessor API
  virtual void RulesMatching(ElementRuleProcessorData* aData);
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData);
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData);
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData);
#endif
  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData);
  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData);
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData);
  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext);
  virtual NS_MUST_OVERRIDE size_t
    SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const MOZ_OVERRIDE;
  virtual NS_MUST_OVERRIDE size_t
    SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const MOZ_OVERRIDE;
  size_t DOMSizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  nsresult Init(nsIURI* aURL, nsIDocument* aDocument);
  void Reset(nsIURI* aURL);
  nsresult SetLinkColor(nscolor aColor);
  nsresult SetActiveLinkColor(nscolor aColor);
  nsresult SetVisitedLinkColor(nscolor aColor);

  // Mapped Attribute management methods
  already_AddRefed<nsMappedAttributes>
    UniqueMappedAttributes(nsMappedAttributes* aMapped);
  void DropMappedAttributes(nsMappedAttributes* aMapped);

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
    virtual void MapRuleInfoInto(nsRuleData* aRuleData);
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
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
    virtual void MapRuleInfoInto(nsRuleData* aRuleData) = 0;
  #ifdef DEBUG
    virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  #endif
  };

  // this rule handles <th> inheritance
  class TableTHRule;
  friend class TableTHRule;
  class TableTHRule MOZ_FINAL : public GenericTableRule {
  public:
    TableTHRule() {}

    virtual void MapRuleInfoInto(nsRuleData* aRuleData);
  };

  // Rule to handle quirk table colors
  class TableQuirkColorRule MOZ_FINAL : public GenericTableRule {
  public:
    TableQuirkColorRule() {}

    virtual void MapRuleInfoInto(nsRuleData* aRuleData);
  };

  nsCOMPtr<nsIURI>        mURL;
  nsIDocument*            mDocument;
  nsRefPtr<HTMLColorRule> mLinkRule;
  nsRefPtr<HTMLColorRule> mVisitedRule;
  nsRefPtr<HTMLColorRule> mActiveRule;
  nsRefPtr<TableQuirkColorRule> mTableQuirkColorRule;
  nsRefPtr<TableTHRule>   mTableTHRule;

  PLDHashTable            mMappedAttrTable;
};

// XXX convenience method. Calls Initialize() automatically.
nsresult
NS_NewHTMLStyleSheet(nsHTMLStyleSheet** aInstancePtrResult, nsIURI* aURL, 
                     nsIDocument* aDocument);

nsresult
NS_NewHTMLStyleSheet(nsHTMLStyleSheet** aInstancePtrResult);

#endif /* !defined(nsHTMLStyleSheet_h_) */
