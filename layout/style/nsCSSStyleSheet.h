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
#include "nsIStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsCOMArray.h"

class nsXMLNameSpaceMap;
class nsCSSRuleProcessor;
class nsMediaList;
class nsIPrincipal;
class nsIURI;
class nsMediaList;
class nsMediaQueryResultCacheKey;
class nsCSSStyleSheet;
class nsPresContext;
template<class E, class A> class nsTArray;

namespace mozilla {
namespace css {
class Rule;
class GroupRule;
class ImportRule;
}
}

// -------------------------------
// CSS Style Sheet Inner Data Container
//

class nsCSSStyleSheetInner {
public:
  friend class nsCSSStyleSheet;
  friend class nsCSSRuleProcessor;
  friend nsresult NS_NewCSSStyleSheet(nsCSSStyleSheet** aInstancePtrResult);
private:
  nsCSSStyleSheetInner(nsCSSStyleSheet* aPrimarySheet);
  nsCSSStyleSheetInner(nsCSSStyleSheetInner& aCopy,
                       nsCSSStyleSheet* aPrimarySheet);
  ~nsCSSStyleSheetInner();

  nsCSSStyleSheetInner* CloneFor(nsCSSStyleSheet* aPrimarySheet);
  void AddSheet(nsCSSStyleSheet* aSheet);
  void RemoveSheet(nsCSSStyleSheet* aSheet);

  void RebuildNameSpaces();

  // Create a new namespace map
  nsresult CreateNamespaceMap();

  nsAutoTArray<nsCSSStyleSheet*, 8> mSheets;
  nsCOMPtr<nsIURI>       mSheetURI; // for error reports, etc.
  nsCOMPtr<nsIURI>       mOriginalSheetURI;  // for GetHref.  Can be null.
  nsCOMPtr<nsIURI>       mBaseURI; // for resolving relative URIs
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMArray<mozilla::css::Rule> mOrderedRules;
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

// CID for the nsCSSStyleSheet class
// ca926f30-2a7e-477e-8467-803fb32af20a
#define NS_CSS_STYLE_SHEET_IMPL_CID     \
{ 0xca926f30, 0x2a7e, 0x477e, \
 { 0x84, 0x67, 0x80, 0x3f, 0xb3, 0x2a, 0xf2, 0x0a } }


class NS_FINAL_CLASS nsCSSStyleSheet : public nsIStyleSheet,
                                       public nsIDOMCSSStyleSheet,
                                       public nsICSSLoaderObserver
{
public:
  nsCSSStyleSheet();

  NS_DECL_ISUPPORTS

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_STYLE_SHEET_IMPL_CID)

  // nsIStyleSheet interface
  virtual nsIURI* GetSheetURI() const;
  virtual nsIURI* GetBaseURI() const;
  virtual void GetTitle(nsString& aTitle) const;
  virtual void GetType(nsString& aType) const;
  virtual PRBool HasRules() const;
  virtual PRBool IsApplicable() const;
  virtual void SetEnabled(PRBool aEnabled);
  virtual PRBool IsComplete() const;
  virtual void SetComplete();
  virtual nsIStyleSheet* GetParentSheet() const;  // may be null
  virtual nsIDocument* GetOwningDocument() const;  // may be null
  virtual void SetOwningDocument(nsIDocument* aDocument);

  // Find the ID of the owner inner window.
  PRUint64 FindOwningWindowInnerID() const;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  void AppendStyleSheet(nsCSSStyleSheet* aSheet);
  void InsertStyleSheetAt(nsCSSStyleSheet* aSheet, PRInt32 aIndex);

  // XXX do these belong here or are they generic?
  void PrependStyleRule(mozilla::css::Rule* aRule);
  void AppendStyleRule(mozilla::css::Rule* aRule);
  void ReplaceStyleRule(mozilla::css::Rule* aOld, mozilla::css::Rule* aNew);

  PRInt32 StyleRuleCount() const;
  nsresult GetStyleRuleAt(PRInt32 aIndex, mozilla::css::Rule*& aRule) const;

  nsresult DeleteRuleFromGroup(mozilla::css::GroupRule* aGroup, PRUint32 aIndex);
  nsresult InsertRuleIntoGroup(const nsAString& aRule, mozilla::css::GroupRule* aGroup, PRUint32 aIndex, PRUint32* _retval);
  nsresult ReplaceRuleInGroup(mozilla::css::GroupRule* aGroup, mozilla::css::Rule* aOld, mozilla::css::Rule* aNew);

  PRInt32 StyleSheetCount() const;

  /**
   * SetURIs must be called on all sheets before parsing into them.
   * SetURIs may only be called while the sheet is 1) incomplete and 2)
   * has no rules in it
   */
  void SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI, nsIURI* aBaseURI);

  /**
   * SetPrincipal should be called on all sheets before parsing into them.
   * This can only be called once with a non-null principal.  Calling this with
   * a null pointer is allowed and is treated as a no-op.
   */
  void SetPrincipal(nsIPrincipal* aPrincipal);

  // Principal() never returns a null pointer.
  nsIPrincipal* Principal() const { return mInner->mPrincipal; }

  void SetTitle(const nsAString& aTitle) { mTitle = aTitle; }
  void SetMedia(nsMediaList* aMedia);
  void SetOwningNode(nsIDOMNode* aOwningNode) { mOwningNode = aOwningNode; /* Not ref counted */ }

  void SetOwnerRule(mozilla::css::ImportRule* aOwnerRule) { mOwnerRule = aOwnerRule; /* Not ref counted */ }
  mozilla::css::ImportRule* GetOwnerRule() const { return mOwnerRule; }

  nsXMLNameSpaceMap* GetNameSpaceMap() const { return mInner->mNameSpaceMap; }

  already_AddRefed<nsCSSStyleSheet> Clone(nsCSSStyleSheet* aCloneParent,
                                          mozilla::css::ImportRule* aCloneOwnerRule,
                                          nsIDocument* aCloneDocument,
                                          nsIDOMNode* aCloneOwningNode) const;

  PRBool IsModified() const { return mDirty; }

  void SetModifiedByChildRule() {
    NS_ASSERTION(mDirty,
                 "sheet must be marked dirty before handing out child rules");
    DidDirty();
  }

  nsresult AddRuleProcessor(nsCSSRuleProcessor* aProcessor);
  nsresult DropRuleProcessor(nsCSSRuleProcessor* aProcessor);

  /**
   * Like the DOM insertRule() method, but doesn't do any security checks
   */
  nsresult InsertRuleInternal(const nsAString& aRule,
                              PRUint32 aIndex, PRUint32* aReturn);

  /* Get the URI this sheet was originally loaded from, if any.  Can
     return null */
  virtual nsIURI* GetOriginalURI() const;

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet* aSheet, PRBool aWasAlternate,
                              nsresult aStatus);

  enum EnsureUniqueInnerResult {
    // No work was needed to ensure a unique inner.
    eUniqueInner_AlreadyUnique,
    // A clone was done to ensure a unique inner (which means the style
    // rules in this sheet have changed).
    eUniqueInner_ClonedInner,
    // A clone was attempted, but it failed.
    eUniqueInner_CloneFailed
  };
  EnsureUniqueInnerResult EnsureUniqueInner();

  // Append all of this sheet's child sheets to aArray.  Return PR_TRUE
  // on success and PR_FALSE on allocation failure.
  PRBool AppendAllChildSheets(nsTArray<nsCSSStyleSheet*>& aArray);

  PRBool UseForPresentation(nsPresContext* aPresContext,
                            nsMediaQueryResultCacheKey& aKey) const;

  // nsIDOMStyleSheet interface
  NS_DECL_NSIDOMSTYLESHEET

  // nsIDOMCSSStyleSheet interface
  NS_DECL_NSIDOMCSSSTYLESHEET

  // Function used as a callback to rebuild our inner's child sheet
  // list after we clone a unique inner for ourselves.
  static PRBool RebuildChildList(mozilla::css::Rule* aRule, void* aBuilder);

private:
  nsCSSStyleSheet(const nsCSSStyleSheet& aCopy,
                  nsCSSStyleSheet* aParentToUse,
                  mozilla::css::ImportRule* aOwnerRuleToUse,
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
  nsresult RegisterNamespaceRule(mozilla::css::Rule* aRule);

protected:
  nsString              mTitle;
  nsRefPtr<nsMediaList> mMedia;
  nsRefPtr<nsCSSStyleSheet> mNext;
  nsCSSStyleSheet*      mParent;    // weak ref
  mozilla::css::ImportRule* mOwnerRule; // weak ref

  CSSRuleListImpl*      mRuleCollection;
  nsIDocument*          mDocument; // weak ref; parents maintain this for their children
  nsIDOMNode*           mOwningNode; // weak ref
  PRPackedBool          mDisabled;
  PRPackedBool          mDirty; // has been modified 

  nsCSSStyleSheetInner* mInner;

  nsAutoTArray<nsCSSRuleProcessor*, 8>* mRuleProcessors;

  friend class nsMediaList;
  friend class nsCSSRuleProcessor;
  friend nsresult NS_NewCSSStyleSheet(nsCSSStyleSheet** aInstancePtrResult);
  friend struct ChildSheetListBuilder;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCSSStyleSheet, NS_CSS_STYLE_SHEET_IMPL_CID)

nsresult NS_NewCSSStyleSheet(nsCSSStyleSheet** aInstancePtrResult);

#endif /* !defined(nsCSSStyleSheet_h_) */
