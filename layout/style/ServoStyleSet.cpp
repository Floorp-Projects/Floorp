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
#include "mozilla/RestyleManagerInlines.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSPseudoElements.h"
#include "nsDeviceContext.h"
#include "nsHTMLStyleSheet.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIDocumentInlines.h"
#include "nsMediaFeatures.h"
#include "nsPrintfCString.h"
#include "nsSMILAnimationController.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"
#include "gfxUserFontSet.h"

using namespace mozilla;
using namespace mozilla::dom;

ServoStyleSet* ServoStyleSet::sInServoTraversal = nullptr;

#ifdef DEBUG
bool
ServoStyleSet::IsCurrentThreadInServoTraversal()
{
  return sInServoTraversal && (NS_IsMainThread() || Servo_IsWorkerThread());
}
#endif

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

ServoStyleSet::ServoStyleSet(Kind aKind)
  : mKind(aKind)
  , mPresContext(nullptr)
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

UniquePtr<ServoStyleSet>
ServoStyleSet::CreateXBLServoStyleSet(
  nsPresContext* aPresContext,
  const nsTArray<RefPtr<ServoStyleSheet>>& aNewSheets)
{
  auto set = MakeUnique<ServoStyleSet>(Kind::ForXBL);
  set->Init(aPresContext, nullptr);

  // The XBL style sheets aren't document level sheets, but we need to
  // decide a particular SheetType to add them to style set. This type
  // doesn't affect the place where we pull those rules from
  // stylist::push_applicable_declarations_as_xbl_only_stylist().
  set->ReplaceSheets(SheetType::Doc, aNewSheets);

  // Update stylist immediately.
  set->UpdateStylist();

  // The PresContext of the bound document could be destroyed anytime later,
  // which shouldn't be used for XBL styleset, so we clear it here to avoid
  // dangling pointer.
  set->mPresContext = nullptr;

  return set;
}

void
ServoStyleSet::Init(nsPresContext* aPresContext, nsBindingManager* aBindingManager)
{
  mPresContext = aPresContext;
  mLastPresContextUsesXBLStyleSet = aPresContext;

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
ServoStyleSet::Shutdown()
{
  // Make sure we drop our cached style contexts before the presshell arena
  // starts going away.
  ClearNonInheritingStyleContexts();
  mRawSet = nullptr;
  mStyleRuleMap = nullptr;

  // Also drop the reference to the pres context to avoid notifications from our
  // stylesheets to dereference a null restyle manager, see bug 1422634.
  //
  // We should really fix bug 154199...
  mPresContext = nullptr;
}

void
ServoStyleSet::InvalidateStyleForCSSRuleChanges()
{
  MOZ_ASSERT(StylistNeedsUpdate());
  mPresContext->RestyleManager()->AsServo()->PostRestyleEventForCSSRuleChanges();
}

bool
ServoStyleSet::SetPresContext(nsPresContext* aPresContext)
{
  MOZ_ASSERT(IsForXBL(), "Only XBL styleset can set PresContext!");

  mLastPresContextUsesXBLStyleSet = aPresContext;

  const OriginFlags rulesChanged = static_cast<OriginFlags>(
    Servo_StyleSet_SetDevice(mRawSet.get(), aPresContext));

  if (rulesChanged != OriginFlags(0)) {
    MarkOriginsDirty(rulesChanged);
    return true;
  }

  return false;
}

nsRestyleHint
ServoStyleSet::MediumFeaturesChanged(bool aViewportChanged)
{
  bool viewportUnitsUsed = false;
  bool rulesChanged = MediumFeaturesChangedRules(&viewportUnitsUsed);

  if (mBindingManager &&
      mBindingManager->MediumFeaturesChanged(mPresContext)) {
    SetStylistXBLStyleSheetsDirty();
    rulesChanged = true;
  }

  if (rulesChanged) {
    return eRestyle_Subtree;
  }

  if (viewportUnitsUsed && aViewportChanged) {
    return eRestyle_ForceDescendants;
  }

  return nsRestyleHint(0);
}

bool
ServoStyleSet::MediumFeaturesChangedRules(bool* aViewportUnitsUsed)
{
  MOZ_ASSERT(aViewportUnitsUsed);

  const OriginFlags rulesChanged = static_cast<OriginFlags>(
    Servo_StyleSet_MediumFeaturesChanged(mRawSet.get(), aViewportUnitsUsed));

  if (rulesChanged != OriginFlags(0)) {
    MarkOriginsDirty(rulesChanged);
    return true;
  }

  return false;
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoStyleSetMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoStyleSetMallocEnclosingSizeOf)

void
ServoStyleSet::AddSizeOfIncludingThis(nsWindowSizes& aSizes) const
{
  MallocSizeOf mallocSizeOf = aSizes.mState.mMallocSizeOf;

  aSizes.mLayoutServoStyleSetsOther += mallocSizeOf(this);

  if (mRawSet) {
    aSizes.mLayoutServoStyleSetsOther += mallocSizeOf(mRawSet.get());
    ServoStyleSetSizes sizes;
    // Measure mRawSet. We use ServoStyleSetMallocSizeOf rather than
    // aMallocSizeOf to distinguish in DMD's output the memory measured within
    // Servo code.
    Servo_StyleSet_AddSizeOfExcludingThis(ServoStyleSetMallocSizeOf,
                                          ServoStyleSetMallocEnclosingSizeOf,
                                          &sizes, mRawSet.get());

    // The StyleSet does not contain precomputed pseudos; they are in the UA
    // cache.
    MOZ_RELEASE_ASSERT(sizes.mPrecomputedPseudos == 0);

    aSizes.mLayoutServoStyleSetsStylistRuleTree += sizes.mRuleTree;
    aSizes.mLayoutServoStyleSetsStylistElementAndPseudosMaps +=
      sizes.mElementAndPseudosMaps;
    aSizes.mLayoutServoStyleSetsStylistInvalidationMap +=
      sizes.mInvalidationMap;
    aSizes.mLayoutServoStyleSetsStylistRevalidationSelectors +=
      sizes.mRevalidationSelectors;
    aSizes.mLayoutServoStyleSetsStylistOther += sizes.mOther;
  }

  if (mStyleRuleMap) {
    aSizes.mLayoutServoStyleSetsOther +=
      mStyleRuleMap->SizeOfIncludingThis(aSizes.mState.mMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mSheets
  // - mNonInheritingStyleContexts
  //
  // The following members are not measured:
  // - mPresContext, because it a non-owning pointer
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
  MarkOriginsDirty(OriginFlags::Author);

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
  if (aMayCompute == LazyComputeBehavior::Allow) {
    PreTraverseSync();
    return ResolveStyleLazilyInternal(
        aElement, CSSPseudoElementType::NotPseudo);
  }

  return ResolveServoStyle(aElement);
}

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

  nsMediaFeatures::InitSystemMetrics();

  LookAndFeel::NativeInit();

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
    mPresContext->Document()->HasAnimationController()
    ? mPresContext->Document()->GetAnimationController()
    : nullptr;

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
    nsAtom* aAnonBox)
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

static inline bool
LazyPseudoIsCacheable(CSSPseudoElementType aType,
                      Element* aOriginatingElement,
                      ServoStyleContext* aParentContext)
{
  return aParentContext &&
         !nsCSSPseudoElements::IsEagerlyCascadedInServo(aType) &&
         aOriginatingElement->HasServoData() &&
         !Servo_Element_IsPrimaryStyleReusedViaRuleNode(aOriginatingElement);
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
      LazyPseudoIsCacheable(aType, aOriginatingElement, aParentContext);
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
                                  StyleRuleInclusion aRuleInclusion)
{
  PreTraverseSync();

  return ResolveStyleLazilyInternal(aElement, aPseudoType,
                                    aRuleInclusion);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveInheritingAnonymousBoxStyle(nsAtom* aPseudoTag,
                                                  ServoStyleContext* aParentContext)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudoTag) &&
             !nsCSSAnonBoxes::IsNonInheritingAnonBox(aPseudoTag));
  MOZ_ASSERT_IF(aParentContext, !StylistNeedsUpdate());

  UpdateStylistIfNeeded();

  RefPtr<ServoStyleContext> style = nullptr;

  if (aParentContext) {
    style = aParentContext->GetCachedInheritingAnonBoxStyle(aPseudoTag);
  }

  if (!style) {
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
ServoStyleSet::ResolveNonInheritingAnonymousBoxStyle(nsAtom* aPseudoTag)
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

#ifdef MOZ_XUL
already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveXULTreePseudoStyle(dom::Element* aParentElement,
                                         nsICSSAnonBoxPseudo* aPseudoTag,
                                         ServoStyleContext* aParentContext,
                                         const AtomArray& aInputWord)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsTreePseudoElement(aPseudoTag));
  MOZ_ASSERT(aParentContext);
  MOZ_ASSERT(!StylistNeedsUpdate());

  return Servo_ComputedValues_ResolveXULTreePseudoStyle(
      aParentElement,
      aPseudoTag,
      aParentContext,
      &aInputWord,
      mRawSet.get()
  ).Consume();
}
#endif

// manage the set of style sheets in the style set
nsresult
ServoStyleSet::AppendStyleSheet(SheetType aType,
                                ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(IsCSSSheetType(aType));
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

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(*aSheet);
  }

  return NS_OK;
}

nsresult
ServoStyleSet::PrependStyleSheet(SheetType aType,
                                 ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(IsCSSSheetType(aType));
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

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(*aSheet);
  }

  return NS_OK;
}

nsresult
ServoStyleSet::RemoveStyleSheet(SheetType aType,
                                ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(IsCSSSheetType(aType));

  RemoveSheetOfType(aType, aSheet);
  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), aSheet);
    SetStylistStyleSheetsDirty();
  }

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetRemoved(*aSheet);
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

  // Just don't bother calling SheetRemoved / SheetAdded, and recreate the rule
  // map when needed.
  mStyleRuleMap = nullptr;
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

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(*aNewSheet);
  }

  return NS_OK;
}

int32_t
ServoStyleSet::SheetCount(SheetType aType) const
{
  MOZ_ASSERT(IsCSSSheetType(aType));
  return mSheets[aType].Length();
}

ServoStyleSheet*
ServoStyleSet::StyleSheetAt(SheetType aType, int32_t aIndex) const
{
  MOZ_ASSERT(IsCSSSheetType(aType));
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
    aDocument->FindDocStyleSheetInsertionPoint(mSheets[SheetType::Doc], *aSheet);

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

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(*aSheet);
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
    LazyPseudoIsCacheable(aType, aOriginatingElement, aParentContext);

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

bool
ServoStyleSet::StyleDocument(ServoTraversalFlags aFlags)
{
  nsIDocument* doc = mPresContext->Document();
  if (!doc->GetServoRestyleRoot()) {
    return false;
  }

  PreTraverse(aFlags);
  AutoPrepareTraversal guard(this);
  const SnapshotTable& snapshots = Snapshots();

  // Restyle the document from the root element and each of the document level
  // NAC subtree roots.
  bool postTraversalRequired = false;

  Element* rootElement = doc->GetRootElement();
  MOZ_ASSERT_IF(rootElement, rootElement->HasServoData());

  if (ShouldTraverseInParallel()) {
    aFlags |= ServoTraversalFlags::ParallelTraversal;
  }

  // Do the first traversal.
  DocumentStyleRootIterator iter(doc->GetServoRestyleRoot());
  while (Element* root = iter.GetNextStyleRoot()) {
    MOZ_ASSERT(MayTraverseFrom(root));

    Element* parent = root->GetFlattenedTreeParentElementForStyle();
    MOZ_ASSERT_IF(parent,
                  !parent->HasAnyOfFlags(Element::kAllServoDescendantBits));

    postTraversalRequired |=
      Servo_TraverseSubtree(root, mRawSet.get(), &snapshots, aFlags);
    postTraversalRequired |=
      root->HasAnyOfFlags(Element::kAllServoDescendantBits | NODE_NEEDS_FRAME);

    if (parent) {
      MOZ_ASSERT(root == doc->GetServoRestyleRoot());
      if (parent->HasDirtyDescendantsForServo()) {
        // If any style invalidation was triggered in our siblings, then we may
        // need to post-traverse them, even if the root wasn't restyled after
        // all.
        uint32_t existingBits = doc->GetServoRestyleRootDirtyBits();
        // We need to propagate the existing bits to the parent.
        parent->SetFlags(existingBits);
        doc->SetServoRestyleRoot(
            parent,
            existingBits | ELEMENT_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
        postTraversalRequired = true;
      }
    }
  }

  // If there are still animation restyles needed, trigger a second traversal to
  // update CSS animations or transitions' styles.
  //
  // Note that we need to check the style root again, because doing another
  // PreTraverse on the EffectCompositor might alter the style root. But we
  // don't need to worry about NAC, since document-level NAC shouldn't have
  // animations.
  //
  // We don't need to do this for SMIL since SMIL only updates its animation
  // values once at the begin of a tick. As a result, even if the previous
  // traversal caused, for example, the font-size to change, the SMIL style
  // won't be updated until the next tick anyway.
  if (mPresContext->EffectCompositor()->PreTraverse(aFlags)) {
    nsINode* styleRoot = doc->GetServoRestyleRoot();
    Element* root = styleRoot->IsElement() ? styleRoot->AsElement() : rootElement;

    postTraversalRequired |=
      Servo_TraverseSubtree(root, mRawSet.get(), &snapshots, aFlags);
    postTraversalRequired |=
      root->HasAnyOfFlags(Element::kAllServoDescendantBits | NODE_NEEDS_FRAME);
  }

  return postTraversalRequired;
}

void
ServoStyleSet::StyleNewSubtree(Element* aRoot)
{
  MOZ_ASSERT(!aRoot->HasServoData());
  PreTraverseSync();
  AutoPrepareTraversal guard(this);

  // Do the traversal. The snapshots will not be used.
  const SnapshotTable& snapshots = Snapshots();
  auto flags = ServoTraversalFlags::Empty;
  if (ShouldTraverseInParallel()) {
    flags |= ServoTraversalFlags::ParallelTraversal;
  }

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
ServoStyleSet::MarkOriginsDirty(OriginFlags aChangedOrigins)
{
  if (MOZ_UNLIKELY(!mRawSet)) {
    return;
  }

  SetStylistStyleSheetsDirty();
  Servo_StyleSet_NoteStyleSheetsChanged(mRawSet.get(),
                                        mAuthorStyleDisabled,
                                        aChangedOrigins);
}

void
ServoStyleSet::SetStylistStyleSheetsDirty()
{
  mStylistState |= StylistState::StyleSheetsDirty;

  // We need to invalidate cached style in getComputedStyle for undisplayed
  // elements, since we don't know if any of the style sheet change that we
  // do would affect undisplayed elements.
  if (mPresContext) {
    // XBL sheets don't have a pres context, but invalidating the restyle generation
    // in that case is handled by SetXBLStyleSheetsDirty in the "master" stylist.
    mPresContext->RestyleManager()->AsServo()->IncrementUndisplayedRestyleGeneration();
  }
}

void
ServoStyleSet::SetStylistXBLStyleSheetsDirty()
{
  mStylistState |= StylistState::XBLStyleSheetsDirty;

  // We need to invalidate cached style in getComputedStyle for undisplayed
  // elements, since we don't know if any of the style sheet change that we
  // do would affect undisplayed elements.
  MOZ_ASSERT(mPresContext);
  mPresContext->RestyleManager()->AsServo()->IncrementUndisplayedRestyleGeneration();
}

void
ServoStyleSet::RuleAdded(ServoStyleSheet& aSheet, css::Rule& aRule)
{
  if (mStyleRuleMap) {
    mStyleRuleMap->RuleAdded(aSheet, aRule);
  }

  // FIXME(emilio): Could be more granular based on aRule.
  MarkOriginsDirty(aSheet.GetOrigin());
}

void
ServoStyleSet::RuleRemoved(ServoStyleSheet& aSheet, css::Rule& aRule)
{
  if (mStyleRuleMap) {
    mStyleRuleMap->RuleRemoved(aSheet, aRule);
  }

  // FIXME(emilio): Could be more granular based on aRule.
  MarkOriginsDirty(aSheet.GetOrigin());
}

void
ServoStyleSet::RuleChanged(ServoStyleSheet& aSheet, css::Rule* aRule)
{
  // FIXME(emilio): Could be more granular based on aRule.
  MarkOriginsDirty(aSheet.GetOrigin());
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
ServoStyleSet::GetKeyframesForName(nsAtom* aName,
                                   const nsTimingFunction& aTimingFunction,
                                   nsTArray<Keyframe>& aKeyframes)
{
  UpdateStylistIfNeeded();

  return Servo_StyleSet_GetKeyframesForName(mRawSet.get(),
                                            aName,
                                            &aTimingFunction,
                                            &aKeyframes);
}

nsTArray<ComputedKeyframeValues>
ServoStyleSet::GetComputedKeyframeValuesFor(
  const nsTArray<Keyframe>& aKeyframes,
  Element* aElement,
  const ServoStyleContext* aContext)
{
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
  Servo_GetAnimationValues(aDeclarations,
                           aElement,
                           aStyleContext,
                           mRawSet.get(),
                           &aAnimationValues);
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::GetBaseContextForElement(
  Element* aElement,
  nsPresContext* aPresContext,
  const ServoStyleContext* aStyle)
{
  return Servo_StyleSet_GetBaseComputedValuesForElement(mRawSet.get(),
                                                        aElement,
                                                        aStyle,
                                                        &Snapshots()).Consume();
}

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveServoStyleByAddingAnimation(
  Element* aElement,
  const ServoStyleContext* aStyle,
  RawServoAnimationValue* aAnimationValue)
{
  return Servo_StyleSet_GetComputedValuesByAddingAnimation(
    mRawSet.get(),
    aElement,
    aStyle,
    &Snapshots(),
    aAnimationValue).Consume();
}

already_AddRefed<RawServoAnimationValue>
ServoStyleSet::ComputeAnimationValue(
  Element* aElement,
  RawServoDeclarationBlock* aDeclarations,
  const ServoStyleContext* aContext)
{
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

    // Only call EnsureUniqueInner for complete sheets. If we do call it on
    // incomplete sheets, we'll cause problems when the sheet is actually
    // loaded. We don't care about incomplete sheets here anyway, because this
    // method is only invoked by nsPresContext::EnsureSafeToHandOutCSSRules.
    // The CSSRule objects we are handing out won't contain any rules derived
    // from incomplete sheets (because they aren't yet applied in styling).
    if (sheet->IsComplete()) {
      sheet->EnsureUniqueInner();
    }

    // Enqueue all the sheet's children.
    sheet->AppendAllChildSheets(queue);
  }

  if (mNeedsRestyleAfterEnsureUniqueInner) {
    // TODO(emilio): We could make this faster if needed tracking the specific
    // origins and all that, but the only caller of this doesn't seem to really
    // care about perf.
    MarkOriginsDirty(OriginFlags::All);
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

already_AddRefed<ServoStyleContext>
ServoStyleSet::ResolveServoStyle(Element* aElement)
{
  RefPtr<ServoStyleContext> result =
    Servo_ResolveStyle(aElement, mRawSet.get()).Consume();
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
                                          StyleRuleInclusion aRuleInclusion)
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
                             /* aIgnoreExistingStyles = */ false).Consume();

  if (mPresContext->EffectCompositor()->PreTraverse(aElement, aPseudoType)) {
    computedValues =
      Servo_ResolveStyleLazily(elementForStyleResolution,
                               pseudoTypeForStyleResolution,
                               aRuleInclusion,
                               &Snapshots(),
                               mRawSet.get(),
                               /* aIgnoreExistingStyles = */ false).Consume();
  }

  MOZ_DIAGNOSTIC_ASSERT(computedValues->PresContext() == mPresContext ||
                        aElement->OwnerDoc()->GetBFCacheEntry());

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
ServoStyleSet::CounterStyleRuleForName(nsAtom* aName)
{
  return Servo_StyleSet_GetCounterStyleRule(mRawSet.get(), aName);
}

already_AddRefed<gfxFontFeatureValueSet>
ServoStyleSet::BuildFontFeatureValueSet()
{
  UpdateStylistIfNeeded();
  RefPtr<gfxFontFeatureValueSet> set =
    Servo_StyleSet_BuildFontFeatureValueSet(mRawSet.get());
  return set.forget();
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

  if (mStylistState & StylistState::StyleSheetsDirty) {
    // There's no need to compute invalidations and such for an XBL styleset,
    // since they are loaded and unloaded synchronously, and they don't have to
    // deal with dynamic content changes.
    Element* root =
      IsMaster() ? mPresContext->Document()->GetDocumentElement() : nullptr;

    Servo_StyleSet_FlushStyleSheets(mRawSet.get(), root);
  }

  if (MOZ_UNLIKELY(mStylistState & StylistState::XBLStyleSheetsDirty)) {
    MOZ_ASSERT(IsMaster(), "Only master styleset can mark XBL stylesets dirty!");
    mBindingManager->UpdateBoundContentBindingsForServo(mPresContext);
  }

  mStylistState = StylistState::NotDirty;
}

void
ServoStyleSet::MaybeGCRuleTree()
{
  MOZ_ASSERT(NS_IsMainThread());
  Servo_MaybeGCRuleTree(mRawSet.get());
}

/* static */ bool
ServoStyleSet::MayTraverseFrom(const Element* aElement)
{
  MOZ_ASSERT(aElement->IsInComposedDoc());
  nsINode* parent = aElement->GetFlattenedTreeParentNodeForStyle();
  if (!parent) {
    return false;
  }

  if (!parent->IsElement()) {
    MOZ_ASSERT(parent->IsNodeOfType(nsINode::eDOCUMENT));
    return true;
  }

  if (!parent->AsElement()->HasServoData()) {
    return false;
  }

  return !Servo_Element_IsDisplayNone(parent->AsElement());
}

bool
ServoStyleSet::ShouldTraverseInParallel() const
{
  return mPresContext->PresShell()->IsActive();
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
    mStyleRuleMap = MakeUnique<ServoStyleRuleMap>(this);
  }
  return mStyleRuleMap.get();
}

bool
ServoStyleSet::MightHaveAttributeDependency(const Element& aElement,
                                            nsAtom* aAttribute) const
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

bool
ServoStyleSet::HasDocumentStateDependency(EventStates aState) const
{
  return Servo_StyleSet_HasDocumentStateDependency(
      mRawSet.get(), aState.ServoValue());
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

NS_IMPL_ISUPPORTS(UACacheReporter, nsIMemoryReporter)

MOZ_DEFINE_MALLOC_SIZE_OF(ServoUACacheMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoUACacheMallocEnclosingSizeOf)

NS_IMETHODIMP
UACacheReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                nsISupports* aData, bool aAnonymize)
{
  ServoStyleSetSizes sizes;
  Servo_UACache_AddSizeOf(ServoUACacheMallocSizeOf,
                          ServoUACacheMallocEnclosingSizeOf, &sizes);

#define REPORT(_path, _amount, _desc) \
  do { \
    size_t __amount = _amount;  /* evaluate _amount only once */ \
    if (__amount > 0) { \
      MOZ_COLLECT_REPORT(_path, KIND_HEAP, UNITS_BYTES, __amount, _desc); \
    } \
  } while (0)

  // The UA cache does not contain the rule tree; that's in the StyleSet.
  MOZ_RELEASE_ASSERT(sizes.mRuleTree == 0);

  REPORT("explicit/layout/servo-ua-cache/precomputed-pseudos",
         sizes.mPrecomputedPseudos,
         "Memory used by precomputed pseudo-element declarations within the "
         "UA cache.");

  REPORT("explicit/layout/servo-ua-cache/element-and-pseudos-maps",
         sizes.mElementAndPseudosMaps,
         "Memory used by element and pseudos maps within the UA cache.");

  REPORT("explicit/layout/servo-ua-cache/invalidation-map",
         sizes.mInvalidationMap,
         "Memory used by invalidation maps within the UA cache.");

  REPORT("explicit/layout/servo-ua-cache/revalidation-selectors",
         sizes.mRevalidationSelectors,
         "Memory used by selectors for cache revalidation within the UA "
         "cache.");

  REPORT("explicit/layout/servo-ua-cache/other",
         sizes.mOther,
         "Memory used by other data within the UA cache");

  return NS_OK;
}
