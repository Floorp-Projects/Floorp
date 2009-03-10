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
#include "nsString.h"

class nsICSSRule;
class nsIDOMNode;
class nsXMLNameSpaceMap;
class nsCSSRuleProcessor;
class nsMediaList;
class nsICSSGroupRule;
class nsICSSImportRule;
class nsIPrincipal;

// IID for the nsICSSStyleSheet interface
// ba09b3a4-4a29-495d-987b-cfbb58c5c6ec
#define NS_ICSS_STYLE_SHEET_IID     \
{ 0xba09b3a4, 0x4a29, 0x495d, \
 { 0x98, 0x7b, 0xcf, 0xbb, 0x58, 0xc5, 0xc6, 0xec } }

class nsICSSStyleSheet : public nsIStyleSheet {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSS_STYLE_SHEET_IID)

  NS_IMETHOD  AppendStyleSheet(nsICSSStyleSheet* aSheet) = 0;
  NS_IMETHOD  InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex) = 0;

  // XXX do these belong here or are they generic?
  NS_IMETHOD  PrependStyleRule(nsICSSRule* aRule) = 0;
  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule) = 0;
  NS_IMETHOD  ReplaceStyleRule(nsICSSRule* aOld, nsICSSRule* aNew) = 0;

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const = 0;

  NS_IMETHOD  DeleteRuleFromGroup(nsICSSGroupRule* aGroup, PRUint32 aIndex) = 0;
  NS_IMETHOD  InsertRuleIntoGroup(const nsAString & aRule, nsICSSGroupRule* aGroup, PRUint32 aIndex, PRUint32* _retval) = 0;
  NS_IMETHOD  ReplaceRuleInGroup(nsICSSGroupRule* aGroup, nsICSSRule* aOld, nsICSSRule* aNew) = 0;

  NS_IMETHOD  StyleSheetCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const = 0;

  /**
   * SetURIs must be called on all sheets before parsing into them.
   * SetURIs may only be called while the sheet is 1) incomplete and 2)
   * has no rules in it
   */
  NS_IMETHOD  SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                      nsIURI* aBaseURI) = 0;

  /**
   * SetPrincipal should be called on all sheets before parsing into them.
   * This can only be called once with a non-null principal.  Calling this with
   * a null pointer is allowed and is treated as a no-op.
   */
  virtual NS_HIDDEN_(void) SetPrincipal(nsIPrincipal* aPrincipal) = 0;

  // Principal() never returns a null pointer.
  virtual NS_HIDDEN_(nsIPrincipal*) Principal() const = 0;
  
  NS_IMETHOD  SetTitle(const nsAString& aTitle) = 0;
  NS_IMETHOD  SetMedia(nsMediaList* aMedia) = 0;
  NS_IMETHOD  SetOwningNode(nsIDOMNode* aOwningNode) = 0;

  NS_IMETHOD  SetOwnerRule(nsICSSImportRule* aOwnerRule) = 0;
  NS_IMETHOD  GetOwnerRule(nsICSSImportRule** aOwnerRule) = 0;
  
  // get namespace map for sheet
  virtual NS_HIDDEN_(nsXMLNameSpaceMap*) GetNameSpaceMap() const = 0;

  NS_IMETHOD  Clone(nsICSSStyleSheet* aCloneParent,
                    nsICSSImportRule* aCloneOwnerRule,
                    nsIDocument* aCloneDocument,
                    nsIDOMNode* aCloneOwningNode,
                    nsICSSStyleSheet** aClone) const = 0;

  NS_IMETHOD  IsModified(PRBool* aModified) const = 0; // returns the mDirty status of the sheet
  NS_IMETHOD  SetModified(PRBool aModified) = 0;

  NS_IMETHOD  AddRuleProcessor(nsCSSRuleProcessor* aProcessor) = 0;
  NS_IMETHOD  DropRuleProcessor(nsCSSRuleProcessor* aProcessor) = 0;

  /**
   * Like the DOM insertRule() method, but doesn't do any security checks
   */
  NS_IMETHOD InsertRuleInternal(const nsAString& aRule,
                                PRUint32 aIndex, PRUint32* aReturn) = 0;

  /* Get the URI this sheet was originally loaded from, if any.  Can
     return null */
  NS_IMETHOD_(nsIURI*) GetOriginalURI() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSStyleSheet, NS_ICSS_STYLE_SHEET_IID)

nsresult
NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult);

#endif /* nsICSSStyleSheet_h___ */
