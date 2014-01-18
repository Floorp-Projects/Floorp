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
#include "nsStyleAnimation.h"
#include "nsCSSProperty.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "nsSMILKeySpline.h"
#include "nsStyleStruct.h"
#include "mozilla/Attributes.h"
#include "nsCSSPseudoElements.h"

class nsPresContext;
class nsIFrame;


namespace mozilla {
namespace css {

bool IsGeometricProperty(nsCSSProperty aProperty);

struct CommonElementAnimationData;

class CommonAnimationManager : public nsIStyleRuleProcessor,
                               public nsARefreshObserver {
public:
  CommonAnimationManager(nsPresContext *aPresContext);
  virtual ~CommonAnimationManager();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIStyleRuleProcessor (parts)
  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual nsRestyleHint HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) MOZ_OVERRIDE;
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;

  /**
   * Notify the manager that the pres context is going away.
   */
  void Disconnect();

  enum FlushFlags {
    Can_Throttle,
    Cannot_Throttle
  };

  static bool ExtractComputedValueForTransition(
                  nsCSSProperty aProperty,
                  nsStyleContext* aStyleContext,
                  nsStyleAnimation::Value& aComputedValue);
protected:
  friend struct CommonElementAnimationData; // for ElementDataRemoved

  virtual void AddElementData(CommonElementAnimationData* aData) = 0;
  virtual void ElementDataRemoved() = 0;
  void RemoveAllElementData();

  // Update the style on aElement from the transition stored in this manager and
  // the new parent style - aParentStyle. aElement must be transitioning or
  // animated. Returns the updated style.
  nsStyleContext* UpdateThrottledStyle(mozilla::dom::Element* aElement,
                                       nsStyleContext* aParentStyle,
                                       nsStyleChangeList &aChangeList);
  // Reparent the style of aContent and any :before and :after pseudo-elements.
  already_AddRefed<nsStyleContext> ReparentContent(nsIContent* aContent,
                                                  nsStyleContext* aParentStyle);
  // reparent :before and :after pseudo elements of aElement
  static void ReparentBeforeAndAfter(dom::Element* aElement,
                                     nsIFrame* aPrimaryFrame,
                                     nsStyleContext* aNewStyle,
                                     nsStyleSet* aStyleSet);

  PRCList mElementData;
  nsPresContext *mPresContext; // weak (non-null from ctor to Disconnect)
};

// The internals of UpdateAllThrottledStyles, used by nsAnimationManager and
// nsTransitionManager, see the comments in the declaration of the latter.
#define IMPL_UPDATE_ALL_THROTTLED_STYLES_INTERNAL(class_, animations_getter_)  \
void                                                                           \
class_::UpdateAllThrottledStylesInternal()                                     \
{                                                                              \
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();          \
                                                                               \
  nsStyleChangeList changeList;                                                \
                                                                               \
  /* update each transitioning element by finding its root-most ancestor
     with a transition, and flushing the style on that ancestor and all
     its descendants*/                                                         \
  PRCList *next = PR_LIST_HEAD(&mElementData);                                 \
  while (next != &mElementData) {                                              \
    CommonElementAnimationData* ea =                                           \
      static_cast<CommonElementAnimationData*>(next);                          \
    next = PR_NEXT_LINK(next);                                                 \
                                                                               \
    if (ea->mFlushGeneration == now) {                                         \
      /* this element has been ticked already */                               \
      continue;                                                                \
    }                                                                          \
                                                                               \
    /* element is initialised to the starting element (i.e., one we know has
       an animation) and ends up with the root-most animated ancestor,
       that is, the element where we begin updates. */                         \
    dom::Element* element = ea->mElement;                                      \
    /* make a list of ancestors */                                             \
    nsTArray<dom::Element*> ancestors;                                         \
    do {                                                                       \
      ancestors.AppendElement(element);                                        \
    } while ((element = element->GetParentElement()));                         \
                                                                               \
    /* walk down the ancestors until we find one with a throttled transition */\
    for (int32_t i = ancestors.Length() - 1; i >= 0; --i) {                    \
      if (animations_getter_(ancestors[i],                                     \
                            nsCSSPseudoElements::ePseudo_NotPseudoElement,     \
                            false)) {                                          \
        element = ancestors[i];                                                \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
    nsIFrame* primaryFrame;                                                    \
    if (element &&                                                             \
        (primaryFrame = nsLayoutUtils::GetStyleFrame(element))) {              \
      UpdateThrottledStylesForSubtree(element,                                 \
        primaryFrame->StyleContext()->GetParent(), changeList);                \
    }                                                                          \
  }                                                                            \
                                                                               \
  RestyleManager* restyleManager = mPresContext->RestyleManager();             \
  restyleManager->ProcessRestyledFrames(changeList);                           \
  restyleManager->FlushOverflowChangedTracker();                               \
}

/**
 * A style rule that maps property-nsStyleAnimation::Value pairs.
 */
class AnimValuesStyleRule MOZ_FINAL : public nsIStyleRule
{
public:
  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // nsIStyleRule implementation
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif

  void AddValue(nsCSSProperty aProperty, nsStyleAnimation::Value &aStartValue)
  {
    PropertyValuePair v = { aProperty, aStartValue };
    mPropertyValuePairs.AppendElement(v);
  }

  // Caller must fill in returned value.
  nsStyleAnimation::Value* AddEmptyValue(nsCSSProperty aProperty)
  {
    PropertyValuePair *p = mPropertyValuePairs.AppendElement();
    p->mProperty = aProperty;
    return &p->mValue;
  }

  struct PropertyValuePair {
    nsCSSProperty mProperty;
    nsStyleAnimation::Value mValue;
  };

private:
  InfallibleTArray<PropertyValuePair> mPropertyValuePairs;
};

class ComputedTimingFunction {
public:
  typedef nsTimingFunction::Type Type;
  void Init(const nsTimingFunction &aFunction);
  double GetValue(double aPortion) const;
  const nsSMILKeySpline* GetFunction() const {
    NS_ASSERTION(mType == nsTimingFunction::Function, "Type mismatch");
    return &mTimingFunction;
  }
  Type GetType() const { return mType; }
  uint32_t GetSteps() const { return mSteps; }
private:
  Type mType;
  nsSMILKeySpline mTimingFunction;
  uint32_t mSteps;
};

struct CommonElementAnimationData : public PRCList
{
  CommonElementAnimationData(dom::Element *aElement, nsIAtom *aElementProperty,
                             CommonAnimationManager *aManager, TimeStamp aNow)
    : mElement(aElement)
    , mElementProperty(aElementProperty)
    , mManager(aManager)
    , mAnimationGeneration(0)
    , mFlushGeneration(aNow)
#ifdef DEBUG
    , mCalledPropertyDtor(false)
#endif
  {
    MOZ_COUNT_CTOR(CommonElementAnimationData);
    PR_INIT_CLIST(this);
  }
  ~CommonElementAnimationData()
  {
    NS_ABORT_IF_FALSE(mCalledPropertyDtor,
                      "must call destructor through element property dtor");
    MOZ_COUNT_DTOR(CommonElementAnimationData);
    PR_REMOVE_LINK(this);
    mManager->ElementDataRemoved();
  }

  void Destroy()
  {
    // This will call our destructor.
    mElement->DeleteProperty(mElementProperty);
  }

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
  // Do not pass CanAnimate_AllowPartial to make sure that all properties of this
  // animation are supported by the compositor.
  virtual bool CanPerformOnCompositorThread(CanAnimateFlags aFlags) const = 0;
  virtual bool HasAnimationOfProperty(nsCSSProperty aProperty) const = 0;

  static void LogAsyncAnimationFailure(nsCString& aMessage,
                                       const nsIContent* aContent = nullptr);

  dom::Element *mElement;

  // the atom we use in mElement's prop table (must be a static atom,
  // i.e., in an atom list)
  nsIAtom *mElementProperty;

  CommonAnimationManager *mManager;

  // This style rule contains the style data for currently animating
  // values.  It only matches when styling with animation.  When we
  // style without animation, we need to not use it so that we can
  // detect any new changes; if necessary we restyle immediately
  // afterwards with animation.
  // NOTE: If we don't need to apply any styles, mStyleRule will be
  // null, but mStyleRuleRefreshTime will still be valid.
  nsRefPtr<mozilla::css::AnimValuesStyleRule> mStyleRule;

  // nsCSSFrameConstructor keeps track of the number of animation 'mini-flushes'
  // (see nsTransitionManager::UpdateAllThrottledStyles()). mFlushCount is
  // the last flush where a transition/animation changed. We keep a similar
  // count on the corresponding layer so we can check that the layer is up to
  // date with the animation manager.
  uint64_t mAnimationGeneration;
  // Update mFlushCount to nsCSSFrameConstructor's count
  void UpdateAnimationGeneration(nsPresContext* aPresContext);

  // The refresh time associated with mStyleRule.
  TimeStamp mStyleRuleRefreshTime;

  // Generation counter for flushes of throttled animations.
  // Used to prevent updating the styles twice for a given element during
  // UpdateAllThrottledStyles.
  TimeStamp mFlushGeneration;

#ifdef DEBUG
  bool mCalledPropertyDtor;
#endif
};

}
}

#endif /* !defined(mozilla_css_AnimationCommon_h) */
