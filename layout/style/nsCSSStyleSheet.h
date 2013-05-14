/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a CSS style sheet */

#ifndef nsCSSStyleSheet_h_
#define nsCSSStyleSheet_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsString.h"
#include "mozilla/CORSMode.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsXMLNameSpaceMap;
class nsCSSRuleProcessor;
class nsIPrincipal;
class nsIURI;
class nsMediaList;
class nsMediaQueryResultCacheKey;
class nsCSSStyleSheet;
class nsPresContext;

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
private:
  nsCSSStyleSheetInner(nsCSSStyleSheet* aPrimarySheet,
                       mozilla::CORSMode aCORSMode);
  nsCSSStyleSheetInner(nsCSSStyleSheetInner& aCopy,
                       nsCSSStyleSheet* aPrimarySheet);
  ~nsCSSStyleSheetInner();

  nsCSSStyleSheetInner* CloneFor(nsCSSStyleSheet* aPrimarySheet);
  void AddSheet(nsCSSStyleSheet* aSheet);
  void RemoveSheet(nsCSSStyleSheet* aSheet);

  void RebuildNameSpaces();

  // Create a new namespace map
  nsresult CreateNamespaceMap();

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

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
  mozilla::CORSMode      mCORSMode;
  bool                   mComplete;

#ifdef DEBUG
  bool                   mPrincipalSet;
#endif
};


// -------------------------------
// CSS Style Sheet
//

class CSSRuleListImpl;

// CID for the nsCSSStyleSheet class
// ca926f30-2a7e-477e-8467-803fb32af20a
#define NS_CSS_STYLE_SHEET_IMPL_CID     \
{ 0xca926f30, 0x2a7e, 0x477e, \
 { 0x84, 0x67, 0x80, 0x3f, 0xb3, 0x2a, 0xf2, 0x0a } }


class nsCSSStyleSheet MOZ_FINAL : public nsIStyleSheet,
                                  public nsIDOMCSSStyleSheet,
                                  public nsICSSLoaderObserver,
                                  public nsWrapperCache
{
public:
  nsCSSStyleSheet(mozilla::CORSMode aCORSMode);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsCSSStyleSheet,
                                                         nsIStyleSheet)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_STYLE_SHEET_IMPL_CID)

  // nsIStyleSheet interface
  virtual nsIURI* GetSheetURI() const MOZ_OVERRIDE;
  virtual nsIURI* GetBaseURI() const MOZ_OVERRIDE;
  virtual void GetTitle(nsString& aTitle) const MOZ_OVERRIDE;
  virtual void GetType(nsString& aType) const MOZ_OVERRIDE;
  virtual bool HasRules() const MOZ_OVERRIDE;
  virtual bool IsApplicable() const MOZ_OVERRIDE;
  virtual void SetEnabled(bool aEnabled) MOZ_OVERRIDE;
  virtual bool IsComplete() const MOZ_OVERRIDE;
  virtual void SetComplete() MOZ_OVERRIDE;
  virtual nsIStyleSheet* GetParentSheet() const MOZ_OVERRIDE;  // may be null
  virtual nsIDocument* GetOwningDocument() const MOZ_OVERRIDE;  // may be null
  virtual void SetOwningDocument(nsIDocument* aDocument) MOZ_OVERRIDE;

  // Find the ID of the owner inner window.
  uint64_t FindOwningWindowInnerID() const;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif

  void AppendStyleSheet(nsCSSStyleSheet* aSheet);
  void InsertStyleSheetAt(nsCSSStyleSheet* aSheet, int32_t aIndex);

  // XXX do these belong here or are they generic?
  void PrependStyleRule(mozilla::css::Rule* aRule);
  void AppendStyleRule(mozilla::css::Rule* aRule);
  void ReplaceStyleRule(mozilla::css::Rule* aOld, mozilla::css::Rule* aNew);

  int32_t StyleRuleCount() const;
  nsresult GetStyleRuleAt(int32_t aIndex, mozilla::css::Rule*& aRule) const;

  nsresult DeleteRuleFromGroup(mozilla::css::GroupRule* aGroup, uint32_t aIndex);
  nsresult InsertRuleIntoGroup(const nsAString& aRule, mozilla::css::GroupRule* aGroup, uint32_t aIndex, uint32_t* _retval);
  nsresult ReplaceRuleInGroup(mozilla::css::GroupRule* aGroup, mozilla::css::Rule* aOld, mozilla::css::Rule* aNew);

  int32_t StyleSheetCount() const;

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

  // The document this style sheet is associated with.  May be null
  nsIDocument* GetDocument() const { return mDocument; }

  void SetTitle(const nsAString& aTitle) { mTitle = aTitle; }
  void SetMedia(nsMediaList* aMedia);
  void SetOwningNode(nsINode* aOwningNode) { mOwningNode = aOwningNode; /* Not ref counted */ }

  void SetOwnerRule(mozilla::css::ImportRule* aOwnerRule) { mOwnerRule = aOwnerRule; /* Not ref counted */ }
  mozilla::css::ImportRule* GetOwnerRule() const { return mOwnerRule; }

  nsXMLNameSpaceMap* GetNameSpaceMap() const { return mInner->mNameSpaceMap; }

  already_AddRefed<nsCSSStyleSheet> Clone(nsCSSStyleSheet* aCloneParent,
                                          mozilla::css::ImportRule* aCloneOwnerRule,
                                          nsIDocument* aCloneDocument,
                                          nsINode* aCloneOwningNode) const;

  bool IsModified() const { return mDirty; }

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
                              uint32_t aIndex, uint32_t* aReturn);

  /* Get the URI this sheet was originally loaded from, if any.  Can
     return null */
  virtual nsIURI* GetOriginalURI() const;

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus) MOZ_OVERRIDE;

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

  // Append all of this sheet's child sheets to aArray.  Return true
  // on success and false on allocation failure.
  bool AppendAllChildSheets(nsTArray<nsCSSStyleSheet*>& aArray);

  bool UseForPresentation(nsPresContext* aPresContext,
                            nsMediaQueryResultCacheKey& aKey) const;

  nsresult ParseSheet(const nsAString& aInput);

  // nsIDOMStyleSheet interface
  NS_DECL_NSIDOMSTYLESHEET

  // nsIDOMCSSStyleSheet interface
  NS_DECL_NSIDOMCSSSTYLESHEET

  // Function used as a callback to rebuild our inner's child sheet
  // list after we clone a unique inner for ourselves.
  static bool RebuildChildList(mozilla::css::Rule* aRule, void* aBuilder);

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const MOZ_OVERRIDE;

  // Get this style sheet's CORS mode
  mozilla::CORSMode GetCORSMode() const { return mInner->mCORSMode; }

  mozilla::dom::Element* GetScopeElement() const { return mScopeElement; }
  void SetScopeElement(mozilla::dom::Element* aScopeElement)
  {
    mScopeElement = aScopeElement;
  }

  // WebIDL StyleSheet API
  // Our nsIStyleSheet::GetType is a const method, so it ends up
  // ambiguous with with the XPCOM version.  Just disambiguate.
  void GetType(nsString& aType) {
    const_cast<const nsCSSStyleSheet*>(this)->GetType(aType);
  }
  // Our XPCOM GetHref is fine for WebIDL
  nsINode* GetOwnerNode() const { return mOwningNode; }
  nsCSSStyleSheet* GetParentStyleSheet() const { return mParent; }
  // Our nsIStyleSheet::GetTitle is a const method, so it ends up
  // ambiguous with with the XPCOM version.  Just disambiguate.
  void GetTitle(nsString& aTitle) {
    const_cast<const nsCSSStyleSheet*>(this)->GetTitle(aTitle);
  }
  nsIDOMMediaList* Media();
  bool Disabled() const { return mDisabled; }
  // The XPCOM SetDisabled is fine for WebIDL

  // WebIDL CSSStyleSheet API
  // Can't be inline because we can't include ImportRule here.  And can't be
  // called GetOwnerRule because that would be ambiguous with the ImportRule
  // version.
  nsIDOMCSSRule* GetDOMOwnerRule() const;
  nsIDOMCSSRuleList* GetCssRules(mozilla::ErrorResult& aRv);
  uint32_t InsertRule(const nsAString& aRule, uint32_t aIndex,
                      mozilla::ErrorResult& aRv) {
    uint32_t retval;
    aRv = InsertRule(aRule, aIndex, &retval);
    return retval;
  }
  void DeleteRule(uint32_t aIndex, mozilla::ErrorResult& aRv) {
    aRv = DeleteRule(aIndex);
  }

  // WebIDL miscellaneous bits
  mozilla::dom::ParentObject GetParentObject() const {
    if (mOwningNode) {
      return mozilla::dom::ParentObject(mOwningNode);
    }

    return mozilla::dom::ParentObject(static_cast<nsIStyleSheet*>(mParent),
                                      mParent);
  }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

private:
  nsCSSStyleSheet(const nsCSSStyleSheet& aCopy,
                  nsCSSStyleSheet* aParentToUse,
                  mozilla::css::ImportRule* aOwnerRuleToUse,
                  nsIDocument* aDocumentToUse,
                  nsINode* aOwningNodeToUse);

  nsCSSStyleSheet(const nsCSSStyleSheet& aCopy) MOZ_DELETE;
  nsCSSStyleSheet& operator=(const nsCSSStyleSheet& aCopy) MOZ_DELETE;

protected:
  virtual ~nsCSSStyleSheet();

  void ClearRuleCascades();

  nsresult WillDirty();
  void     DidDirty();

  // Return success if the subject principal subsumes the principal of our
  // inner, error otherwise.  This will also succeed if the subject has
  // UniversalXPConnect or if access is allowed by CORS.  In the latter case,
  // it will set the principal of the inner to the subject principal.
  nsresult SubjectSubsumesInnerPrincipal();

  // Add the namespace mapping from this @namespace rule to our namespace map
  nsresult RegisterNamespaceRule(mozilla::css::Rule* aRule);

  // Drop our reference to mRuleCollection
  void DropRuleCollection();

  // Drop our reference to mMedia
  void DropMedia();

  // Unlink our inner, if needed, for cycle collection
  void UnlinkInner();
  // Traverse our inner, if needed, for cycle collection
  void TraverseInner(nsCycleCollectionTraversalCallback &);

protected:
  nsString              mTitle;
  nsRefPtr<nsMediaList> mMedia;
  nsRefPtr<nsCSSStyleSheet> mNext;
  nsCSSStyleSheet*      mParent;    // weak ref
  mozilla::css::ImportRule* mOwnerRule; // weak ref

  nsRefPtr<CSSRuleListImpl> mRuleCollection;
  nsIDocument*          mDocument; // weak ref; parents maintain this for their children
  nsINode*              mOwningNode; // weak ref
  bool                  mDisabled;
  bool                  mDirty; // has been modified 
  nsRefPtr<mozilla::dom::Element> mScopeElement;

  nsCSSStyleSheetInner* mInner;

  nsAutoTArray<nsCSSRuleProcessor*, 8>* mRuleProcessors;

  friend class nsMediaList;
  friend class nsCSSRuleProcessor;
  friend struct ChildSheetListBuilder;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCSSStyleSheet, NS_CSS_STYLE_SHEET_IMPL_CID)

#endif /* !defined(nsCSSStyleSheet_h_) */
