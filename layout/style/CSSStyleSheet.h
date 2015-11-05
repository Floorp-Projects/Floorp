/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a CSS style sheet */

#ifndef mozilla_CSSStyleSheet_h
#define mozilla_CSSStyleSheet_h

#include "mozilla/Attributes.h"
#include "mozilla/IncrementalClearCOMRuleArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsTArrayForwardDeclare.h"
#include "nsString.h"
#include "mozilla/CORSMode.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/dom/SRIMetadata.h"

class CSSRuleListImpl;
class nsCSSRuleProcessor;
class nsIPrincipal;
class nsIURI;
class nsMediaList;
class nsMediaQueryResultCacheKey;
class nsPresContext;
class nsXMLNameSpaceMap;

namespace mozilla {
struct ChildSheetListBuilder;
class CSSStyleSheet;

namespace css {
class Rule;
class GroupRule;
class ImportRule;
} // namespace css
namespace dom {
class CSSRuleList;
} // namespace dom

// -------------------------------
// CSS Style Sheet Inner Data Container
//

class CSSStyleSheetInner
{
public:
  friend class mozilla::CSSStyleSheet;
  friend class ::nsCSSRuleProcessor;
  typedef net::ReferrerPolicy ReferrerPolicy;

private:
  CSSStyleSheetInner(CSSStyleSheet* aPrimarySheet,
                     CORSMode aCORSMode,
                     ReferrerPolicy aReferrerPolicy,
                     const dom::SRIMetadata& aIntegrity);
  CSSStyleSheetInner(CSSStyleSheetInner& aCopy,
                     CSSStyleSheet* aPrimarySheet);
  ~CSSStyleSheetInner();

  CSSStyleSheetInner* CloneFor(CSSStyleSheet* aPrimarySheet);
  void AddSheet(CSSStyleSheet* aSheet);
  void RemoveSheet(CSSStyleSheet* aSheet);

  void RebuildNameSpaces();

  // Create a new namespace map
  nsresult CreateNamespaceMap();

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  nsAutoTArray<CSSStyleSheet*, 8> mSheets;
  nsCOMPtr<nsIURI>       mSheetURI; // for error reports, etc.
  nsCOMPtr<nsIURI>       mOriginalSheetURI;  // for GetHref.  Can be null.
  nsCOMPtr<nsIURI>       mBaseURI; // for resolving relative URIs
  nsCOMPtr<nsIPrincipal> mPrincipal;
  IncrementalClearCOMRuleArray mOrderedRules;
  nsAutoPtr<nsXMLNameSpaceMap> mNameSpaceMap;
  // Linked list of child sheets.  This is al fundamentally broken, because
  // each of the child sheets has a unique parent... We can only hope (and
  // currently this is the case) that any time page JS can get ts hands on a
  // child sheet that means we've already ensured unique inners throughout its
  // parent chain and things are good.
  RefPtr<CSSStyleSheet> mFirstChild;
  CORSMode               mCORSMode;
  // The Referrer Policy of a stylesheet is used for its child sheets, so it is
  // stored here.
  ReferrerPolicy         mReferrerPolicy;
  dom::SRIMetadata       mIntegrity;
  bool                   mComplete;

#ifdef DEBUG
  bool                   mPrincipalSet;
#endif
};


// -------------------------------
// CSS Style Sheet
//

// CID for the CSSStyleSheet class
// ca926f30-2a7e-477e-8467-803fb32af20a
#define NS_CSS_STYLE_SHEET_IMPL_CID     \
{ 0xca926f30, 0x2a7e, 0x477e, \
 { 0x84, 0x67, 0x80, 0x3f, 0xb3, 0x2a, 0xf2, 0x0a } }


class CSSStyleSheet final : public nsIStyleSheet,
                            public nsIDOMCSSStyleSheet,
                            public nsICSSLoaderObserver,
                            public nsWrapperCache
{
public:
  typedef net::ReferrerPolicy ReferrerPolicy;
  CSSStyleSheet(CORSMode aCORSMode, ReferrerPolicy aReferrerPolicy);
  CSSStyleSheet(CORSMode aCORSMode, ReferrerPolicy aReferrerPolicy,
                const dom::SRIMetadata& aIntegrity);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(CSSStyleSheet,
                                                         nsIStyleSheet)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_STYLE_SHEET_IMPL_CID)

  // nsIStyleSheet interface
  virtual nsIURI* GetSheetURI() const override;
  virtual nsIURI* GetBaseURI() const override;
  virtual void GetTitle(nsString& aTitle) const override;
  virtual void GetType(nsString& aType) const override;
  virtual bool HasRules() const override;
  virtual bool IsApplicable() const override;
  virtual void SetEnabled(bool aEnabled) override;
  virtual bool IsComplete() const override;
  virtual void SetComplete() override;
  virtual nsIStyleSheet* GetParentSheet() const override;  // may be null
  virtual nsIDocument* GetOwningDocument() const override;  // may be null
  virtual void SetOwningDocument(nsIDocument* aDocument) override;

  // Find the ID of the owner inner window.
  uint64_t FindOwningWindowInnerID() const;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

  void AppendStyleSheet(CSSStyleSheet* aSheet);
  void InsertStyleSheetAt(CSSStyleSheet* aSheet, int32_t aIndex);

  // XXX do these belong here or are they generic?
  void AppendStyleRule(css::Rule* aRule);
  void ReplaceStyleRule(css::Rule* aOld, css::Rule* aNew);

  int32_t StyleRuleCount() const;
  css::Rule* GetStyleRuleAt(int32_t aIndex) const;

  nsresult DeleteRuleFromGroup(css::GroupRule* aGroup, uint32_t aIndex);
  nsresult InsertRuleIntoGroup(const nsAString& aRule, css::GroupRule* aGroup, uint32_t aIndex, uint32_t* _retval);
  nsresult ReplaceRuleInGroup(css::GroupRule* aGroup, css::Rule* aOld, css::Rule* aNew);

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

  void SetOwnerRule(css::ImportRule* aOwnerRule) { mOwnerRule = aOwnerRule; /* Not ref counted */ }
  css::ImportRule* GetOwnerRule() const { return mOwnerRule; }

  nsXMLNameSpaceMap* GetNameSpaceMap() const { return mInner->mNameSpaceMap; }

  already_AddRefed<CSSStyleSheet> Clone(CSSStyleSheet* aCloneParent,
                                        css::ImportRule* aCloneOwnerRule,
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

  void AddStyleSet(nsStyleSet* aStyleSet);
  void DropStyleSet(nsStyleSet* aStyleSet);

  /**
   * Like the DOM insertRule() method, but doesn't do any security checks
   */
  nsresult InsertRuleInternal(const nsAString& aRule,
                              uint32_t aIndex, uint32_t* aReturn);

  /* Get the URI this sheet was originally loaded from, if any.  Can
     return null */
  virtual nsIURI* GetOriginalURI() const;

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(CSSStyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus) override;

  void EnsureUniqueInner();

  // Append all of this sheet's child sheets to aArray.
  void AppendAllChildSheets(nsTArray<CSSStyleSheet*>& aArray);

  bool UseForPresentation(nsPresContext* aPresContext,
                            nsMediaQueryResultCacheKey& aKey) const;

  nsresult ReparseSheet(const nsAString& aInput);

  void SetInRuleProcessorCache() { mInRuleProcessorCache = true; }

  // nsIDOMStyleSheet interface
  NS_DECL_NSIDOMSTYLESHEET

  // nsIDOMCSSStyleSheet interface
  NS_DECL_NSIDOMCSSSTYLESHEET

  // Function used as a callback to rebuild our inner's child sheet
  // list after we clone a unique inner for ourselves.
  static bool RebuildChildList(css::Rule* aRule, void* aBuilder);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  // Get this style sheet's CORS mode
  CORSMode GetCORSMode() const { return mInner->mCORSMode; }

  // Get this style sheet's Referrer Policy
  ReferrerPolicy GetReferrerPolicy() const { return mInner->mReferrerPolicy; }

  // Get this style sheet's integrity metadata
  dom::SRIMetadata GetIntegrity() const { return mInner->mIntegrity; }

  dom::Element* GetScopeElement() const { return mScopeElement; }
  void SetScopeElement(dom::Element* aScopeElement)
  {
    mScopeElement = aScopeElement;
  }

  // WebIDL StyleSheet API
  // Our nsIStyleSheet::GetType is a const method, so it ends up
  // ambiguous with with the XPCOM version.  Just disambiguate.
  void GetType(nsString& aType) {
    const_cast<const CSSStyleSheet*>(this)->GetType(aType);
  }
  // Our XPCOM GetHref is fine for WebIDL
  nsINode* GetOwnerNode() const { return mOwningNode; }
  CSSStyleSheet* GetParentStyleSheet() const { return mParent; }
  // Our nsIStyleSheet::GetTitle is a const method, so it ends up
  // ambiguous with with the XPCOM version.  Just disambiguate.
  void GetTitle(nsString& aTitle) {
    const_cast<const CSSStyleSheet*>(this)->GetTitle(aTitle);
  }
  nsMediaList* Media();
  bool Disabled() const { return mDisabled; }
  // The XPCOM SetDisabled is fine for WebIDL

  // WebIDL CSSStyleSheet API
  // Can't be inline because we can't include ImportRule here.  And can't be
  // called GetOwnerRule because that would be ambiguous with the ImportRule
  // version.
  nsIDOMCSSRule* GetDOMOwnerRule() const;
  dom::CSSRuleList* GetCssRules(ErrorResult& aRv);
  uint32_t InsertRule(const nsAString& aRule, uint32_t aIndex,
                      ErrorResult& aRv) {
    uint32_t retval;
    aRv = InsertRule(aRule, aIndex, &retval);
    return retval;
  }
  void DeleteRule(uint32_t aIndex, ErrorResult& aRv) {
    aRv = DeleteRule(aIndex);
  }

  // WebIDL miscellaneous bits
  dom::ParentObject GetParentObject() const {
    if (mOwningNode) {
      return dom::ParentObject(mOwningNode);
    }

    return dom::ParentObject(static_cast<nsIStyleSheet*>(mParent), mParent);
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  CSSStyleSheet(const CSSStyleSheet& aCopy,
                CSSStyleSheet* aParentToUse,
                css::ImportRule* aOwnerRuleToUse,
                nsIDocument* aDocumentToUse,
                nsINode* aOwningNodeToUse);

  CSSStyleSheet(const CSSStyleSheet& aCopy) = delete;
  CSSStyleSheet& operator=(const CSSStyleSheet& aCopy) = delete;

protected:
  virtual ~CSSStyleSheet();

  void ClearRuleCascades();

  void     WillDirty();
  void     DidDirty();

  // Return success if the subject principal subsumes the principal of our
  // inner, error otherwise.  This will also succeed if the subject has
  // UniversalXPConnect or if access is allowed by CORS.  In the latter case,
  // it will set the principal of the inner to the subject principal.
  nsresult SubjectSubsumesInnerPrincipal();

  // Add the namespace mapping from this @namespace rule to our namespace map
  nsresult RegisterNamespaceRule(css::Rule* aRule);

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
  RefPtr<nsMediaList> mMedia;
  RefPtr<CSSStyleSheet> mNext;
  CSSStyleSheet*        mParent;    // weak ref
  css::ImportRule*      mOwnerRule; // weak ref

  RefPtr<CSSRuleListImpl> mRuleCollection;
  nsIDocument*          mDocument; // weak ref; parents maintain this for their children
  nsINode*              mOwningNode; // weak ref
  bool                  mDisabled;
  bool                  mDirty; // has been modified 
  bool                  mInRuleProcessorCache;
  RefPtr<dom::Element> mScopeElement;

  CSSStyleSheetInner*   mInner;

  nsAutoTArray<nsCSSRuleProcessor*, 8>* mRuleProcessors;
  nsTArray<nsStyleSet*> mStyleSets;

  friend class ::nsMediaList;
  friend class ::nsCSSRuleProcessor;
  friend struct mozilla::ChildSheetListBuilder;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CSSStyleSheet, NS_CSS_STYLE_SHEET_IMPL_CID)

} // namespace mozilla

#endif /* !defined(mozilla_CSSStyleSheet_h) */
