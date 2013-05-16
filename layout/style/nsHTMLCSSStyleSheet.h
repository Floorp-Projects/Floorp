/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing style attributes
 */

#ifndef nsHTMLCSSStyleSheet_h_
#define nsHTMLCSSStyleSheet_h_

#include "mozilla/Attributes.h"

#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsIStyleSheet.h"
#include "nsIStyleRuleProcessor.h"

struct MiscContainer;

class nsHTMLCSSStyleSheet MOZ_FINAL : public nsIStyleSheet,
                                      public nsIStyleRuleProcessor
{
public:
  nsHTMLCSSStyleSheet(nsIURI* aURL, nsIDocument* aDocument);
  ~nsHTMLCSSStyleSheet();

  NS_DECL_ISUPPORTS

  void Reset(nsIURI* aURL);

  // nsIStyleSheet
  virtual nsIURI* GetSheetURI() const MOZ_OVERRIDE;
  virtual nsIURI* GetBaseURI() const MOZ_OVERRIDE;
  virtual void GetTitle(nsString& aTitle) const MOZ_OVERRIDE;
  virtual void GetType(nsString& aType) const MOZ_OVERRIDE;
  virtual bool HasRules() const MOZ_OVERRIDE;
  virtual bool IsApplicable() const MOZ_OVERRIDE;
  virtual void SetEnabled(bool aEnabled) MOZ_OVERRIDE;
  virtual bool IsComplete() const MOZ_OVERRIDE;
  virtual void SetComplete() MOZ_OVERRIDE;
  virtual nsIStyleSheet* GetParentSheet() const MOZ_OVERRIDE;  // will be null
  virtual nsIDocument* GetOwningDocument() const MOZ_OVERRIDE;
  virtual void SetOwningDocument(nsIDocument* aDocument) MOZ_OVERRIDE;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif

  // nsIStyleRuleProcessor
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
  virtual size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;

  void CacheStyleAttr(const nsAString& aSerialized, MiscContainer* aValue);
  void EvictStyleAttr(const nsAString& aSerialized, MiscContainer* aValue);
  MiscContainer* LookupStyleAttr(const nsAString& aSerialized);

private: 
  nsHTMLCSSStyleSheet(const nsHTMLCSSStyleSheet& aCopy) MOZ_DELETE;
  nsHTMLCSSStyleSheet& operator=(const nsHTMLCSSStyleSheet& aCopy) MOZ_DELETE;

protected:
  nsCOMPtr<nsIURI> mURL;
  nsIDocument*     mDocument;
  nsDataHashtable<nsStringHashKey, MiscContainer*> mCachedStyleAttrs;
};

inline nsISupports*
ToSupports(nsHTMLCSSStyleSheet* aPointer)
{
  return static_cast<nsIStyleSheet*>(aPointer);
}

#endif /* !defined(nsHTMLCSSStyleSheet_h_) */
