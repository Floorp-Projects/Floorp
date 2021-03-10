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
#include "mozilla/EffectCompositor.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Keyframe.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/SMILAnimationController.h"
#include "mozilla/MediaFeatureChange.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/dom/CSSCounterStyleRule.h"
#include "mozilla/dom/CSSRuleBinding.h"
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/CSSFontFeatureValuesRule.h"
#include "mozilla/dom/CSSImportRule.h"
#include "mozilla/dom/CSSMediaRule.h"
#include "mozilla/dom/CSSMozDocumentRule.h"
#include "mozilla/dom/CSSKeyframesRule.h"
#include "mozilla/dom/CSSKeyframeRule.h"
#include "mozilla/dom/CSSNamespaceRule.h"
#include "mozilla/dom/CSSPageRule.h"
#include "mozilla/dom/CSSSupportsRule.h"
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
#include "mozilla/dom/DocumentInlines.h"
#include "nsMediaFeatures.h"
#include "nsPrintfCString.h"
#include "gfxUserFontSet.h"
#include "nsWindowSizes.h"

namespace mozilla {

using namespace dom;

#ifdef DEBUG
bool ServoStyleSet::IsCurrentThreadInServoTraversal() {
  return sInServoTraversal && (NS_IsMainThread() || Servo_IsWorkerThread());
}
#endif

// The definition of kOrigins relies on this.
static_assert(static_cast<uint8_t>(StyleOrigin::UserAgent) ==
              static_cast<uint8_t>(OriginFlags::UserAgent));
static_assert(static_cast<uint8_t>(StyleOrigin::User) ==
              static_cast<uint8_t>(OriginFlags::User));
static_assert(static_cast<uint8_t>(StyleOrigin::Author) ==
              static_cast<uint8_t>(OriginFlags::Author));

constexpr const StyleOrigin ServoStyleSet::kOrigins[];

ServoStyleSet* sInServoTraversal = nullptr;

// On construction, sets sInServoTraversal to the given ServoStyleSet.
// On destruction, clears sInServoTraversal and calls RunPostTraversalTasks.
class MOZ_RAII AutoSetInServoTraversal {
 public:
  explicit AutoSetInServoTraversal(ServoStyleSet* aSet) : mSet(aSet) {
    MOZ_ASSERT(!sInServoTraversal);
    MOZ_ASSERT(aSet);
    sInServoTraversal = aSet;
  }

  ~AutoSetInServoTraversal() {
    MOZ_ASSERT(sInServoTraversal);
    sInServoTraversal = nullptr;
    mSet->RunPostTraversalTasks();
  }

 private:
  ServoStyleSet* mSet;
};

// Sets up for one or more calls to Servo_TraverseSubtree.
class MOZ_RAII AutoPrepareTraversal {
 public:
  explicit AutoPrepareTraversal(ServoStyleSet* aSet)
      // For markers for animations, we have already set the markers in
      // RestyleManager::PostRestyleEventForAnimations so that we don't need
      // to care about animation restyles here.
      : mTimelineMarker(aSet->mDocument->GetDocShell(), false),
        mSetInServoTraversal(aSet) {
    MOZ_ASSERT(!aSet->StylistNeedsUpdate());
  }

 private:
  AutoRestyleTimelineMarker mTimelineMarker;
  AutoSetInServoTraversal mSetInServoTraversal;
};

ServoStyleSet::ServoStyleSet(Document& aDocument) : mDocument(&aDocument) {
  PreferenceSheet::EnsureInitialized();
  PodArrayZero(mCachedAnonymousContentStyleIndexes);
  mRawSet.reset(Servo_StyleSet_Init(&aDocument));
}

ServoStyleSet::~ServoStyleSet() {
  MOZ_ASSERT(!IsInServoTraversal());
  EnumerateStyleSheets([&](StyleSheet& aSheet) { aSheet.DropStyleSet(this); });
}

nsPresContext* ServoStyleSet::GetPresContext() {
  return mDocument->GetPresContext();
}

template <typename Functor>
static void EnumerateShadowRoots(const Document& aDoc, const Functor& aCb) {
  const Document::ShadowRootSet& shadowRoots = aDoc.ComposedShadowRoots();
  for (auto iter = shadowRoots.ConstIter(); !iter.Done(); iter.Next()) {
    ShadowRoot* root = iter.Get()->GetKey();
    MOZ_ASSERT(root);
    MOZ_DIAGNOSTIC_ASSERT(root->IsInComposedDoc());
    aCb(*root);
  }
}

void ServoStyleSet::ShellDetachedFromDocument() {
  ClearNonInheritingComputedStyles();
  mCachedAnonymousContentStyles.Clear();
  PodArrayZero(mCachedAnonymousContentStyleIndexes);
  mStyleRuleMap = nullptr;

  // Remove all our stylesheets...
  for (auto origin : kOrigins) {
    for (size_t count = SheetCount(origin); count--;) {
      RemoveStyleSheet(*SheetAt(origin, count));
    }
  }

  // And remove all the CascadeDatas from memory.
  UpdateStylistIfNeeded();

  // Also GC the ruletree if it got big now that the DOM no longer has
  // references to styles around anymore.
  MaybeGCRuleTree();
}

void ServoStyleSet::RecordShadowStyleChange(ShadowRoot& aShadowRoot) {
  // TODO(emilio): We could keep track of the actual shadow roots that need
  // their styles recomputed.
  SetStylistShadowDOMStyleSheetsDirty();

  // FIXME(emilio): This should be done using stylesheet invalidation instead.
  if (nsPresContext* pc = GetPresContext()) {
    pc->RestyleManager()->PostRestyleEvent(
        aShadowRoot.Host(), RestyleHint::RestyleSubtree(), nsChangeHint(0));
  }
}

void ServoStyleSet::InvalidateStyleForDocumentStateChanges(
    EventStates aStatesChanged) {
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
  // for Shadow DOM. Consider just enumerating shadow roots instead and run
  // invalidation individually, passing mRawSet for the UA / User sheets.
  AutoTArray<const RawServoAuthorStyles*, 20> nonDocumentStyles;

  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    if (auto* authorStyles = aShadowRoot.GetServoStyles()) {
      nonDocumentStyles.AppendElement(authorStyles);
    }
  });

  Servo_InvalidateStyleForDocStateChanges(
      root, mRawSet.get(), &nonDocumentStyles, aStatesChanged.ServoValue());
}

static const MediaFeatureChangeReason kMediaFeaturesAffectingDefaultStyle =
    // Zoom changes change the meaning of em units.
    MediaFeatureChangeReason::ZoomChange |
    // A resolution change changes the app-units-per-dev-pixels ratio, which
    // some structs (Border, Outline, Column) store for clamping. We should
    // arguably not do that, maybe doing it on layout directly, to try to avoid
    // relying on the pres context (bug 1418159).
    MediaFeatureChangeReason::ResolutionChange;

RestyleHint ServoStyleSet::MediumFeaturesChanged(
    MediaFeatureChangeReason aReason) {
  AutoTArray<RawServoAuthorStyles*, 20> nonDocumentStyles;

  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    if (auto* authorStyles = aShadowRoot.GetServoStyles()) {
      nonDocumentStyles.AppendElement(authorStyles);
    }
  });

  bool mayAffectDefaultStyle =
      bool(aReason & kMediaFeaturesAffectingDefaultStyle);

  const MediumFeaturesChangedResult result =
      Servo_StyleSet_MediumFeaturesChanged(mRawSet.get(), &nonDocumentStyles,
                                           mayAffectDefaultStyle);

  const bool rulesChanged =
      result.mAffectsDocumentRules || result.mAffectsNonDocumentRules;

  if (result.mAffectsDocumentRules) {
    SetStylistStyleSheetsDirty();
  }

  if (result.mAffectsNonDocumentRules) {
    SetStylistShadowDOMStyleSheetsDirty();
  }

  if (rulesChanged) {
    // TODO(emilio): This could be more granular.
    return RestyleHint::RestyleSubtree();
  }

  const bool viewportChanged =
      bool(aReason & MediaFeatureChangeReason::ViewportChange);
  if (result.mUsesViewportUnits && viewportChanged) {
    return RestyleHint::RecascadeSubtree();
  }

  return RestyleHint{0};
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoStyleSetMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoStyleSetMallocEnclosingSizeOf)

void ServoStyleSet::AddSizeOfIncludingThis(nsWindowSizes& aSizes) const {
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
    aSizes.mLayoutStyleSetsStylistInvalidationMap += sizes.mInvalidationMap;
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

void ServoStyleSet::SetAuthorStyleDisabled(bool aStyleDisabled) {
  if (mAuthorStyleDisabled == aStyleDisabled) {
    return;
  }

  mAuthorStyleDisabled = aStyleDisabled;
  if (Element* root = mDocument->GetRootElement()) {
    if (nsPresContext* pc = GetPresContext()) {
      pc->RestyleManager()->PostRestyleEvent(
          root, RestyleHint::RestyleSubtree(), nsChangeHint(0));
    }
  }
  Servo_StyleSet_SetAuthorStyleDisabled(mRawSet.get(), mAuthorStyleDisabled);
  // XXX Workaround for bug 1437785.
  SetStylistStyleSheetsDirty();
}

const ServoElementSnapshotTable& ServoStyleSet::Snapshots() {
  MOZ_ASSERT(GetPresContext(), "Styling a document without a shell?");
  return GetPresContext()->RestyleManager()->Snapshots();
}

void ServoStyleSet::ResolveMappedAttrDeclarationBlocks() {
  if (nsHTMLStyleSheet* sheet = mDocument->GetAttributeStyleSheet()) {
    sheet->CalculateMappedServoDeclarations();
  }

  mDocument->ResolveScheduledSVGPresAttrs();
}

void ServoStyleSet::PreTraverseSync() {
  // Get the Document's root element to ensure that the cache is valid before
  // calling into the (potentially-parallel) Servo traversal, where a cache hit
  // is necessary to avoid a data race when updating the cache.
  Unused << mDocument->GetRootElement();

  // FIXME(emilio): This shouldn't be needed in theory, the call to the same
  // function in PresShell should do the work, but as it turns out we
  // ProcessPendingRestyles() twice, and runnables from frames just constructed
  // can end up doing editing stuff, which adds stylesheets etc...
  mDocument->FlushUserFontSet();

  ResolveMappedAttrDeclarationBlocks();

  nsMediaFeatures::InitSystemMetrics();

  LookAndFeel::NativeInit();

  mDocument->CacheAllKnownLangPrefs();

  if (gfxUserFontSet* userFontSet = mDocument->GetUserFontSet()) {
    nsPresContext* presContext = GetPresContext();
    MOZ_ASSERT(presContext,
               "For now, we don't call into here without a pres context");

    // Ensure that the @font-face data is not stale
    uint64_t generation = userFontSet->GetGeneration();
    if (generation != mUserFontSetUpdateGeneration) {
      mDocument->GetFonts()->CacheFontLoadability();
      presContext->DeviceContext()->UpdateFontCacheUserFonts(userFontSet);
      mUserFontSetUpdateGeneration = generation;
    }
  }

  MOZ_ASSERT(!StylistNeedsUpdate());
}

void ServoStyleSet::PreTraverse(ServoTraversalFlags aFlags, Element* aRoot) {
  PreTraverseSync();

  // Process animation stuff that we should avoid doing during the parallel
  // traversal.
  SMILAnimationController* smilController =
      mDocument->HasAnimationController() ? mDocument->GetAnimationController()
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
ResolveStyleForTextOrFirstLetterContinuation(const RawServoStyleSet* aStyleSet,
                                             ComputedStyle& aParent,
                                             PseudoStyleType aType) {
  MOZ_ASSERT(aType == PseudoStyleType::mozText ||
             aType == PseudoStyleType::firstLetterContinuation);
  auto inheritTarget = aType == PseudoStyleType::mozText
                           ? InheritTarget::Text
                           : InheritTarget::FirstLetterContinuation;

  RefPtr<ComputedStyle> style = aParent.GetCachedInheritingAnonBoxStyle(aType);
  if (!style) {
    style =
        Servo_ComputedValues_Inherit(aStyleSet, aType, &aParent, inheritTarget)
            .Consume();
    MOZ_ASSERT(style);
    aParent.SetCachedInheritedAnonBoxStyle(style);
  }

  return style.forget();
}

already_AddRefed<ComputedStyle> ServoStyleSet::ResolveStyleForText(
    nsIContent* aTextNode, ComputedStyle* aParentStyle) {
  MOZ_ASSERT(aTextNode && aTextNode->IsText());
  MOZ_ASSERT(aTextNode->GetParent());
  MOZ_ASSERT(aParentStyle);

  return ResolveStyleForTextOrFirstLetterContinuation(
      mRawSet.get(), *aParentStyle, PseudoStyleType::mozText);
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveStyleForFirstLetterContinuation(
    ComputedStyle* aParentStyle) {
  MOZ_ASSERT(aParentStyle);

  return ResolveStyleForTextOrFirstLetterContinuation(
      mRawSet.get(), *aParentStyle, PseudoStyleType::firstLetterContinuation);
}

already_AddRefed<ComputedStyle> ServoStyleSet::ResolveStyleForPlaceholder() {
  RefPtr<ComputedStyle>& cache = mNonInheritingComputedStyles
      [nsCSSAnonBoxes::NonInheriting::oofPlaceholder];
  if (cache) {
    RefPtr<ComputedStyle> retval = cache;
    return retval.forget();
  }

  RefPtr<ComputedStyle> computedValues =
      Servo_ComputedValues_Inherit(mRawSet.get(),
                                   PseudoStyleType::oofPlaceholder, nullptr,
                                   InheritTarget::PlaceholderFrame)
          .Consume();
  MOZ_ASSERT(computedValues);

  cache = computedValues;
  return computedValues.forget();
}

static inline bool LazyPseudoIsCacheable(PseudoStyleType aType,
                                         const Element& aOriginatingElement,
                                         ComputedStyle* aParentStyle) {
  return aParentStyle &&
         !nsCSSPseudoElements::IsEagerlyCascadedInServo(aType) &&
         aOriginatingElement.HasServoData() &&
         !Servo_Element_IsPrimaryStyleReusedViaRuleNode(&aOriginatingElement);
}

already_AddRefed<ComputedStyle> ServoStyleSet::ResolvePseudoElementStyle(
    const Element& aOriginatingElement, PseudoStyleType aType,
    ComputedStyle* aParentStyle, IsProbe aIsProbe) {
  // Runs from frame construction, this should have clean styles already, except
  // with non-lazy FC...
  UpdateStylistIfNeeded();
  MOZ_ASSERT(PseudoStyle::IsPseudoElement(aType));

  const bool cacheable =
      LazyPseudoIsCacheable(aType, aOriginatingElement, aParentStyle);
  RefPtr<ComputedStyle> style =
      cacheable ? aParentStyle->GetCachedLazyPseudoStyle(aType) : nullptr;

  const bool isProbe = aIsProbe == IsProbe::Yes;

  if (!style) {
    // FIXME(emilio): Why passing null for probing as the parent style?
    //
    // There are callers which do pass the wrong parent style and it would
    // assert (like ComputeSelectionStyle()). That's messy!
    style = Servo_ResolvePseudoStyle(&aOriginatingElement, aType, isProbe,
                                     isProbe ? nullptr : aParentStyle,
                                     mRawSet.get())
                .Consume();
    if (!style) {
      MOZ_ASSERT(isProbe);
      return nullptr;
    }
    if (cacheable) {
      aParentStyle->SetCachedLazyPseudoStyle(style);
    }
  }

  MOZ_ASSERT(style);

  if (isProbe && !GeneratedContentPseudoExists(*aParentStyle, *style)) {
    return nullptr;
  }

  return style.forget();
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveInheritingAnonymousBoxStyle(PseudoStyleType aType,
                                                  ComputedStyle* aParentStyle) {
  MOZ_ASSERT(PseudoStyle::IsInheritingAnonBox(aType));
  MOZ_ASSERT_IF(aParentStyle, !StylistNeedsUpdate());

  UpdateStylistIfNeeded();

  RefPtr<ComputedStyle> style = nullptr;

  if (aParentStyle) {
    style = aParentStyle->GetCachedInheritingAnonBoxStyle(aType);
  }

  if (!style) {
    style = Servo_ComputedValues_GetForAnonymousBox(aParentStyle, aType,
                                                    mRawSet.get())
                .Consume();
    MOZ_ASSERT(style);
    if (aParentStyle) {
      aParentStyle->SetCachedInheritedAnonBoxStyle(style);
    }
  }

  return style.forget();
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveNonInheritingAnonymousBoxStyle(PseudoStyleType aType) {
  MOZ_ASSERT(PseudoStyle::IsNonInheritingAnonBox(aType));
  nsCSSAnonBoxes::NonInheriting type =
      nsCSSAnonBoxes::NonInheritingTypeForPseudoType(aType);
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
  MOZ_ASSERT(!PseudoStyle::IsNonInheritingAnonBox(PseudoStyleType::viewport),
             "viewport needs fixup to handle blockifying it");

  RefPtr<ComputedStyle> computedValues =
      Servo_ComputedValues_GetForAnonymousBox(nullptr, aType, mRawSet.get())
          .Consume();
  MOZ_ASSERT(computedValues);

  cache = computedValues;
  return computedValues.forget();
}

#ifdef MOZ_XUL
already_AddRefed<ComputedStyle> ServoStyleSet::ResolveXULTreePseudoStyle(
    dom::Element* aParentElement, nsCSSAnonBoxPseudoStaticAtom* aPseudoTag,
    ComputedStyle* aParentStyle, const AtomArray& aInputWord) {
  MOZ_ASSERT(nsCSSAnonBoxes::IsTreePseudoElement(aPseudoTag));
  MOZ_ASSERT(aParentStyle);
  MOZ_ASSERT(!StylistNeedsUpdate());

  return Servo_ComputedValues_ResolveXULTreePseudoStyle(
             aParentElement, aPseudoTag, aParentStyle, &aInputWord,
             mRawSet.get())
      .Consume();
}
#endif

// manage the set of style sheets in the style set
void ServoStyleSet::AppendStyleSheet(StyleSheet& aSheet) {
  MOZ_ASSERT(aSheet.IsApplicable());
  MOZ_ASSERT(aSheet.RawContents(),
             "Raw sheet should be in place before insertion.");

  aSheet.AddStyleSet(this);

  // Maintain a mirrored list of sheets on the servo side.
  // Servo will remove aSheet from its original position as part of the call
  // to Servo_StyleSet_AppendStyleSheet.
  Servo_StyleSet_AppendStyleSheet(mRawSet.get(), &aSheet);
  SetStylistStyleSheetsDirty();

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(aSheet);
  }
}

void ServoStyleSet::RemoveStyleSheet(StyleSheet& aSheet) {
  aSheet.DropStyleSet(this);

  // Maintain a mirrored list of sheets on the servo side.
  Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), &aSheet);
  SetStylistStyleSheetsDirty();

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetRemoved(aSheet);
  }
}

void ServoStyleSet::InsertStyleSheetBefore(StyleSheet& aNewSheet,
                                           StyleSheet& aReferenceSheet) {
  MOZ_ASSERT(aNewSheet.IsApplicable());
  MOZ_ASSERT(aReferenceSheet.IsApplicable());
  MOZ_ASSERT(&aNewSheet != &aReferenceSheet,
             "Can't place sheet before itself.");
  MOZ_ASSERT(aNewSheet.GetOrigin() == aReferenceSheet.GetOrigin(),
             "Sheets should be in the same origin");
  MOZ_ASSERT(aNewSheet.RawContents(),
             "Raw sheet should be in place before insertion.");
  MOZ_ASSERT(aReferenceSheet.RawContents(),
             "Reference sheet should have a raw sheet.");

  // Servo will remove aNewSheet from its original position as part of the
  // call to Servo_StyleSet_InsertStyleSheetBefore.
  aNewSheet.AddStyleSet(this);

  // Maintain a mirrored list of sheets on the servo side.
  Servo_StyleSet_InsertStyleSheetBefore(mRawSet.get(), &aNewSheet,
                                        &aReferenceSheet);
  SetStylistStyleSheetsDirty();

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(aNewSheet);
  }
}

size_t ServoStyleSet::SheetCount(Origin aOrigin) const {
  return Servo_StyleSet_GetSheetCount(mRawSet.get(), aOrigin);
}

StyleSheet* ServoStyleSet::SheetAt(Origin aOrigin, size_t aIndex) const {
  return const_cast<StyleSheet*>(
      Servo_StyleSet_GetSheetAt(mRawSet.get(), aOrigin, aIndex));
}

void ServoStyleSet::AppendAllNonDocumentAuthorSheets(
    nsTArray<StyleSheet*>& aArray) const {
  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    for (auto index : IntegerRange(aShadowRoot.SheetCount())) {
      aArray.AppendElement(aShadowRoot.SheetAt(index));
    }
  });
}

void ServoStyleSet::AddDocStyleSheet(StyleSheet& aSheet) {
  MOZ_ASSERT(aSheet.IsApplicable());
  MOZ_ASSERT(aSheet.RawContents(),
             "Raw sheet should be in place by this point.");

  size_t index = mDocument->FindDocStyleSheetInsertionPoint(aSheet);
  aSheet.AddStyleSet(this);

  if (index < SheetCount(Origin::Author)) {
    // This case is insert before.
    StyleSheet* beforeSheet = SheetAt(Origin::Author, index);

    // Maintain a mirrored list of sheets on the servo side.
    Servo_StyleSet_InsertStyleSheetBefore(mRawSet.get(), &aSheet, beforeSheet);
    SetStylistStyleSheetsDirty();
  } else {
    // Maintain a mirrored list of sheets on the servo side.
    Servo_StyleSet_AppendStyleSheet(mRawSet.get(), &aSheet);
    SetStylistStyleSheetsDirty();
  }

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(aSheet);
  }
}

bool ServoStyleSet::GeneratedContentPseudoExists(
    const ComputedStyle& aParentStyle, const ComputedStyle& aPseudoStyle) {
  auto type = aPseudoStyle.GetPseudoType();
  MOZ_ASSERT(type != PseudoStyleType::NotPseudo);

  if (type == PseudoStyleType::marker) {
    // ::marker only exist for list items (for now).
    if (!aParentStyle.StyleDisplay()->IsListItem()) {
      return false;
    }
    // display:none is equivalent to not having the pseudo-element at all.
    if (aPseudoStyle.StyleDisplay()->mDisplay == StyleDisplay::None) {
      return false;
    }
  }

  // For :before and :after pseudo-elements, having display: none or no
  // 'content' property is equivalent to not having the pseudo-element
  // at all.
  if (type == PseudoStyleType::before || type == PseudoStyleType::after) {
    if (aPseudoStyle.StyleDisplay()->mDisplay == StyleDisplay::None) {
      return false;
    }
    if (!aPseudoStyle.StyleContent()->ContentCount()) {
      return false;
    }
  }

  return true;
}

bool ServoStyleSet::StyleDocument(ServoTraversalFlags aFlags) {
  AUTO_PROFILER_LABEL_CATEGORY_PAIR(LAYOUT_StyleComputation);
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
    postTraversalRequired |= root->HasAnyOfFlags(
        Element::kAllServoDescendantBits | NODE_NEEDS_FRAME);

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
            parent, existingBits | ELEMENT_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
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
    Element* root =
        styleRoot->IsElement() ? styleRoot->AsElement() : rootElement;

    postTraversalRequired |=
        Servo_TraverseSubtree(root, mRawSet.get(), &snapshots, aFlags);
    postTraversalRequired |= root->HasAnyOfFlags(
        Element::kAllServoDescendantBits | NODE_NEEDS_FRAME);
  }

  return postTraversalRequired;
}

void ServoStyleSet::StyleNewSubtree(Element* aRoot) {
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
  if (GetPresContext()->EffectCompositor()->PreTraverseInSubtree(flags,
                                                                 aRoot)) {
    postTraversalRequired =
        Servo_TraverseSubtree(aRoot, mRawSet.get(), &snapshots,
                              ServoTraversalFlags::AnimationOnly |
                                  ServoTraversalFlags::FinalAnimationTraversal);
    MOZ_ASSERT(!postTraversalRequired);
  }
}

void ServoStyleSet::MarkOriginsDirty(OriginFlags aChangedOrigins) {
  SetStylistStyleSheetsDirty();
  Servo_StyleSet_NoteStyleSheetsChanged(mRawSet.get(), aChangedOrigins);
}

void ServoStyleSet::SetStylistStyleSheetsDirty() {
  mStylistState |= StylistState::StyleSheetsDirty;

  // We need to invalidate cached style in getComputedStyle for undisplayed
  // elements, since we don't know if any of the style sheet change that we do
  // would affect undisplayed elements.
  //
  // We don't allow to call getComputedStyle in elements without a pres shell
  // yet, so it is fine if there's no pres context here.
  if (nsPresContext* presContext = GetPresContext()) {
    presContext->RestyleManager()->IncrementUndisplayedRestyleGeneration();
  }
}

void ServoStyleSet::SetStylistShadowDOMStyleSheetsDirty() {
  mStylistState |= StylistState::ShadowDOMStyleSheetsDirty;
  if (nsPresContext* presContext = GetPresContext()) {
    presContext->RestyleManager()->IncrementUndisplayedRestyleGeneration();
  }
}

static OriginFlags ToOriginFlags(StyleOrigin aOrigin) {
  switch (aOrigin) {
    case StyleOrigin::UserAgent:
      return OriginFlags::UserAgent;
    case StyleOrigin::User:
      return OriginFlags::User;
    default:
      MOZ_FALLTHROUGH_ASSERT("Unknown origin?");
    case StyleOrigin::Author:
      return OriginFlags::Author;
  }
}

void ServoStyleSet::ImportRuleLoaded(dom::CSSImportRule&, StyleSheet& aSheet) {
  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(aSheet);
  }

  // TODO: Should probably consider ancestor sheets too.
  if (!aSheet.IsApplicable()) {
    return;
  }

  // TODO(emilio): Could handle it better given we know it is an insertion, and
  // use the style invalidation machinery stuff that we do for regular sheet
  // insertions.
  MarkOriginsDirty(ToOriginFlags(aSheet.GetOrigin()));
}

void ServoStyleSet::RuleAdded(StyleSheet& aSheet, css::Rule& aRule) {
  if (mStyleRuleMap) {
    mStyleRuleMap->RuleAdded(aSheet, aRule);
  }

  if (!aSheet.IsApplicable() || aRule.IsIncompleteImportRule()) {
    return;
  }

  RuleChangedInternal(aSheet, aRule, StyleRuleChangeKind::Insertion);
}

void ServoStyleSet::RuleRemoved(StyleSheet& aSheet, css::Rule& aRule) {
  if (mStyleRuleMap) {
    mStyleRuleMap->RuleRemoved(aSheet, aRule);
  }

  if (!aSheet.IsApplicable()) {
    return;
  }

  RuleChangedInternal(aSheet, aRule, StyleRuleChangeKind::Removal);
}

void ServoStyleSet::RuleChangedInternal(StyleSheet& aSheet, css::Rule& aRule,
                                        StyleRuleChangeKind aKind) {
  MOZ_ASSERT(aSheet.IsApplicable());
  SetStylistStyleSheetsDirty();

#define CASE_FOR(constant_, type_)                                       \
  case CSSRule_Binding::constant_##_RULE:                                \
    return Servo_StyleSet_##type_##RuleChanged(                          \
        mRawSet.get(), static_cast<dom::CSS##type_##Rule&>(aRule).Raw(), \
        &aSheet, aKind);

  switch (aRule.Type()) {
    CASE_FOR(COUNTER_STYLE, CounterStyle)
    CASE_FOR(STYLE, Style)
    CASE_FOR(IMPORT, Import)
    CASE_FOR(MEDIA, Media)
    CASE_FOR(KEYFRAMES, Keyframes)
    CASE_FOR(FONT_FEATURE_VALUES, FontFeatureValues)
    CASE_FOR(FONT_FACE, FontFace)
    CASE_FOR(PAGE, Page)
    CASE_FOR(DOCUMENT, MozDocument)
    CASE_FOR(SUPPORTS, Supports)
    // @namespace can only be inserted / removed when there are only other
    // @namespace and @import rules, and can't be mutated.
    case CSSRule_Binding::NAMESPACE_RULE:
    case CSSRule_Binding::CHARSET_RULE:
      break;
    case CSSRule_Binding::KEYFRAME_RULE:
      // FIXME: We should probably just forward to the parent @keyframes rule? I
      // think that'd do the right thing, but meanwhile...
      return MarkOriginsDirty(ToOriginFlags(aSheet.GetOrigin()));
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown rule type changed");
      return MarkOriginsDirty(ToOriginFlags(aSheet.GetOrigin()));
  }

#undef CASE_FOR
}

void ServoStyleSet::RuleChanged(StyleSheet& aSheet, css::Rule* aRule,
                                StyleRuleChangeKind aKind) {
  if (!aSheet.IsApplicable()) {
    return;
  }

  if (!aRule) {
    // FIXME: This is done for StyleSheet.media attribute changes and such
    MarkOriginsDirty(ToOriginFlags(aSheet.GetOrigin()));
  } else {
    RuleChangedInternal(aSheet, *aRule, aKind);
  }
}

#ifdef DEBUG
void ServoStyleSet::AssertTreeIsClean() {
  DocumentStyleRootIterator iter(mDocument);
  while (Element* root = iter.GetNextStyleRoot()) {
    Servo_AssertTreeIsClean(root);
  }
}
#endif

bool ServoStyleSet::GetKeyframesForName(const Element& aElement,
                                        const ComputedStyle& aStyle,
                                        nsAtom* aName,
                                        const nsTimingFunction& aTimingFunction,
                                        nsTArray<Keyframe>& aKeyframes) {
  MOZ_ASSERT(!StylistNeedsUpdate());
  return Servo_StyleSet_GetKeyframesForName(
      mRawSet.get(), &aElement, &aStyle, aName, &aTimingFunction, &aKeyframes);
}

nsTArray<ComputedKeyframeValues> ServoStyleSet::GetComputedKeyframeValuesFor(
    const nsTArray<Keyframe>& aKeyframes, Element* aElement,
    PseudoStyleType aPseudoType, const ComputedStyle* aStyle) {
  nsTArray<ComputedKeyframeValues> result(aKeyframes.Length());

  // Construct each nsTArray<PropertyStyleAnimationValuePair> here.
  result.AppendElements(aKeyframes.Length());

  Servo_GetComputedKeyframeValues(&aKeyframes, aElement, aPseudoType, aStyle,
                                  mRawSet.get(), &result);
  return result;
}

void ServoStyleSet::GetAnimationValues(
    RawServoDeclarationBlock* aDeclarations, Element* aElement,
    const ComputedStyle* aComputedStyle,
    nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues) {
  // Servo_GetAnimationValues below won't handle ignoring existing element
  // data for bfcached documents. (See comment in ResolveStyleLazily
  // about these bfcache issues.)
  Servo_GetAnimationValues(aDeclarations, aElement, aComputedStyle,
                           mRawSet.get(), &aAnimationValues);
}

already_AddRefed<ComputedStyle> ServoStyleSet::GetBaseContextForElement(
    Element* aElement, const ComputedStyle* aStyle) {
  return Servo_StyleSet_GetBaseComputedValuesForElement(mRawSet.get(), aElement,
                                                        aStyle, &Snapshots())
      .Consume();
}

already_AddRefed<ComputedStyle>
ServoStyleSet::ResolveServoStyleByAddingAnimation(
    Element* aElement, const ComputedStyle* aStyle,
    RawServoAnimationValue* aAnimationValue) {
  return Servo_StyleSet_GetComputedValuesByAddingAnimation(
             mRawSet.get(), aElement, aStyle, &Snapshots(), aAnimationValue)
      .Consume();
}

already_AddRefed<RawServoAnimationValue> ServoStyleSet::ComputeAnimationValue(
    Element* aElement, RawServoDeclarationBlock* aDeclarations,
    const ComputedStyle* aStyle) {
  return Servo_AnimationValue_Compute(aElement, aDeclarations, aStyle,
                                      mRawSet.get())
      .Consume();
}

bool ServoStyleSet::EnsureUniqueInnerOnCSSSheets() {
  using SheetOwner = Variant<ServoStyleSet*, ShadowRoot*>;

  AutoTArray<std::pair<StyleSheet*, SheetOwner>, 32> queue;
  EnumerateStyleSheets([&](StyleSheet& aSheet) {
    queue.AppendElement(std::make_pair(&aSheet, SheetOwner{this}));
  });

  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    for (auto index : IntegerRange(aShadowRoot.SheetCount())) {
      queue.AppendElement(
          std::make_pair(aShadowRoot.SheetAt(index), SheetOwner{&aShadowRoot}));
    }
  });

  while (!queue.IsEmpty()) {
    auto [sheet, owner] = queue.PopLastElement();

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
    for (StyleSheet* child : sheet->ChildSheets()) {
      queue.AppendElement(std::make_pair(child, owner));
    }
  }

  if (mNeedsRestyleAfterEnsureUniqueInner) {
    // TODO(emilio): We could make this faster if needed tracking the specific
    // origins and sheets that have been cloned. But the only caller of this
    // doesn't seem to really care about perf.
    MarkOriginsDirty(OriginFlags::All);
    ForceDirtyAllShadowStyles();
  }
  bool res = mNeedsRestyleAfterEnsureUniqueInner;
  mNeedsRestyleAfterEnsureUniqueInner = false;
  return res;
}

void ServoStyleSet::ClearCachedStyleData() {
  ClearNonInheritingComputedStyles();
  Servo_StyleSet_RebuildCachedData(mRawSet.get());
  mCachedAnonymousContentStyles.Clear();
  PodArrayZero(mCachedAnonymousContentStyleIndexes);
}

void ServoStyleSet::ForceDirtyAllShadowStyles() {
  bool anyShadow = false;
  EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
    if (auto* authorStyles = aShadowRoot.GetServoStyles()) {
      anyShadow = true;
      Servo_AuthorStyles_ForceDirty(authorStyles);
    }
  });
  if (anyShadow) {
    SetStylistShadowDOMStyleSheetsDirty();
  }
}

void ServoStyleSet::CompatibilityModeChanged() {
  Servo_StyleSet_CompatModeChanged(mRawSet.get());
  SetStylistStyleSheetsDirty();
  ForceDirtyAllShadowStyles();
}

void ServoStyleSet::ClearNonInheritingComputedStyles() {
  for (RefPtr<ComputedStyle>& ptr : mNonInheritingComputedStyles) {
    ptr = nullptr;
  }
}

already_AddRefed<ComputedStyle> ServoStyleSet::ResolveStyleLazily(
    const Element& aElement, PseudoStyleType aPseudoType,
    StyleRuleInclusion aRuleInclusion) {
  PreTraverseSync();
  MOZ_ASSERT(GetPresContext(),
             "For now, no style resolution without a pres context");
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
  const Element* elementForStyleResolution = &aElement;
  PseudoStyleType pseudoTypeForStyleResolution = aPseudoType;
  if (aPseudoType == PseudoStyleType::before) {
    if (Element* pseudo = nsLayoutUtils::GetBeforePseudo(&aElement)) {
      elementForStyleResolution = pseudo;
      pseudoTypeForStyleResolution = PseudoStyleType::NotPseudo;
    }
  } else if (aPseudoType == PseudoStyleType::after) {
    if (Element* pseudo = nsLayoutUtils::GetAfterPseudo(&aElement)) {
      elementForStyleResolution = pseudo;
      pseudoTypeForStyleResolution = PseudoStyleType::NotPseudo;
    }
  } else if (aPseudoType == PseudoStyleType::marker) {
    if (Element* pseudo = nsLayoutUtils::GetMarkerPseudo(&aElement)) {
      elementForStyleResolution = pseudo;
      pseudoTypeForStyleResolution = PseudoStyleType::NotPseudo;
    }
  }

  return Servo_ResolveStyleLazily(elementForStyleResolution,
                                  pseudoTypeForStyleResolution, aRuleInclusion,
                                  &Snapshots(), mRawSet.get())
      .Consume();
}

void ServoStyleSet::AppendFontFaceRules(
    nsTArray<nsFontFaceRuleContainer>& aArray) {
  // TODO(emilio): Can we make this so this asserts instead?
  UpdateStylistIfNeeded();
  Servo_StyleSet_GetFontFaceRules(mRawSet.get(), &aArray);
}

const RawServoCounterStyleRule* ServoStyleSet::CounterStyleRuleForName(
    nsAtom* aName) {
  MOZ_ASSERT(!StylistNeedsUpdate());
  return Servo_StyleSet_GetCounterStyleRule(mRawSet.get(), aName);
}

already_AddRefed<gfxFontFeatureValueSet>
ServoStyleSet::BuildFontFeatureValueSet() {
  MOZ_ASSERT(!StylistNeedsUpdate());
  RefPtr<gfxFontFeatureValueSet> set =
      Servo_StyleSet_BuildFontFeatureValueSet(mRawSet.get());
  return set.forget();
}

already_AddRefed<ComputedStyle> ServoStyleSet::ResolveForDeclarations(
    const ComputedStyle* aParentOrNull,
    const RawServoDeclarationBlock* aDeclarations) {
  // No need to update the stylist, we're only cascading aDeclarations.
  return Servo_StyleSet_ResolveForDeclarations(mRawSet.get(), aParentOrNull,
                                               aDeclarations)
      .Consume();
}

void ServoStyleSet::UpdateStylist() {
  MOZ_ASSERT(StylistNeedsUpdate());

  if (mStylistState & StylistState::StyleSheetsDirty) {
    Element* root = mDocument->GetRootElement();
    const ServoElementSnapshotTable* snapshots = nullptr;
    if (nsPresContext* pc = GetPresContext()) {
      snapshots = &pc->RestyleManager()->Snapshots();
    }
    Servo_StyleSet_FlushStyleSheets(mRawSet.get(), root, snapshots);
  }

  if (MOZ_UNLIKELY(mStylistState & StylistState::ShadowDOMStyleSheetsDirty)) {
    EnumerateShadowRoots(*mDocument, [&](ShadowRoot& aShadowRoot) {
      if (auto* authorStyles = aShadowRoot.GetServoStyles()) {
        Servo_AuthorStyles_Flush(authorStyles, mRawSet.get());
      }
    });
    Servo_StyleSet_RemoveUniqueEntriesFromAuthorStylesCache(mRawSet.get());
  }

  mStylistState = StylistState::NotDirty;
}

void ServoStyleSet::MaybeGCRuleTree() {
  MOZ_ASSERT(NS_IsMainThread());
  Servo_MaybeGCRuleTree(mRawSet.get());
}

/* static */
bool ServoStyleSet::MayTraverseFrom(const Element* aElement) {
  MOZ_ASSERT(aElement->IsInComposedDoc());
  nsINode* parent = aElement->GetFlattenedTreeParentNodeForStyle();
  if (!parent) {
    return false;
  }

  if (!parent->IsElement()) {
    MOZ_ASSERT(parent->IsDocument());
    return true;
  }

  if (!parent->AsElement()->HasServoData()) {
    return false;
  }

  return !Servo_Element_IsDisplayNone(parent->AsElement());
}

bool ServoStyleSet::ShouldTraverseInParallel() const {
  MOZ_ASSERT(mDocument->GetPresShell(), "Styling a document without a shell?");
  if (!mDocument->GetPresShell()->IsActive()) {
    return false;
  }
#ifdef MOZ_GECKO_PROFILER
  if (profiler_feature_active(ProfilerFeature::SequentialStyle)) {
    return false;
  }
#endif
  return true;
}

void ServoStyleSet::RunPostTraversalTasks() {
  MOZ_ASSERT(!IsInServoTraversal());

  if (mPostTraversalTasks.IsEmpty()) {
    return;
  }

  nsTArray<PostTraversalTask> tasks = std::move(mPostTraversalTasks);

  for (auto& task : tasks) {
    task.Run();
  }
}

ServoStyleRuleMap* ServoStyleSet::StyleRuleMap() {
  if (!mStyleRuleMap) {
    mStyleRuleMap = MakeUnique<ServoStyleRuleMap>();
  }
  mStyleRuleMap->EnsureTable(*this);
  return mStyleRuleMap.get();
}

bool ServoStyleSet::MightHaveAttributeDependency(const Element& aElement,
                                                 nsAtom* aAttribute) const {
  return Servo_StyleSet_MightHaveAttributeDependency(mRawSet.get(), &aElement,
                                                     aAttribute);
}

bool ServoStyleSet::HasStateDependency(const Element& aElement,
                                       EventStates aState) const {
  return Servo_StyleSet_HasStateDependency(mRawSet.get(), &aElement,
                                           aState.ServoValue());
}

bool ServoStyleSet::HasDocumentStateDependency(EventStates aState) const {
  return Servo_StyleSet_HasDocumentStateDependency(mRawSet.get(),
                                                   aState.ServoValue());
}

already_AddRefed<ComputedStyle> ServoStyleSet::ReparentComputedStyle(
    ComputedStyle* aComputedStyle, ComputedStyle* aNewParent,
    ComputedStyle* aNewParentIgnoringFirstLine, ComputedStyle* aNewLayoutParent,
    Element* aElement) {
  return Servo_ReparentStyle(aComputedStyle, aNewParent,
                             aNewParentIgnoringFirstLine, aNewLayoutParent,
                             aElement, mRawSet.get())
      .Consume();
}

NS_IMPL_ISUPPORTS(UACacheReporter, nsIMemoryReporter)

MOZ_DEFINE_MALLOC_SIZE_OF(ServoUACacheMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoUACacheMallocEnclosingSizeOf)

NS_IMETHODIMP
UACacheReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                nsISupports* aData, bool aAnonymize) {
  ServoStyleSetSizes sizes;
  Servo_UACache_AddSizeOf(ServoUACacheMallocSizeOf,
                          ServoUACacheMallocEnclosingSizeOf, &sizes);

#define REPORT(_path, _amount, _desc)                                     \
  do {                                                                    \
    size_t __amount = _amount; /* evaluate _amount only once */           \
    if (__amount > 0) {                                                   \
      MOZ_COLLECT_REPORT(_path, KIND_HEAP, UNITS_BYTES, __amount, _desc); \
    }                                                                     \
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

  REPORT("explicit/layout/servo-ua-cache/other", sizes.mOther,
         "Memory used by other data within the UA cache");

  return NS_OK;
}

}  // namespace mozilla
