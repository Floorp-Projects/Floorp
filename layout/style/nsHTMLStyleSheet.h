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
#ifndef nsHTMLStyleSheet_h_
#define nsHTMLStyleSheet_h_

#include "nsIStyleSheet.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "pldhash.h"
#include "nsCOMPtr.h"
class nsMappedAttributes;

class nsHTMLStyleSheet : public nsIStyleSheet, public nsIStyleRuleProcessor {
public:
  nsHTMLStyleSheet(void);
  nsresult Init();

  NS_DECL_ISUPPORTS

  // nsIStyleSheet api
  NS_IMETHOD GetURL(nsIURI*& aURL) const;
  NS_IMETHOD GetTitle(nsString& aTitle) const;
  NS_IMETHOD GetType(nsString& aType) const;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const;
  NS_IMETHOD_(PRBool) UseForMedium(nsIAtom* aMedium) const;
  NS_IMETHOD_(PRBool) HasRules() const;
  NS_IMETHOD GetApplicable(PRBool& aApplicable) const;
  NS_IMETHOD SetEnabled(PRBool aEnabled);
  NS_IMETHOD GetComplete(PRBool& aComplete) const;
  NS_IMETHOD SetComplete();
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const;  // will be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const;
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocumemt);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsIStyleRuleProcessor API
  NS_IMETHOD RulesMatching(ElementRuleProcessorData* aData,
                           nsIAtom* aMedium);
  NS_IMETHOD RulesMatching(PseudoRuleProcessorData* aData,
                           nsIAtom* aMedium);
  NS_IMETHOD HasStateDependentStyle(StateRuleProcessorData* aData,
                                    nsIAtom* aMedium,
                                    nsReStyleHint* aResult);
  NS_IMETHOD HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                        nsIAtom* aMedium,
                                        nsReStyleHint* aResult);

  nsresult Init(nsIURI* aURL, nsIDocument* aDocument);
  nsresult Reset(nsIURI* aURL);
  nsresult GetLinkColor(nscolor& aColor);
  nsresult GetActiveLinkColor(nscolor& aColor);
  nsresult GetVisitedLinkColor(nscolor& aColor);
  nsresult SetLinkColor(nscolor aColor);
  nsresult SetActiveLinkColor(nscolor aColor);
  nsresult SetVisitedLinkColor(nscolor aColor);

  // Mapped Attribute management methods
  already_AddRefed<nsMappedAttributes>
    UniqueMappedAttributes(nsMappedAttributes* aMapped);
  void DropMappedAttributes(nsMappedAttributes* aMapped);


private: 
  // These are not supported and are not implemented! 
  nsHTMLStyleSheet(const nsHTMLStyleSheet& aCopy); 
  nsHTMLStyleSheet& operator=(const nsHTMLStyleSheet& aCopy); 

  ~nsHTMLStyleSheet();

  class HTMLColorRule;
  friend class HTMLColorRule;
  class HTMLColorRule : public nsIStyleRule {
  public:
    HTMLColorRule(nsHTMLStyleSheet* aSheet);
    virtual ~HTMLColorRule();

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

    // The new mapping function.
    NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

  #ifdef DEBUG
    NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  #endif

    nscolor             mColor;
    nsHTMLStyleSheet*  mSheet;
  };

  class GenericTableRule;
  friend class GenericTableRule;
  class GenericTableRule: public nsIStyleRule {
  public:
    GenericTableRule(nsHTMLStyleSheet* aSheet);
    virtual ~GenericTableRule();

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

    // The new mapping function.
    NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

  #ifdef DEBUG
    NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  #endif

    nsHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
  };

  // this rule handles <th> inheritance
  class TableTHRule;
  friend class TableTHRule;
  class TableTHRule: public GenericTableRule {
  public:
    TableTHRule(nsHTMLStyleSheet* aSheet);
    virtual ~TableTHRule();

    NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
  };

  // this rule handles borders on a <thead>, <tbody>, <tfoot> when rules
  // is set on its <table>
  class TableTbodyRule;
  friend class TableTbodyRule;
  class TableTbodyRule: public GenericTableRule {
  public:
    TableTbodyRule(nsHTMLStyleSheet* aSheet);
    virtual ~TableTbodyRule();

    NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
  };

  // this rule handles borders on a <row> when rules is set on its <table>
  class TableRowRule;
  friend class TableRowRule;
  class TableRowRule: public GenericTableRule {
  public:
    TableRowRule(nsHTMLStyleSheet* aSheet);
    virtual ~TableRowRule();

    NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
  };

  // this rule handles borders on a <colgroup> when rules is set on its <table>
  class TableColgroupRule;
  friend class TableColgroupRule;
  class TableColgroupRule: public GenericTableRule {
  public:
    TableColgroupRule(nsHTMLStyleSheet* aSheet);
    virtual ~TableColgroupRule();

    NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
  };

  // this rule handles borders on a <col> when rules is set on its <table>
  class TableColRule;
  friend class TableColRule;
  class TableColRule: public GenericTableRule {
  public:
    TableColRule(nsHTMLStyleSheet* aSheet);
    virtual ~TableColRule();

    NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
  };

  nsIURI*              mURL;
  nsIDocument*         mDocument;
  HTMLColorRule*       mLinkRule;
  HTMLColorRule*       mVisitedRule;
  HTMLColorRule*       mActiveRule;
  HTMLColorRule*       mDocumentColorRule;
  TableTbodyRule*      mTableTbodyRule;
  TableRowRule*        mTableRowRule;
  TableColgroupRule*   mTableColgroupRule;
  TableColRule*        mTableColRule;
  TableTHRule*         mTableTHRule;

  PLDHashTable         mMappedAttrTable;
};

// XXX convenience method. Calls Initialize() automatically.
nsresult
NS_NewHTMLStyleSheet(nsHTMLStyleSheet** aInstancePtrResult, nsIURI* aURL, 
                     nsIDocument* aDocument);

nsresult
NS_NewHTMLStyleSheet(nsHTMLStyleSheet** aInstancePtrResult);

#endif /* !defined(nsHTMLStyleSheet_h_) */
