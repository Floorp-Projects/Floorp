/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
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
class nsPresContext;
class nsXMLNameSpaceMap;

namespace mozilla {
class CSSStyleSheet;
struct ChildSheetListBuilder;

namespace css {
class GroupRule;
} // namespace css
namespace dom {
class CSSRuleList;
class Element;
} // namespace dom

  // -------------------------------
// CSS Style Sheet Inner Data Container
//

struct CSSStyleSheetInner : public StyleSheetInfo
{
  CSSStyleSheetInner(CORSMode aCORSMode,
                     ReferrerPolicy aReferrerPolicy,
                     const dom::SRIMetadata& aIntegrity);
  CSSStyleSheetInner(CSSStyleSheetInner& aCopy,
                     CSSStyleSheet* aPrimarySheet);
  ~CSSStyleSheetInner();

  StyleSheetInfo* CloneFor(StyleSheet* aPrimarySheet) override;
  void RemoveSheet(StyleSheet* aSheet) override;

  void RebuildNameSpaces();

  // Create a new namespace map
  nsresult CreateNamespaceMap();

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  IncrementalClearCOMRuleArray mOrderedRules;
  nsAutoPtr<nsXMLNameSpaceMap> mNameSpaceMap;
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

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

  // XXX do these belong here or are they generic?
  void AppendStyleRule(css::Rule* aRule);

  int32_t StyleRuleCount() const;
  css::Rule* GetStyleRuleAt(int32_t aIndex) const;

  nsXMLNameSpaceMap* GetNameSpaceMap() const {
    return Inner()->mNameSpaceMap;
  }

  already_AddRefed<StyleSheet> Clone(StyleSheet* aCloneParent,
    dom::CSSImportRule* aCloneOwnerRule,
    nsIDocument* aCloneDocument,
    nsINode* aCloneOwningNode) const final;

  nsresult AddRuleProcessor(nsCSSRuleProcessor* aProcessor);
  nsresult DropRuleProcessor(nsCSSRuleProcessor* aProcessor);

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus) override;

  bool UseForPresentation(nsPresContext* aPresContext,
                            nsMediaQueryResultCacheKey& aKey) const;

  nsresult ReparseSheet(const nsAString& aInput);

  void SetInRuleProcessorCache() { mInRuleProcessorCache = true; }

  // Function used as a callback to rebuild our inner's child sheet
  // list after we clone a unique inner for ourselves.
  static bool RebuildChildList(css::Rule* aRule,
                               ChildSheetListBuilder* aBuilder);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  dom::Element* GetScopeElement() const { return mScopeElement; }
  void SetScopeElement(dom::Element* aScopeElement);

  void DidDirty() override;

private:
  CSSStyleSheet(const CSSStyleSheet& aCopy,
                CSSStyleSheet* aParentToUse,
                dom::CSSImportRule* aOwnerRuleToUse,
                nsIDocument* aDocumentToUse,
                nsINode* aOwningNodeToUse);

  CSSStyleSheet(const CSSStyleSheet& aCopy) = delete;
  CSSStyleSheet& operator=(const CSSStyleSheet& aCopy) = delete;

protected:
  virtual ~CSSStyleSheet();

  void LastRelease();

  void ClearRuleCascades();

  // Add the namespace mapping from this @namespace rule to our namespace map
  nsresult RegisterNamespaceRule(css::Rule* aRule);

  // Drop our reference to mRuleCollection
  void DropRuleCollection();

  CSSStyleSheetInner* Inner() const
  {
    return static_cast<CSSStyleSheetInner*>(mInner);
  }

  // Unlink our inner, if needed, for cycle collection
  virtual void UnlinkInner() override;
  // Traverse our inner, if needed, for cycle collection
  virtual void TraverseInner(nsCycleCollectionTraversalCallback &) override;

protected:
  // Internal methods which do not have security check and completeness check.
  dom::CSSRuleList* GetCssRulesInternal();
  uint32_t InsertRuleInternal(const nsAString& aRule,
                              uint32_t aIndex, ErrorResult& aRv);
  void DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv);
  nsresult InsertRuleIntoGroupInternal(const nsAString& aRule,
                                       css::GroupRule* aGroup,
                                       uint32_t aIndex);

  void EnabledStateChangedInternal();

  RefPtr<CSSRuleListImpl> mRuleCollection;
  bool                  mInRuleProcessorCache;
  RefPtr<dom::Element> mScopeElement;

  AutoTArray<nsCSSRuleProcessor*, 8>* mRuleProcessors;

  friend class mozilla::StyleSheet;
  friend class ::nsCSSRuleProcessor;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CSSStyleSheet, NS_CSS_STYLE_SHEET_IMPL_CID)

} // namespace mozilla

#endif /* !defined(mozilla_CSSStyleSheet_h) */
