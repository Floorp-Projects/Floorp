/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSet.h"

#include "gfxPlatformFontList.h"
#include "mozilla/AutoRestyleTimelineMarker.h"
#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/RestyleManagerInlines.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRuleProcessor.h"
#include "nsDeviceContext.h"
#include "nsHTMLStyleSheet.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIDocumentInlines.h"
#include "nsPrintfCString.h"
#include "nsSMILAnimationController.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"
#include "gfxUserFontSet.h"

using namespace mozilla;
using namespace mozilla::dom;

ServoStyleSet* ServoStyleSet::sInServoTraversal = nullptr;

namespace mozilla {
// On construction, sets sInServoTraversal to the given ServoStyleSet.
// On destruction, clears sInServoTraversal and calls RunPostTraversalTasks.
class MOZ_RAII AutoSetInServoTraversal
{
public:
  explicit AutoSetInServoTraversal(ServoStyleSet* aSet)
    : mSet(aSet)
  {
    MOZ_ASSERT(!ServoStyleSet::sInServoTraversal);
    MOZ_ASSERT(aSet);
    ServoStyleSet::sInServoTraversal = aSet;
  }

  ~AutoSetInServoTraversal()
  {
    MOZ_ASSERT(ServoStyleSet::sInServoTraversal);
    ServoStyleSet::sInServoTraversal = nullptr;
    mSet->RunPostTraversalTasks();
  }

private:
  ServoStyleSet* mSet;
};

// Sets up for one or more calls to Servo_TraverseSubtree.
class MOZ_RAII AutoPrepareTraversal
{
public:
  explicit AutoPrepareTraversal(ServoStyleSet* aSet)
    // For markers for animations, we have already set the markers in
    // ServoRestyleManager::PostRestyleEventForAnimations so that we don't need
    // to care about animation restyles here.
    : mTimelineMarker(aSet->mPresContext->GetDocShell(), false)
    , mSetInServoTraversal(aSet)
  {
    MOZ_ASSERT(!aSet->StylistNeedsUpdate());
  }

private:
  AutoRestyleTimelineMarker mTimelineMarker;
  AutoSetInServoTraversal mSetInServoTraversal;
};

} // namespace mozilla

ServoStyleSet::ServoStyleSet()
  : mPresContext(nullptr)
  , mAuthorStyleDisabled(false)
  , mStylistState(StylistState::NotDirty)
  , mUserFontSetUpdateGeneration(0)
  , mUserFontCacheUpdateGeneration(0)
  , mNeedsRestyleAfterEnsureUniqueInner(false)
{
}

ServoStyleSet::~ServoStyleSet()
{
  for (auto& sheetArray : mSheets) {
    for (auto& sheet : sheetArray) {
      sheet->DropStyleSet(this);
    }
  }
}

void
ServoStyleSet::Init(nsPresContext* aPresContext, nsBindingManager* aBindingManager)
{
  mPresContext = aPresContext;
  mRawSet.reset(Servo_StyleSet_Init(aPresContext));
  mBindingManager = aBindingManager;

  mPresContext->DeviceContext()->InitFontCache();

  // Now that we have an mRawSet, go ahead and notify about whatever stylesheets
  // we have so far.
  for (auto& sheetArray : mSheets) {
    for (auto& sheet : sheetArray) {
      // There's no guarantee this will create a list on the servo side whose
      // ordering matches the list that would have been created had all those
      // sheets been appended/prepended/etc after we had mRawSet. That's okay
      // because Servo only needs to maintain relative ordering within a sheet
      // type, which this preserves.

      MOZ_ASSERT(sheet->RawContents(),
                 "We should only append non-null raw sheets.");
      Servo_StyleSet_AppendStyleSheet(mRawSet.get(), sheet);
    }
  }

  // We added prefilled stylesheets into mRawSet, so the stylist is dirty.
  // The Stylist should be updated later when necessary.
  SetStylistStyleSheetsDirty();
}

void
ServoStyleSet::BeginShutdown()
{
  nsIDocument* doc = mPresContext->Document();

  // Remove the style rule map from document's observer and drop it.
  if (mStyleRuleMap) {
    doc->RemoveObserver(mStyleRuleMap);
    doc->CSSLoader()->RemoveObserver(mStyleRuleMap);
    mStyleRuleMap = nullptr;
  }
}

void
ServoStyleSet::Shutdown()
{
  // Make sure we drop our cached style contexts before the presshell arena
  // starts going away.
  ClearNonInheritingStyleContexts();
  mRawSet = nullptr;
}

void
ServoStyleSet::InvalidateStyleForCSSRuleChanges()
{
  MOZ_ASSERT(StylistNeedsUpdate());
  mPresContext->RestyleManager()->AsServo()->PostRestyleEventForCSSRuleChanges();
}

nsRestyleHint
ServoStyleSet::MediumFeaturesChanged(bool aViewportChanged)
{
  bool viewportUnitsUsed = false;
  const bool rulesChanged =
    Servo_StyleSet_MediumFeaturesChanged(mRawSet.get(), &viewportUnitsUsed);

  if (rulesChanged) {
    ForceAllStyleDirty();
    return eRestyle_Subtree;
  }

  if (viewportUnitsUsed && aViewportChanged) {
    return eRestyle_ForceDescendants;
  }

  return nsRestyleHint(0);
}

size_t
ServoStyleSet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  if (mStyleRuleMap) {
    n += mStyleRuleMap->SizeOfIncludingThis(aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mRawSet
  // - mSheets
  // - mNonInheritingStyleContexts
  //
  // The following members are not measured:
  // - mPresContext, because it a non-owning pointer

  return n;
}

bool
ServoStyleSet::GetAuthorStyleDisabled() const
{
  return mAuthorStyleDisabled;
}

nsresult
ServoStyleSet::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  if (mAuthorStyleDisabled == aStyleDisabled) {
    return NS_OK;
  }

  mAuthorStyleDisabled = aStyleDisabled;
  ForceAllStyleDirty();

  return NS_OK;
}

void
ServoStyleSet::BeginUpdate()
{
}

nsresult
ServoStyleSet::EndUpdate()
{
  return NS_OK;
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               ServoStyleContext* aParentContext,
                               LazyComputeBehavior aMayCompute)
{
  RefPtr<ServoStyleContext> computedValues;
  if (aMayCompute == LazyComputeBehavior::Allow) {
    PreTraverseSync();
    return ResolveStyleLazilyInternal(
        aElement, CSSPseudoElementType::NotPseudo, nullptr, aParentContext);
  }

  return ResolveServoStyle(aElement);
}

/**
 * Clears any stale Servo element data that might existing in the specified
 * element's document.  Upon destruction, asserts that the element and all
 * its ancestors still have no element data, if the document has no pres shell.
 */
class MOZ_STACK_CLASS AutoClearStaleData
{
public:
  explicit AutoClearStaleData(Element* aElement)
#ifdef DEBUG
    : mElement(aElement)
#endif
  {
    aElement->OwnerDoc()->ClearStaleServoDataFromDocument();
  }

  ~AutoClearStaleData()
  {
#ifdef DEBUG
    // Assert that the element and its ancestors are all unstyled, if the
    // document has no pres shell.
    if (mElement->OwnerDoc()->HasShellOrBFCacheEntry()) {
      // We must check whether we're in the bfcache because its presence
      // means we have a "hidden" pres shell with up-to-date data in the
      // tree.
      return;
    }
    for (Element* e = mElement; e; e = e->GetParentElement()) {
      MOZ_ASSERT(!e->HasServoData(), "expected element to be unstyled");
    }
#endif
  }

private:
#ifdef DEBUG
  Element* mElement;
#endif
};

const ServoElementSnapshotTable&
ServoStyleSet::Snapshots()
{
  return mPresContext->RestyleManager()->AsServo()->Snapshots();
}

void
ServoStyleSet::ResolveMappedAttrDeclarationBlocks()
{
  if (nsHTMLStyleSheet* sheet = mPresContext->Document()->GetAttributeStyleSheet()) {
    sheet->CalculateMappedServoDeclarations(mPresContext);
  }

  mPresContext->Document()->ResolveScheduledSVGPresAttrs();
}

void
ServoStyleSet::PreTraverseSync()
{
  // Get the Document's root element to ensure that the cache is valid before
  // calling into the (potentially-parallel) Servo traversal, where a cache hit
  // is necessary to avoid a data race when updating the cache.
  mozilla::Unused << mPresContext->Document()->GetRootElement();

  ResolveMappedAttrDeclarationBlocks();

  nsCSSRuleProcessor::InitSystemMetrics();

  LookAndFeel::NativeInit();

  // This is lazily computed and pseudo matching needs to access
  // it so force computation early.
  mPresContext->Document()->GetDocumentState();

  if (gfxUserFontSet* userFontSet = mPresContext->Document()->GetUserFontSet()) {
    // Ensure that the @font-face data is not stale
    uint64_t generation = userFontSet->GetGeneration();
    if (generation != mUserFontSetUpdateGeneration) {
      mPresContext->DeviceContext()->UpdateFontCacheUserFonts(userFontSet);
      mUserFontSetUpdateGeneration = generation;
    }

    // Ensure that the FontFaceSet's cached document principal is up to date.
    FontFaceSet* fontFaceSet =
      static_cast<FontFaceSet::UserFontSet*>(userFontSet)->GetFontFaceSet();
    fontFaceSet->UpdateStandardFontLoadPrincipal();
    bool principalChanged = fontFaceSet->HasStandardFontLoadPrincipalChanged();

    // Ensure that the user font cache holds up-to-date data on whether
    // our font set is allowed to re-use fonts from the cache.
    uint32_t cacheGeneration = gfxUserFontSet::UserFontCache::Generation();
    if (principalChanged) {
      gfxUserFontSet::UserFontCache::ClearAllowedFontSets(userFontSet);
    }
    if (cacheGeneration != mUserFontCacheUpdateGeneration || principalChanged) {
      gfxUserFontSet::UserFontCache::UpdateAllowedFontSets(userFontSet);
      mUserFontCacheUpdateGeneration = cacheGeneration;
    }
  }

  UpdateStylistIfNeeded();
  mPresContext->CacheAllLangs();
}

void
ServoStyleSet::PreTraverse(ServoTraversalFlags aFlags, Element* aRoot)
{
  PreTraverseSync();

  // Process animation stuff that we should avoid doing during the parallel
  // traversal.
  nsSMILAnimationController* smilController =
    mPresContext->Document()->GetAnimationController();
  if (aRoot) {
    mPresContext->EffectCompositor()
                ->PreTraverseInSubtree(aFlags, aRoot);
    if (smilController) {
      smilController->PreTraverseInSubtree(aRoot);
    }
  } else {
    mPresContext->EffectCompositor()->PreTraverse(aFlags);
    if (smilController) {
      smilController->PreTraverse();
    }
  }
}

static inline already_AddRefed<ServoStyleContext>
ResolveStyleForTextOrFirstLetterContinuation(
    RawServoStyleSetBorrowed aStyleSet,
    ServoStyleContext& aParent,
    nsIAtom* aAnonBox)
{
  MOZ_ASSERT(aAnonBox == nsCSSAnonBoxes::mozText ||
             aAnonBox == nsCSSAnonBoxes::firstLetterContinuation);
  auto inheritTarget = aAnonBox == nsCSSAnonBoxes::mozText
    ? InheritTarget::Text
    : InheritTarget::FirstLetterContinuation;

  RefPtr<ServoStyleContext> style =
    aParent.GetCachedInheritingAnonBoxStyle(aAnonBox);
  if (!style) {
    style = Servo_ComputedValues_Inherit(aStyleSet,
                                         aAnonBox,
                                         &aParent,
                                         inheritTarget).Consume();
    MOZ_ASSERT(style);
    aParent.SetCachedInheritedAnonBoxStyle(aAnonBox, style);
  }

  return style.forget();
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveStyleForText(nsIContent* aTextNode,
                                   ServoStyleContext* aParentContext)
{
  MOZ_ASSERT(aTextNode && aTextNode->IsNodeOfType(nsINode::eTEXT));
  MOZ_ASSERT(aTextNode->GetParent());
  MOZ_ASSERT(aParentContext);

  return ResolveStyleForTextOrFirstLetterContinuation(
      mRawSet.get(), *aParentContext, nsCSSAnonBoxes::mozText);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveStyleForFirstLetterContinuation(ServoStyleContext* aParentContext)
{
  MOZ_ASSERT(aParentContext);

  return ResolveStyleForTextOrFirstLetterContinuation(
      mRawSet.get(), *aParentContext, nsCSSAnonBoxes::firstLetterContinuation);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveStyleForPlaceholder()
{
  RefPtr<ServoStyleContext>& cache =
    mNonInheritingStyleContexts[nsCSSAnonBoxes::NonInheriting::oofPlaceholder];
  if (cache) {
    RefPtr<ServoStyleContext> retval = cache;
    return retval.forget();
  }

  RefPtr<ServoStyleContext> computedValues =
    Servo_ComputedValues_Inherit(mRawSet.get(),
                                 nsCSSAnonBoxes::oofPlaceholder,
                                 nullptr,
                                 InheritTarget::PlaceholderFrame)
                                 .Consume();
  MOZ_ASSERT(computedValues);

  cache = computedValues;
  return computedValues.forget();
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolvePseudoElementStyle(Element* aOriginatingElement,
                                         CSSPseudoElementType aType,
                                         ServoStyleContext* aParentContext,
                                         Element* aPseudoElement)
{
  UpdateStylistIfNeeded();

  MOZ_ASSERT(aType < CSSPseudoElementType::Count);

  RefPtr<ServoStyleContext> computedValues;

  if (aPseudoElement) {
    MOZ_ASSERT(aType == aPseudoElement->GetPseudoElementType());
    computedValues =
      Servo_ResolveStyle(aPseudoElement, mRawSet.get()).Consume();
  } else {
    bool cacheable =
      !nsCSSPseudoElements::IsEagerlyCascadedInServo(aType) && aParentContext;
    computedValues =
      cacheable ? aParentContext->GetCachedLazyPseudoStyle(aType) : nullptr;

    if (!computedValues) {
      computedValues = Servo_ResolvePseudoStyle(aOriginatingElement,
                                                aType,
                                                /* is_probe = */ false,
                                                aParentContext,
                                                mRawSet.get()).Consume();
      if (cacheable) {
        aParentContext->SetCachedLazyPseudoStyle(computedValues);
      }
    }
  }

  MOZ_ASSERT(computedValues);
  return computedValues.forget();
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveStyleLazily(Element* aElement,
                                  CSSPseudoElementType aPseudoType,
                                  nsIAtom* aPseudoTag,
                                  StyleRuleInclusion aRuleInclusion)
{
  // Lazy style computation avoids storing any new data in the tree.
  // If the tree has stale data in it, then the AutoClearStaleData below
  // will ensure it's cleared so we don't use it. But if the document is
  // in the bfcache, then we will have valid, usable data in the tree,
  // but we don't want to use it. Instead we want to pretend as if the
  // document has no pres shell and no styles.
  //
  // If we don't do this, then we can very easily mix styles from different
  // style sets in the tree. For example, calling getComputedStyle on an
  // element in a display:none iframe (which has no pres shell) will use the
  // caller's style set for any styling. If we allowed this to re-use any
  // existing styles in the DOM, then we would do selector matching on the
  // undisplayed element with the caller's style set's rules, but inherit from
  // values that were computed with the style set from the target element's
  // hidden-by-the-bfcache-entry pres shell.
  bool ignoreExistingStyles = aElement->OwnerDoc()->GetBFCacheEntry();

  AutoClearStaleData guard(aElement);
  PreTraverseSync();
  return ResolveStyleLazilyInternal(aElement, aPseudoType, aPseudoTag,
                                    nullptr, aRuleInclusion,
                                    ignoreExistingStyles);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveInheritingAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                                  ServoStyleContext* aParentContext)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudoTag) &&
             !nsCSSAnonBoxes::IsNonInheritingAnonBox(aPseudoTag));
  RefPtr<ServoStyleContext> style = nullptr;

  if (aParentContext) {
    style = aParentContext->GetCachedInheritingAnonBoxStyle(aPseudoTag);
  }

  if (!style) {
    // People like to call into here from random attribute notifications (see
    // bug 1388234, and bug 1389029).
    //
    // We may get a wrong cached style if the stylist needs an update, but we'll
    // have a whole restyle scheduled anyway.
    UpdateStylistIfNeeded();

    style =
      Servo_ComputedValues_GetForAnonymousBox(aParentContext,
                                              aPseudoTag,
                                              mRawSet.get()).Consume();
    MOZ_ASSERT(style);
    if (aParentContext) {
      aParentContext->SetCachedInheritedAnonBoxStyle(aPseudoTag, style);
    }
  }

  return style.forget();
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveNonInheritingAnonymousBoxStyle(nsIAtom* aPseudoTag)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudoTag) &&
             nsCSSAnonBoxes::IsNonInheritingAnonBox(aPseudoTag));
  MOZ_ASSERT(aPseudoTag != nsCSSAnonBoxes::pageContent,
             "If nsCSSAnonBoxes::pageContent ends up non-inheriting, check "
             "whether we need to do anything to move the "
             "@page handling from ResolveInheritingAnonymousBoxStyle to "
             "ResolveNonInheritingAnonymousBoxStyle");

  nsCSSAnonBoxes::NonInheriting type =
    nsCSSAnonBoxes::NonInheritingTypeForPseudoTag(aPseudoTag);
  RefPtr<ServoStyleContext>& cache = mNonInheritingStyleContexts[type];
  if (cache) {
    RefPtr<ServoStyleContext> retval = cache;
    return retval.forget();
  }

  UpdateStylistIfNeeded();

  // We always want to skip parent-based display fixup here.  It never makes
  // sense for non-inheriting anonymous boxes.  (Static assertions in
  // nsCSSAnonBoxes.cpp ensure that all non-inheriting non-anonymous boxes
  // are indeed annotated as skipping this fixup.)
  MOZ_ASSERT(!nsCSSAnonBoxes::IsNonInheritingAnonBox(nsCSSAnonBoxes::viewport),
             "viewport needs fixup to handle blockifying it");
  RefPtr<ServoStyleContext> computedValues =
    Servo_ComputedValues_GetForAnonymousBox(nullptr,
                                            aPseudoTag,
                                            mRawSet.get()).Consume();
#ifdef DEBUG
  if (!computedValues) {
    nsString pseudo;
    aPseudoTag->ToString(pseudo);
    NS_ERROR(nsPrintfCString("stylo: could not get anon-box: %s",
             NS_ConvertUTF16toUTF8(pseudo).get()).get());
    MOZ_CRASH();
  }
#endif

  cache = computedValues;
  return computedValues.forget();
}

// manage the set of style sheets in the style set
nsresult
ServoStyleSet::AppendStyleSheet(SheetType aType,
                                ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  MOZ_ASSERT(aSheet->RawContents(), "Raw sheet should be in place before insertion.");

  RemoveSheetOfType(aType, aSheet);
  AppendSheetOfType(aType, aSheet);

  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    // Servo will remove aSheet from its original position as part of the call
    // to Servo_StyleSet_AppendStyleSheet.
    Servo_StyleSet_AppendStyleSheet(mRawSet.get(), aSheet);
    SetStylistStyleSheetsDirty();
  }

  return NS_OK;
}

nsresult
ServoStyleSet::PrependStyleSheet(SheetType aType,
                                 ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  MOZ_ASSERT(aSheet->RawContents(),
             "Raw sheet should be in place before insertion.");

  RemoveSheetOfType(aType, aSheet);
  PrependSheetOfType(aType, aSheet);

  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    // Servo will remove aSheet from its original position as part of the call
    // to Servo_StyleSet_PrependStyleSheet.
    Servo_StyleSet_PrependStyleSheet(mRawSet.get(), aSheet);
    SetStylistStyleSheetsDirty();
  }

  return NS_OK;
}

nsresult
ServoStyleSet::RemoveStyleSheet(SheetType aType,
                                ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));

  RemoveSheetOfType(aType, aSheet);
  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), aSheet);
    SetStylistStyleSheetsDirty();
  }

  return NS_OK;
}

nsresult
ServoStyleSet::ReplaceSheets(SheetType aType,
                             const nsTArray<RefPtr<ServoStyleSheet>>& aNewSheets)
{
  // Gecko uses a two-dimensional array keyed by sheet type, whereas Servo
  // stores a flattened list. This makes ReplaceSheets a pretty clunky thing
  // to express. If the need ever arises, we can easily make this more efficent,
  // probably by aligning the representations better between engines.

  SetStylistStyleSheetsDirty();

  // Remove all the existing sheets first.
  for (const auto& sheet : mSheets[aType]) {
    sheet->DropStyleSet(this);
    if (mRawSet) {
      Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), sheet);
    }
  }
  mSheets[aType].Clear();

  // Add in all the new sheets.
  for (auto& sheet : aNewSheets) {
    AppendSheetOfType(aType, sheet);
    if (mRawSet) {
      MOZ_ASSERT(sheet->RawContents(), "Raw sheet should be in place before replacement.");
      Servo_StyleSet_AppendStyleSheet(mRawSet.get(), sheet);
    }
  }

  return NS_OK;
}

nsresult
ServoStyleSet::InsertStyleSheetBefore(SheetType aType,
                                      ServoStyleSheet* aNewSheet,
                                      ServoStyleSheet* aReferenceSheet)
{
  MOZ_ASSERT(aNewSheet);
  MOZ_ASSERT(aReferenceSheet);
  MOZ_ASSERT(aNewSheet->IsApplicable());
  MOZ_ASSERT(aNewSheet != aReferenceSheet, "Can't place sheet before itself.");
  MOZ_ASSERT(aNewSheet->RawContents(), "Raw sheet should be in place before insertion.");
  MOZ_ASSERT(aReferenceSheet->RawContents(), "Reference sheet should have a raw sheet.");

  // Servo will remove aNewSheet from its original position as part of the
  // call to Servo_StyleSet_InsertStyleSheetBefore.
  RemoveSheetOfType(aType, aNewSheet);
  InsertSheetOfType(aType, aNewSheet, aReferenceSheet);

  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    Servo_StyleSet_InsertStyleSheetBefore(
        mRawSet.get(), aNewSheet, aReferenceSheet);
    SetStylistStyleSheetsDirty();
  }

  return NS_OK;
}

void
ServoStyleSet::UpdateStyleSheet(ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  // TODO(emilio): Get rid of this.
}

int32_t
ServoStyleSet::SheetCount(SheetType aType) const
{
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  return mSheets[aType].Length();
}

ServoStyleSheet*
ServoStyleSet::StyleSheetAt(SheetType aType, int32_t aIndex) const
{
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  return mSheets[aType][aIndex];
}

void
ServoStyleSet::AppendAllXBLStyleSheets(nsTArray<StyleSheet*>& aArray) const
{
  if (mBindingManager) {
    mBindingManager->AppendAllSheets(aArray);
  }
}

nsresult
ServoStyleSet::RemoveDocStyleSheet(ServoStyleSheet* aSheet)
{
  return RemoveStyleSheet(SheetType::Doc, aSheet);
}

nsresult
ServoStyleSet::AddDocStyleSheet(ServoStyleSheet* aSheet,
                                nsIDocument* aDocument)
{
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(aSheet->RawContents(), "Raw sheet should be in place by this point.");

  RefPtr<StyleSheet> strong(aSheet);

  RemoveSheetOfType(SheetType::Doc, aSheet);

  size_t index =
    aDocument->FindDocStyleSheetInsertionPoint(mSheets[SheetType::Doc], aSheet);

  if (index < mSheets[SheetType::Doc].Length()) {
    // This case is insert before.
    ServoStyleSheet *beforeSheet = mSheets[SheetType::Doc][index];
    InsertSheetOfType(SheetType::Doc, aSheet, beforeSheet);

    if (mRawSet) {
      // Maintain a mirrored list of sheets on the servo side.
      Servo_StyleSet_InsertStyleSheetBefore(mRawSet.get(), aSheet, beforeSheet);
      SetStylistStyleSheetsDirty();
    }
  } else {
    // This case is append.
    AppendSheetOfType(SheetType::Doc, aSheet);

    if (mRawSet) {
      // Maintain a mirrored list of sheets on the servo side.
      Servo_StyleSet_AppendStyleSheet(mRawSet.get(), aSheet);
      SetStylistStyleSheetsDirty();
    }
  }

  return NS_OK;
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aOriginatingElement,
                                       CSSPseudoElementType aType,
                                       ServoStyleContext* aParentContext)
{
  UpdateStylistIfNeeded();

  // NB: We ignore aParentContext, because in some cases
  // (first-line/first-letter on anonymous box blocks) Gecko passes something
  // nonsensical there.  In all other cases we want to inherit directly from
  // aOriginatingElement's styles anyway.
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);

  bool cacheable =
    !nsCSSPseudoElements::IsEagerlyCascadedInServo(aType) && aParentContext;

  RefPtr<ServoStyleContext> computedValues =
    cacheable ? aParentContext->GetCachedLazyPseudoStyle(aType) : nullptr;
  if (!computedValues) {
    computedValues = Servo_ResolvePseudoStyle(aOriginatingElement, aType,
                                              /* is_probe = */ true,
                                              nullptr,
                                              mRawSet.get()).Consume();
    if (!computedValues) {
      return nullptr;
    }

    if (cacheable) {
      // NB: We don't need to worry about the before/after handling below
      // because those are eager and thus not |cacheable| anyway.
      aParentContext->SetCachedLazyPseudoStyle(computedValues);
    }
  }

  // For :before and :after pseudo-elements, having display: none or no
  // 'content' property is equivalent to not having the pseudo-element
  // at all.
  bool isBeforeOrAfter = aType == CSSPseudoElementType::before ||
                         aType == CSSPseudoElementType::after;
  if (isBeforeOrAfter) {
    const nsStyleDisplay* display = computedValues->ComputedData()->GetStyleDisplay();
    const nsStyleContent* content = computedValues->ComputedData()->GetStyleContent();
    if (display->mDisplay == StyleDisplay::None ||
        content->ContentCount() == 0) {
      return nullptr;
    }
  }

  return computedValues.forget();
}

nsRestyleHint
ServoStyleSet::HasStateDependentStyle(dom::Element* aElement,
                                      EventStates aStateMask)
{
  NS_WARNING("stylo: HasStateDependentStyle always returns zero!");
  return nsRestyleHint(0);
}

nsRestyleHint
ServoStyleSet::HasStateDependentStyle(dom::Element* aElement,
                                      CSSPseudoElementType aPseudoType,
                                      dom::Element* aPseudoElement,
                                      EventStates aStateMask)
{
  NS_WARNING("stylo: HasStateDependentStyle always returns zero!");
  return nsRestyleHint(0);
}

bool
ServoStyleSet::StyleDocument(ServoTraversalFlags aBaseFlags)
{
  PreTraverse(aBaseFlags);

  // Restyle the document from the root element and each of the document level
  // NAC subtree roots.
  bool postTraversalRequired = false;
  DocumentStyleRootIterator iter(mPresContext->Document());
  while (Element* root = iter.GetNextStyleRoot()) {
    MOZ_ASSERT(MayTraverseFrom(const_cast<Element*>(root)));
    AutoPrepareTraversal guard(this);
    const SnapshotTable& snapshots = Snapshots();
    bool isInitial = !root->HasServoData();
    auto flags = aBaseFlags;

    // Allow the parallel traversal, unless we're traversing traversing one of
    // the native anonymous document style roots, which are tiny and not worth
    // parallelizing over.
    if (!root->IsInNativeAnonymousSubtree()) {
      flags |= ServoTraversalFlags::ParallelTraversal;
    }

    // Do the first traversal.
    bool required = Servo_TraverseSubtree(root, mRawSet.get(), &snapshots, flags);
    MOZ_ASSERT_IF(isInitial, !required);
    postTraversalRequired |= required;

    // If there are still animation restyles needed, trigger a second traversal to
    // update CSS animations or transitions' styles.
    //
    // We don't need to do this for SMIL since SMIL only updates its animation
    // values once at the begin of a tick. As a result, even if the previous
    // traversal caused, for example, the font-size to change, the SMIL style
    // won't be updated until the next tick anyway.
    if (mPresContext->EffectCompositor()->PreTraverse(flags)) {
      if (isInitial) {
        // We're doing initial styling, and the additional animation
        // traversal will change the styles that were set by the first traversal.
        // This would normally require a post-traversal to update the style
        // contexts, but since this is actually the initial styling, there are
        // no style contexts to update and no frames to apply the change hints to,
        // so we just do a forgetful traversal and clear the flags on the way.
        flags |= ServoTraversalFlags::Forgetful |
                 ServoTraversalFlags::ClearAnimationOnlyDirtyDescendants;
      }

      required = Servo_TraverseSubtree(root, mRawSet.get(), &snapshots, flags);
      MOZ_ASSERT_IF(isInitial, !required);
      postTraversalRequired |= required;
    }
  }

  return postTraversalRequired;
}

void
ServoStyleSet::StyleNewSubtree(Element* aRoot)
{
  MOZ_ASSERT(!aRoot->HasServoData(), "Should have called StyleNewChildren");
  PreTraverseSync();
  AutoPrepareTraversal guard(this);

  // Do the traversal. The snapshots will not be used.
  const SnapshotTable& snapshots = Snapshots();
  auto flags = ServoTraversalFlags::Empty;
  DebugOnly<bool> postTraversalRequired =
    Servo_TraverseSubtree(aRoot, mRawSet.get(), &snapshots, flags);
  MOZ_ASSERT(!postTraversalRequired);

  // Annoyingly, the newly-styled content may have animations that need
  // starting, which requires traversing them again. Mark the elements
  // that need animation processing, then do a forgetful traversal to
  // update the styles and clear the animation bits.
  if (mPresContext->EffectCompositor()->PreTraverseInSubtree(flags, aRoot)) {
    postTraversalRequired =
      Servo_TraverseSubtree(aRoot, mRawSet.get(), &snapshots,
                            ServoTraversalFlags::AnimationOnly |
                            ServoTraversalFlags::Forgetful |
                            ServoTraversalFlags::ClearAnimationOnlyDirtyDescendants);
    MOZ_ASSERT(!postTraversalRequired);
  }
}

void
ServoStyleSet::StyleNewChildren(Element* aParent)
{
  MOZ_ASSERT(aParent->HasServoData(), "Should have called StyleNewSubtree");
  PreTraverseSync();
  AutoPrepareTraversal guard(this);

  // Implementing StyleNewChildren correctly is very annoying, for two reasons:
  // (1) We have to tiptoe around existing invalidations in the tree. In rare
  //     cases Gecko calls into the frame constructor with pending invalidations,
  //     and in other rare cases the frame constructor needs to perform
  //     synchronous styling rather than using the normal lazy frame
  //     construction mechanism. If both of these cases happen together, then we
  //     get an |aParent| with dirty style and/or dirty descendants, which we
  //     can't process right now because we're not set up to update the frames
  //     and process the change hints. We handle this case by passing the
  //     UnstyledOnly flag to servo.
  // (2) We don't have a good way to handle animations. When styling unstyled
  //     content, a followup animation traversal may be required (for example
  //     to change the transition style from the after-change style we used in
  //     the animation cascade to the timeline-correct style). But once we do
  //     the initial styling, we don't have a good way to distinguish the new
  //     content and scope our animation processing to that. We should handle
  //     this somehow, but for now we just don't do the followup animation
  //     traversal, which is buggy.

  // Set ourselves up to find the children by marking the parent as having
  // dirty descendants.
  bool hadDirtyDescendants = aParent->HasDirtyDescendantsForServo();
  aParent->SetHasDirtyDescendantsForServo();

  auto flags = ServoTraversalFlags::UnstyledOnly;

  // If there is an XBL binding on the root element, we do the initial document
  // styling with this API. Not clear how common that is, but we allow parallel
  // traversals in this case to preserve the old behavior (where Servo would
  // use the parallel traversal i.f.f. the traversal root was the document root).
  if (aParent == aParent->OwnerDoc()->GetRootElement()) {
    flags |= ServoTraversalFlags::ParallelTraversal;
  }

  // Do the traversal. The snapshots will be ignored.
  const SnapshotTable& snapshots = Snapshots();
  Servo_TraverseSubtree(aParent, mRawSet.get(), &snapshots, flags);

  // Restore the old state of the dirty descendants bit.
  if (!hadDirtyDescendants) {
    aParent->UnsetHasDirtyDescendantsForServo();
  }
}

void
ServoStyleSet::StyleNewlyBoundElement(Element* aElement)
{
  // In general the element is always styled by the time we're applying XBL
  // bindings, because we need to style the element to know what the binding
  // URI is. However, programmatic consumers of the XBL service (like the
  // XML pretty printer) _can_ apply bindings without having styled the bound
  // element. We could assert against this and require the callers manually
  // resolve the style first, but it's easy enough to just handle here.
  if (MOZ_LIKELY(aElement->HasServoData())) {
    StyleNewChildren(aElement);
  } else {
    StyleNewSubtree(aElement);
  }
}

void
ServoStyleSet::StyleSubtreeForReconstruct(Element* aRoot)
{
  MOZ_ASSERT(MayTraverseFrom(aRoot));
  MOZ_ASSERT(aRoot->HasServoData());
  auto flags = ServoTraversalFlags::Forgetful |
               ServoTraversalFlags::AggressivelyForgetful |
               ServoTraversalFlags::ClearDirtyDescendants |
               ServoTraversalFlags::ClearAnimationOnlyDirtyDescendants;
  PreTraverse(flags);

  AutoPrepareTraversal guard(this);

  const SnapshotTable& snapshots = Snapshots();

  DebugOnly<bool> postTraversalRequired =
    Servo_TraverseSubtree(aRoot, mRawSet.get(), &snapshots, flags);
  MOZ_ASSERT(!postTraversalRequired);

  if (mPresContext->EffectCompositor()->PreTraverseInSubtree(flags, aRoot)) {
    postTraversalRequired =
      Servo_TraverseSubtree(aRoot, mRawSet.get(), &snapshots, flags);
    MOZ_ASSERT(!postTraversalRequired);
  }
}

void
ServoStyleSet::ForceAllStyleDirty()
{
  SetStylistStyleSheetsDirty();
  Servo_StyleSet_NoteStyleSheetsChanged(mRawSet.get(), mAuthorStyleDisabled);
}

void
ServoStyleSet::RecordStyleSheetChange(
    ServoStyleSheet* aSheet,
    StyleSheet::ChangeType aChangeType)
{
  SetStylistStyleSheetsDirty();
  switch (aChangeType) {
    case StyleSheet::ChangeType::RuleAdded:
    case StyleSheet::ChangeType::RuleRemoved:
    case StyleSheet::ChangeType::RuleChanged:
      // FIXME(emilio): We can presumably do better in a bunch of these.
      return ForceAllStyleDirty();
    case StyleSheet::ChangeType::ApplicableStateChanged:
    case StyleSheet::ChangeType::Added:
    case StyleSheet::ChangeType::Removed:
      // Do nothing, we've already recorded the change in the
      // Append/Remove/Replace methods, etc, and will act consequently.
      return;
  }
}

#ifdef DEBUG
void
ServoStyleSet::AssertTreeIsClean()
{
  DocumentStyleRootIterator iter(mPresContext->Document());
  while (Element* root = iter.GetNextStyleRoot()) {
    Servo_AssertTreeIsClean(root);
  }
}
#endif

bool
ServoStyleSet::GetKeyframesForName(const nsString& aName,
                                   const nsTimingFunction& aTimingFunction,
                                   nsTArray<Keyframe>& aKeyframes)
{
  UpdateStylistIfNeeded();

  NS_ConvertUTF16toUTF8 name(aName);
  return Servo_StyleSet_GetKeyframesForName(mRawSet.get(),
                                            &name,
                                            &aTimingFunction,
                                            &aKeyframes);
}

nsTArray<ComputedKeyframeValues>
ServoStyleSet::GetComputedKeyframeValuesFor(
  const nsTArray<Keyframe>& aKeyframes,
  Element* aElement,
  const ServoStyleContext* aContext)
{
  // Servo_GetComputedKeyframeValues below won't handle ignoring existing
  // element data for bfcached documents. (See comment in ResolveStyleLazily
  // about these bfcache issues.)
  MOZ_RELEASE_ASSERT(!aElement->OwnerDoc()->GetBFCacheEntry());

  AutoClearStaleData guard(aElement);
  nsTArray<ComputedKeyframeValues> result(aKeyframes.Length());

  // Construct each nsTArray<PropertyStyleAnimationValuePair> here.
  result.AppendElements(aKeyframes.Length());

  Servo_GetComputedKeyframeValues(&aKeyframes,
                                  aElement,
                                  aContext,
                                  mRawSet.get(),
                                  &result);
  return result;
}

void
ServoStyleSet::GetAnimationValues(
  RawServoDeclarationBlock* aDeclarations,
  Element* aElement,
  const ServoStyleContext* aStyleContext,
  nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues)
{
  // Servo_GetAnimationValues below won't handle ignoring existing element
  // data for bfcached documents. (See comment in ResolveStyleLazily
  // about these bfcache issues.)
  MOZ_RELEASE_ASSERT(!aElement->OwnerDoc()->GetBFCacheEntry());

  AutoClearStaleData guard(aElement);
  Servo_GetAnimationValues(aDeclarations,
                           aElement,
                           aStyleContext,
                           mRawSet.get(),
                           &aAnimationValues);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::GetBaseContextForElement(
  Element* aElement,
  ServoStyleContext* aParentContext,
  nsPresContext* aPresContext,
  nsIAtom* aPseudoTag,
  CSSPseudoElementType aPseudoType,
  const ServoStyleContext* aStyle)
{
  // Servo_StyleSet_GetBaseComputedValuesForElement below won't handle ignoring
  // existing element data for bfcached documents. (See comment in
  // ResolveStyleLazily about these bfcache issues.)
  MOZ_RELEASE_ASSERT(!aElement->OwnerDoc()->GetBFCacheEntry(),
             "GetBaseContextForElement does not support documents in the "
             "bfcache");

  AutoClearStaleData guard(aElement);
  return Servo_StyleSet_GetBaseComputedValuesForElement(mRawSet.get(),
                                                        aElement,
                                                        aStyle,
                                                        &Snapshots(),
                                                        aPseudoType).Consume();
}

already_AddRefed<RawServoAnimationValue>
ServoStyleSet::ComputeAnimationValue(
  Element* aElement,
  RawServoDeclarationBlock* aDeclarations,
  const ServoStyleContext* aContext)
{
  // Servo_AnimationValue_Compute below won't handle ignoring existing element
  // data for bfcached documents. (See comment in ResolveStyleLazily about
  // these bfcache issues.)
  MOZ_RELEASE_ASSERT(!aElement->OwnerDoc()->GetBFCacheEntry());

  AutoClearStaleData guard(aElement);
  return Servo_AnimationValue_Compute(aElement,
                                      aDeclarations,
                                      aContext,
                                      mRawSet.get()).Consume();
}

bool
ServoStyleSet::EnsureUniqueInnerOnCSSSheets()
{
  AutoTArray<StyleSheet*, 32> queue;
  for (auto& entryArray : mSheets) {
    for (auto& sheet : entryArray) {
      queue.AppendElement(sheet);
    }
  }
  // This is a stub until more of the functionality of nsStyleSet is
  // replicated for Servo here.

  // Bug 1290276 will replicate the nsStyleSet work of checking
  // a nsBindingManager

  while (!queue.IsEmpty()) {
    uint32_t idx = queue.Length() - 1;
    StyleSheet* sheet = queue[idx];
    queue.RemoveElementAt(idx);

    sheet->EnsureUniqueInner();

    // Enqueue all the sheet's children.
    sheet->AppendAllChildSheets(queue);
  }

  bool res = mNeedsRestyleAfterEnsureUniqueInner;
  mNeedsRestyleAfterEnsureUniqueInner = false;
  return res;
}

void
ServoStyleSet::ClearCachedStyleData()
{
  ClearNonInheritingStyleContexts();
  Servo_StyleSet_RebuildCachedData(mRawSet.get());
}

void
ServoStyleSet::CompatibilityModeChanged()
{
  Servo_StyleSet_CompatModeChanged(mRawSet.get());
}

inline static void
UpdateBodyTextColorIfNeeded(
    const Element& aElement,
    ServoStyleContext& aStyleContext,
    nsPresContext& aPresContext)
{
  if (aPresContext.CompatibilityMode() != eCompatibility_NavQuirks) {
    return;
  }

  if (!aElement.IsHTMLElement(nsGkAtoms::body)) {
    return;
  }

  nsIDocument* doc = aElement.GetUncomposedDoc();
  if (!doc || doc->GetBodyElement() != &aElement) {
    return;
  }

  MOZ_ASSERT(!aStyleContext.GetPseudo());

  // NOTE(emilio): We do the ComputedData() dance to avoid triggering the
  // IsInServoTraversal() assertion in StyleColor(), which seems useful enough
  // in the general case, I guess...
  aPresContext.SetBodyTextColor(
      aStyleContext.ComputedData()->GetStyleColor()->mColor);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveServoStyle(Element* aElement)
{
  UpdateStylistIfNeeded();
  RefPtr<ServoStyleContext> result =
    Servo_ResolveStyle(aElement, mRawSet.get()).Consume();
  UpdateBodyTextColorIfNeeded(*aElement, *result, *mPresContext);
  return result.forget();
}

void
ServoStyleSet::ClearNonInheritingStyleContexts()
{
  for (RefPtr<ServoStyleContext>& ptr : mNonInheritingStyleContexts) {
    ptr = nullptr;
  }
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveStyleLazilyInternal(Element* aElement,
                                          CSSPseudoElementType aPseudoType,
                                          nsIAtom* aPseudoTag,
                                          const ServoStyleContext* aParentContext,
                                          StyleRuleInclusion aRuleInclusion,
                                          bool aIgnoreExistingStyles)
{
  mPresContext->EffectCompositor()->PreTraverse(aElement, aPseudoType);
  MOZ_ASSERT(!StylistNeedsUpdate());

  AutoSetInServoTraversal guard(this);

  /**
   * NB: This is needed because we process animations and transitions on the
   * pseudo-elements themselves, not on the parent's EagerPseudoStyles.
   *
   * That means that that style doesn't account for animations, and we can't do
   * that easily from the traversal without doing wasted work.
   *
   * As such, we just lie here a bit, which is the entrypoint of
   * getComputedStyle, the only API where this can be observed, to look at the
   * style of the pseudo-element if it exists instead.
   */
  Element* elementForStyleResolution = aElement;
  CSSPseudoElementType pseudoTypeForStyleResolution = aPseudoType;
  if (aPseudoType == CSSPseudoElementType::before) {
    if (Element* pseudo = nsLayoutUtils::GetBeforePseudo(aElement)) {
      elementForStyleResolution = pseudo;
      pseudoTypeForStyleResolution = CSSPseudoElementType::NotPseudo;
    }
  } else if (aPseudoType == CSSPseudoElementType::after) {
    if (Element* pseudo = nsLayoutUtils::GetAfterPseudo(aElement)) {
      elementForStyleResolution = pseudo;
      pseudoTypeForStyleResolution = CSSPseudoElementType::NotPseudo;
    }
  }

  RefPtr<ServoStyleContext> computedValues =
    Servo_ResolveStyleLazily(elementForStyleResolution,
                             pseudoTypeForStyleResolution,
                             aRuleInclusion,
                             &Snapshots(),
                             mRawSet.get(),
                             aIgnoreExistingStyles).Consume();

  if (mPresContext->EffectCompositor()->PreTraverse(aElement, aPseudoType)) {
    computedValues =
      Servo_ResolveStyleLazily(elementForStyleResolution,
                               pseudoTypeForStyleResolution,
                               aRuleInclusion,
                               &Snapshots(),
                               mRawSet.get(),
                               aIgnoreExistingStyles).Consume();
  }

  if (aPseudoType == CSSPseudoElementType::NotPseudo) {
    UpdateBodyTextColorIfNeeded(*aElement, *computedValues, *mPresContext);
  }

  return computedValues.forget();
}

bool
ServoStyleSet::AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray)
{
  UpdateStylistIfNeeded();
  Servo_StyleSet_GetFontFaceRules(mRawSet.get(), &aArray);
  return true;
}

nsCSSCounterStyleRule*
ServoStyleSet::CounterStyleRuleForName(nsIAtom* aName)
{
  return Servo_StyleSet_GetCounterStyleRule(mRawSet.get(), aName);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveForDeclarations(
  const ServoStyleContext* aParentOrNull,
  RawServoDeclarationBlockBorrowed aDeclarations)
{
  UpdateStylistIfNeeded();
  return Servo_StyleSet_ResolveForDeclarations(mRawSet.get(),
                                               aParentOrNull,
                                               aDeclarations).Consume();
}

void
ServoStyleSet::UpdateStylist()
{
  MOZ_ASSERT(StylistNeedsUpdate());
  Element* root = mPresContext->Document()->GetDocumentElement();
  Servo_StyleSet_FlushStyleSheets(mRawSet.get(), root);
  mStylistState = StylistState::NotDirty;
}

void
ServoStyleSet::MaybeGCRuleTree()
{
  MOZ_ASSERT(NS_IsMainThread());
  Servo_MaybeGCRuleTree(mRawSet.get());
}

bool
ServoStyleSet::MayTraverseFrom(Element* aElement)
{
  MOZ_ASSERT(aElement->IsInComposedDoc());
  Element* parent = aElement->GetFlattenedTreeParentElementForStyle();
  if (!parent) {
    return true;
  }

  if (!parent->HasServoData()) {
    return false;
  }

  RefPtr<ServoStyleContext> sc = Servo_ResolveStyleAllowStale(parent).Consume();
  return sc->StyleDisplay()->mDisplay != StyleDisplay::None;
}

void
ServoStyleSet::PrependSheetOfType(SheetType aType,
                                  ServoStyleSheet* aSheet)
{
  aSheet->AddStyleSet(this);
  mSheets[aType].InsertElementAt(0, aSheet);
}

void
ServoStyleSet::AppendSheetOfType(SheetType aType,
                                 ServoStyleSheet* aSheet)
{
  aSheet->AddStyleSet(this);
  mSheets[aType].AppendElement(aSheet);
}

void
ServoStyleSet::InsertSheetOfType(SheetType aType,
                                 ServoStyleSheet* aSheet,
                                 ServoStyleSheet* aBeforeSheet)
{
  for (uint32_t i = 0; i < mSheets[aType].Length(); ++i) {
    if (mSheets[aType][i] == aBeforeSheet) {
      aSheet->AddStyleSet(this);
      mSheets[aType].InsertElementAt(i, aSheet);
      return;
    }
  }
}

void
ServoStyleSet::RemoveSheetOfType(SheetType aType,
                                 ServoStyleSheet* aSheet)
{
  for (uint32_t i = 0; i < mSheets[aType].Length(); ++i) {
    if (mSheets[aType][i] == aSheet) {
      aSheet->DropStyleSet(this);
      mSheets[aType].RemoveElementAt(i);
    }
  }
}

void
ServoStyleSet::RunPostTraversalTasks()
{
  MOZ_ASSERT(!IsInServoTraversal());

  if (mPostTraversalTasks.IsEmpty()) {
    return;
  }

  nsTArray<PostTraversalTask> tasks;
  tasks.SwapElements(mPostTraversalTasks);

  for (auto& task : tasks) {
    task.Run();
  }
}

ServoStyleRuleMap*
ServoStyleSet::StyleRuleMap()
{
  if (!mStyleRuleMap) {
    mStyleRuleMap = new ServoStyleRuleMap(this);
    nsIDocument* doc = mPresContext->Document();
    doc->AddObserver(mStyleRuleMap);
    doc->CSSLoader()->AddObserver(mStyleRuleMap);
  }
  return mStyleRuleMap;
}

bool
ServoStyleSet::MightHaveAttributeDependency(const Element& aElement,
                                            nsIAtom* aAttribute) const
{
  return Servo_StyleSet_MightHaveAttributeDependency(
      mRawSet.get(), &aElement, aAttribute);
}

bool
ServoStyleSet::HasStateDependency(const Element& aElement,
                                  EventStates aState) const
{
  return Servo_StyleSet_HasStateDependency(
      mRawSet.get(), &aElement, aState.ServoValue());
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ReparentStyleContext(ServoStyleContext* aStyleContext,
                                    ServoStyleContext* aNewParent,
                                    ServoStyleContext* aNewParentIgnoringFirstLine,
                                    ServoStyleContext* aNewLayoutParent,
                                    Element* aElement)
{
  return Servo_ReparentStyle(aStyleContext, aNewParent,
                             aNewParentIgnoringFirstLine, aNewLayoutParent,
                             aElement, mRawSet.get()).Consume();
}
