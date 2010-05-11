/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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

/* representation of a CSS style sheet */

#ifndef nsICSSStyleSheet_h___
#define nsICSSStyleSheet_h___

#include "nsIStyleSheet.h"
#include "nsCOMPtr.h"

class nsICSSRule;
class nsIDOMNode;
class nsXMLNameSpaceMap;
class nsCSSRuleProcessor;
class nsMediaList;
class nsICSSGroupRule;
class nsICSSImportRule;
class nsIPrincipal;
class nsAString;

// IID for the nsICSSStyleSheet interface
// 94d4d747-f690-4eb6-96c0-196a1b3659dc
#define NS_ICSS_STYLE_SHEET_IID     \
{ 0x94d4d747, 0xf690, 0x4eb6, \
 { 0x96, 0xc0, 0x19, 0x6a, 0x1b, 0x36, 0x59, 0xdc } }

class nsICSSStyleSheet : public nsIStyleSheet {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSS_STYLE_SHEET_IID)

  virtual void AppendStyleSheet(nsICSSStyleSheet* aSheet) = 0;
  virtual void InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex) = 0;

  // XXX do these belong here or are they generic?
  virtual void PrependStyleRule(nsICSSRule* aRule) = 0;
  virtual void AppendStyleRule(nsICSSRule* aRule) = 0;
  virtual void ReplaceStyleRule(nsICSSRule* aOld, nsICSSRule* aNew) = 0;

  virtual PRInt32 StyleRuleCount() const = 0;
  virtual nsresult GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const = 0;

  virtual nsresult DeleteRuleFromGroup(nsICSSGroupRule* aGroup, PRUint32 aIndex) = 0;
  virtual nsresult InsertRuleIntoGroup(const nsAString & aRule, nsICSSGroupRule* aGroup,
                                       PRUint32 aIndex, PRUint32* _retval) = 0;
  virtual nsresult ReplaceRuleInGroup(nsICSSGroupRule* aGroup, nsICSSRule* aOld,
                                      nsICSSRule* aNew) = 0;

  virtual PRInt32 StyleSheetCount() const = 0;
  virtual already_AddRefed<nsICSSStyleSheet> GetStyleSheetAt(PRInt32 aIndex) const = 0;

  /**
   * SetURIs must be called on all sheets before parsing into them.
   * SetURIs may only be called while the sheet is 1) incomplete and 2)
   * has no rules in it
   */
  virtual void SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                       nsIURI* aBaseURI) = 0;

  /**
   * SetPrincipal should be called on all sheets before parsing into them.
   * This can only be called once with a non-null principal.  Calling this with
   * a null pointer is allowed and is treated as a no-op.
   */
  virtual void SetPrincipal(nsIPrincipal* aPrincipal) = 0;

  // Principal() never returns a null pointer.
  virtual nsIPrincipal* Principal() const = 0;

  virtual void SetTitle(const nsAString& aTitle) = 0;
  virtual void SetMedia(nsMediaList* aMedia) = 0;
  virtual void SetOwningNode(nsIDOMNode* aOwningNode) = 0;

  virtual void SetOwnerRule(nsICSSImportRule* aOwnerRule) = 0;
  virtual already_AddRefed<nsICSSImportRule> GetOwnerRule() = 0;
  
  // get namespace map for sheet
  virtual nsXMLNameSpaceMap* GetNameSpaceMap() const = 0;

  virtual already_AddRefed<nsICSSStyleSheet> Clone(nsICSSStyleSheet* aCloneParent,
                                                   nsICSSImportRule* aCloneOwnerRule,
                                                   nsIDocument* aCloneDocument,
                                                   nsIDOMNode* aCloneOwningNode) const = 0;

  virtual PRBool IsModified() const = 0; // returns the mDirty status of the sheet
  virtual void SetModified(PRBool aModified) = 0;

  virtual nsresult AddRuleProcessor(nsCSSRuleProcessor* aProcessor) = 0;
  virtual nsresult DropRuleProcessor(nsCSSRuleProcessor* aProcessor) = 0;

  /**
   * Like the DOM insertRule() method, but doesn't do any security checks
   */
  virtual nsresult InsertRuleInternal(const nsAString& aRule,
                                PRUint32 aIndex, PRUint32* aReturn) = 0;

  /* Get the URI this sheet was originally loaded from, if any.  Can
     return null */
  virtual nsIURI* GetOriginalURI() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSStyleSheet, NS_ICSS_STYLE_SHEET_IID)

nsresult
NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult);

#endif /* nsICSSStyleSheet_h___ */
