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

/* representation of a CSS style sheet */

#ifndef nsCSSStyleSheet_h_
#define nsCSSStyleSheet_h_

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsICSSStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsTArray.h"
#include "nsCOMArray.h"

class nsIURI;
class nsMediaList;
class nsMediaQueryResultCacheKey;
class nsCSSStyleSheet;

// -------------------------------
// CSS Style Sheet Inner Data Container
//

class nsCSSStyleSheetInner {
public:
  nsCSSStyleSheetInner(nsICSSStyleSheet* aPrimarySheet);
  nsCSSStyleSheetInner(nsCSSStyleSheetInner& aCopy,
                       nsCSSStyleSheet* aPrimarySheet);
  ~nsCSSStyleSheetInner();

  nsCSSStyleSheetInner* CloneFor(nsCSSStyleSheet* aPrimarySheet);
  void AddSheet(nsICSSStyleSheet* aSheet);
  void RemoveSheet(nsICSSStyleSheet* aSheet);

  void RebuildNameSpaces();

  // Create a new namespace map
  nsresult CreateNamespaceMap();

  nsAutoTArray<nsICSSStyleSheet*, 8> mSheets;
  nsCOMPtr<nsIURI>       mSheetURI; // for error reports, etc.
  nsCOMPtr<nsIURI>       mOriginalSheetURI;  // for GetHref.  Can be null.
  nsCOMPtr<nsIURI>       mBaseURI; // for resolving relative URIs
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMArray<nsICSSRule> mOrderedRules;
  nsAutoPtr<nsXMLNameSpaceMap> mNameSpaceMap;
  // Linked list of child sheets.  This is al fundamentally broken, because
  // each of the child sheets has a unique parent... We can only hope (and
  // currently this is the case) that any time page JS can get ts hands on a
  // child sheet that means we've already ensured unique inners throughout its
  // parent chain and things are good.
  nsRefPtr<nsCSSStyleSheet> mFirstChild;
  PRBool                 mComplete;

#ifdef DEBUG
  PRBool                 mPrincipalSet;
#endif
};


// -------------------------------
// CSS Style Sheet
//

class CSSRuleListImpl;
struct ChildSheetListBuilder;

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
  NS_IMETHOD SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                     nsIURI* aBaseURI);
  virtual NS_HIDDEN_(void) SetPrincipal(nsIPrincipal* aPrincipal);
  virtual NS_HIDDEN_(nsIPrincipal*) Principal() const;
  NS_IMETHOD SetTitle(const nsAString& aTitle);
  NS_IMETHOD SetMedia(nsMediaList* aMedia);
  NS_IMETHOD SetOwningNode(nsIDOMNode* aOwningNode);
  NS_IMETHOD SetOwnerRule(nsICSSImportRule* aOwnerRule);
  NS_IMETHOD GetOwnerRule(nsICSSImportRule** aOwnerRule);
  virtual NS_HIDDEN_(nsXMLNameSpaceMap*) GetNameSpaceMap() const;
  NS_IMETHOD Clone(nsICSSStyleSheet* aCloneParent,
                   nsICSSImportRule* aCloneOwnerRule,
                   nsIDocument* aCloneDocument,
                   nsIDOMNode* aCloneOwningNode,
                   nsICSSStyleSheet** aClone) const;
  NS_IMETHOD IsModified(PRBool* aSheetModified) const;
  NS_IMETHOD SetModified(PRBool aModified);
  NS_IMETHOD AddRuleProcessor(nsCSSRuleProcessor* aProcessor);
  NS_IMETHOD DropRuleProcessor(nsCSSRuleProcessor* aProcessor);
  NS_IMETHOD InsertRuleInternal(const nsAString& aRule,
                                PRUint32 aIndex, PRUint32* aReturn);
  NS_IMETHOD_(nsIURI*) GetOriginalURI() const;

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aWasAlternate,
                              nsresult aStatus);
  
  nsresult EnsureUniqueInner();

  PRBool UseForPresentation(nsPresContext* aPresContext,
                            nsMediaQueryResultCacheKey& aKey) const;

  // nsIDOMStyleSheet interface
  NS_DECL_NSIDOMSTYLESHEET

  // nsIDOMCSSStyleSheet interface
  NS_DECL_NSIDOMCSSSTYLESHEET

  // Function used as a callback to rebuild our inner's child sheet
  // list after we clone a unique inner for ourselves.
  static PRBool RebuildChildList(nsICSSRule* aRule, void* aBuilder);

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

  // Return success if the subject principal subsumes the principal of our
  // inner, error otherwise.  This will also succeed if the subject has
  // UniversalBrowserWrite.
  nsresult SubjectSubsumesInnerPrincipal() const;

  // Add the namespace mapping from this @namespace rule to our namespace map
  nsresult RegisterNamespaceRule(nsICSSRule* aRule);

protected:
  nsString              mTitle;
  nsCOMPtr<nsMediaList> mMedia;
  nsRefPtr<nsCSSStyleSheet> mNext;
  nsICSSStyleSheet*     mParent;    // weak ref
  nsICSSImportRule*     mOwnerRule; // weak ref

  CSSRuleListImpl*      mRuleCollection;
  nsIDocument*          mDocument; // weak ref; parents maintain this for their children
  nsIDOMNode*           mOwningNode; // weak ref
  PRPackedBool          mDisabled;
  PRPackedBool          mDirty; // has been modified 

  nsCSSStyleSheetInner* mInner;

  nsAutoTArray<nsCSSRuleProcessor*, 8>* mRuleProcessors;

  friend class nsMediaList;
  friend class nsCSSRuleProcessor;
  friend nsresult NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult);
  friend struct ChildSheetListBuilder;
};

#endif /* !defined(nsCSSStyleSheet_h_) */
