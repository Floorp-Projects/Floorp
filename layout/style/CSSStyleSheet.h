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
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInfo.h"
#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/dom/Element.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
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
class nsStyleSet;
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

class CSSStyleSheetInner : public StyleSheetInfo
{
public:
  friend class mozilla::CSSStyleSheet;
  friend class ::nsCSSRuleProcessor;

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

  AutoTArray<CSSStyleSheet*, 8> mSheets;
  IncrementalClearCOMRuleArray mOrderedRules;
  nsAutoPtr<nsXMLNameSpaceMap> mNameSpaceMap;
  // Linked list of child sheets.  This is al fundamentally broken, because
  // each of the child sheets has a unique parent... We can only hope (and
  // currently this is the case) that any time page JS can get ts hands on a
  // child sheet that means we've already ensured unique inners throughout its
  // parent chain and things are good.
  RefPtr<CSSStyleSheet> mFirstChild;
};


// -------------------------------
// CSS Style Sheet
//

// CID for the CSSStyleSheet class
// 7985c7ac-9ddc-444d-9899-0c86ec122f54
#define NS_CSS_STYLE_SHEET_IMPL_CID     \
{ 0x7985c7ac, 0x9ddc, 0x444d, \
  { 0x98, 0x99, 0x0c, 0x86, 0xec, 0x12, 0x2f, 0x54 } }


class CSSStyleSheet final : public nsIDOMCSSStyleSheet,
                            public nsICSSLoaderObserver,
                            public nsWrapperCache,
                            public StyleSheet
{
public:
  typedef net::ReferrerPolicy ReferrerPolicy;
  CSSStyleSheet(CORSMode aCORSMode, ReferrerPolicy aReferrerPolicy);
  CSSStyleSheet(CORSMode aCORSMode, ReferrerPolicy aReferrerPolicy,
                const dom::SRIMetadata& aIntegrity);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(CSSStyleSheet,
                                                         nsIDOMCSSStyleSheet)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_STYLE_SHEET_IMPL_CID)

  nsIURI* GetSheetURI() const;
  nsIURI* GetBaseURI() const;
  void GetTitle(nsString& aTitle) const;
  void GetType(nsString& aType) const;
  bool HasRules() const;

  /**
   * Whether the sheet is applicable.  A sheet that is not applicable
   * should never be inserted into a style set.  A sheet may not be
   * applicable for a variety of reasons including being disabled and
   * being incomplete.
   */
  bool IsApplicable() const;

  /**
   * Set the stylesheet to be enabled.  This may or may not make it
   * applicable.  Note that this WILL inform the sheet's document of
   * its new applicable state if the state changes but WILL NOT call
   * BeginUpdate() or EndUpdate() on the document -- calling those is
   * the caller's responsibility.  This allows use of SetEnabled when
   * batched updates are desired.  If you want updates handled for
   * you, see nsIDOMStyleSheet::SetDisabled().
   */
  void SetEnabled(bool aEnabled);

  // style sheet owner info
  CSSStyleSheet* GetParentSheet() const;  // may be null
  nsIDocument* GetOwningDocument() const;  // may be null
  void SetOwningDocument(nsIDocument* aDocument);

  // Find the ID of the owner inner window.
  uint64_t FindOwningWindowInnerID() const;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const;
#endif

  void AppendStyleSheet(CSSStyleSheet* aSheet);

  // XXX do these belong here or are they generic?
  void AppendStyleRule(css::Rule* aRule);

  int32_t StyleRuleCount() const;
  css::Rule* GetStyleRuleAt(int32_t aIndex) const;

  nsresult DeleteRuleFromGroup(css::GroupRule* aGroup, uint32_t aIndex);
  nsresult InsertRuleIntoGroup(const nsAString& aRule, css::GroupRule* aGroup, uint32_t aIndex, uint32_t* _retval);

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
  nsIURI* GetOriginalURI() const { return mInner->mOriginalSheetURI; }

  // Whether the sheet is for an inline <style> element.
  bool IsInline() const { return mInner->IsInline(); }

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(StyleSheetHandle aSheet, bool aWasAlternate,
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

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // Get this style sheet's CORS mode
  CORSMode GetCORSMode() const { return mInner->mCORSMode; }

  // Get this style sheet's Referrer Policy
  ReferrerPolicy GetReferrerPolicy() const { return mInner->mReferrerPolicy; }

  // Get this style sheet's integrity metadata
  void GetIntegrity(dom::SRIMetadata& aResult) const { aResult = mInner->mIntegrity; }

  dom::Element* GetScopeElement() const { return mScopeElement; }
  void SetScopeElement(dom::Element* aScopeElement)
  {
    mScopeElement = aScopeElement;
  }

  // WebIDL StyleSheet API
  // Our CSSStyleSheet::GetType is a const method, so it ends up
  // ambiguous with with the XPCOM version.  Just disambiguate.
  void GetType(nsString& aType) {
    const_cast<const CSSStyleSheet*>(this)->GetType(aType);
  }
  // Our XPCOM GetHref is fine for WebIDL
  nsINode* GetOwnerNode() const { return mOwningNode; }
  CSSStyleSheet* GetParentStyleSheet() const { return mParent; }
  // Our CSSStyleSheet::GetTitle is a const method, so it ends up
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

    return dom::ParentObject(static_cast<nsIDOMCSSStyleSheet*>(mParent), mParent);
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Changes to sheets should be inside of a WillDirty-DidDirty pair.
  // However, the calls do not need to be matched; it's ok to call
  // WillDirty and then make no change and skip the DidDirty call.
  void WillDirty();
  void DidDirty();

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
  bool                  mDirty; // has been modified 
  bool                  mInRuleProcessorCache;
  RefPtr<dom::Element> mScopeElement;

  CSSStyleSheetInner*   mInner;

  AutoTArray<nsCSSRuleProcessor*, 8>* mRuleProcessors;
  nsTArray<nsStyleSet*> mStyleSets;

  friend class ::nsMediaList;
  friend class ::nsCSSRuleProcessor;
  friend class mozilla::StyleSheet;
  friend struct mozilla::ChildSheetListBuilder;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CSSStyleSheet, NS_CSS_STYLE_SHEET_IMPL_CID)

} // namespace mozilla

#endif /* !defined(mozilla_CSSStyleSheet_h) */
