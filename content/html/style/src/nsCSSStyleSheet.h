/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
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
 *   L. David Baron <dbaron@dbaron.org>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Daniel Glazman <glazman@netscape.com>
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

#ifndef nsCSSStyleSheet_h_
#define nsCSSStyleSheet_h_

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsICSSStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsVoidArray.h"

class nsIURI;
class nsINameSpace;
class nsISupportsArray;

// -------------------------------
// CSS Style Sheet Inner Data Container
//

class nsCSSStyleSheetInner {
public:
  nsCSSStyleSheetInner(nsICSSStyleSheet* aParentSheet);
  nsCSSStyleSheetInner(nsCSSStyleSheetInner& aCopy, nsICSSStyleSheet* aParentSheet);
  virtual ~nsCSSStyleSheetInner();

  virtual nsCSSStyleSheetInner* CloneFor(nsICSSStyleSheet* aParentSheet);
  virtual void AddSheet(nsICSSStyleSheet* aParentSheet);
  virtual void RemoveSheet(nsICSSStyleSheet* aParentSheet);

  virtual void RebuildNameSpaces();

  nsAutoVoidArray        mSheets;
  nsCOMPtr<nsIURI>       mSheetURI; // for error reports, etc.
  nsCOMPtr<nsIURI>       mBaseURI; // for resolving relative URIs
  nsISupportsArray*      mOrderedRules;
  nsCOMPtr<nsINameSpace> mNameSpace;
  PRPackedBool           mComplete;
};


// -------------------------------
// CSS Style Sheet
//

class CSSImportsCollectionImpl;
class CSSRuleListImpl;
class DOMMediaListImpl;
static PRBool CascadeSheetRulesInto(nsICSSStyleSheet* aSheet, void* aData);

class nsCSSStyleSheet : public nsICSSStyleSheet, 
                        public nsIDOMCSSStyleSheet,
                        public nsICSSLoaderObserver
{
public:
  nsCSSStyleSheet();

  NS_DECL_ISUPPORTS

  // nsIStyleSheet interface
  NS_IMETHOD GetSheetURI(nsIURI** aSheetURI) const;
  NS_IMETHOD GetBaseURI(nsIURI** aBaseURI) const;
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
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const;  // may be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const;  // may be null
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocument);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif
  
  // nsICSSStyleSheet interface
  NS_IMETHOD ContainsStyleSheet(nsIURI* aURL, PRBool& aContains,
                                nsIStyleSheet** aTheChild=nsnull);
  NS_IMETHOD AppendStyleSheet(nsICSSStyleSheet* aSheet);
  NS_IMETHOD InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex);
  NS_IMETHOD PrependStyleRule(nsICSSRule* aRule);
  NS_IMETHOD AppendStyleRule(nsICSSRule* aRule);
  NS_IMETHOD ReplaceStyleRule(nsICSSRule* aOld, nsICSSRule* aNew);
  NS_IMETHOD StyleRuleCount(PRInt32& aCount) const;
  NS_IMETHOD GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const;
  NS_IMETHOD DeleteRuleFromGroup(nsICSSGroupRule* aGroup, PRUint32 aIndex);
  NS_IMETHOD InsertRuleIntoGroup(const nsAString& aRule, nsICSSGroupRule* aGroup, PRUint32 aIndex, PRUint32* _retval);
  NS_IMETHOD ReplaceRuleInGroup(nsICSSGroupRule* aGroup, nsICSSRule* aOld, nsICSSRule* aNew);
  NS_IMETHOD StyleSheetCount(PRInt32& aCount) const;
  NS_IMETHOD GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const;
  NS_IMETHOD SetURIs(nsIURI* aSheetURI, nsIURI* aBaseURI);
  NS_IMETHOD SetTitle(const nsAString& aTitle);
  NS_IMETHOD AppendMedium(nsIAtom* aMedium);
  NS_IMETHOD ClearMedia();
  NS_IMETHOD SetOwningNode(nsIDOMNode* aOwningNode);
  NS_IMETHOD SetOwnerRule(nsICSSImportRule* aOwnerRule);
  NS_IMETHOD GetOwnerRule(nsICSSImportRule** aOwnerRule);
  NS_IMETHOD GetNameSpace(nsINameSpace*& aNameSpace) const;
  NS_IMETHOD Clone(nsICSSStyleSheet* aCloneParent,
                   nsICSSImportRule* aCloneOwnerRule,
                   nsIDocument* aCloneDocument,
                   nsIDOMNode* aCloneOwningNode,
                   nsICSSStyleSheet** aClone) const;
  NS_IMETHOD IsModified(PRBool* aSheetModified) const;
  NS_IMETHOD SetModified(PRBool aModified);
  NS_IMETHOD AddRuleProcessor(nsCSSRuleProcessor* aProcessor);
  NS_IMETHOD DropRuleProcessor(nsCSSRuleProcessor* aProcessor);

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet*aSheet, PRBool aNotify);
  
  nsresult EnsureUniqueInner();

  // nsIDOMStyleSheet interface
  NS_DECL_NSIDOMSTYLESHEET

  // nsIDOMCSSStyleSheet interface
  NS_DECL_NSIDOMCSSSTYLESHEET

private:
  nsCSSStyleSheet(const nsCSSStyleSheet& aCopy,
                  nsICSSStyleSheet* aParentToUse,
                  nsICSSImportRule* aOwnerRuleToUse,
                  nsIDocument* aDocumentToUse,
                  nsIDOMNode* aOwningNodeToUse);
  
  // These are not supported and are not implemented! 
  nsCSSStyleSheet(const nsCSSStyleSheet& aCopy); 
  nsCSSStyleSheet& operator=(const nsCSSStyleSheet& aCopy); 

protected:
  virtual ~nsCSSStyleSheet();

  void ClearRuleCascades();

  nsresult WillDirty();
  void     DidDirty();

protected:
  nsString              mTitle;
  DOMMediaListImpl*     mMedia;
  nsCSSStyleSheet*      mFirstChild;
  nsCSSStyleSheet*      mNext;
  nsICSSStyleSheet*     mParent;    // weak ref
  nsICSSImportRule*     mOwnerRule; // weak ref

  CSSImportsCollectionImpl* mImportsCollection;
  CSSRuleListImpl*      mRuleCollection;
  nsIDocument*          mDocument; // weak ref; parents maintain this for their children
  nsIDOMNode*           mOwningNode; // weak ref
  PRPackedBool          mDisabled;
  PRPackedBool          mDirty; // has been modified 

  nsCSSStyleSheetInner* mInner;

  nsAutoVoidArray*      mRuleProcessors;

  friend class DOMMediaListImpl;
  friend PRBool CascadeSheetRulesInto(nsICSSStyleSheet* aSheet, void* aData);
};

#endif /* !defined(nsCSSStyleSheet_h_) */
