/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
class nsMappedAttributes;

class nsHTMLStyleSheet : public nsIStyleSheet, public nsIStyleRuleProcessor {
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

  nsresult Init(nsIURI* aURL, nsIDocument* aDocument);
  void Reset(nsIURI* aURL);
  nsresult SetLinkColor(nscolor aColor);
  nsresult SetActiveLinkColor(nscolor aColor);
  nsresult SetVisitedLinkColor(nscolor aColor);

  // Mapped Attribute management methods
  already_AddRefed<nsMappedAttributes>
    UniqueMappedAttributes(nsMappedAttributes* aMapped);
  void DropMappedAttributes(nsMappedAttributes* aMapped);

  PRInt64 DOMSizeOf() const;
private: 
  nsHTMLStyleSheet(const nsHTMLStyleSheet& aCopy) MOZ_DELETE;
  nsHTMLStyleSheet& operator=(const nsHTMLStyleSheet& aCopy) MOZ_DELETE;

  ~nsHTMLStyleSheet();

  class HTMLColorRule;
  friend class HTMLColorRule;
  class HTMLColorRule : public nsIStyleRule {
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
  class GenericTableRule: public nsIStyleRule {
  public:
    GenericTableRule() {}

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
  class TableTHRule: public GenericTableRule {
  public:
    TableTHRule() {}

    virtual void MapRuleInfoInto(nsRuleData* aRuleData);
  };

  // Rule to handle quirk table colors
  class TableQuirkColorRule : public GenericTableRule {
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
