/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/KeyframeUtils.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Move.h"
#include "mozilla/RangedArray.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/TimingParams.h"
#include "mozilla/dom/BaseKeyframeTypesBinding.h" // For FastBaseKeyframe etc.
#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h" // For PropertyValuesPair etc.
#include "jsapi.h" // For ForOfIterator etc.
#include "nsClassHashtable.h"
#include "nsCSSParser.h"
#include "nsCSSPropertyIDSet.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h" // For CSSPseudoElementType
#include "nsTArray.h"
#include <algorithm> // For std::stable_sort

namespace mozilla {

// ------------------------------------------------------------------
//
// Internal data types
//
// ------------------------------------------------------------------

// This is used while calculating paced spacing. If the keyframe is not pacable,
// we set its cumulative distance to kNotPaceable, so we can use this to check.
const double kNotPaceable = -1.0;

// For the aAllowList parameter of AppendStringOrStringSequence and
// GetPropertyValuesPairs.
enum class ListAllowance { eDisallow, eAllow };

/**
 * A comparator to sort nsCSSPropertyID values such that longhands are sorted
 * before shorthands, and shorthands with fewer components are sorted before
 * shorthands with more components.
 *
 * Using this allows us to prioritize values specified by longhands (or smaller
 * shorthand subsets) when longhands and shorthands are both specified
 * on the one keyframe.
 *
 * Example orderings that result from this:
 *
 *   margin-left, margin
 *
 * and:
 *
 *   border-top-color, border-color, border-top, border
 */
class PropertyPriorityComparator
{
public:
  PropertyPriorityComparator()
    : mSubpropertyCountInitialized(false) {}

  bool Equals(nsCSSPropertyID aLhs, nsCSSPropertyID aRhs) const
  {
    return aLhs == aRhs;
  }

  bool LessThan(nsCSSPropertyID aLhs,
                nsCSSPropertyID aRhs) const
  {
    bool isShorthandLhs = nsCSSProps::IsShorthand(aLhs);
    bool isShorthandRhs = nsCSSProps::IsShorthand(aRhs);

    if (isShorthandLhs) {
      if (isShorthandRhs) {
        // First, sort shorthands by the number of longhands they have.
        uint32_t subpropCountLhs = SubpropertyCount(aLhs);
        uint32_t subpropCountRhs = SubpropertyCount(aRhs);
        if (subpropCountLhs != subpropCountRhs) {
          return subpropCountLhs < subpropCountRhs;
        }
        // Otherwise, sort by IDL name below.
      } else {
        // Put longhands before shorthands.
        return false;
      }
    } else {
      if (isShorthandRhs) {
        // Put longhands before shorthands.
        return true;
      }
    }
    // For two longhand properties, or two shorthand with the same number
    // of longhand components, sort by IDL name.
    return nsCSSProps::PropertyIDLNameSortPosition(aLhs) <
           nsCSSProps::PropertyIDLNameSortPosition(aRhs);
  }

  uint32_t SubpropertyCount(nsCSSPropertyID aProperty) const
  {
    if (!mSubpropertyCountInitialized) {
      PodZero(&mSubpropertyCount);
      mSubpropertyCountInitialized = true;
    }
    if (mSubpropertyCount[aProperty] == 0) {
      uint32_t count = 0;
      CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
          p, aProperty, CSSEnabledState::eForAllContent) {
        ++count;
      }
      mSubpropertyCount[aProperty] = count;
    }
    return mSubpropertyCount[aProperty];
  }

private:
  // Cache of shorthand subproperty counts.
  mutable RangedArray<
    uint32_t,
    eCSSProperty_COUNT_no_shorthands,
    eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands> mSubpropertyCount;
  mutable bool mSubpropertyCountInitialized;
};

/**
 * Adaptor for PropertyPriorityComparator to sort objects which have
 * a mProperty member.
 */
template <typename T>
class TPropertyPriorityComparator : PropertyPriorityComparator
{
public:
  bool Equals(const T& aLhs, const T& aRhs) const
  {
    return PropertyPriorityComparator::Equals(aLhs.mProperty, aRhs.mProperty);
  }
  bool LessThan(const T& aLhs, const T& aRhs) const
  {
    return PropertyPriorityComparator::LessThan(aLhs.mProperty, aRhs.mProperty);
  }
};

/**
 * Iterator to walk through a PropertyValuePair array using the ordering
 * provided by PropertyPriorityComparator.
 */
class PropertyPriorityIterator
{
public:
  explicit PropertyPriorityIterator(
    const nsTArray<PropertyValuePair>& aProperties)
    : mProperties(aProperties)
  {
    mSortedPropertyIndices.SetCapacity(mProperties.Length());
    for (size_t i = 0, len = mProperties.Length(); i < len; ++i) {
      PropertyAndIndex propertyIndex = { mProperties[i].mProperty, i };
      mSortedPropertyIndices.AppendElement(propertyIndex);
    }
    mSortedPropertyIndices.Sort(PropertyAndIndex::Comparator());
  }

  class Iter
  {
  public:
    explicit Iter(const PropertyPriorityIterator& aParent)
      : mParent(aParent)
      , mIndex(0) { }

    static Iter EndIter(const PropertyPriorityIterator &aParent)
    {
      Iter iter(aParent);
      iter.mIndex = aParent.mSortedPropertyIndices.Length();
      return iter;
    }

    bool operator!=(const Iter& aOther) const
    {
      return mIndex != aOther.mIndex;
    }

    Iter& operator++()
    {
      MOZ_ASSERT(mIndex + 1 <= mParent.mSortedPropertyIndices.Length(),
                 "Should not seek past end iterator");
      mIndex++;
      return *this;
    }

    const PropertyValuePair& operator*()
    {
      MOZ_ASSERT(mIndex < mParent.mSortedPropertyIndices.Length(),
                 "Should not try to dereference an end iterator");
      return mParent.mProperties[mParent.mSortedPropertyIndices[mIndex].mIndex];
    }

  private:
    const PropertyPriorityIterator& mParent;
    size_t mIndex;
  };

  Iter begin() { return Iter(*this); }
  Iter end()   { return Iter::EndIter(*this); }

private:
  struct PropertyAndIndex
  {
    nsCSSPropertyID mProperty;
    size_t mIndex; // Index of mProperty within mProperties

    typedef TPropertyPriorityComparator<PropertyAndIndex> Comparator;
  };

  const nsTArray<PropertyValuePair>& mProperties;
  nsTArray<PropertyAndIndex> mSortedPropertyIndices;
};

/**
 * A property-values pair obtained from the open-ended properties
 * discovered on a regular keyframe or property-indexed keyframe object.
 *
 * Single values (as required by a regular keyframe, and as also supported
 * on property-indexed keyframes) are stored as the only element in
 * mValues.
 */
struct PropertyValuesPair
{
  nsCSSPropertyID mProperty;
  nsTArray<nsString> mValues;

  typedef TPropertyPriorityComparator<PropertyValuesPair> Comparator;
};

/**
 * An additional property (for a property-values pair) found on a
 * BaseKeyframe or BasePropertyIndexedKeyframe object.
 */
struct AdditionalProperty
{
  nsCSSPropertyID mProperty;
  size_t mJsidIndex;        // Index into |ids| in GetPropertyValuesPairs.

  struct PropertyComparator
  {
    bool Equals(const AdditionalProperty& aLhs,
                const AdditionalProperty& aRhs) const
    {
      return aLhs.mProperty == aRhs.mProperty;
    }
    bool LessThan(const AdditionalProperty& aLhs,
                  const AdditionalProperty& aRhs) const
    {
      return nsCSSProps::PropertyIDLNameSortPosition(aLhs.mProperty) <
             nsCSSProps::PropertyIDLNameSortPosition(aRhs.mProperty);
    }
  };
};

/**
 * Data for a segment in a keyframe animation of a given property
 * whose value is a StyleAnimationValue.
 *
 * KeyframeValueEntry is used in GetAnimationPropertiesFromKeyframes
 * to gather data for each individual segment.
 */
struct KeyframeValueEntry
{
  nsCSSPropertyID mProperty;
  StyleAnimationValue mValue;
  float mOffset;
  Maybe<ComputedTimingFunction> mTimingFunction;

  struct PropertyOffsetComparator
  {
    static bool Equals(const KeyframeValueEntry& aLhs,
                       const KeyframeValueEntry& aRhs)
    {
      return aLhs.mProperty == aRhs.mProperty &&
             aLhs.mOffset == aRhs.mOffset;
    }
    static bool LessThan(const KeyframeValueEntry& aLhs,
                         const KeyframeValueEntry& aRhs)
    {
      // First, sort by property IDL name.
      int32_t order = nsCSSProps::PropertyIDLNameSortPosition(aLhs.mProperty) -
                      nsCSSProps::PropertyIDLNameSortPosition(aRhs.mProperty);
      if (order != 0) {
        return order < 0;
      }

      // Then, by offset.
      return aLhs.mOffset < aRhs.mOffset;
    }
  };
};

class ComputedOffsetComparator
{
public:
  static bool Equals(const Keyframe& aLhs, const Keyframe& aRhs)
  {
    return aLhs.mComputedOffset == aRhs.mComputedOffset;
  }

  static bool LessThan(const Keyframe& aLhs, const Keyframe& aRhs)
  {
    return aLhs.mComputedOffset < aRhs.mComputedOffset;
  }
};

// ------------------------------------------------------------------
//
// Inlined helper methods
//
// ------------------------------------------------------------------

inline bool
IsInvalidValuePair(const PropertyValuePair& aPair, StyleBackendType aBackend)
{
  if (aBackend == StyleBackendType::Servo) {
    return !aPair.mServoDeclarationBlock;
  }

  // There are three types of values we store as token streams:
  //
  // * Shorthand values (where we manually extract the token stream's string
  //   value) and pass that along to various parsing methods
  // * Longhand values with variable references
  // * Invalid values
  //
  // We can distinguish between the last two cases because for invalid values
  // we leave the token stream's mPropertyID as eCSSProperty_UNKNOWN.
  return !nsCSSProps::IsShorthand(aPair.mProperty) &&
         aPair.mValue.GetUnit() == eCSSUnit_TokenStream &&
         aPair.mValue.GetTokenStreamValue()->mPropertyID
           == eCSSProperty_UNKNOWN;
}


// ------------------------------------------------------------------
//
// Internal helper method declarations
//
// ------------------------------------------------------------------

static void
GetKeyframeListFromKeyframeSequence(JSContext* aCx,
                                    nsIDocument* aDocument,
                                    JS::ForOfIterator& aIterator,
                                    nsTArray<Keyframe>& aResult,
                                    ErrorResult& aRv);

static bool
ConvertKeyframeSequence(JSContext* aCx,
                        nsIDocument* aDocument,
                        JS::ForOfIterator& aIterator,
                        nsTArray<Keyframe>& aResult);

static bool
GetPropertyValuesPairs(JSContext* aCx,
                       JS::Handle<JSObject*> aObject,
                       ListAllowance aAllowLists,
                       nsTArray<PropertyValuesPair>& aResult);

static bool
AppendStringOrStringSequenceToArray(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue,
                                    ListAllowance aAllowLists,
                                    nsTArray<nsString>& aValues);

static bool
AppendValueAsString(JSContext* aCx,
                    nsTArray<nsString>& aValues,
                    JS::Handle<JS::Value> aValue);

static PropertyValuePair
MakePropertyValuePair(nsCSSPropertyID aProperty, const nsAString& aStringValue,
                      nsCSSParser& aParser, nsIDocument* aDocument);

static bool
HasValidOffsets(const nsTArray<Keyframe>& aKeyframes);

static void
MarkAsComputeValuesFailureKey(PropertyValuePair& aPair);

static bool
IsComputeValuesFailureKey(const PropertyValuePair& aPair);

static void
BuildSegmentsFromValueEntries(nsTArray<KeyframeValueEntry>& aEntries,
                              nsTArray<AnimationProperty>& aResult);

static void
GetKeyframeListFromPropertyIndexedKeyframe(JSContext* aCx,
                                           nsIDocument* aDocument,
                                           JS::Handle<JS::Value> aValue,
                                           nsTArray<Keyframe>& aResult,
                                           ErrorResult& aRv);

static bool
RequiresAdditiveAnimation(const nsTArray<Keyframe>& aKeyframes,
                          nsIDocument* aDocument);

static void
DistributeRange(const Range<Keyframe>& aSpacingRange,
                const Range<Keyframe>& aRangeToAdjust);

static void
DistributeRange(const Range<Keyframe>& aSpacingRange);

static void
PaceRange(const Range<Keyframe>& aKeyframes,
          const Range<double>& aCumulativeDistances);

static nsTArray<double>
GetCumulativeDistances(const nsTArray<ComputedKeyframeValues>& aValues,
                       nsCSSPropertyID aProperty,
                       nsStyleContext* aStyleContext);

// ------------------------------------------------------------------
//
// Public API
//
// ------------------------------------------------------------------

/* static */ nsTArray<Keyframe>
KeyframeUtils::GetKeyframesFromObject(JSContext* aCx,
                                      nsIDocument* aDocument,
                                      JS::Handle<JSObject*> aFrames,
                                      ErrorResult& aRv)
{
  MOZ_ASSERT(!aRv.Failed());

  nsTArray<Keyframe> keyframes;

  if (!aFrames) {
    // The argument was explicitly null meaning no keyframes.
    return keyframes;
  }

  // At this point we know we have an object. We try to convert it to a
  // sequence of keyframes first, and if that fails due to not being iterable,
  // we try to convert it to a property-indexed keyframe.
  JS::Rooted<JS::Value> objectValue(aCx, JS::ObjectValue(*aFrames));
  JS::ForOfIterator iter(aCx);
  if (!iter.init(objectValue, JS::ForOfIterator::AllowNonIterable)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return keyframes;
  }

  if (iter.valueIsIterable()) {
    GetKeyframeListFromKeyframeSequence(aCx, aDocument, iter, keyframes, aRv);
  } else {
    GetKeyframeListFromPropertyIndexedKeyframe(aCx, aDocument, objectValue,
                                               keyframes, aRv);
  }

  if (aRv.Failed()) {
    MOZ_ASSERT(keyframes.IsEmpty(),
               "Should not set any keyframes when there is an error");
    return keyframes;
  }

  // We currently don't support additive animation. However, Web Animations
  // says that if you don't have a keyframe at offset 0 or 1, then you should
  // synthesize one using an additive zero value when you go to compose style.
  // Until we implement additive animations we just throw if we encounter any
  // set of keyframes that would put us in that situation.

  if (RequiresAdditiveAnimation(keyframes, aDocument)) {
    aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
    keyframes.Clear();
  }

  return keyframes;
}

/* static */ void
KeyframeUtils::ApplySpacing(nsTArray<Keyframe>& aKeyframes,
                            SpacingMode aSpacingMode,
                            nsCSSPropertyID aProperty,
                            nsTArray<ComputedKeyframeValues>& aComputedValues,
                            nsStyleContext* aStyleContext)
{
  if (aKeyframes.IsEmpty()) {
    return;
  }

  nsTArray<double> cumulativeDistances;
  if (aSpacingMode == SpacingMode::paced) {
    MOZ_ASSERT(IsAnimatableProperty(aProperty),
               "Paced property should be animatable");

    cumulativeDistances = GetCumulativeDistances(aComputedValues, aProperty,
                                                 aStyleContext);
    // Reset the computed offsets if using paced spacing.
    for (Keyframe& keyframe : aKeyframes) {
      keyframe.mComputedOffset = Keyframe::kComputedOffsetNotSet;
    }
  }

  // If the first keyframe has an unspecified offset, fill it in with 0%.
  // If there is only a single keyframe, then it gets 100%.
  if (aKeyframes.Length() > 1) {
    Keyframe& firstElement = aKeyframes[0];
    firstElement.mComputedOffset = firstElement.mOffset.valueOr(0.0);
    // We will fill in the last keyframe's offset below
  } else {
    Keyframe& lastElement = aKeyframes.LastElement();
    lastElement.mComputedOffset = lastElement.mOffset.valueOr(1.0);
  }

  // Fill in remaining missing offsets.
  const Keyframe* const last = aKeyframes.cend() - 1;
  const RangedPtr<Keyframe> begin(aKeyframes.begin(), aKeyframes.Length());
  RangedPtr<Keyframe> keyframeA = begin;
  while (keyframeA != last) {
    // Find keyframe A and keyframe B *between* which we will apply spacing.
    RangedPtr<Keyframe> keyframeB = keyframeA + 1;
    while (keyframeB->mOffset.isNothing() && keyframeB != last) {
      ++keyframeB;
    }
    keyframeB->mComputedOffset = keyframeB->mOffset.valueOr(1.0);

    // Fill computed offsets in (keyframe A, keyframe B).
    if (aSpacingMode == SpacingMode::distribute) {
      DistributeRange(Range<Keyframe>(keyframeA, keyframeB + 1));
    } else {
      // a) Find Paced A (first paceable keyframe) and
      //    Paced B (last paceable keyframe) in [keyframe A, keyframe B].
      RangedPtr<Keyframe> pacedA = keyframeA;
      while (pacedA < keyframeB &&
             cumulativeDistances[pacedA - begin] == kNotPaceable) {
        ++pacedA;
      }
      RangedPtr<Keyframe> pacedB = keyframeB;
      while (pacedB > keyframeA &&
             cumulativeDistances[pacedB - begin] == kNotPaceable) {
        --pacedB;
      }
      // As spec says, if there is no paceable keyframe
      // in [keyframe A, keyframe B], we let Paced A and Paced B refer to
      // keyframe B.
      if (pacedA > pacedB) {
        pacedA = pacedB = keyframeB;
      }
      // b) Apply distributing offsets in (keyframe A, Paced A] and
      //    [Paced B, keyframe B).
      DistributeRange(Range<Keyframe>(keyframeA, keyframeB + 1),
                      Range<Keyframe>(keyframeA + 1, pacedA + 1));
      DistributeRange(Range<Keyframe>(keyframeA, keyframeB + 1),
                      Range<Keyframe>(pacedB, keyframeB));
      // c) Apply paced offsets to each paceable keyframe in (Paced A, Paced B).
      //    We pass the range [Paced A, Paced B] since PaceRange needs the end
      //    points of the range in order to calculate the correct offset.
      PaceRange(Range<Keyframe>(pacedA, pacedB + 1),
                Range<double>(&cumulativeDistances[pacedA - begin],
                              pacedB - pacedA + 1));
      // d) Fill in any computed offsets in (Paced A, Paced B) that are still
      //    not set (e.g. because the keyframe was not paceable, or because the
      //    cumulative distance between paceable properties was zero).
      for (RangedPtr<Keyframe> frame = pacedA + 1; frame < pacedB; ++frame) {
        if (frame->mComputedOffset != Keyframe::kComputedOffsetNotSet) {
          continue;
        }

        RangedPtr<Keyframe> start = frame - 1;
        RangedPtr<Keyframe> end = frame + 1;
        while (end < pacedB &&
               end->mComputedOffset == Keyframe::kComputedOffsetNotSet) {
          ++end;
        }
        DistributeRange(Range<Keyframe>(start, end + 1));
        frame = end;
      }
    }
    keyframeA = keyframeB;
  }
}

/* static */ void
KeyframeUtils::ApplyDistributeSpacing(nsTArray<Keyframe>& aKeyframes)
{
  nsTArray<ComputedKeyframeValues> emptyArray;
  ApplySpacing(aKeyframes, SpacingMode::distribute, eCSSProperty_UNKNOWN,
               emptyArray, nullptr);
}

/* static */ nsTArray<ComputedKeyframeValues>
KeyframeUtils::GetComputedKeyframeValues(const nsTArray<Keyframe>& aKeyframes,
                                         dom::Element* aElement,
                                         nsStyleContext* aStyleContext)
{
  MOZ_ASSERT(aStyleContext);
  MOZ_ASSERT(aElement);

  StyleBackendType styleBackend = aElement->OwnerDoc()->GetStyleBackendType();

  const size_t len = aKeyframes.Length();
  nsTArray<ComputedKeyframeValues> result(len);

  for (const Keyframe& frame : aKeyframes) {
    nsCSSPropertyIDSet propertiesOnThisKeyframe;
    ComputedKeyframeValues* computedValues = result.AppendElement();
    for (const PropertyValuePair& pair :
           PropertyPriorityIterator(frame.mPropertyValues)) {
      MOZ_ASSERT(!pair.mServoDeclarationBlock ||
                 styleBackend == StyleBackendType::Servo,
                 "Animation values were parsed using Servo backend but target"
                 " element is not using Servo backend?");

      if (IsInvalidValuePair(pair, styleBackend)) {
        continue;
      }

      // Expand each value into the set of longhands and produce
      // a KeyframeValueEntry for each value.
      nsTArray<PropertyStyleAnimationValuePair> values;

      if (styleBackend == StyleBackendType::Servo) {
        if (!StyleAnimationValue::ComputeValues(pair.mProperty,
              CSSEnabledState::eForAllContent, aStyleContext,
              *pair.mServoDeclarationBlock, values)) {
          continue;
        }
      } else {
        // For shorthands, we store the string as a token stream so we need to
        // extract that first.
        if (nsCSSProps::IsShorthand(pair.mProperty)) {
          nsCSSValueTokenStream* tokenStream = pair.mValue.GetTokenStreamValue();
          if (!StyleAnimationValue::ComputeValues(pair.mProperty,
                CSSEnabledState::eForAllContent, aElement, aStyleContext,
                tokenStream->mTokenStream, /* aUseSVGMode */ false, values) ||
              IsComputeValuesFailureKey(pair)) {
            continue;
          }
        } else {
          if (!StyleAnimationValue::ComputeValues(pair.mProperty,
                CSSEnabledState::eForAllContent, aElement, aStyleContext,
                pair.mValue, /* aUseSVGMode */ false, values)) {
            continue;
          }
          MOZ_ASSERT(values.Length() == 1,
                    "Longhand properties should produce a single"
                    " StyleAnimationValue");
        }
      }

      for (auto& value : values) {
        // If we already got a value for this property on the keyframe,
        // skip this one.
        if (propertiesOnThisKeyframe.HasProperty(value.mProperty)) {
          continue;
        }
        computedValues->AppendElement(value);
        propertiesOnThisKeyframe.AddProperty(value.mProperty);
      }
    }
  }

  MOZ_ASSERT(result.Length() == aKeyframes.Length(), "Array length mismatch");
  return result;
}

/* static */ nsTArray<AnimationProperty>
KeyframeUtils::GetAnimationPropertiesFromKeyframes(
  const nsTArray<Keyframe>& aKeyframes,
  const nsTArray<ComputedKeyframeValues>& aComputedValues,
  nsStyleContext* aStyleContext)
{
  MOZ_ASSERT(aKeyframes.Length() == aComputedValues.Length(),
             "Array length mismatch");

  nsTArray<KeyframeValueEntry> entries(aKeyframes.Length());

  const size_t len = aKeyframes.Length();
  for (size_t i = 0; i < len; ++i) {
    const Keyframe& frame = aKeyframes[i];
    for (auto& value : aComputedValues[i]) {
      MOZ_ASSERT(frame.mComputedOffset != Keyframe::kComputedOffsetNotSet,
                 "Invalid computed offset");
      KeyframeValueEntry* entry = entries.AppendElement();
      entry->mOffset = frame.mComputedOffset;
      entry->mProperty = value.mProperty;
      entry->mValue = value.mValue;
      entry->mTimingFunction = frame.mTimingFunction;
    }
  }

  nsTArray<AnimationProperty> result;
  BuildSegmentsFromValueEntries(entries, result);
  return result;
}

/* static */ bool
KeyframeUtils::IsAnimatableProperty(nsCSSPropertyID aProperty)
{
  if (aProperty == eCSSProperty_UNKNOWN) {
    return false;
  }

  if (!nsCSSProps::IsShorthand(aProperty)) {
    return nsCSSProps::kAnimTypeTable[aProperty] != eStyleAnimType_None;
  }

  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, aProperty,
                                       CSSEnabledState::eForAllContent) {
    if (nsCSSProps::kAnimTypeTable[*subprop] != eStyleAnimType_None) {
      return true;
    }
  }

  return false;
}

// ------------------------------------------------------------------
//
// Internal helpers
//
// ------------------------------------------------------------------

/**
 * Converts a JS object to an IDL sequence<Keyframe>.
 *
 * @param aCx The JSContext corresponding to |aIterator|.
 * @param aDocument The document to use when parsing CSS properties.
 * @param aIterator An already-initialized ForOfIterator for the JS
 *   object to iterate over as a sequence.
 * @param aResult The array into which the resulting Keyframe objects will be
 *   appended.
 * @param aRv Out param to store any errors thrown by this function.
 */
static void
GetKeyframeListFromKeyframeSequence(JSContext* aCx,
                                    nsIDocument* aDocument,
                                    JS::ForOfIterator& aIterator,
                                    nsTArray<Keyframe>& aResult,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(!aRv.Failed());
  MOZ_ASSERT(aResult.IsEmpty());

  // Convert the object in aIterator to a sequence of keyframes producing
  // an array of Keyframe objects.
  if (!ConvertKeyframeSequence(aCx, aDocument, aIterator, aResult)) {
    aRv.Throw(NS_ERROR_FAILURE);
    aResult.Clear();
    return;
  }

  // If the sequence<> had zero elements, we won't generate any
  // keyframes.
  if (aResult.IsEmpty()) {
    return;
  }

  // Check that the keyframes are loosely sorted and with values all
  // between 0% and 100%.
  if (!HasValidOffsets(aResult)) {
    aRv.ThrowTypeError<dom::MSG_INVALID_KEYFRAME_OFFSETS>();
    aResult.Clear();
    return;
  }
}

/**
 * Converts a JS object wrapped by the given JS::ForIfIterator to an
 * IDL sequence<Keyframe> and stores the resulting Keyframe objects in
 * aResult.
 */
static bool
ConvertKeyframeSequence(JSContext* aCx,
                        nsIDocument* aDocument,
                        JS::ForOfIterator& aIterator,
                        nsTArray<Keyframe>& aResult)
{
  JS::Rooted<JS::Value> value(aCx);
  nsCSSParser parser(aDocument->CSSLoader());

  for (;;) {
    bool done;
    if (!aIterator.next(&value, &done)) {
      return false;
    }
    if (done) {
      break;
    }
    // Each value found when iterating the object must be an object
    // or null/undefined (which gets treated as a default {} dictionary
    // value).
    if (!value.isObject() && !value.isNullOrUndefined()) {
      dom::ThrowErrorMessage(aCx, dom::MSG_NOT_OBJECT,
                             "Element of sequence<Keyframe> argument");
      return false;
    }

    // Convert the JS value into a BaseKeyframe dictionary value.
    dom::binding_detail::FastBaseKeyframe keyframeDict;
    if (!keyframeDict.Init(aCx, value,
                           "Element of sequence<Keyframe> argument")) {
      return false;
    }

    Keyframe* keyframe = aResult.AppendElement(fallible);
    if (!keyframe) {
      return false;
    }
    if (!keyframeDict.mOffset.IsNull()) {
      keyframe->mOffset.emplace(keyframeDict.mOffset.Value());
    }

    ErrorResult rv;
    keyframe->mTimingFunction =
      TimingParams::ParseEasing(keyframeDict.mEasing, aDocument, rv);
    if (rv.MaybeSetPendingException(aCx)) {
      return false;
    }

    // Look for additional property-values pairs on the object.
    nsTArray<PropertyValuesPair> propertyValuePairs;
    if (value.isObject()) {
      JS::Rooted<JSObject*> object(aCx, &value.toObject());
      if (!GetPropertyValuesPairs(aCx, object,
                                  ListAllowance::eDisallow,
                                  propertyValuePairs)) {
        return false;
      }
    }

    for (PropertyValuesPair& pair : propertyValuePairs) {
      MOZ_ASSERT(pair.mValues.Length() == 1);
      keyframe->mPropertyValues.AppendElement(
        MakePropertyValuePair(pair.mProperty, pair.mValues[0], parser,
                              aDocument));

      // When we go to convert keyframes into arrays of property values we
      // call StyleAnimation::ComputeValues. This should normally return true
      // but in order to test the case where it does not, BaseKeyframeDict
      // includes a chrome-only member that can be set to indicate that
      // ComputeValues should fail for shorthand property values on that
      // keyframe.
      if (nsCSSProps::IsShorthand(pair.mProperty) &&
          keyframeDict.mSimulateComputeValuesFailure) {
        MarkAsComputeValuesFailureKey(keyframe->mPropertyValues.LastElement());
      }
    }
  }

  return true;
}

/**
 * Reads the property-values pairs from the specified JS object.
 *
 * @param aObject The JS object to look at.
 * @param aAllowLists If eAllow, values will be converted to
 *   (DOMString or sequence<DOMString); if eDisallow, values
 *   will be converted to DOMString.
 * @param aResult The array into which the enumerated property-values
 *   pairs will be stored.
 * @return false on failure or JS exception thrown while interacting
 *   with aObject; true otherwise.
 */
static bool
GetPropertyValuesPairs(JSContext* aCx,
                       JS::Handle<JSObject*> aObject,
                       ListAllowance aAllowLists,
                       nsTArray<PropertyValuesPair>& aResult)
{
  nsTArray<AdditionalProperty> properties;

  // Iterate over all the properties on aObject and append an
  // entry to properties for them.
  //
  // We don't compare the jsids that we encounter with those for
  // the explicit dictionary members, since we know that none
  // of the CSS property IDL names clash with them.
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, aObject, &ids)) {
    return false;
  }
  for (size_t i = 0, n = ids.length(); i < n; i++) {
    nsAutoJSString propName;
    if (!propName.init(aCx, ids[i])) {
      return false;
    }
    nsCSSPropertyID property =
      nsCSSProps::LookupPropertyByIDLName(propName,
                                          CSSEnabledState::eForAllContent);
    if (KeyframeUtils::IsAnimatableProperty(property)) {
      AdditionalProperty* p = properties.AppendElement();
      p->mProperty = property;
      p->mJsidIndex = i;
    }
  }

  // Sort the entries by IDL name and then get each value and
  // convert it either to a DOMString or to a
  // (DOMString or sequence<DOMString>), depending on aAllowLists,
  // and build up aResult.
  properties.Sort(AdditionalProperty::PropertyComparator());

  for (AdditionalProperty& p : properties) {
    JS::Rooted<JS::Value> value(aCx);
    if (!JS_GetPropertyById(aCx, aObject, ids[p.mJsidIndex], &value)) {
      return false;
    }
    PropertyValuesPair* pair = aResult.AppendElement();
    pair->mProperty = p.mProperty;
    if (!AppendStringOrStringSequenceToArray(aCx, value, aAllowLists,
                                             pair->mValues)) {
      return false;
    }
  }

  return true;
}

/**
 * Converts aValue to DOMString, if aAllowLists is eDisallow, or
 * to (DOMString or sequence<DOMString>) if aAllowLists is aAllow.
 * The resulting strings are appended to aValues.
 */
static bool
AppendStringOrStringSequenceToArray(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue,
                                    ListAllowance aAllowLists,
                                    nsTArray<nsString>& aValues)
{
  if (aAllowLists == ListAllowance::eAllow && aValue.isObject()) {
    // The value is an object, and we want to allow lists; convert
    // aValue to (DOMString or sequence<DOMString>).
    JS::ForOfIterator iter(aCx);
    if (!iter.init(aValue, JS::ForOfIterator::AllowNonIterable)) {
      return false;
    }
    if (iter.valueIsIterable()) {
      // If the object is iterable, convert it to sequence<DOMString>.
      JS::Rooted<JS::Value> element(aCx);
      for (;;) {
        bool done;
        if (!iter.next(&element, &done)) {
          return false;
        }
        if (done) {
          break;
        }
        if (!AppendValueAsString(aCx, aValues, element)) {
          return false;
        }
      }
      return true;
    }
  }

  // Either the object is not iterable, or aAllowLists doesn't want
  // a list; convert it to DOMString.
  if (!AppendValueAsString(aCx, aValues, aValue)) {
    return false;
  }

  return true;
}

/**
 * Converts aValue to DOMString and appends it to aValues.
 */
static bool
AppendValueAsString(JSContext* aCx,
                    nsTArray<nsString>& aValues,
                    JS::Handle<JS::Value> aValue)
{
  return ConvertJSValueToString(aCx, aValue, dom::eStringify, dom::eStringify,
                                *aValues.AppendElement());
}

/**
 * Construct a PropertyValuePair parsing the given string into a suitable
 * nsCSSValue object.
 *
 * @param aProperty The CSS property.
 * @param aStringValue The property value to parse.
 * @param aParser The CSS parser object to use.
 * @param aDocument The document to use when parsing.
 * @return The constructed PropertyValuePair object.
 */
static PropertyValuePair
MakePropertyValuePair(nsCSSPropertyID aProperty, const nsAString& aStringValue,
                      nsCSSParser& aParser, nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);
  PropertyValuePair result;

  result.mProperty = aProperty;

  if (aDocument->GetStyleBackendType() == StyleBackendType::Servo) {
    nsCString name = nsCSSProps::GetStringValue(aProperty);

    NS_ConvertUTF16toUTF8 value(aStringValue);
    RefPtr<ThreadSafeURIHolder> base =
      new ThreadSafeURIHolder(aDocument->GetDocumentURI());
    RefPtr<ThreadSafeURIHolder> referrer =
      new ThreadSafeURIHolder(aDocument->GetDocumentURI());
    RefPtr<ThreadSafePrincipalHolder> principal =
      new ThreadSafePrincipalHolder(aDocument->NodePrincipal());

    nsCString baseString;
    aDocument->GetDocumentURI()->GetSpec(baseString);

    RefPtr<RawServoDeclarationBlock> servoDeclarationBlock =
      Servo_ParseProperty(&name, &value, &baseString,
                          base, referrer, principal).Consume();

    if (servoDeclarationBlock) {
      result.mServoDeclarationBlock = servoDeclarationBlock.forget();
      return result;
    }
  }

  nsCSSValue value;
  if (!nsCSSProps::IsShorthand(aProperty)) {
    aParser.ParseLonghandProperty(aProperty,
                                  aStringValue,
                                  aDocument->GetDocumentURI(),
                                  aDocument->GetDocumentURI(),
                                  aDocument->NodePrincipal(),
                                  value);
  }

  if (value.GetUnit() == eCSSUnit_Null) {
    // Either we have a shorthand, or we failed to parse a longhand.
    // In either case, store the string value as a token stream.
    nsCSSValueTokenStream* tokenStream = new nsCSSValueTokenStream;
    tokenStream->mTokenStream = aStringValue;

    // We are about to convert a null value to a token stream value but
    // by leaving the mPropertyID as unknown, we will be able to
    // distinguish between invalid values and valid token stream values
    // (e.g. values with variable references).
    MOZ_ASSERT(tokenStream->mPropertyID == eCSSProperty_UNKNOWN,
               "The property of a token stream should be initialized"
               " to unknown");

    // By leaving mShorthandPropertyID as unknown, we ensure that when
    // we call nsCSSValue::AppendToString we get back the string stored
    // in mTokenStream.
    MOZ_ASSERT(tokenStream->mShorthandPropertyID == eCSSProperty_UNKNOWN,
               "The shorthand property of a token stream should be initialized"
               " to unknown");
    value.SetTokenStreamValue(tokenStream);
  } else {
    // If we succeeded in parsing with Gecko, but not Servo the animation is
    // not going to work since, for the purposes of animation, we're going to
    // ignore |mValue| when the backend is Servo.
    NS_WARNING_ASSERTION(aDocument->GetStyleBackendType() !=
                           StyleBackendType::Servo,
                         "Gecko succeeded in parsing where Servo failed");
  }

  result.mValue = value;

  return result;
}

/**
 * Checks that the given keyframes are loosely ordered (each keyframe's
 * offset that is not null is greater than or equal to the previous
 * non-null offset) and that all values are within the range [0.0, 1.0].
 *
 * @return true if the keyframes' offsets are correctly ordered and
 *   within range; false otherwise.
 */
static bool
HasValidOffsets(const nsTArray<Keyframe>& aKeyframes)
{
  double offset = 0.0;
  for (const Keyframe& keyframe : aKeyframes) {
    if (keyframe.mOffset) {
      double thisOffset = keyframe.mOffset.value();
      if (thisOffset < offset || thisOffset > 1.0f) {
        return false;
      }
      offset = thisOffset;
    }
  }
  return true;
}

/**
 * Takes a property-value pair for a shorthand property and modifies the
 * value to indicate that when we call StyleAnimationValue::ComputeValues on
 * that value we should behave as if that function had failed.
 *
 * @param aPair The PropertyValuePair to modify. |aPair.mProperty| must be
 *              a shorthand property.
 */
static void
MarkAsComputeValuesFailureKey(PropertyValuePair& aPair)
{
  MOZ_ASSERT(nsCSSProps::IsShorthand(aPair.mProperty),
             "Only shorthand property values can be marked as failure values");

  // We store shorthand values as nsCSSValueTokenStream objects whose mProperty
  // and mShorthandPropertyID are eCSSProperty_UNKNOWN and whose mTokenStream
  // member contains the shorthand property's value as a string.
  //
  // We need to leave mShorthandPropertyID as eCSSProperty_UNKNOWN so that
  // nsCSSValue::AppendToString returns the mTokenStream value, but we can
  // update mPropertyID to a special value to indicate that this is
  // a special failure sentinel.
  nsCSSValueTokenStream* tokenStream = aPair.mValue.GetTokenStreamValue();
  MOZ_ASSERT(tokenStream->mPropertyID == eCSSProperty_UNKNOWN,
             "Shorthand value should initially have an unknown property ID");
  tokenStream->mPropertyID = eCSSPropertyExtra_no_properties;
}

/**
 * Returns true if |aPair| is a property-value pair on which we have
 * previously called MarkAsComputeValuesFailureKey (and hence we should
 * simulate failure when calling StyleAnimationValue::ComputeValues using its
 * value).
 *
 * @param aPair The property-value pair to test.
 * @return True if |aPair| represents a failure value.
 */
static bool
IsComputeValuesFailureKey(const PropertyValuePair& aPair)
{
  return nsCSSProps::IsShorthand(aPair.mProperty) &&
         aPair.mValue.GetTokenStreamValue()->mPropertyID ==
           eCSSPropertyExtra_no_properties;
}

/**
 * Builds an array of AnimationProperty objects to represent the keyframe
 * animation segments in aEntries.
 */
static void
BuildSegmentsFromValueEntries(nsTArray<KeyframeValueEntry>& aEntries,
                              nsTArray<AnimationProperty>& aResult)
{
  if (aEntries.IsEmpty()) {
    return;
  }

  // Sort the KeyframeValueEntry objects so that all entries for a given
  // property are together, and the entries are sorted by offset otherwise.
  std::stable_sort(aEntries.begin(), aEntries.end(),
                   &KeyframeValueEntry::PropertyOffsetComparator::LessThan);

  // For a given index i, we want to generate a segment from aEntries[i]
  // to aEntries[j], if:
  //
  //   * j > i,
  //   * aEntries[i + 1]'s offset/property is different from aEntries[i]'s, and
  //   * aEntries[j - 1]'s offset/property is different from aEntries[j]'s.
  //
  // That will eliminate runs of same offset/property values where there's no
  // point generating zero length segments in the middle of the animation.
  //
  // Additionally we need to generate a zero length segment at offset 0 and at
  // offset 1, if we have multiple values for a given property at that offset,
  // since we need to retain the very first and very last value so they can
  // be used for reverse and forward filling.
  //
  // Typically, for each property in |aEntries|, we expect there to be at least
  // one KeyframeValueEntry with offset 0.0, and at least one with offset 1.0.
  // However, since it is possible that when building |aEntries|, the call to
  // StyleAnimationValue::ComputeValues might fail, this can't be guaranteed.
  // Furthermore, since we don't yet implement additive animation and hence
  // don't have sensible fallback behavior when these values are missing, the
  // following loop takes care to identify properties that lack a value at
  // offset 0.0/1.0 and drops those properties from |aResult|.

  nsCSSPropertyID lastProperty = eCSSProperty_UNKNOWN;
  AnimationProperty* animationProperty = nullptr;

  size_t i = 0, n = aEntries.Length();

  while (i < n) {
    // Check that the last property ends with an entry at offset 1.
    if (i + 1 == n) {
      if (aEntries[i].mOffset != 1.0f && animationProperty) {
        aResult.RemoveElementAt(aResult.Length() - 1);
        animationProperty = nullptr;
      }
      break;
    }

    MOZ_ASSERT(aEntries[i].mProperty != eCSSProperty_UNKNOWN &&
               aEntries[i + 1].mProperty != eCSSProperty_UNKNOWN,
               "Each entry should specify a valid property");

    // Skip properties that don't have an entry with offset 0.
    if (aEntries[i].mProperty != lastProperty &&
        aEntries[i].mOffset != 0.0f) {
      // Since the entries are sorted by offset for a given property, and
      // since we don't update |lastProperty|, we will keep hitting this
      // condition until we change property.
      ++i;
      continue;
    }

    // Drop properties that don't end with an entry with offset 1.
    if (aEntries[i].mProperty != aEntries[i + 1].mProperty &&
        aEntries[i].mOffset != 1.0f) {
      if (animationProperty) {
        aResult.RemoveElementAt(aResult.Length() - 1);
        animationProperty = nullptr;
      }
      ++i;
      continue;
    }

    // Starting from i, determine the next [i, j] interval from which to
    // generate a segment.
    size_t j;
    if (aEntries[i].mOffset == 0.0f && aEntries[i + 1].mOffset == 0.0f) {
      // We need to generate an initial zero-length segment.
      MOZ_ASSERT(aEntries[i].mProperty == aEntries[i + 1].mProperty);
      j = i + 1;
      while (aEntries[j + 1].mOffset == 0.0f &&
             aEntries[j + 1].mProperty == aEntries[j].mProperty) {
        ++j;
      }
    } else if (aEntries[i].mOffset == 1.0f) {
      if (aEntries[i + 1].mOffset == 1.0f &&
          aEntries[i + 1].mProperty == aEntries[i].mProperty) {
        // We need to generate a final zero-length segment.
        j = i + 1;
        while (j + 1 < n &&
               aEntries[j + 1].mOffset == 1.0f &&
               aEntries[j + 1].mProperty == aEntries[j].mProperty) {
          ++j;
        }
      } else {
        // New property.
        MOZ_ASSERT(aEntries[i].mProperty != aEntries[i + 1].mProperty);
        animationProperty = nullptr;
        ++i;
        continue;
      }
    } else {
      while (aEntries[i].mOffset == aEntries[i + 1].mOffset &&
             aEntries[i].mProperty == aEntries[i + 1].mProperty) {
        ++i;
      }
      j = i + 1;
    }

    // If we've moved on to a new property, create a new AnimationProperty
    // to insert segments into.
    if (aEntries[i].mProperty != lastProperty) {
      MOZ_ASSERT(aEntries[i].mOffset == 0.0f);
      MOZ_ASSERT(!animationProperty);
      animationProperty = aResult.AppendElement();
      animationProperty->mProperty = aEntries[i].mProperty;
      lastProperty = aEntries[i].mProperty;
    }

    MOZ_ASSERT(animationProperty, "animationProperty should be valid pointer.");

    // Now generate the segment.
    AnimationPropertySegment* segment =
      animationProperty->mSegments.AppendElement();
    segment->mFromKey   = aEntries[i].mOffset;
    segment->mToKey     = aEntries[j].mOffset;
    segment->mFromValue = aEntries[i].mValue;
    segment->mToValue   = aEntries[j].mValue;
    segment->mTimingFunction = aEntries[i].mTimingFunction;

    i = j;
  }
}

/**
 * Converts a JS object representing a property-indexed keyframe into
 * an array of Keyframe objects.
 *
 * @param aCx The JSContext for |aValue|.
 * @param aDocument The document to use when parsing CSS properties.
 * @param aValue The JS object.
 * @param aResult The array into which the resulting AnimationProperty
 *   objects will be appended.
 * @param aRv Out param to store any errors thrown by this function.
 */
static void
GetKeyframeListFromPropertyIndexedKeyframe(JSContext* aCx,
                                           nsIDocument* aDocument,
                                           JS::Handle<JS::Value> aValue,
                                           nsTArray<Keyframe>& aResult,
                                           ErrorResult& aRv)
{
  MOZ_ASSERT(aValue.isObject());
  MOZ_ASSERT(aResult.IsEmpty());
  MOZ_ASSERT(!aRv.Failed());

  // Convert the object to a property-indexed keyframe dictionary to
  // get its explicit dictionary members.
  dom::binding_detail::FastBasePropertyIndexedKeyframe keyframeDict;
  if (!keyframeDict.Init(aCx, aValue, "BasePropertyIndexedKeyframe argument",
                         false)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  Maybe<ComputedTimingFunction> easing =
    TimingParams::ParseEasing(keyframeDict.mEasing, aDocument, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Get all the property--value-list pairs off the object.
  JS::Rooted<JSObject*> object(aCx, &aValue.toObject());
  nsTArray<PropertyValuesPair> propertyValuesPairs;
  if (!GetPropertyValuesPairs(aCx, object, ListAllowance::eAllow,
                              propertyValuesPairs)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Create a set of keyframes for each property.
  nsCSSParser parser(aDocument->CSSLoader());
  nsClassHashtable<nsFloatHashKey, Keyframe> processedKeyframes;
  for (const PropertyValuesPair& pair : propertyValuesPairs) {
    size_t count = pair.mValues.Length();
    if (count == 0) {
      // No animation values for this property.
      continue;
    }
    if (count == 1) {
      // We don't support additive values and so can't support an
      // animation that goes from the underlying value to this
      // specified value.  Throw an exception until we do support this.
      aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
      return;
    }

    size_t n = pair.mValues.Length() - 1;
    size_t i = 0;

    for (const nsString& stringValue : pair.mValues) {
      double offset = i++ / double(n);
      Keyframe* keyframe = processedKeyframes.LookupOrAdd(offset);
      if (keyframe->mPropertyValues.IsEmpty()) {
        keyframe->mTimingFunction = easing;
        keyframe->mComputedOffset = offset;
      }
      keyframe->mPropertyValues.AppendElement(
        MakePropertyValuePair(pair.mProperty, stringValue, parser, aDocument));
    }
  }

  aResult.SetCapacity(processedKeyframes.Count());
  for (auto iter = processedKeyframes.Iter(); !iter.Done(); iter.Next()) {
    aResult.AppendElement(Move(*iter.UserData()));
  }

  aResult.Sort(ComputedOffsetComparator());
}

/**
 * Returns true if the supplied set of keyframes has keyframe values for
 * any property for which it does not also supply a value for the 0% and 100%
 * offsets. In this case we are supposed to synthesize an additive zero value
 * but since we don't support additive animation yet we can't support this
 * case. We try to detect that here so we can throw an exception. The check is
 * not entirely accurate but should detect most common cases.
 *
 * @param aKeyframes The set of keyframes to analyze.
 * @param aDocument The document to use when parsing keyframes so we can
 *   try to detect where we have an invalid value at 0%/100%.
 */
static bool
RequiresAdditiveAnimation(const nsTArray<Keyframe>& aKeyframes,
                          nsIDocument* aDocument)
{
  // We are looking to see if that every property referenced in |aKeyframes|
  // has a valid property at offset 0.0 and 1.0. The check as to whether a
  // property is valid or not, however, is not precise. We only check if the
  // property can be parsed, NOT whether it can also be converted to a
  // StyleAnimationValue since doing that requires a target element bound to
  // a document which we might not always have at the point where we want to
  // perform this check.
  //
  // This is only a temporary measure until we implement additive animation.
  // So as long as this check catches most cases, and we don't do anything
  // horrible in one of the cases we can't detect, it should be sufficient.

  nsCSSPropertyIDSet properties;              // All properties encountered.
  nsCSSPropertyIDSet propertiesWithFromValue; // Those with a defined 0% value.
  nsCSSPropertyIDSet propertiesWithToValue;   // Those with a defined 100% value.

  auto addToPropertySets = [&](nsCSSPropertyID aProperty, double aOffset) {
    properties.AddProperty(aProperty);
    if (aOffset == 0.0) {
      propertiesWithFromValue.AddProperty(aProperty);
    } else if (aOffset == 1.0) {
      propertiesWithToValue.AddProperty(aProperty);
    }
  };

  StyleBackendType styleBackend = aDocument->GetStyleBackendType();

  for (size_t i = 0, len = aKeyframes.Length(); i < len; i++) {
    const Keyframe& frame = aKeyframes[i];

    // We won't have called ApplySpacing when this is called so
    // we can't use frame.mComputedOffset. Instead we do a rough version
    // of that algorithm that substitutes null offsets with 0.0 for the first
    // frame, 1.0 for the last frame, and 0.5 for everything else.
    double computedOffset = i == len - 1
                            ? 1.0
                            : i == 0 ? 0.0 : 0.5;
    double offsetToUse = frame.mOffset
                         ? frame.mOffset.value()
                         : computedOffset;

    for (const PropertyValuePair& pair : frame.mPropertyValues) {
      if (IsInvalidValuePair(pair, styleBackend)) {
        continue;
      }

      if (nsCSSProps::IsShorthand(pair.mProperty)) {
        if (styleBackend == StyleBackendType::Gecko) {
          nsCSSValueTokenStream* tokenStream =
            pair.mValue.GetTokenStreamValue();
          nsCSSParser parser(aDocument->CSSLoader());
          if (!parser.IsValueValidForProperty(pair.mProperty,
                                              tokenStream->mTokenStream)) {
            continue;
          }
        }
        // For the Servo backend, invalid shorthand values are represented by
        // a null mServoDeclarationBlock member which we skip above in
        // IsInvalidValuePair.
        MOZ_ASSERT(styleBackend != StyleBackendType::Servo ||
                   pair.mServoDeclarationBlock);
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
            prop, pair.mProperty, CSSEnabledState::eForAllContent) {
          addToPropertySets(*prop, offsetToUse);
        }
      } else {
        addToPropertySets(pair.mProperty, offsetToUse);
      }
    }
  }

  return !propertiesWithFromValue.Equals(properties) ||
         !propertiesWithToValue.Equals(properties);
}

/**
 * Evenly distribute the computed offsets in (A, B).
 * We pass the range keyframes in [A, B] and use A, B to calculate distributing
 * computed offsets in (A, B). The second range, aRangeToAdjust, is passed, so
 * we can know which keyframe we want to apply to. aRangeToAdjust should be in
 * the range of aSpacingRange.
 *
 * @param aSpacingRange The sequence of keyframes between whose endpoints we
 *   should apply distribute spacing.
 * @param aRangeToAdjust The range of keyframes we want to apply to.
 */
static void
DistributeRange(const Range<Keyframe>& aSpacingRange,
                const Range<Keyframe>& aRangeToAdjust)
{
  MOZ_ASSERT(aRangeToAdjust.begin() >= aSpacingRange.begin() &&
             aRangeToAdjust.end() <= aSpacingRange.end(),
             "Out of range");
  const size_t n = aSpacingRange.length() - 1;
  const double startOffset = aSpacingRange[0].mComputedOffset;
  const double diffOffset = aSpacingRange[n].mComputedOffset - startOffset;
  for (auto iter = aRangeToAdjust.begin();
       iter != aRangeToAdjust.end();
       ++iter) {
    size_t index = iter - aSpacingRange.begin();
    iter->mComputedOffset = startOffset + double(index) / n * diffOffset;
  }
}

/**
 * Overload of DistributeRange to apply distribute spacing to all keyframes in
 * between the endpoints of the given range.
 *
 * @param aSpacingRange The sequence of keyframes between whose endpoints we
 *   should apply distribute spacing.
 */
static void
DistributeRange(const Range<Keyframe>& aSpacingRange)
{
  // We don't need to apply distribute spacing to keyframe A and keyframe B.
  DistributeRange(aSpacingRange,
                  Range<Keyframe>(aSpacingRange.begin() + 1,
                                  aSpacingRange.end() - 1));
}

/**
 * Apply paced spacing to all paceable keyframes in between the endpoints of the
 * given range.
 *
 * @param aKeyframes The range of keyframes between whose endpoints we should
 *   apply paced spacing. Both endpoints should be paceable, i.e. the
 *   corresponding elements in |aCumulativeDist| should not be kNotPaceable.
 *   Within this function, we refer to the start and end points of this range
 *   as Paced A and Paced B respectively in keeping with the notation used in
 *   the spec.
 * @param aCumulativeDistances The sequence of cumulative distances of the paced
 *   property as returned by GetCumulativeDistances(). This acts as a
 *   parallel range to |aKeyframes|.
 */
static void
PaceRange(const Range<Keyframe>& aKeyframes,
          const Range<double>& aCumulativeDistances)
{
  MOZ_ASSERT(aKeyframes.length() == aCumulativeDistances.length(),
             "Range length mismatch");

  const size_t len = aKeyframes.length();
  // If there is nothing between the end points, there is nothing to space.
  if (len < 3) {
    return;
  }

  const double distA = *(aCumulativeDistances.begin());
  const double distB = *(aCumulativeDistances.end() - 1);
  MOZ_ASSERT(distA != kNotPaceable && distB != kNotPaceable,
             "Both Paced A and Paced B should be paceable");

  // If the total distance is zero, we should fall back to distribute spacing.
  // The caller will fill-in any keyframes without a computed offset using
  // distribute spacing so we can just return here.
  if (distA == distB) {
    return;
  }

  const RangedPtr<Keyframe> pacedA = aKeyframes.begin();
  const RangedPtr<Keyframe> pacedB = aKeyframes.end() - 1;
  MOZ_ASSERT(pacedA->mComputedOffset != Keyframe::kComputedOffsetNotSet &&
             pacedB->mComputedOffset != Keyframe::kComputedOffsetNotSet,
             "Both Paced A and Paced B should have valid computed offsets");

  // Apply computed offset.
  const double offsetA     = pacedA->mComputedOffset;
  const double diffOffset  = pacedB->mComputedOffset - offsetA;
  const double initialDist = distA;
  const double totalDist   = distB - initialDist;
  for (auto iter = pacedA + 1; iter != pacedB; ++iter) {
    size_t k = iter - aKeyframes.begin();
    if (aCumulativeDistances[k] == kNotPaceable) {
      continue;
    }

    double dist = aCumulativeDistances[k] - initialDist;
    iter->mComputedOffset = offsetA + diffOffset * dist / totalDist;
  }
}

/**
 * Get cumulative distances for the paced property.
 *
 * @param aValues The computed values returned by GetComputedKeyframeValues.
 * @param aPacedProperty The paced property.
 * @param aStyleContext The style context for computing distance on transform.
 * @return The cumulative distances for the paced property. The length will be
 *   the same as aValues.
 */
static nsTArray<double>
GetCumulativeDistances(const nsTArray<ComputedKeyframeValues>& aValues,
                       nsCSSPropertyID aPacedProperty,
                       nsStyleContext* aStyleContext)
{
  // a) If aPacedProperty is a shorthand property, get its components.
  //    Otherwise, just add the longhand property into the set.
  size_t pacedPropertyCount = 0;
  nsCSSPropertyIDSet pacedPropertySet;
  bool isShorthand = nsCSSProps::IsShorthand(aPacedProperty);
  if (isShorthand) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPacedProperty,
                                         CSSEnabledState::eForAllContent) {
      pacedPropertySet.AddProperty(*p);
      ++pacedPropertyCount;
    }
  } else {
    pacedPropertySet.AddProperty(aPacedProperty);
    pacedPropertyCount = 1;
  }

  // b) Search each component (shorthand) or the longhand property, and
  //    calculate the cumulative distances of paceable keyframe pairs.
  const size_t len = aValues.Length();
  nsTArray<double> cumulativeDistances(len);
  // cumulativeDistances is a parallel array to |aValues|, so set its length to
  // the length of |aValues|.
  cumulativeDistances.SetLength(len);
  ComputedKeyframeValues prevPacedValues;
  size_t preIdx = 0;
  for (size_t i = 0; i < len; ++i) {
    // Find computed values of the paced property.
    ComputedKeyframeValues pacedValues;
    for (const PropertyStyleAnimationValuePair& pair : aValues[i]) {
      if (pacedPropertySet.HasProperty(pair.mProperty)) {
        pacedValues.AppendElement(pair);
      }
    }

    // Check we have values for all the paceable longhand components.
    if (pacedValues.Length() != pacedPropertyCount) {
      // This keyframe is not paceable, assign kNotPaceable and skip it.
      cumulativeDistances[i] = kNotPaceable;
      continue;
    }

    // Sort the pacedValues first, so the order of subproperties of
    // pacedValues is always the same as that of prevPacedValues.
    if (isShorthand) {
      pacedValues.Sort(
        TPropertyPriorityComparator<PropertyStyleAnimationValuePair>());
    }

    if (prevPacedValues.IsEmpty()) {
      // This is the first paceable keyframe so its cumulative distance is 0.0.
      cumulativeDistances[i] = 0.0;
    } else {
      double dist = 0.0;
      if (isShorthand) {
        // Apply the distance by the square root of the sum of squares of
        // longhand component distances.
        for (size_t propIdx = 0; propIdx < pacedPropertyCount; ++propIdx) {
          nsCSSPropertyID prop = prevPacedValues[propIdx].mProperty;
          MOZ_ASSERT(pacedValues[propIdx].mProperty == prop,
                     "Property mismatch");

          double componentDistance = 0.0;
          if (StyleAnimationValue::ComputeDistance(
                prop,
                prevPacedValues[propIdx].mValue,
                pacedValues[propIdx].mValue,
                aStyleContext,
                componentDistance)) {
            dist += componentDistance * componentDistance;
          }
        }
        dist = sqrt(dist);
      } else {
        // If the property is longhand, we just use the 1st value.
        // If ComputeDistance() fails, |dist| will remain zero so there will be
        // no distance between the previous paced value and this value.
        Unused <<
          StyleAnimationValue::ComputeDistance(aPacedProperty,
                                               prevPacedValues[0].mValue,
                                               pacedValues[0].mValue,
                                               aStyleContext,
                                               dist);
      }
      cumulativeDistances[i] = cumulativeDistances[preIdx] + dist;
    }
    prevPacedValues.SwapElements(pacedValues);
    preIdx = i;
  }
  return cumulativeDistances;
}

} // namespace mozilla
