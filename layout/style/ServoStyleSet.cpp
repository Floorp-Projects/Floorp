/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSet.h"

#include "gfxPlatformFontList.h"
#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/RestyleManagerInlines.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRuleProcessor.h"
#include "nsDeviceContext.h"
#include "nsHTMLStyleSheet.h"
#include "nsIDocumentInlines.h"
#include "nsPrintfCString.h"
#include "nsSMILAnimationController.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"
#include "gfxUserFontSet.h"

using namespace mozilla;
using namespace mozilla::dom;

static inline uint64_t UniqueIDForSheet(ServoStyleSheet* aSheet)
{
  // Servo tracks sheets by unique ID, and it's important that a given
  // ServoStyleSheet has the same unique ID throughout its lifetime.
  // Instead of tracking an arbitrary unique ID for each sheet,
  // we use the sheet address as a unique ID.
  return reinterpret_cast<uint64_t>(aSheet);
}

ServoStyleSet::ServoStyleSet()
  : mPresContext(nullptr)
  , mAllowResolveStaleStyles(false)
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

void
ServoStyleSet::Init(nsPresContext* aPresContext)
{
  mPresContext = aPresContext;
  mRawSet.reset(Servo_StyleSet_Init(aPresContext));

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

      MOZ_ASSERT(sheet->RawSheet(), "We should only append non-null raw sheets.");
      Servo_StyleSet_AppendStyleSheet(mRawSet.get(),
                                      sheet->RawSheet(),
                                      UniqueIDForSheet(sheet));
    }
  }

  // No need to Servo_StyleSet_FlushStyleSheets because we just created the
  // mRawSet, so there was nothing to flush.
}

void
ServoStyleSet::BeginShutdown()
{
  nsIDocument* doc = mPresContext->Document();

  // It's important to do this before mRawSet is released, since that will cause
  // a RuleTree GC, which needs to happen after we have dropped all of the
  // document's strong references to RuleNodes.  We also need to do it here,
  // in BeginShutdown, and not in Shutdown, since Shutdown happens after the
  // frame tree has been destroyed, but before the script runners that delete
  // native anonymous content (which also could be holding on the RuleNodes)
  // have run.  By clearing style here, before the frame tree is destroyed,
  // the AllChildrenIterator will find the anonymous content.
  //
  // Note that this is pretty bad for performance; we should find a way to
  // get by with the ServoNodeDatas being dropped as part of the document
  // going away.
  DocumentStyleRootIterator iter(doc);
  while (Element* root = iter.GetNextStyleRoot()) {
    ServoRestyleManager::ClearServoDataFromSubtree(root);
  }

  // We can also have some cloned canvas custom content stored in the document
  // (as done in nsCanvasFrame::DestroyFrom), due to bug 1348480, when we create
  // the clone (wastefully) during PresShell destruction.  Clear data from that
  // clone.
  for (RefPtr<AnonymousContent>& ac : doc->GetAnonymousContents()) {
    ServoRestyleManager::ClearServoDataFromSubtree(ac->GetContentNode());
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

bool
ServoStyleSet::MediumFeaturesChanged() const
{
  return Servo_StyleSet_MediumFeaturesChanged(mRawSet.get());
}

size_t
ServoStyleSet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

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

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               nsStyleContext* aParentContext,
                               LazyComputeBehavior aMayCompute)
{
  return GetContext(aElement, aParentContext, nullptr,
                    CSSPseudoElementType::NotPseudo, aMayCompute);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::GetContext(nsIContent* aContent,
                          nsStyleContext* aParentContext,
                          nsIAtom* aPseudoTag,
                          CSSPseudoElementType aPseudoType,
                          LazyComputeBehavior aMayCompute)
{
  MOZ_ASSERT(aContent->IsElement());
  Element* element = aContent->AsElement();

  RefPtr<ServoComputedValues> computedValues;
  if (aMayCompute == LazyComputeBehavior::Allow) {
    PreTraverseSync();
    computedValues =
      ResolveStyleLazily(element, CSSPseudoElementType::NotPseudo);
  } else {
    computedValues = ResolveServoStyle(element);
  }

  MOZ_ASSERT(computedValues);
  return GetContext(computedValues.forget(), aParentContext, aPseudoTag, aPseudoType,
                    element);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::GetContext(already_AddRefed<ServoComputedValues> aComputedValues,
                          nsStyleContext* aParentContext,
                          nsIAtom* aPseudoTag,
                          CSSPseudoElementType aPseudoType,
                          Element* aElementForAnimation)
{
  bool isLink = false;
  bool isVisitedLink = false;
  // If we need visited styles for callers where `aElementForAnimation` is null,
  // we can precompute these and pass them as flags, similar to nsStyleSet.cpp.
  if (aElementForAnimation) {
    isLink = nsCSSRuleProcessor::IsLink(aElementForAnimation);
    isVisitedLink = nsCSSRuleProcessor::GetContentState(aElementForAnimation)
                                       .HasState(NS_EVENT_STATE_VISITED);
  }

  RefPtr<ServoComputedValues> computedValues = Move(aComputedValues);
  RefPtr<ServoComputedValues> visitedComputedValues =
    Servo_ComputedValues_GetVisitedStyle(computedValues).Consume();

  // If `visitedComputedValues` is non-null, then there was a relevant link and
  // visited styles were computed.  This corresponds to the cases where Gecko's
  // style system produces `aVisitedRuleNode`.
  // Set up `parentIfVisited` depending on whether our parent context has a
  // a visited style.  If it doesn't but we do have visited styles, use the
  // regular parent context for visited.
  nsStyleContext *parentIfVisited =
    aParentContext ? aParentContext->GetStyleIfVisited() : nullptr;
  if (!parentIfVisited) {
    if (visitedComputedValues) {
      parentIfVisited = aParentContext;
    }
  }

  // The true visited state of the relevant link is used to decided whether
  // visited styles should be consulted for all visited dependent properties.
  bool relevantLinkVisited = isLink ? isVisitedLink :
    (aParentContext && aParentContext->RelevantLinkVisited());

  RefPtr<nsStyleContext> result =
    NS_NewStyleContext(aParentContext, mPresContext, aPseudoTag, aPseudoType,
                       computedValues.forget());

  if (visitedComputedValues) {
    RefPtr<nsStyleContext> resultIfVisited =
      NS_NewStyleContext(parentIfVisited, mPresContext, aPseudoTag, aPseudoType,
                         visitedComputedValues.forget());
    resultIfVisited->SetIsStyleIfVisited();
    result->SetStyleIfVisited(resultIfVisited.forget());

    if (relevantLinkVisited) {
      result->AddStyleBit(NS_STYLE_RELEVANT_LINK_VISITED);
    }
  }

  // Set the body color on the pres context. See nsStyleSet::GetContext
  if (aElementForAnimation &&
      aElementForAnimation->IsHTMLElement(nsGkAtoms::body) &&
      aPseudoType == CSSPseudoElementType::NotPseudo &&
      mPresContext->CompatibilityMode() == eCompatibility_NavQuirks) {
    nsIDocument* doc = aElementForAnimation->GetUncomposedDoc();
    if (doc && doc->GetBodyElement() == aElementForAnimation) {
      // Update the prescontext's body color
      mPresContext->SetBodyTextColor(result->StyleColor()->mColor);
    }
  }
  return result.forget();
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
  ResolveMappedAttrDeclarationBlocks();

  nsCSSRuleProcessor::InitSystemMetrics();

  // This is lazily computed and pseudo matching needs to access
  // it so force computation early.
  mPresContext->Document()->GetDocumentState();

  // Ensure that the @font-face data is not stale
  if (gfxUserFontSet* userFontSet = mPresContext->Document()->GetUserFontSet()) {
    uint64_t generation = userFontSet->GetGeneration();
    if (generation != mUserFontSetUpdateGeneration) {
      mPresContext->DeviceContext()->UpdateFontCacheUserFonts(userFontSet);
      mUserFontSetUpdateGeneration = generation;
    }
  }

  UpdateStylistIfNeeded();
  mPresContext->CacheAllLangs();
}

void
ServoStyleSet::PreTraverse(Element* aRoot,
                           EffectCompositor::AnimationRestyleType aRestyleType)
{
  PreTraverseSync();

  // Process animation stuff that we should avoid doing during the parallel
  // traversal.
  nsSMILAnimationController* smilController =
    mPresContext->Document()->GetAnimationController();
  if (aRoot) {
    mPresContext->EffectCompositor()
                ->PreTraverseInSubtree(aRoot, aRestyleType);
    if (smilController) {
      smilController->PreTraverseInSubtree(aRoot);
    }
  } else {
    mPresContext->EffectCompositor()->PreTraverse(aRestyleType);
    if (smilController) {
      smilController->PreTraverse();
    }
  }
}

bool
ServoStyleSet::PrepareAndTraverseSubtree(
  RawGeckoElementBorrowed aRoot,
  TraversalRootBehavior aRootBehavior,
  TraversalRestyleBehavior aRestyleBehavior)
{
  // Get the Document's root element to ensure that the cache is valid before
  // calling into the (potentially-parallel) Servo traversal, where a cache hit
  // is necessary to avoid a data race when updating the cache.
  mozilla::Unused << aRoot->OwnerDoc()->GetRootElement();

  MOZ_ASSERT(!StylistNeedsUpdate());
  AutoSetInServoTraversal guard(this);

  const SnapshotTable& snapshots = Snapshots();

  bool isInitial = !aRoot->HasServoData();
  bool forReconstruct =
    aRestyleBehavior == TraversalRestyleBehavior::ForReconstruct;
  bool forAnimationOnly =
    aRestyleBehavior == TraversalRestyleBehavior::ForAnimationOnly;
#ifdef DEBUG
  bool forNewlyBoundElement =
    aRestyleBehavior == TraversalRestyleBehavior::ForNewlyBoundElement;
#endif
  bool postTraversalRequired = Servo_TraverseSubtree(
    aRoot, mRawSet.get(), &snapshots, aRootBehavior, aRestyleBehavior);
  MOZ_ASSERT(!(isInitial || forReconstruct || forNewlyBoundElement) ||
             !postTraversalRequired);

  // Don't need to trigger a second traversal if this restyle only needs
  // animation-only restyle.
  if (forAnimationOnly) {
    return postTraversalRequired;
  }

  auto root = const_cast<Element*>(aRoot);

  // If there are still animation restyles needed, trigger a second traversal to
  // update CSS animations or transitions' styles.
  //
  // We don't need to do this for SMIL since SMIL only updates its animation
  // values once at the begin of a tick. As a result, even if the previous
  // traversal caused, for example, the font-size to change, the SMIL style
  // won't be updated until the next tick anyway.
  EffectCompositor* compositor = mPresContext->EffectCompositor();
  EffectCompositor::AnimationRestyleType restyleType =
    EffectCompositor::AnimationRestyleType::Throttled;
  if (forReconstruct ? compositor->PreTraverseInSubtree(root, restyleType)
                     : compositor->PreTraverse(restyleType)) {
    if (Servo_TraverseSubtree(
          aRoot, mRawSet.get(), &snapshots, aRootBehavior, aRestyleBehavior)) {
      MOZ_ASSERT(!forReconstruct);
      if (isInitial) {
        // We're doing initial styling, and the additional animation
        // traversal changed the styles that were set by the first traversal.
        // This would normally require a post-traversal to update the style
        // contexts, and the DOM now has dirty descendant bits and RestyleData
        // in expectation of that post-traversal. But since this is actually
        // the initial styling, there are no style contexts to update and no
        // frames to apply the change hints to, so we don't need to do that
        // post-traversal. Instead, just drop this state and tell the caller
        // that no post-traversal is required.
        MOZ_ASSERT(!postTraversalRequired);
        ServoRestyleManager::ClearRestyleStateFromSubtree(root);
      } else {
        postTraversalRequired = true;
      }
    }
  }

  return postTraversalRequired;
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForText(nsIContent* aTextNode,
                                   nsStyleContext* aParentContext)
{
  MOZ_ASSERT(aTextNode && aTextNode->IsNodeOfType(nsINode::eTEXT));
  MOZ_ASSERT(aTextNode->GetParent());
  MOZ_ASSERT(aParentContext);

  // Gecko expects text node style contexts to be like elements that match no
  // rules: inherit the inherit structs, reset the reset structs. This is cheap
  // enough to do on the main thread, which means that the parallel style system
  // can avoid worrying about text nodes.
  const ServoComputedValues* parentComputedValues =
    aParentContext->ComputedValues();
  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_Inherit(mRawSet.get(),
                                 parentComputedValues,
                                 InheritTarget::Text).Consume();

  return GetContext(computedValues.forget(), aParentContext,
                    nsCSSAnonBoxes::mozText,
                    CSSPseudoElementType::InheritingAnonBox,
                    nullptr);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForFirstLetterContinuation(nsStyleContext* aParentContext)
{
  const ServoComputedValues* parent =
    aParentContext->ComputedValues();
  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_Inherit(mRawSet.get(),
                                 parent,
                                 InheritTarget::FirstLetterContinuation)
                                 .Consume();
  MOZ_ASSERT(computedValues);

  return GetContext(computedValues.forget(), aParentContext,
                    nsCSSAnonBoxes::firstLetterContinuation,
                    CSSPseudoElementType::InheritingAnonBox,
                    nullptr);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForPlaceholder()
{
  RefPtr<nsStyleContext>& cache =
    mNonInheritingStyleContexts[nsCSSAnonBoxes::NonInheriting::oofPlaceholder];
  if (cache) {
    RefPtr<nsStyleContext> retval = cache;
    return retval.forget();
  }

  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_Inherit(mRawSet.get(),
                                 nullptr,
                                 InheritTarget::PlaceholderFrame)
                                 .Consume();
  MOZ_ASSERT(computedValues);

  RefPtr<nsStyleContext> retval =
    GetContext(computedValues.forget(), nullptr,
               nsCSSAnonBoxes::oofPlaceholder,
               CSSPseudoElementType::NonInheritingAnonBox,
               nullptr);
  cache = retval;
  return retval.forget();
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolvePseudoElementStyle(Element* aOriginatingElement,
                                         CSSPseudoElementType aType,
                                         nsStyleContext* aParentContext,
                                         Element* aPseudoElement)
{
  UpdateStylistIfNeeded();

  // NB: We ignore aParentContext, on the assumption that pseudo element styles
  // should just inherit from aOriginatingElement's primary style, which Servo
  // already knows.
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);

  RefPtr<ServoComputedValues> computedValues;
  if (aPseudoElement) {
    MOZ_ASSERT(aType == aPseudoElement->GetPseudoElementType());
    computedValues = Servo_ResolveStyle(aPseudoElement, mRawSet.get(),
                                        mAllowResolveStaleStyles).Consume();
  } else {
    computedValues =
      Servo_ResolvePseudoStyle(aOriginatingElement,
                               aType,
                               /* is_probe = */ false,
                               mRawSet.get()).Consume();
  }

  MOZ_ASSERT(computedValues);

  bool isBeforeOrAfter = aType == CSSPseudoElementType::before ||
                         aType == CSSPseudoElementType::after;

  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);
  return GetContext(computedValues.forget(), aParentContext, pseudoTag, aType,
                    isBeforeOrAfter ? aOriginatingElement : nullptr);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveTransientStyle(Element* aElement,
                                     nsIAtom* aPseudoTag,
                                     CSSPseudoElementType aPseudoType,
                                     StyleRuleInclusion aRuleInclusion)
{
  RefPtr<ServoComputedValues> computedValues =
    ResolveTransientServoStyle(aElement, aPseudoType, aRuleInclusion);

  return GetContext(computedValues.forget(),
                    nullptr,
                    aPseudoTag,
                    aPseudoType, nullptr);
}

already_AddRefed<ServoComputedValues>
ServoStyleSet::ResolveTransientServoStyle(
    Element* aElement,
    CSSPseudoElementType aPseudoType,
    StyleRuleInclusion aRuleInclusion)
{
  PreTraverseSync();
  return ResolveStyleLazily(aElement, aPseudoType, aRuleInclusion);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveInheritingAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                                  nsStyleContext* aParentContext)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudoTag) &&
             !nsCSSAnonBoxes::IsNonInheritingAnonBox(aPseudoTag));

  UpdateStylistIfNeeded();

  bool skipFixup =
    nsCSSAnonBoxes::AnonBoxSkipsParentDisplayBasedStyleFixup(aPseudoTag);

  const ServoComputedValues* parentStyle =
    aParentContext ? aParentContext->ComputedValues()
                   : nullptr;
  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_GetForAnonymousBox(parentStyle, aPseudoTag, skipFixup,
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

  return GetContext(computedValues.forget(), aParentContext, aPseudoTag,
                    CSSPseudoElementType::InheritingAnonBox, nullptr);
}

already_AddRefed<nsStyleContext>
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
  RefPtr<nsStyleContext>& cache = mNonInheritingStyleContexts[type];
  if (cache) {
    RefPtr<nsStyleContext> retval = cache;
    return retval.forget();
  }

  UpdateStylistIfNeeded();

  // We always want to skip parent-based display fixup here.  It never makes
  // sense for non-inheriting anonymous boxes.  (Static assertions in
  // nsCSSAnonBoxes.cpp ensure that all non-inheriting non-anonymous boxes
  // are indeed annotated as skipping this fixup.)
  MOZ_ASSERT(!nsCSSAnonBoxes::IsNonInheritingAnonBox(nsCSSAnonBoxes::viewport),
             "viewport needs fixup to handle blockifying it");
  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_GetForAnonymousBox(nullptr, aPseudoTag, true,
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

  RefPtr<nsStyleContext> retval =
    GetContext(computedValues.forget(), nullptr, aPseudoTag,
               CSSPseudoElementType::NonInheritingAnonBox, nullptr);
  cache = retval;
  return retval.forget();
}

// manage the set of style sheets in the style set
nsresult
ServoStyleSet::AppendStyleSheet(SheetType aType,
                                ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  MOZ_ASSERT(aSheet->RawSheet(), "Raw sheet should be in place before insertion.");

  RemoveSheetOfType(aType, aSheet);
  AppendSheetOfType(aType, aSheet);

  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    // Servo will remove aSheet from its original position as part of the call
    // to Servo_StyleSet_AppendStyleSheet.
    Servo_StyleSet_AppendStyleSheet(mRawSet.get(),
                                    aSheet->RawSheet(),
                                    UniqueIDForSheet(aSheet));
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
  MOZ_ASSERT(aSheet->RawSheet(), "Raw sheet should be in place before insertion.");

  RemoveSheetOfType(aType, aSheet);
  PrependSheetOfType(aType, aSheet);

  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    // Servo will remove aSheet from its original position as part of the call
    // to Servo_StyleSet_PrependStyleSheet.
    Servo_StyleSet_PrependStyleSheet(mRawSet.get(),
                                     aSheet->RawSheet(),
                                     UniqueIDForSheet(aSheet));
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
    Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), UniqueIDForSheet(aSheet));
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
      Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), UniqueIDForSheet(sheet));
    }
  }
  mSheets[aType].Clear();

  // Add in all the new sheets.
  for (auto& sheet : aNewSheets) {
    AppendSheetOfType(aType, sheet);
    if (mRawSet) {
      MOZ_ASSERT(sheet->RawSheet(), "Raw sheet should be in place before replacement.");
      Servo_StyleSet_AppendStyleSheet(mRawSet.get(),
                                      sheet->RawSheet(),
                                      UniqueIDForSheet(sheet));
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
  MOZ_ASSERT(aNewSheet->RawSheet(), "Raw sheet should be in place before insertion.");
  MOZ_ASSERT(aReferenceSheet->RawSheet(), "Reference sheet should have a raw sheet.");

  // Servo will remove aNewSheet from its original position as part of the
  // call to Servo_StyleSet_InsertStyleSheetBefore.
  RemoveSheetOfType(aType, aNewSheet);
  InsertSheetOfType(aType, aNewSheet, aReferenceSheet);

  if (mRawSet) {
    // Maintain a mirrored list of sheets on the servo side.
    Servo_StyleSet_InsertStyleSheetBefore(mRawSet.get(),
                                          aNewSheet->RawSheet(),
                                          UniqueIDForSheet(aNewSheet),
                                          UniqueIDForSheet(aReferenceSheet));
    SetStylistStyleSheetsDirty();
  }

  return NS_OK;
}

int32_t
ServoStyleSet::SheetCount(SheetType aType) const
{
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  return mSheets[aType].Length();
}

ServoStyleSheet*
ServoStyleSet::StyleSheetAt(SheetType aType,
                            int32_t aIndex) const
{
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  return mSheets[aType][aIndex];
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
  MOZ_ASSERT(aSheet->RawSheet(), "Raw sheet should be in place by this point.");

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
      Servo_StyleSet_InsertStyleSheetBefore(mRawSet.get(),
                                            aSheet->RawSheet(),
                                            UniqueIDForSheet(aSheet),
                                            UniqueIDForSheet(beforeSheet));
      SetStylistStyleSheetsDirty();
    }
  } else {
    // This case is append.
    AppendSheetOfType(SheetType::Doc, aSheet);

    if (mRawSet) {
      // Maintain a mirrored list of sheets on the servo side.
      Servo_StyleSet_AppendStyleSheet(mRawSet.get(),
                                      aSheet->RawSheet(),
                                      UniqueIDForSheet(aSheet));
      SetStylistStyleSheetsDirty();
    }
  }

  return NS_OK;
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aOriginatingElement,
                                       CSSPseudoElementType aType,
                                       nsStyleContext* aParentContext,
                                       Element* aPseudoElement)
{
  UpdateStylistIfNeeded();
  if (aPseudoElement) {
    NS_ERROR("stylo: We don't support CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE yet");
  }

  // NB: We ignore aParentContext, on the assumption that pseudo element styles
  // should just inherit from aOriginatingElement's primary style, which Servo
  // already knows.
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);

  RefPtr<ServoComputedValues> computedValues =
    Servo_ResolvePseudoStyle(aOriginatingElement, aType,
                             /* is_probe = */ true, mRawSet.get()).Consume();
  if (!computedValues) {
    return nullptr;
  }

  // For :before and :after pseudo-elements, having display: none or no
  // 'content' property is equivalent to not having the pseudo-element
  // at all.
  bool isBeforeOrAfter = aType == CSSPseudoElementType::before ||
                         aType == CSSPseudoElementType::after;
  if (isBeforeOrAfter) {
    const nsStyleDisplay* display = Servo_GetStyleDisplay(computedValues);
    const nsStyleContent* content = Servo_GetStyleContent(computedValues);
    // XXXldb What is contentCount for |content: ""|?
    if (display->mDisplay == StyleDisplay::None ||
        content->ContentCount() == 0) {
      return nullptr;
    }
  }

  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);
  return GetContext(computedValues.forget(), aParentContext, pseudoTag, aType,
                    isBeforeOrAfter ? aOriginatingElement : nullptr);
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
ServoStyleSet::StyleDocument(TraversalRestyleBehavior aRestyleBehavior)
{
  MOZ_ASSERT(
    aRestyleBehavior == TraversalRestyleBehavior::Normal ||
    aRestyleBehavior == TraversalRestyleBehavior::ForCSSRuleChanges,
    "StyleDocument() should be only called for normal traversal or CSS rule "
    "changes");

  PreTraverse();

  // Restyle the document from the root element and each of the document level
  // NAC subtree roots.
  bool postTraversalRequired = false;
  DocumentStyleRootIterator iter(mPresContext->Document());
  while (Element* root = iter.GetNextStyleRoot()) {
    if (PrepareAndTraverseSubtree(root,
                                  TraversalRootBehavior::Normal,
                                  aRestyleBehavior)) {
      postTraversalRequired = true;
    }
  }
  return postTraversalRequired;
}

bool
ServoStyleSet::StyleDocumentForAnimationOnly()
{
  PreTraverse(nullptr, EffectCompositor::AnimationRestyleType::Full);

  bool postTraversalRequired = false;
  DocumentStyleRootIterator iter(mPresContext->Document());
  while (Element* root = iter.GetNextStyleRoot()) {
    if (PrepareAndTraverseSubtree(root,
                                  TraversalRootBehavior::Normal,
                                  TraversalRestyleBehavior::ForAnimationOnly)) {
      postTraversalRequired = true;
    }
  }
  return postTraversalRequired;
}

void
ServoStyleSet::StyleNewSubtree(Element* aRoot)
{
  MOZ_ASSERT(!aRoot->HasServoData());

  PreTraverse();

  DebugOnly<bool> postTraversalRequired =
    PrepareAndTraverseSubtree(aRoot,
                              TraversalRootBehavior::Normal,
                              TraversalRestyleBehavior::Normal);
  MOZ_ASSERT(!postTraversalRequired);
}

void
ServoStyleSet::StyleNewChildren(Element* aParent)
{
  PreTraverse();

  PrepareAndTraverseSubtree(aParent,
                            TraversalRootBehavior::UnstyledChildrenOnly,
                            TraversalRestyleBehavior::Normal);
  // We can't assert that Servo_TraverseSubtree returns false, since aParent
  // or some of its other children might have pending restyles.
}

void
ServoStyleSet::StyleNewlyBoundElement(Element* aElement)
{
  PreTraverse();

  // In general the element is always styled by the time we're applying XBL
  // bindings, because we need to style the element to know what the binding
  // URI is. However, programmatic consumers of the XBL service (like the
  // XML pretty printer) _can_ apply bindings without having styled the bound
  // element. We could assert against this and require the callers manually
  // resolve the style first, but it's easy enough to just handle here.
  //
  // Also, when applying XBL bindings to elements within a display:none or
  // unstyled subtree (for example, when <object> elements are wrapped to be
  // exposed to JS), we need to tell the traversal that it is OK to
  // skip restyling, rather than panic when trying to unwrap the styles
  // it expects to have just computed.

  TraversalRootBehavior rootBehavior =
    MOZ_UNLIKELY(!aElement->HasServoData())
      ? TraversalRootBehavior::Normal
      : TraversalRootBehavior::UnstyledChildrenOnly;

  PrepareAndTraverseSubtree(aElement,
                            rootBehavior,
                            TraversalRestyleBehavior::ForNewlyBoundElement);
}

void
ServoStyleSet::StyleSubtreeForReconstruct(Element* aRoot)
{
  PreTraverse(aRoot);

  DebugOnly<bool> postTraversalRequired =
    PrepareAndTraverseSubtree(aRoot,
                              TraversalRootBehavior::Normal,
                              TraversalRestyleBehavior::ForReconstruct);
  MOZ_ASSERT(!postTraversalRequired);
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
                                   const ServoComputedValues* aComputedValues,
                                   nsTArray<Keyframe>& aKeyframes)
{
  UpdateStylistIfNeeded();

  NS_ConvertUTF16toUTF8 name(aName);
  return Servo_StyleSet_GetKeyframesForName(mRawSet.get(),
                                            &name,
                                            &aTimingFunction,
                                            aComputedValues,
                                            &aKeyframes);
}

nsTArray<ComputedKeyframeValues>
ServoStyleSet::GetComputedKeyframeValuesFor(
  const nsTArray<Keyframe>& aKeyframes,
  Element* aElement,
  ServoComputedValuesBorrowed aComputedValues)
{
  nsTArray<ComputedKeyframeValues> result(aKeyframes.Length());

  // Construct each nsTArray<PropertyStyleAnimationValuePair> here.
  result.AppendElements(aKeyframes.Length());

  Servo_GetComputedKeyframeValues(&aKeyframes,
                                  aElement,
                                  aComputedValues,
                                  mRawSet.get(),
                                  &result);
  return result;
}

void
ServoStyleSet::GetAnimationValues(
  RawServoDeclarationBlock* aDeclarations,
  Element* aElement,
  ServoComputedValuesBorrowed aComputedValues,
  nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues)
{
  Servo_GetAnimationValues(aDeclarations,
                           aElement,
                           aComputedValues,
                           mRawSet.get(),
                           &aAnimationValues);
}

already_AddRefed<ServoComputedValues>
ServoStyleSet::GetBaseComputedValuesForElement(Element* aElement,
                                               CSSPseudoElementType aPseudoType)
{
  return Servo_StyleSet_GetBaseComputedValuesForElement(mRawSet.get(),
                                                        aElement,
                                                        &Snapshots(),
                                                        aPseudoType).Consume();
}

already_AddRefed<RawServoAnimationValue>
ServoStyleSet::ComputeAnimationValue(
  Element* aElement,
  RawServoDeclarationBlock* aDeclarations,
  ServoComputedValuesBorrowed aComputedValues)
{
  return Servo_AnimationValue_Compute(aElement,
                                      aDeclarations,
                                      aComputedValues,
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
ServoStyleSet::RebuildData()
{
  ClearNonInheritingStyleContexts();
  Servo_StyleSet_RebuildData(mRawSet.get());
}

void
ServoStyleSet::ClearDataAndMarkDeviceDirty()
{
  ClearNonInheritingStyleContexts();
  Servo_StyleSet_Clear(mRawSet.get());
  mStylistState |= StylistState::FullyDirty;
}

void
ServoStyleSet::CompatibilityModeChanged()
{
  Servo_StyleSet_CompatModeChanged(mRawSet.get());
}

already_AddRefed<ServoComputedValues>
ServoStyleSet::ResolveServoStyle(Element* aElement)
{
  UpdateStylistIfNeeded();
  return Servo_ResolveStyle(aElement, mRawSet.get(),
                            mAllowResolveStaleStyles).Consume();
}

void
ServoStyleSet::ClearNonInheritingStyleContexts()
{
  for (RefPtr<nsStyleContext>& ptr : mNonInheritingStyleContexts) {
    ptr = nullptr;
  }
}

already_AddRefed<ServoComputedValues>
ServoStyleSet::ResolveStyleLazily(Element* aElement,
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

  RefPtr<ServoComputedValues> computedValues =
    Servo_ResolveStyleLazily(elementForStyleResolution,
                             pseudoTypeForStyleResolution,
                             aRuleInclusion,
                             &Snapshots(),
                             mRawSet.get()).Consume();

  if (mPresContext->EffectCompositor()->PreTraverse(aElement, aPseudoType)) {
    computedValues =
      Servo_ResolveStyleLazily(elementForStyleResolution,
                               pseudoTypeForStyleResolution,
                               aRuleInclusion,
                               &Snapshots(),
                               mRawSet.get()).Consume();
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

already_AddRefed<ServoComputedValues>
ServoStyleSet::ResolveForDeclarations(
  ServoComputedValuesBorrowedOrNull aParentOrNull,
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
  if (mStylistState & StylistState::FullyDirty) {
    RebuildData();

    if (mStylistState & StylistState::StyleSheetsDirty) {
      // Normally, whoever was in charge of posting a RebuildAllStyleDataEvent,
      // would also be in charge of posting a restyle/change hint according to
      // it.
      //
      // However, other stylesheets may have been added to the document in the
      // same period, so when both bits are set, we need to do a full subtree
      // update, because we can no longer reason about the state of the style
      // data.
      //
      // We could not clear the invalidations when rebuilding the data and
      // process them here... But it's not clear if that complexity is worth
      // to handle this edge case more efficiently.
      if (Element* root = mPresContext->Document()->GetDocumentElement()) {
        Servo_NoteExplicitHints(root, eRestyle_Subtree, nsChangeHint(0));
      }
    }
  } else {
    MOZ_ASSERT(mStylistState & StylistState::StyleSheetsDirty);
    Element* root = mPresContext->Document()->GetDocumentElement();
    Servo_StyleSet_FlushStyleSheets(mRawSet.get(), root);
  }
  mStylistState = StylistState::NotDirty;
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

ServoStyleSet* ServoStyleSet::sInServoTraversal = nullptr;
