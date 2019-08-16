/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheet_h
#define mozilla_StyleSheet_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/dom/CSSStyleSheetBinding.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/CORSMode.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/StyleSheetInfo.h"
#include "mozilla/URLExtraData.h"
#include "nsICSSLoaderObserver.h"
#include "nsWrapperCache.h"
#include "nsCompatibility.h"
#include "nsStringFwd.h"

class nsINode;
class nsIPrincipal;
struct nsLayoutStylesheetCacheShm;
struct RawServoSharedMemoryBuilder;
class nsIReferrerInfo;

namespace mozilla {

class ServoCSSRuleList;
class ServoStyleSet;

typedef MozPromise</* Dummy */ bool,
                   /* Dummy */ bool,
                   /* IsExclusive = */ true>
    StyleSheetParsePromise;

namespace css {
class GroupRule;
class Loader;
class LoaderReusableStyleSheets;
class Rule;
class SheetLoadData;
}  // namespace css

namespace dom {
class CSSImportRule;
class CSSRuleList;
class DocumentOrShadowRoot;
class MediaList;
class ShadowRoot;
class SRIMetadata;
}  // namespace dom

enum class StyleSheetState : uint8_t {
  // Whether the sheet is disabled. Sheets can be made disabled via CSSOM, or
  // via alternate links and such.
  Disabled = 1 << 0,
  // Whether the sheet is complete. The sheet is complete if it's finished
  // loading. See StyleSheet::SetComplete.
  Complete = 1 << 1,
  // Whether we've forced a unique inner. StyleSheet objects share an 'inner'
  // StyleSheetInfo object if they share URL, CORS mode, etc.
  //
  // See the Loader's `mCompleteSheets` and `mLoadingSheets`.
  ForcedUniqueInner = 1 << 2,
  // Whether this stylesheet has suffered any modification to the rules via
  // CSSOM.
  //
  // FIXME(emilio): I think as of right now we also set this flag for normal
  // @import rules, which looks very fishy.
  ModifiedRules = 1 << 3,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(StyleSheetState)

class StyleSheet final : public nsICSSLoaderObserver, public nsWrapperCache {
  StyleSheet(const StyleSheet& aCopy, StyleSheet* aParentToUse,
             dom::CSSImportRule* aOwnerRuleToUse,
             dom::DocumentOrShadowRoot* aDocOrShadowRootToUse,
             nsINode* aOwningNodeToUse);

  virtual ~StyleSheet();

  using State = StyleSheetState;

 public:
  StyleSheet(css::SheetParsingMode aParsingMode, CORSMode aCORSMode,
             const dom::SRIMetadata& aIntegrity);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(StyleSheet)

  already_AddRefed<StyleSheet> CreateEmptyChildSheet(
      already_AddRefed<dom::MediaList> aMediaList) const;

  bool HasRules() const;

  // Parses a stylesheet. The load data argument corresponds to the
  // SheetLoadData for this stylesheet.
  RefPtr<StyleSheetParsePromise> ParseSheet(css::Loader&,
                                            const nsACString& aBytes,
                                            css::SheetLoadData&);

  // Common code that needs to be called after servo finishes parsing. This is
  // shared between the parallel and sequential paths.
  void FinishAsyncParse(
      already_AddRefed<RawServoStyleSheetContents> aSheetContents);

  // Similar to the above, but guarantees that parsing will be performed
  // synchronously.
  //
  // The load data may be null sometimes.
  void ParseSheetSync(
      css::Loader* aLoader, const nsACString& aBytes,
      css::SheetLoadData* aLoadData, uint32_t aLineNumber,
      css::LoaderReusableStyleSheets* aReusableSheets = nullptr);

  nsresult ReparseSheet(const nsAString& aInput);

  const RawServoStyleSheetContents* RawContents() const {
    return Inner().mContents;
  }

  void SetContentsForImport(const RawServoStyleSheetContents* aContents) {
    MOZ_ASSERT(!Inner().mContents);
    Inner().mContents = aContents;
  }

  URLExtraData* URLData() const { return Inner().mURLData; }

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) final;

  // Internal GetCssRules methods which do not have security check and
  // completeness check.
  ServoCSSRuleList* GetCssRulesInternal();

  // Returns the stylesheet's Servo origin as a StyleOrigin value.
  mozilla::StyleOrigin GetOrigin() const;

  /**
   * The different changes that a stylesheet may go through.
   *
   * Used by the StyleSets in order to handle more efficiently some kinds of
   * changes.
   */
  enum class ChangeType {
    Added,
    Removed,
    ApplicableStateChanged,
    RuleAdded,
    RuleRemoved,
    RuleChanged,
  };

  void SetOwningNode(nsINode* aOwningNode) { mOwningNode = aOwningNode; }

  css::SheetParsingMode ParsingMode() const { return mParsingMode; }
  mozilla::dom::CSSStyleSheetParsingMode ParsingModeDOM();

  /**
   * Whether the sheet is complete.
   */
  bool IsComplete() const { return bool(mState & State::Complete); }

  void SetComplete();

  void SetEnabled(bool aEnabled) { SetDisabled(!aEnabled); }

  // Whether the sheet is for an inline <style> element.
  bool IsInline() const { return !GetOriginalURI(); }

  nsIURI* GetSheetURI() const { return Inner().mSheetURI; }

  /**
   * Get the URI this sheet was originally loaded from, if any. Can return null.
   */
  nsIURI* GetOriginalURI() const { return Inner().mOriginalSheetURI; }

  nsIURI* GetBaseURI() const { return Inner().mBaseURI; }

  /**
   * SetURIs must be called on all sheets before parsing into them.
   * SetURIs may only be called while the sheet is 1) incomplete and 2)
   * has no rules in it.
   *
   * FIXME(emilio): Can we pass this down when constructing the sheet instead?
   */
  inline void SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                      nsIURI* aBaseURI);

  /**
   * Whether the sheet is applicable.  A sheet that is not applicable
   * should never be inserted into a style set.  A sheet may not be
   * applicable for a variety of reasons including being disabled and
   * being incomplete.
   */
  bool IsApplicable() const { return !Disabled() && IsComplete(); }

  already_AddRefed<StyleSheet> Clone(
      StyleSheet* aCloneParent, dom::CSSImportRule* aCloneOwnerRule,
      dom::DocumentOrShadowRoot* aCloneDocumentOrShadowRoot,
      nsINode* aCloneOwningNode) const;

  bool HasForcedUniqueInner() const {
    return bool(mState & State::ForcedUniqueInner);
  }

  bool HasModifiedRules() const { return bool(mState & State::ModifiedRules); }

  void ClearModifiedRules() { mState &= ~State::ModifiedRules; }

  bool HasUniqueInner() const { return Inner().mSheets.Length() == 1; }

  void AssertHasUniqueInner() const { MOZ_ASSERT(HasUniqueInner()); }

  void EnsureUniqueInner();

  // Append all of this sheet's child sheets to aArray.
  void AppendAllChildSheets(nsTArray<StyleSheet*>& aArray);

  // style sheet owner info
  enum AssociationMode : uint8_t {
    // OwnedByDocumentOrShadowRoot means mDocumentOrShadowRoot owns us (possibly
    // via a chain of other stylesheets).
    OwnedByDocumentOrShadowRoot,
    // NotOwnedByDocument means we're owned by something that might have a
    // different lifetime than mDocument.
    NotOwnedByDocumentOrShadowRoot
  };
  dom::DocumentOrShadowRoot* GetAssociatedDocumentOrShadowRoot() const {
    return mDocumentOrShadowRoot;
  }

  // Whether this stylesheet is kept alive by the associated document or
  // associated shadow root's document somehow, and thus at least has the same
  // lifetime as GetAssociatedDocument().
  bool IsKeptAliveByDocument() const;

  // Returns the document whose styles this sheet is affecting.
  dom::Document* GetComposedDoc() const;

  // Returns the document we're associated to, via mDocumentOrShadowRoot.
  //
  // Non-null iff GetAssociatedDocumentOrShadowRoot is non-null.
  dom::Document* GetAssociatedDocument() const;

  void SetAssociatedDocumentOrShadowRoot(dom::DocumentOrShadowRoot*,
                                         AssociationMode);
  void ClearAssociatedDocumentOrShadowRoot() {
    SetAssociatedDocumentOrShadowRoot(nullptr, NotOwnedByDocumentOrShadowRoot);
  }

  nsINode* GetOwnerNode() const { return mOwningNode; }

  StyleSheet* GetParentSheet() const { return mParent; }

  void SetOwnerRule(dom::CSSImportRule* aOwnerRule) {
    mOwnerRule = aOwnerRule; /* Not ref counted */
  }
  dom::CSSImportRule* GetOwnerRule() const { return mOwnerRule; }

  void PrependStyleSheet(StyleSheet* aSheet);

  // Prepend a stylesheet to the child list without calling WillDirty.
  void PrependStyleSheetSilently(StyleSheet* aSheet);

  StyleSheet* GetFirstChild() const;
  StyleSheet* GetMostRecentlyAddedChildSheet() const {
    // New child sheet can only be prepended into the linked list of
    // child sheets, so the most recently added one is always the first.
    return GetFirstChild();
  }

  // Principal() never returns a null pointer.
  nsIPrincipal* Principal() const { return Inner().mPrincipal; }

  /**
   * SetPrincipal should be called on all sheets before parsing into them.
   * This can only be called once with a non-null principal.
   *
   * Calling this with a null pointer is allowed and is treated as a no-op.
   *
   * FIXME(emilio): Can we get this at construction time instead?
   */
  void SetPrincipal(nsIPrincipal* aPrincipal) {
    StyleSheetInfo& info = Inner();
    MOZ_ASSERT(!info.mPrincipalSet, "Should only set principal once");
    if (aPrincipal) {
      info.mPrincipal = aPrincipal;
#ifdef DEBUG
      info.mPrincipalSet = true;
#endif
    }
  }

  void SetTitle(const nsAString& aTitle) { mTitle = aTitle; }
  void SetMedia(dom::MediaList* aMedia);

  // Get this style sheet's CORS mode
  CORSMode GetCORSMode() const { return Inner().mCORSMode; }

  // Get this style sheet's ReferrerInfo
  nsIReferrerInfo* GetReferrerInfo() const { return Inner().mReferrerInfo; }

  // Set this style sheet's ReferrerInfo
  void SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
    Inner().mReferrerInfo = aReferrerInfo;
  }

  // Get this style sheet's integrity metadata
  void GetIntegrity(dom::SRIMetadata& aResult) const {
    aResult = Inner().mIntegrity;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
#ifdef DEBUG
  void List(FILE* aOut = stdout, int32_t aIndex = 0) const;
#endif

  // WebIDL StyleSheet API
  void GetType(nsAString& aType);
  void GetHref(nsAString& aHref, ErrorResult& aRv);
  // GetOwnerNode is defined above.
  StyleSheet* GetParentStyleSheet() const { return GetParentSheet(); }
  void GetTitle(nsAString& aTitle);
  dom::MediaList* Media();
  bool Disabled() const { return bool(mState & State::Disabled); }
  void SetDisabled(bool aDisabled);
  void GetSourceMapURL(nsAString& aTitle);
  void SetSourceMapURL(const nsAString& aSourceMapURL);
  void SetSourceMapURLFromComment(const nsAString& aSourceMapURLFromComment);
  void GetSourceURL(nsAString& aSourceURL);
  void SetSourceURL(const nsAString& aSourceURL);

  // WebIDL CSSStyleSheet API
  // Can't be inline because we can't include ImportRule here.  And can't be
  // called GetOwnerRule because that would be ambiguous with the ImportRule
  // version.
  css::Rule* GetDOMOwnerRule() const;
  dom::CSSRuleList* GetCssRules(nsIPrincipal& aSubjectPrincipal, ErrorResult&);
  uint32_t InsertRule(const nsAString& aRule, uint32_t aIndex,
                      nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv);
  void DeleteRule(uint32_t aIndex, nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aRv);
  int32_t AddRule(const nsAString& aSelector, const nsAString& aBlock,
                  const dom::Optional<uint32_t>& aIndex,
                  nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv);

  // WebIDL miscellaneous bits
  inline dom::ParentObject GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  // Changes to sheets should be after a WillDirty call.
  void WillDirty();

  // Called when a rule changes from CSSOM.
  //
  // FIXME(emilio): This shouldn't allow null, but MediaList doesn't know about
  // it's owning media rule, plus it's used for the stylesheet media itself.
  void RuleChanged(css::Rule*);

  void AddStyleSet(ServoStyleSet* aStyleSet);
  void DropStyleSet(ServoStyleSet* aStyleSet);

  nsresult DeleteRuleFromGroup(css::GroupRule* aGroup, uint32_t aIndex);
  nsresult InsertRuleIntoGroup(const nsAString& aRule, css::GroupRule* aGroup,
                               uint32_t aIndex);

  // Find the ID of the owner inner window.
  uint64_t FindOwningWindowInnerID() const;

  template <typename Func>
  void EnumerateChildSheets(Func aCallback) {
    for (StyleSheet* child = GetFirstChild(); child; child = child->mNext) {
      aCallback(child);
    }
  }

  // Copy the contents of this style sheet into the shared memory buffer managed
  // by aBuilder.  Returns the pointer into the buffer that the sheet contents
  // were stored at.  (The returned pointer is to an Arc<Locked<Rules>> value.)
  const ServoCssRules* ToShared(RawServoSharedMemoryBuilder* aBuilder);

  // Sets the contents of this style sheet to the specified aSharedRules
  // pointer, which must be a pointer somewhere in the aSharedMemory buffer
  // as previously returned by a ToShared() call.
  void SetSharedContents(nsLayoutStylesheetCacheShm* aSharedMemory,
                         const ServoCssRules* aSharedRules);

  // Whether this style sheet should not allow any modifications.
  //
  // This is true for any User Agent sheets once they are complete.
  bool IsReadOnly() const;

 private:
  dom::ShadowRoot* GetContainingShadow() const;

  StyleSheetInfo& Inner() {
    MOZ_ASSERT(mInner);
    return *mInner;
  }

  const StyleSheetInfo& Inner() const {
    MOZ_ASSERT(mInner);
    return *mInner;
  }

  // Check if the rules are available for read and write.
  // It does the security check as well as whether the rules have been
  // completely loaded. aRv will have an exception set if this function
  // returns false.
  bool AreRulesAvailable(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv);

  void SetURLExtraData();

 protected:
  // Internal methods which do not have security check and completeness check.
  uint32_t InsertRuleInternal(const nsAString& aRule, uint32_t aIndex,
                              ErrorResult&);
  void DeleteRuleInternal(uint32_t aIndex, ErrorResult&);
  nsresult InsertRuleIntoGroupInternal(const nsAString& aRule,
                                       css::GroupRule* aGroup, uint32_t aIndex);

  // Common tail routine for the synchronous and asynchronous parsing paths.
  void FinishParse();

  // Take the recently cloned sheets from the `@import` rules, and reparent them
  // correctly to `aPrimarySheet`.
  void BuildChildListAfterInnerClone();

  void DropRuleList();

  // Called when a rule is removed from the sheet from CSSOM.
  void RuleAdded(css::Rule&);

  // Called when a rule is added to the sheet from CSSOM.
  void RuleRemoved(css::Rule&);

  // Called when a stylesheet is cloned.
  void StyleSheetCloned(StyleSheet&);

  void ApplicableStateChanged(bool aApplicable);

  struct ChildSheetListBuilder {
    RefPtr<StyleSheet>* sheetSlot;
    StyleSheet* parent;

    void SetParentLinks(StyleSheet* aSheet);

    static void ReparentChildList(StyleSheet* aPrimarySheet,
                                  StyleSheet* aFirstChild);
  };

  void UnparentChildren();

  void LastRelease();

  // Return success if the subject principal subsumes the principal of our
  // inner, error otherwise.  This will also succeed if the subject has
  // UniversalXPConnect or if access is allowed by CORS.  In the latter case,
  // it will set the principal of the inner to the subject principal.
  void SubjectSubsumesInnerPrincipal(nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aRv);

  // Drop our reference to mMedia
  void DropMedia();

  // Unlink our inner, if needed, for cycle collection.
  void UnlinkInner();
  // Traverse our inner, if needed, for cycle collection
  void TraverseInner(nsCycleCollectionTraversalCallback&);

  // Return whether the given @import rule has pending child sheet.
  static bool RuleHasPendingChildSheet(css::Rule* aRule);

  StyleSheet* mParent;  // weak ref

  nsString mTitle;

  // weak ref; parents maintain this for their children
  dom::DocumentOrShadowRoot* mDocumentOrShadowRoot;
  nsINode* mOwningNode;            // weak ref
  dom::CSSImportRule* mOwnerRule;  // weak ref

  RefPtr<dom::MediaList> mMedia;

  RefPtr<StyleSheet> mNext;

  // mParsingMode controls access to nonstandard style constructs that
  // are not safe for use on the public Web but necessary in UA sheets
  // and/or useful in user sheets.
  //
  // FIXME(emilio): Given we store the parsed contents in the Inner, this should
  // probably also move there.
  css::SheetParsingMode mParsingMode;

  State mState;

  // mAssociationMode determines whether mDocumentOrShadowRoot directly owns us
  // (in the sense that if it's known-live then we're known-live).
  //
  // Always NotOwnedByDocumentOrShadowRoot when mDocumentOrShadowRoot is null.
  AssociationMode mAssociationMode;

  // Core information we get from parsed sheets, which are shared amongst
  // StyleSheet clones.
  //
  // Always nonnull until LastRelease().
  StyleSheetInfo* mInner;

  nsTArray<ServoStyleSet*> mStyleSets;

  RefPtr<ServoCSSRuleList> mRuleList;

  MozPromiseHolder<StyleSheetParsePromise> mParsePromise;

  // Make StyleSheetInfo and subclasses into friends so they can use
  // ChildSheetListBuilder.
  friend struct mozilla::StyleSheetInfo;
};

}  // namespace mozilla

#endif  // mozilla_StyleSheet_h
