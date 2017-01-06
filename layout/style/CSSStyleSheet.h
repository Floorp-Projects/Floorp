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
#include "nsICSSLoaderObserver.h"
#include "nsTArrayForwardDeclare.h"
#include "nsString.h"
#include "mozilla/CORSMode.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/dom/SRIMetadata.h"

class CSSRuleListImpl;
class nsCSSRuleProcessor;
class nsIURI;
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

struct CSSStyleSheetInner : public StyleSheetInfo
{
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


class CSSStyleSheet final : public StyleSheet
                          , public nsICSSLoaderObserver
{
public:
  typedef net::ReferrerPolicy ReferrerPolicy;
  CSSStyleSheet(css::SheetParsingMode aParsingMode,
                CORSMode aCORSMode, ReferrerPolicy aReferrerPolicy);
  CSSStyleSheet(css::SheetParsingMode aParsingMode,
                CORSMode aCORSMode, ReferrerPolicy aReferrerPolicy,
                const dom::SRIMetadata& aIntegrity);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CSSStyleSheet, StyleSheet)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_STYLE_SHEET_IMPL_CID)

  bool HasRules() const;

  // style sheet owner info
  CSSStyleSheet* GetParentSheet() const;  // may be null
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

  void SetOwnerRule(css::ImportRule* aOwnerRule) { mOwnerRule = aOwnerRule; /* Not ref counted */ }
  css::ImportRule* GetOwnerRule() const { return mOwnerRule; }
  // Workaround overloaded-virtual warning in GCC.
  using StyleSheet::GetOwnerRule;

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

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus) override;

  void EnsureUniqueInner();

  // Append all of this sheet's child sheets to aArray.
  void AppendAllChildSheets(nsTArray<CSSStyleSheet*>& aArray);

  bool UseForPresentation(nsPresContext* aPresContext,
                            nsMediaQueryResultCacheKey& aKey) const;

  nsresult ReparseSheet(const nsAString& aInput);

  void SetInRuleProcessorCache() { mInRuleProcessorCache = true; }

  // Function used as a callback to rebuild our inner's child sheet
  // list after we clone a unique inner for ourselves.
  static bool RebuildChildList(css::Rule* aRule, void* aBuilder);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  dom::Element* GetScopeElement() const { return mScopeElement; }
  void SetScopeElement(dom::Element* aScopeElement)
  {
    mScopeElement = aScopeElement;
  }

  // WebIDL CSSStyleSheet API
  // Can't be inline because we can't include ImportRule here.  And can't be
  // called GetOwnerRule because that would be ambiguous with the ImportRule
  // version.
  nsIDOMCSSRule* GetDOMOwnerRule() const final;

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

  // Add the namespace mapping from this @namespace rule to our namespace map
  nsresult RegisterNamespaceRule(css::Rule* aRule);

  // Drop our reference to mRuleCollection
  void DropRuleCollection();

  // Unlink our inner, if needed, for cycle collection
  void UnlinkInner();
  // Traverse our inner, if needed, for cycle collection
  void TraverseInner(nsCycleCollectionTraversalCallback &);

protected:
  // Internal methods which do not have security check and completeness check.
  dom::CSSRuleList* GetCssRulesInternal(ErrorResult& aRv);
  uint32_t InsertRuleInternal(const nsAString& aRule,
                              uint32_t aIndex, ErrorResult& aRv);
  void DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv);

  void EnabledStateChangedInternal();

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

  friend class ::nsCSSRuleProcessor;
  friend class mozilla::StyleSheet;
  friend struct mozilla::ChildSheetListBuilder;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CSSStyleSheet, NS_CSS_STYLE_SHEET_IMPL_CID)

} // namespace mozilla

#endif /* !defined(mozilla_CSSStyleSheet_h) */
