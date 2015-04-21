/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_AnimationCommon_h
#define mozilla_css_AnimationCommon_h

#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "nsRefreshDriver.h"
#include "prclist.h"
#include "nsChangeHint.h"
#include "nsCSSProperty.h"
#include "nsDisplayList.h" // For nsDisplayItem::Type
#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Nullable.h"
#include "nsStyleStruct.h"
#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "nsCSSPseudoElements.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCSSPropertySet.h"

class nsIFrame;
class nsPresContext;
class nsStyleChangeList;

namespace mozilla {

class RestyleTracker;
struct AnimationCollection;

namespace css {

bool IsGeometricProperty(nsCSSProperty aProperty);

class CommonAnimationManager : public nsIStyleRuleProcessor,
                               public nsARefreshObserver {
public:
  explicit CommonAnimationManager(nsPresContext *aPresContext);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIStyleRuleProcessor (parts)
  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) override;
  virtual nsRestyleHint HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData) override;
  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) override;
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData) override;
  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) override;
  virtual void RulesMatching(ElementRuleProcessorData* aData) override;
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) override;
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) override;
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) override;
#endif
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;

#ifdef DEBUG
  static void Initialize();
#endif

  // NOTE:  This can return null after Disconnect().
  nsPresContext* PresContext() const { return mPresContext; }

  /**
   * Notify the manager that the pres context is going away.
   */
  void Disconnect();

  // Tell the restyle tracker about all the styles that we're currently
  // animating, so that it can update the animation rule for these
  // elements.
  void AddStyleUpdatesTo(mozilla::RestyleTracker& aTracker);

  AnimationCollection*
  GetAnimations(dom::Element *aElement,
                nsCSSPseudoElements::Type aPseudoType,
                bool aCreateIfNeeded);

  // Returns true if aContent or any of its ancestors has an animation
  // or transition.
  static bool ContentOrAncestorHasAnimation(nsIContent* aContent) {
    do {
      if (aContent->GetProperty(nsGkAtoms::animationsProperty) ||
          aContent->GetProperty(nsGkAtoms::transitionsProperty)) {
        return true;
      }
    } while ((aContent = aContent->GetParent()));

    return false;
  }

  // Notify this manager that one of its collections of animation players,
  // has been updated.
  void NotifyCollectionUpdated(AnimationCollection& aCollection);

  enum FlushFlags {
    Can_Throttle,
    Cannot_Throttle
  };

  nsIStyleRule* GetAnimationRule(mozilla::dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType);

  static bool ExtractComputedValueForTransition(
                  nsCSSProperty aProperty,
                  nsStyleContext* aStyleContext,
                  mozilla::StyleAnimationValue& aComputedValue);

  // For CSS properties that may be animated on a separate layer, represents
  // a record of the corresponding layer type and change hint.
  struct LayerAnimationRecord {
    nsCSSProperty mProperty;
    nsDisplayItem::Type mLayerType;
    nsChangeHint mChangeHint;
  };

protected:
  static const size_t kLayerRecords = 2;

public:
  static const LayerAnimationRecord sLayerAnimationInfo[kLayerRecords];

  // Will return non-null for any property with the
  // CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR flag; should only be called
  // on such properties.
  static const LayerAnimationRecord*
    LayerAnimationRecordFor(nsCSSProperty aProperty);

protected:
  virtual ~CommonAnimationManager();

  // For ElementCollectionRemoved
  friend struct mozilla::AnimationCollection;

  void AddElementCollection(AnimationCollection* aCollection);
  void ElementCollectionRemoved() { MaybeStartOrStopObservingRefreshDriver(); }
  void RemoveAllElementCollections();

  // We should normally only call MaybeStartOrStopObservingRefreshDriver in
  // situations where we will also queue events since otherwise we may stop
  // getting refresh driver ticks before we queue the necessary events.
  void MaybeStartObservingRefreshDriver();
  void MaybeStartOrStopObservingRefreshDriver();
  bool NeedsRefresh() const;

  virtual nsIAtom* GetAnimationsAtom() = 0;
  virtual nsIAtom* GetAnimationsBeforeAtom() = 0;
  virtual nsIAtom* GetAnimationsAfterAtom() = 0;

  virtual bool IsAnimationManager() {
    return false;
  }

  static AnimationCollection*
  GetAnimationsForCompositor(nsIContent* aContent,
                             nsIAtom* aElementProperty,
                             nsCSSProperty aProperty);

  PRCList mElementCollections;
  nsPresContext *mPresContext; // weak (non-null from ctor to Disconnect)
  bool mIsObservingRefreshDriver;
};

/**
 * A style rule that maps property-StyleAnimationValue pairs.
 */
class AnimValuesStyleRule final : public nsIStyleRule
{
public:
  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // nsIStyleRule implementation
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

  void AddValue(nsCSSProperty aProperty,
                mozilla::StyleAnimationValue &aStartValue)
  {
    PropertyValuePair v = { aProperty, aStartValue };
    mPropertyValuePairs.AppendElement(v);
  }

  // Caller must fill in returned value.
  mozilla::StyleAnimationValue* AddEmptyValue(nsCSSProperty aProperty)
  {
    PropertyValuePair *p = mPropertyValuePairs.AppendElement();
    p->mProperty = aProperty;
    return &p->mValue;
  }

  struct PropertyValuePair {
    nsCSSProperty mProperty;
    mozilla::StyleAnimationValue mValue;
  };

  void AddPropertiesToSet(nsCSSPropertySet& aSet) const
  {
    for (size_t i = 0, i_end = mPropertyValuePairs.Length(); i < i_end; ++i) {
      const PropertyValuePair &cv = mPropertyValuePairs[i];
      aSet.AddProperty(cv.mProperty);
    }
  }

private:
  ~AnimValuesStyleRule() {}

  InfallibleTArray<PropertyValuePair> mPropertyValuePairs;
};

} /* end css sub-namespace */

typedef InfallibleTArray<nsRefPtr<dom::Animation>> AnimationPlayerPtrArray;

enum EnsureStyleRuleFlags {
  EnsureStyleRule_IsThrottled,
  EnsureStyleRule_IsNotThrottled
};

struct AnimationCollection : public PRCList
{
  AnimationCollection(dom::Element *aElement, nsIAtom *aElementProperty,
                      mozilla::css::CommonAnimationManager *aManager)
    : mElement(aElement)
    , mElementProperty(aElementProperty)
    , mManager(aManager)
    , mAnimationGeneration(0)
    , mCheckGeneration(0)
    , mNeedsRefreshes(true)
#ifdef DEBUG
    , mCalledPropertyDtor(false)
#endif
  {
    MOZ_COUNT_CTOR(AnimationCollection);
    PR_INIT_CLIST(this);
  }
  ~AnimationCollection()
  {
    MOZ_ASSERT(mCalledPropertyDtor,
               "must call destructor through element property dtor");
    MOZ_COUNT_DTOR(AnimationCollection);
    PR_REMOVE_LINK(this);
    mManager->ElementCollectionRemoved();
  }

  void Destroy()
  {
    for (size_t playerIdx = mPlayers.Length(); playerIdx-- != 0; ) {
      mPlayers[playerIdx]->Cancel();
    }
    // This will call our destructor.
    mElement->DeleteProperty(mElementProperty);
  }

  static void PropertyDtor(void *aObject, nsIAtom *aPropertyName,
                           void *aPropertyValue, void *aData);

  void Tick();

  void EnsureStyleRuleFor(TimeStamp aRefreshTime, EnsureStyleRuleFlags aFlags);

  bool CanThrottleTransformChanges(mozilla::TimeStamp aTime);

  bool CanThrottleAnimation(mozilla::TimeStamp aTime);

  enum CanAnimateFlags {
    // Testing for width, height, top, right, bottom, or left.
    CanAnimate_HasGeometricProperty = 1,
    // Allow the case where OMTA is allowed in general, but not for the
    // specified property.
    CanAnimate_AllowPartial = 2
  };

  static bool
  CanAnimatePropertyOnCompositor(const dom::Element *aElement,
                                 nsCSSProperty aProperty,
                                 CanAnimateFlags aFlags);

  static bool IsCompositorAnimationDisabledForFrame(nsIFrame* aFrame);

  // True if this animation can be performed on the compositor thread.
  //
  // If aFlags contains CanAnimate_AllowPartial, returns whether the
  // state of this element's animations at the current refresh driver
  // time contains animation data that can be done on the compositor
  // thread.  (This is useful for determining whether a layer should be
  // active, or whether to send data to the layer.)
  //
  // If aFlags does not contain CanAnimate_AllowPartial, returns whether
  // the state of this element's animations at the current refresh driver
  // time can be fully represented by data sent to the compositor.
  // (This is useful for determining whether throttle the animation
  // (suppress main-thread style updates).)
  bool CanPerformOnCompositorThread(CanAnimateFlags aFlags) const;

  void PostUpdateLayerAnimations();

  bool HasAnimationOfProperty(nsCSSProperty aProperty) const;

  bool IsForElement() const { // rather than for a pseudo-element
    return mElementProperty == nsGkAtoms::animationsProperty ||
           mElementProperty == nsGkAtoms::transitionsProperty;
  }

  bool IsForBeforePseudo() const {
    return mElementProperty == nsGkAtoms::animationsOfBeforeProperty ||
           mElementProperty == nsGkAtoms::transitionsOfBeforeProperty;
  }

  bool IsForAfterPseudo() const {
    return mElementProperty == nsGkAtoms::animationsOfAfterProperty ||
           mElementProperty == nsGkAtoms::transitionsOfAfterProperty;
  }

  bool IsForTransitions() const {
    return mElementProperty == nsGkAtoms::transitionsProperty ||
           mElementProperty == nsGkAtoms::transitionsOfBeforeProperty ||
           mElementProperty == nsGkAtoms::transitionsOfAfterProperty;
  }

  bool IsForAnimations() const {
    return mElementProperty == nsGkAtoms::animationsProperty ||
           mElementProperty == nsGkAtoms::animationsOfBeforeProperty ||
           mElementProperty == nsGkAtoms::animationsOfAfterProperty;
  }

  nsString PseudoElement() const
  {
    if (IsForElement()) {
      return EmptyString();
    }
    if (IsForBeforePseudo()) {
      return NS_LITERAL_STRING("::before");
    }
    MOZ_ASSERT(IsForAfterPseudo(),
               "::before & ::after should be the only pseudo-elements here");
    return NS_LITERAL_STRING("::after");
  }

  nsCSSPseudoElements::Type PseudoElementType() const
  {
    if (IsForElement()) {
      return nsCSSPseudoElements::ePseudo_NotPseudoElement;
    }
    if (IsForBeforePseudo()) {
      return nsCSSPseudoElements::ePseudo_before;
    }
    MOZ_ASSERT(IsForAfterPseudo(),
               "::before & ::after should be the only pseudo-elements here");
    return nsCSSPseudoElements::ePseudo_after;
  }

  mozilla::dom::Element* GetElementToRestyle() const;

  void PostRestyleForAnimation(nsPresContext *aPresContext) {
    mozilla::dom::Element* element = GetElementToRestyle();
    if (element) {
      nsRestyleHint hint = IsForTransitions() ? eRestyle_CSSTransitions
                                              : eRestyle_CSSAnimations;
      aPresContext->PresShell()->RestyleForAnimation(element, hint);
    }
  }

  void NotifyPlayerUpdated();

  static void LogAsyncAnimationFailure(nsCString& aMessage,
                                       const nsIContent* aContent = nullptr);

  dom::Element *mElement;

  // the atom we use in mElement's prop table (must be a static atom,
  // i.e., in an atom list)
  nsIAtom *mElementProperty;

  mozilla::css::CommonAnimationManager *mManager;

  mozilla::AnimationPlayerPtrArray mPlayers;

  // This style rule contains the style data for currently animating
  // values.  It only matches when styling with animation.  When we
  // style without animation, we need to not use it so that we can
  // detect any new changes; if necessary we restyle immediately
  // afterwards with animation.
  // NOTE: If we don't need to apply any styles, mStyleRule will be
  // null, but mStyleRuleRefreshTime will still be valid.
  nsRefPtr<mozilla::css::AnimValuesStyleRule> mStyleRule;

  // RestyleManager keeps track of the number of animation
  // 'mini-flushes' (see nsTransitionManager::UpdateAllThrottledStyles()).
  // mAnimationGeneration is the sequence number of the last flush where a
  // transition/animation changed.  We keep a similar count on the
  // corresponding layer so we can check that the layer is up to date with
  // the animation manager.
  uint64_t mAnimationGeneration;
  // Update mAnimationGeneration to nsCSSFrameConstructor's count
  void UpdateAnimationGeneration(nsPresContext* aPresContext);

  // For CSS transitions only, we also record the most recent generation
  // for which we've done the transition update, so that we avoid doing
  // it more than once per style change.  This should be greater than or
  // equal to mAnimationGeneration, except when the generation counter
  // cycles, or when animations are updated through the DOM Animation
  // interfaces.
  uint64_t mCheckGeneration;
  // Update mAnimationGeneration to nsCSSFrameConstructor's count
  void UpdateCheckGeneration(nsPresContext* aPresContext);

  // Returns true if there is an animation that has yet to finish.
  bool HasCurrentAnimations() const;
  // Returns true if there is an animation of any of the specified properties
  // that has yet to finish.
  bool HasCurrentAnimationsForProperties(const nsCSSProperty* aProperties,
                                         size_t aPropertyCount) const;

  // The refresh time associated with mStyleRule.
  TimeStamp mStyleRuleRefreshTime;

  // False when we know that our current style rule is valid
  // indefinitely into the future (because all of our animations are
  // either completed or paused).  May be invalidated by a style change.
  bool mNeedsRefreshes;

#ifdef DEBUG
  bool mCalledPropertyDtor;
#endif
};

}

#endif /* !defined(mozilla_css_AnimationCommon_h) */
