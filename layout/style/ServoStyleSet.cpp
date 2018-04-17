/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoStyleSetInlines.h"

#include "gfxPlatformFontList.h"
#include "mozilla/AutoRestyleTimelineMarker.h"
#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/StyleAnimationValue.h"
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
#include "nsXBLPrototypeBinding.h"
#include "gfxUserFontSet.h"
#include "nsBindingManager.h"
#include "nsWindowSizes.h"

using namespace mozilla;
using namespace mozilla::dom;

#ifdef DEBUG
bool
ServoStyleSet::IsCurrentThreadInServoTraversal()
{
  return sInServoTraversal && (NS_IsMainThread() || Servo_IsWorkerThread());
}
#endif

namespace mozilla {
ServoStyleSet* sInServoTraversal = nullptr;


// On construction, sets sInServoTraversal to the given ServoStyleSet.
// On destruction, clears sInServoTraversal and calls RunPostTraversalTasks.
class MOZ_RAII AutoSetInServoTraversal
{
public:
  explicit AutoSetInServoTraversal(ServoStyleSet* aSet)
    : mSet(aSet)
  {
    MOZ_ASSERT(!sInServoTraversal);
    MOZ_ASSERT(aSet);
    sInServoTraversal = aSet;
  }

  ~AutoSetInServoTraversal()
  {
    MOZ_ASSERT(sInServoTraversal);
    sInServoTraversal = nullptr;
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
    // RestyleManager::PostRestyleEventForAnimations so that we don't need
    // to care about animation restyles here.
    : mTimelineMarker(aSet->mDocument->GetDocShell(), false)
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
  : mDocument(nullptr)
  , mAuthorStyleDisabled(false)
  , mStylistState(StylistState::NotDirty)
  , mUserFontSetUpdateGeneration(0)
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

nsPresContext*
ServoStyleSet::GetPresContext()
{
  if (!mDocument) {
    return nullptr;
  }

  return mDocument->GetPresContext();
}

void
ServoStyleSet::Init(nsPresContext* aPresContext)
{
  mDocument = aPresContext->Document();
  MOZ_ASSERT(GetPresContext() == aPresContext);

  mRawSet.reset(Servo_StyleSet_Init(aPresContext));

  aPresContext->DeviceContext()->InitFontCache();

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

  // We may have Shadow DOM style changes that we weren't notified about because
  // the document didn't have a shell, if the ShadowRoot was created in a
  // display: none iframe.
  //
  // Now that we got a shell, we may need to get them up-to-date.
  //
  // TODO(emilio, bug 1418159): This wouldn't be needed if the StyleSet was
  // owned by the document.
  SetStylistXBLStyleSheetsDirty();
}

template<typename Functor>
void
EnumerateShadowRoots(const nsIDocument& aDoc, const Functor& aCb)
{
  if (!aDoc.IsShadowDOMEnabled()) {
    return;
  }

  const nsIDocument::ShadowRootSet& shadowRoots = aDoc.ComposedShadowRoots();
  for (auto iter = shadowRoots.ConstIter(); !iter.Done(); iter.Next()) {
    ShadowRoot* root = iter.Get()->GetKey();
    MOZ_ASSERT(root);
    MOZ_DIAGNOSTIC_ASSERT(root->IsComposedDocParticipant());
    aCb(*root);
  }
}

void
ServoStyleSet::Shutdown()
{
  // Make sure we drop our cached styles before the presshell arena starts going
  // away.
  ClearNonInheritingComputedStyles();
  mRawSet = nullptr;
  mStyleRuleMap = nullptr;
}

void
ServoStyleSet::InvalidateStyleForCSSRuleChanges()
{
  MOZ_ASSERT(StylistNeedsUpdate());
  if (nsPresContext* pc = GetPresContext()) {
    pc->RestyleManager()->PostRestyleEventForCSSRuleChanges();
  }
}

void
ServoStyleSet::RecordShadowStyleChange(ShadowRoot& aShadowRoot)
{
  // TODO(emilio): We could keep track of the actual shadow roots that need
  // their styles recomputed.
  SetStylistXBLStyleSheetsDirty();

  // FIXME(emilio): This should be done using stylesheet invalidation instead.
  if (nsPresContext* pc = GetPresContext()) {
    pc->RestyleManager()->PostRestyleEvent(
      aShadowRoot.Host(), eRestyle_Subtree, nsChangeHint(0));
  }
}

void
ServoStyleSet::InvalidateStyleForDocumentStateChanges(EventStates aStatesChanged)
{
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(!aStatesChanged.IsEmpty());

  nsPresContext* pc = GetPresContext();
  if (!pc) {
    return;
  }

  Element* root = mDocument->GetRootElement();
  if (!root) {
    return;
  }

  // TODO(emilio): It may be nicer to just invalidate stuff in a given subtree
  // for XBL sheets / Shadow DOM. Consider just enumerating bound content
  // instead and run invalidation individually, passing mRawSet for the UA /
  // User sheets.
  AutoTArray<RawServoAuthorStylesBorrowed, 20> nonDocumentStyles;

  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    nonDocumentStyles.AppendElement(aShadowRoot.ServoStyles());
  });

  mDocument->BindingManager()->EnumerateBoundContentProtoBindings(
    [&](nsXBLPrototypeBinding* aProto) {
      if (auto* authorStyles = aProto->GetServoStyles()) {
        nonDocumentStyles.AppendElement(authorStyles);
      }
      return true;
    });

  Servo_InvalidateStyleForDocStateChanges(
    root, mRawSet.get(), &nonDocumentStyles, aStatesChanged.ServoValue());
}

static const MediaFeatureChangeReason kMediaFeaturesAffectingDefaultStyle =
  // Zoom changes change the meaning of em units.
  MediaFeatureChangeReason::ZoomChange |
  // Changes the meaning of em units, depending on which one is the actual
  // min-font-size.
  MediaFeatureChangeReason::MinFontSizeChange |
  // A resolution change changes the app-units-per-dev-pixels ratio, which some
  // structs (Border, Outline, Column) store for clamping. We should arguably
  // not do that, maybe doing it on layout directly, to try to avoid relying on
  // the pres context (bug 1418159).
  MediaFeatureChangeReason::ResolutionChange;

nsRestyleHint
ServoStyleSet::MediumFeaturesChanged(MediaFeatureChangeReason aReason)
{
  AutoTArray<RawServoAuthorStylesBorrowedMut, 20> nonDocumentStyles;

  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    nonDocumentStyles.AppendElement(aShadowRoot.ServoStyles());
  });

  // FIXME(emilio): This is broken for XBL. See bug 1406875.
  mDocument->BindingManager()->EnumerateBoundContentProtoBindings(
    [&](nsXBLPrototypeBinding* aProto) {
      if (auto* authorStyles = aProto->GetServoStyles()) {
        nonDocumentStyles.AppendElement(authorStyles);
      }
      return true;
    });

  bool mayAffectDefaultStyle =
    bool(aReason & kMediaFeaturesAffectingDefaultStyle);

  const MediumFeaturesChangedResult result =
    Servo_StyleSet_MediumFeaturesChanged(
      mRawSet.get(), &nonDocumentStyles, mayAffectDefaultStyle);

  const bool rulesChanged =
    result.mAffectsDocumentRules || result.mAffectsNonDocumentRules;

  if (result.mAffectsDocumentRules) {
    SetStylistStyleSheetsDirty();
  }

  if (result.mAffectsNonDocumentRules) {
    SetStylistXBLStyleSheetsDirty();
  }

  if (rulesChanged) {
    return eRestyle_Subtree;
  }

  const bool viewportChanged =
    bool(aReason & MediaFeatureChangeReason::ViewportChange);
  if (result.mUsesViewportUnits && viewportChanged) {
    return eRestyle_ForceDescendants;
  }

  return nsRestyleHint(0);
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoStyleSetMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoStyleSetMallocEnclosingSizeOf)

void
ServoStyleSet::AddSizeOfIncludingThis(nsWindowSizes& aSizes) const
{
  MallocSizeOf mallocSizeOf = aSizes.mState.mMallocSizeOf;

  aSizes.mLayoutStyleSetsOther += mallocSizeOf(this);

  if (mRawSet) {
    aSizes.mLayoutStyleSetsOther += mallocSizeOf(mRawSet.get());
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

    aSizes.mLayoutStyleSetsStylistRuleTree += sizes.mRuleTree;
    aSizes.mLayoutStyleSetsStylistElementAndPseudosMaps +=
      sizes.mElementAndPseudosMaps;
    aSizes.mLayoutStyleSetsStylistInvalidationMap +=
      sizes.mInvalidationMap;
    aSizes.mLayoutStyleSetsStylistRevalidationSelectors +=
      sizes.mRevalidationSelectors;
    aSizes.mLayoutStyleSetsStylistOther += sizes.mOther;
  }

  if (mStyleRuleMap) {
    aSizes.mLayoutStyleSetsOther +=
      mStyleRuleMap->SizeOfIncludingThis(aSizes.mState.mMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mSheets
  // - mNonInheritingComputedStyles
  //
  // The following members are not measured:
  // - mDocument, because it a non-owning pointer
}

void
ServoStyleSet::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  if (mAuthorStyleDisabled == aStyleDisabled) {
    return;
  }

  mAuthorStyleDisabled = aStyleDisabled;
  if (Element* root = mDocument->GetRootElement()) {
    if (nsPresContext* pc = GetPresContext()) {
      pc->RestyleManager()->PostRestyleEvent(root, eRestyle_Subtree, nsChangeHint(0));
    }
  }
  Servo_StyleSet_SetAuthorStyleDisabled(mRawSet.get(), mAuthorStyleDisabled);
  // XXX Workaround for the assertion in InvalidateStyleForDocumentStateChanges
  // which is called by nsIPresShell::SetAuthorStyleDisabled via nsIPresShell::
  // RestyleForCSSRuleChanges. It is not really necessary because we don't need
  // to rebuild stylist for this change. But we have bug around this, and we
  // may want to rethink how things should work. See bug 1437785.
  SetStylistStyleSheetsDirty();
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

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               ComputedStyle* aParentContext,
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
  MOZ_ASSERT(GetPresContext(), "Styling a document without a shell?");
  return GetPresContext()->RestyleManager()->Snapshots();
}

void
ServoStyleSet::ResolveMappedAttrDeclarationBlocks()
{
  if (nsHTMLStyleSheet* sheet = mDocument->GetAttributeStyleSheet()) {
    sheet->CalculateMappedServoDeclarations();
  }

  mDocument->ResolveScheduledSVGPresAttrs();
}

void
ServoStyleSet::PreTraverseSync()
{
  // Get the Document's root element to ensure that the cache is valid before
  // calling into the (potentially-parallel) Servo traversal, where a cache hit
  // is necessary to avoid a data race when updating the cache.
  mozilla::Unused << mDocument->GetRootElement();

  ResolveMappedAttrDeclarationBlocks();

  nsMediaFeatures::InitSystemMetrics();

  LookAndFeel::NativeInit();

  nsPresContext* presContext = GetPresContext();
  MOZ_ASSERT(presContext,
             "For now, we don't call into here without a pres context");
  if (gfxUserFontSet* userFontSet = mDocument->GetUserFontSet()) {
    // Ensure that the @font-face data is not stale
    uint64_t generation = userFontSet->GetGeneration();
    if (generation != mUserFontSetUpdateGeneration) {
      mDocument->GetFonts()->CacheFontLoadability();
      presContext->DeviceContext()->UpdateFontCacheUserFonts(userFontSet);
      mUserFontSetUpdateGeneration = generation;
    }
  }

  MOZ_ASSERT(!StylistNeedsUpdate());
  presContext->CacheAllLangs();
}

void
ServoStyleSet::PreTraverse(ServoTraversalFlags aFlags, Element* aRoot)
{
  PreTraverseSync();

  // Process animation stuff that we should avoid doing during the parallel
  // traversal.
  nsSMILAnimationController* smilController =
    mDocument->HasAnimationController()
    ? mDocument->GetAnimationController()
    : nullptr;

  MOZ_ASSERT(GetPresContext());
  if (aRoot) {
    GetPresContext()->EffectCompositor()->PreTraverseInSubtree(aFlags, aRoot);
    if (smilController) {
      smilController->PreTraverseInSubtree(aRoot);
    }
  } else {
    GetPresContext()->EffectCompositor()->PreTraverse(aFlags);
    if (smilController) {
      smilController->PreTraverse();
    }
  }
}

static inline already_AddRefed<ComputedStyle>
ResolveStyleForTextOrFirstLetterContinuation(
    RawServoStyleSetBorrowed aStyleSet,
    ComputedStyle& aParent,
    nsAtom* aAnonBox)
{
  MOZ_ASSERT(aAnonBox == nsCSSAnonBoxes::mozText ||
             aAnonBox == nsCSSAnonBoxes::firstLetterContinuation);
  auto inheritTarget = aAnonBox == nsCSSAnonBoxes::mozText
    ? InheritTarget::Text
    : InheritTarget::FirstLetterContinuation;

  RefPtr<ComputedStyle> style =
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

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveStyleForText(nsIContent* aTextNode,
                                   ComputedStyle* aParentContext)
{
  MOZ_ASSERT(aTextNode && aTextNode->IsText());
  MOZ_ASSERT(aTextNode->GetParent());
  MOZ_ASSERT(aParentContext);

  return ResolveStyleForTextOrFirstLetterContinuation(
      mRawSet.get(), *aParentContext, nsCSSAnonBoxes::mozText);
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveStyleForFirstLetterContinuation(ComputedStyle* aParentContext)
{
  MOZ_ASSERT(aParentContext);

  return ResolveStyleForTextOrFirstLetterContinuation(
      mRawSet.get(), *aParentContext, nsCSSAnonBoxes::firstLetterContinuation);
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveStyleForPlaceholder()
{
  RefPtr<ComputedStyle>& cache =
    mNonInheritingComputedStyles[nsCSSAnonBoxes::NonInheriting::oofPlaceholder];
  if (cache) {
    RefPtr<ComputedStyle> retval = cache;
    return retval.forget();
  }

  RefPtr<ComputedStyle> computedValues =
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
                      ComputedStyle* aParentContext)
{
  return aParentContext &&
         !nsCSSPseudoElements::IsEagerlyCascadedInServo(aType) &&
         aOriginatingElement->HasServoData() &&
         !Servo_Element_IsPrimaryStyleReusedViaRuleNode(aOriginatingElement);
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolvePseudoElementStyle(Element* aOriginatingElement,
                                         CSSPseudoElementType aType,
                                         ComputedStyle* aParentContext,
                                         Element* aPseudoElement)
{
  // Runs from frame construction, this should have clean styles already, except
  // with non-lazy FC...
  UpdateStylistIfNeeded();
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);

  RefPtr<ComputedStyle> computedValues;

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

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveStyleLazily(Element* aElement,
                                  CSSPseudoElementType aPseudoType,
                                  StyleRuleInclusion aRuleInclusion)
{
  PreTraverseSync();

  return ResolveStyleLazilyInternal(aElement, aPseudoType,
                                    aRuleInclusion);
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveInheritingAnonymousBoxStyle(nsAtom* aPseudoTag,
                                                  ComputedStyle* aParentContext)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudoTag) &&
             !nsCSSAnonBoxes::IsNonInheritingAnonBox(aPseudoTag));
  MOZ_ASSERT_IF(aParentContext, !StylistNeedsUpdate());

  UpdateStylistIfNeeded();

  RefPtr<ComputedStyle> style = nullptr;

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

already_AddRefed<ComputedStyle>
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
  RefPtr<ComputedStyle>& cache = mNonInheritingComputedStyles[type];
  if (cache) {
    RefPtr<ComputedStyle> retval = cache;
    return retval.forget();
  }

  UpdateStylistIfNeeded();

  // We always want to skip parent-based display fixup here.  It never makes
  // sense for non-inheriting anonymous boxes.  (Static assertions in
  // nsCSSAnonBoxes.cpp ensure that all non-inheriting non-anonymous boxes
  // are indeed annotated as skipping this fixup.)
  MOZ_ASSERT(!nsCSSAnonBoxes::IsNonInheritingAnonBox(nsCSSAnonBoxes::viewport),
             "viewport needs fixup to handle blockifying it");
  RefPtr<ComputedStyle> computedValues =
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
already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveXULTreePseudoStyle(dom::Element* aParentElement,
                                         nsICSSAnonBoxPseudo* aPseudoTag,
                                         ComputedStyle* aParentContext,
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
ServoStyleSet::AppendAllNonDocumentAuthorSheets(nsTArray<StyleSheet*>& aArray) const
{
  if (mDocument) {
    mDocument->BindingManager()->AppendAllSheets(aArray);
    EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
      for (auto index : IntegerRange(aShadowRoot.SheetCount())) {
        aArray.AppendElement(aShadowRoot.SheetAt(index));
      }
    });
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

already_AddRefed<ComputedStyle>
ServoStyleSet::ProbePseudoElementStyle(Element* aOriginatingElement,
                                       CSSPseudoElementType aType,
                                       ComputedStyle* aParentContext)
{
  // Runs from frame construction, this should have clean styles already, except
  // with non-lazy FC...
  UpdateStylistIfNeeded();

  // NB: We ignore aParentContext, because in some cases
  // (first-line/first-letter on anonymous box blocks) Gecko passes something
  // nonsensical there.  In all other cases we want to inherit directly from
  // aOriginatingElement's styles anyway.
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);

  bool cacheable =
    LazyPseudoIsCacheable(aType, aOriginatingElement, aParentContext);

  RefPtr<ComputedStyle> computedValues =
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
  MOZ_ASSERT(GetPresContext(), "Styling a document without a shell?");

  if (!mDocument->GetServoRestyleRoot()) {
    return false;
  }

  PreTraverse(aFlags);
  AutoPrepareTraversal guard(this);
  const SnapshotTable& snapshots = Snapshots();

  // Restyle the document from the root element and each of the document level
  // NAC subtree roots.
  bool postTraversalRequired = false;

  Element* rootElement = mDocument->GetRootElement();
  MOZ_ASSERT_IF(rootElement, rootElement->HasServoData());

  if (ShouldTraverseInParallel()) {
    aFlags |= ServoTraversalFlags::ParallelTraversal;
  }

  // Do the first traversal.
  DocumentStyleRootIterator iter(mDocument->GetServoRestyleRoot());
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
      MOZ_ASSERT(root == mDocument->GetServoRestyleRoot());
      if (parent->HasDirtyDescendantsForServo()) {
        // If any style invalidation was triggered in our siblings, then we may
        // need to post-traverse them, even if the root wasn't restyled after
        // all.
        uint32_t existingBits = mDocument->GetServoRestyleRootDirtyBits();
        // We need to propagate the existing bits to the parent.
        parent->SetFlags(existingBits);
        mDocument->SetServoRestyleRoot(
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
  if (GetPresContext()->EffectCompositor()->PreTraverse(aFlags)) {
    nsINode* styleRoot = mDocument->GetServoRestyleRoot();
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
  MOZ_ASSERT(GetPresContext());
  MOZ_ASSERT(!aRoot->HasServoData());
  MOZ_ASSERT(aRoot->GetFlattenedTreeParentNodeForStyle(),
             "Not in the flat tree? Fishy!");
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
  if (GetPresContext()->EffectCompositor()->PreTraverseInSubtree(flags, aRoot)) {
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
  Servo_StyleSet_NoteStyleSheetsChanged(mRawSet.get(), aChangedOrigins);
}

void
ServoStyleSet::SetStylistStyleSheetsDirty()
{
  // Note that there's another hidden mutator of mStylistState for XBL style
  // sets in MediumFeaturesChanged...
  //
  // We really need to stop using a full-blown StyleSet there...
  mStylistState |= StylistState::StyleSheetsDirty;

  // We need to invalidate cached style in getComputedStyle for undisplayed
  // elements, since we don't know if any of the style sheet change that we
  // do would affect undisplayed elements.
  if (nsPresContext* presContext = GetPresContext()) {
    // XBL sheets don't have a pres context, but invalidating the restyle
    // generation in that case is handled by SetXBLStyleSheetsDirty in the
    // "master" stylist.
    presContext->RestyleManager()->IncrementUndisplayedRestyleGeneration();
  }
}

void
ServoStyleSet::SetStylistXBLStyleSheetsDirty()
{
  mStylistState |= StylistState::XBLStyleSheetsDirty;

  // We need to invalidate cached style in getComputedStyle for undisplayed
  // elements, since we don't know if any of the style sheet change that we
  // do would affect undisplayed elements.
  MOZ_ASSERT(GetPresContext());
  GetPresContext()->RestyleManager()->IncrementUndisplayedRestyleGeneration();
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
  DocumentStyleRootIterator iter(mDocument);
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
  // TODO(emilio): This may need to look at the element itself for handling
  // @keyframes properly in Shadow DOM.
  MOZ_ASSERT(!StylistNeedsUpdate());
  return Servo_StyleSet_GetKeyframesForName(mRawSet.get(),
                                            aName,
                                            &aTimingFunction,
                                            &aKeyframes);
}

nsTArray<ComputedKeyframeValues>
ServoStyleSet::GetComputedKeyframeValuesFor(
  const nsTArray<Keyframe>& aKeyframes,
  Element* aElement,
  const mozilla::ComputedStyle* aStyle)
{
  nsTArray<ComputedKeyframeValues> result(aKeyframes.Length());

  // Construct each nsTArray<PropertyStyleAnimationValuePair> here.
  result.AppendElements(aKeyframes.Length());

  Servo_GetComputedKeyframeValues(&aKeyframes,
                                  aElement,
                                  aStyle,
                                  mRawSet.get(),
                                  &result);
  return result;
}

void
ServoStyleSet::GetAnimationValues(
  RawServoDeclarationBlock* aDeclarations,
  Element* aElement,
  const ComputedStyle* aComputedStyle,
  nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues)
{
  // Servo_GetAnimationValues below won't handle ignoring existing element
  // data for bfcached documents. (See comment in ResolveStyleLazily
  // about these bfcache issues.)
  Servo_GetAnimationValues(aDeclarations,
                           aElement,
                           aComputedStyle,
                           mRawSet.get(),
                           &aAnimationValues);
}

already_AddRefed<ComputedStyle>
ServoStyleSet::GetBaseContextForElement(
  Element* aElement,
  const ComputedStyle* aStyle)
{
  return Servo_StyleSet_GetBaseComputedValuesForElement(mRawSet.get(),
                                                        aElement,
                                                        aStyle,
                                                        &Snapshots()).Consume();
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveServoStyleByAddingAnimation(
  Element* aElement,
  const ComputedStyle* aStyle,
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
  const mozilla::ComputedStyle* aStyle)
{
  return Servo_AnimationValue_Compute(aElement,
                                      aDeclarations,
                                      aStyle,
                                      mRawSet.get()).Consume();
}

bool
ServoStyleSet::EnsureUniqueInnerOnCSSSheets()
{
  using SheetOwner = Variant<ServoStyleSet*, nsXBLPrototypeBinding*, ShadowRoot*>;

  AutoTArray<Pair<StyleSheet*, SheetOwner>, 32> queue;
  for (auto& entryArray : mSheets) {
    for (auto& sheet : entryArray) {
      StyleSheet* downcasted = sheet;
      queue.AppendElement(MakePair(downcasted, SheetOwner { this }));
    }
  }

  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    for (auto index : IntegerRange(aShadowRoot.SheetCount())) {
      queue.AppendElement(
        MakePair(aShadowRoot.SheetAt(index), SheetOwner { &aShadowRoot }));
    }
  });

  mDocument->BindingManager()->EnumerateBoundContentProtoBindings(
      [&](nsXBLPrototypeBinding* aProto) {
        AutoTArray<StyleSheet*, 3> sheets;
        aProto->AppendStyleSheetsTo(sheets);
        for (auto* sheet : sheets) {
          queue.AppendElement(MakePair(sheet, SheetOwner { aProto }));
        }
        return true;
      });

  bool anyNonDocStyleChanged = false;
  while (!queue.IsEmpty()) {
    uint32_t idx = queue.Length() - 1;
    auto* sheet = queue[idx].first();
    SheetOwner owner = queue[idx].second();
    queue.RemoveElementAt(idx);

    if (!sheet->HasUniqueInner()) {
      if (owner.is<ShadowRoot*>()) {
        Servo_AuthorStyles_ForceDirty(owner.as<ShadowRoot*>()->ServoStyles());
        mNeedsRestyleAfterEnsureUniqueInner = true;
        anyNonDocStyleChanged = true;
      } else if (owner.is<nsXBLPrototypeBinding*>()) {
        if (auto* styles = owner.as<nsXBLPrototypeBinding*>()->GetServoStyles()) {
          Servo_AuthorStyles_ForceDirty(styles);
          mNeedsRestyleAfterEnsureUniqueInner = true;
          anyNonDocStyleChanged = true;
        }
      }
    }

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
    AutoTArray<StyleSheet*, 3> children;
    sheet->AppendAllChildSheets(children);
    for (auto* sheet : children) {
      queue.AppendElement(MakePair(sheet, owner));
    }
  }

  if (anyNonDocStyleChanged) {
    SetStylistXBLStyleSheetsDirty();
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
  ClearNonInheritingComputedStyles();
  Servo_StyleSet_RebuildCachedData(mRawSet.get());
}

void
ServoStyleSet::CompatibilityModeChanged()
{
  Servo_StyleSet_CompatModeChanged(mRawSet.get());
  SetStylistStyleSheetsDirty();
}

void
ServoStyleSet::ClearNonInheritingComputedStyles()
{
  for (RefPtr<ComputedStyle>& ptr : mNonInheritingComputedStyles) {
    ptr = nullptr;
  }
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveStyleLazilyInternal(Element* aElement,
                                          CSSPseudoElementType aPseudoType,
                                          StyleRuleInclusion aRuleInclusion)
{
  MOZ_ASSERT(GetPresContext(),
             "For now, no style resolution without a pres context");
  GetPresContext()->EffectCompositor()->PreTraverse(aElement, aPseudoType);
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

  RefPtr<ComputedStyle> computedValues =
    Servo_ResolveStyleLazily(elementForStyleResolution,
                             pseudoTypeForStyleResolution,
                             aRuleInclusion,
                             &Snapshots(),
                             mRawSet.get()).Consume();

  if (GetPresContext()->EffectCompositor()->PreTraverse(aElement, aPseudoType)) {
    computedValues =
      Servo_ResolveStyleLazily(elementForStyleResolution,
                               pseudoTypeForStyleResolution,
                               aRuleInclusion,
                               &Snapshots(),
                               mRawSet.get()).Consume();
  }

  MOZ_DIAGNOSTIC_ASSERT(computedValues->PresContextForFrame() == GetPresContext() ||
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

const RawServoCounterStyleRule*
ServoStyleSet::CounterStyleRuleForName(nsAtom* aName)
{
  MOZ_ASSERT(!StylistNeedsUpdate());
  return Servo_StyleSet_GetCounterStyleRule(mRawSet.get(), aName);
}

already_AddRefed<gfxFontFeatureValueSet>
ServoStyleSet::BuildFontFeatureValueSet()
{
  // FIXME(emilio): This should assert once we update the stylist from
  // FlushPendingNotifications explicitly.
  UpdateStylistIfNeeded();
  RefPtr<gfxFontFeatureValueSet> set =
    Servo_StyleSet_BuildFontFeatureValueSet(mRawSet.get());
  return set.forget();
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveForDeclarations(
  const ComputedStyle* aParentOrNull,
  RawServoDeclarationBlockBorrowed aDeclarations)
{
  // No need to update the stylist, we're only cascading aDeclarations.
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
    Element* root = mDocument->GetRootElement();
    const ServoElementSnapshotTable* snapshots = nullptr;
    if (nsPresContext* pc = GetPresContext()) {
      snapshots = &pc->RestyleManager()->Snapshots();
    }
    Servo_StyleSet_FlushStyleSheets(mRawSet.get(), root, snapshots);
  }

  if (MOZ_UNLIKELY(mStylistState & StylistState::XBLStyleSheetsDirty)) {
    MOZ_ASSERT(GetPresContext(), "How did they get dirty?");

    EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
      Servo_AuthorStyles_Flush(aShadowRoot.ServoStyles(), mRawSet.get());
    });

    mDocument->BindingManager()->EnumerateBoundContentProtoBindings(
      [&](nsXBLPrototypeBinding* aProto) {
        if (auto* authorStyles = aProto->GetServoStyles()) {
          Servo_AuthorStyles_Flush(authorStyles, mRawSet.get());
        }
        return true;
      });
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
  MOZ_ASSERT(mDocument->GetShell(), "Styling a document without a shell?");
  return mDocument->GetShell()->IsActive();
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
    mStyleRuleMap = MakeUnique<ServoStyleRuleMap>();
  }
  mStyleRuleMap->EnsureTable(*this);
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

already_AddRefed<ComputedStyle>
ServoStyleSet::ReparentComputedStyle(ComputedStyle* aComputedStyle,
                                     ComputedStyle* aNewParent,
                                     ComputedStyle* aNewParentIgnoringFirstLine,
                                     ComputedStyle* aNewLayoutParent,
                                     Element* aElement)
{
  return Servo_ReparentStyle(aComputedStyle, aNewParent,
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
