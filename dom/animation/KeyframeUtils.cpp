/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/KeyframeUtils.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/Move.h"
#include "mozilla/RangedArray.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/TimingParams.h"
#include "mozilla/dom/BaseKeyframeTypesBinding.h" // For FastBaseKeyframe etc.
#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h" // For PropertyValuesPair etc.
#include "jsapi.h" // For ForOfIterator etc.
#include "nsClassHashtable.h"
#include "nsContentUtils.h" // For GetContextForContent, and
                            // AnimationsAPICoreEnabled
#include "nsCSSParser.h"
#include "nsCSSPropertyIDSet.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h" // For CSSPseudoElementType
#include "nsDocument.h" // For nsDocument::IsWebAnimationsEnabled
#include "nsIScriptError.h"
#include "nsStyleContext.h"
#include "nsTArray.h"
#include <algorithm> // For std::stable_sort, std::min

namespace mozilla {

// ------------------------------------------------------------------
//
// Internal data types
//
// ------------------------------------------------------------------

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
  AnimationValue mValue;

  float mOffset;
  Maybe<ComputedTimingFunction> mTimingFunction;
  dom::CompositeOperation mComposite;

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
                       StyleBackendType aBackend,
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

static Maybe<PropertyValuePair>
MakePropertyValuePair(nsCSSPropertyID aProperty, const nsAString& aStringValue,
                      nsCSSParser& aParser, nsIDocument* aDocument);

static bool
HasValidOffsets(const nsTArray<Keyframe>& aKeyframes);

#ifdef DEBUG
static void
MarkAsComputeValuesFailureKey(PropertyValuePair& aPair);

static bool
IsComputeValuesFailureKey(const PropertyValuePair& aPair);
#endif

static nsTArray<ComputedKeyframeValues>
GetComputedKeyframeValues(const nsTArray<Keyframe>& aKeyframes,
                          dom::Element* aElement,
                          GeckoStyleContext* aStyleContext);

static nsTArray<ComputedKeyframeValues>
GetComputedKeyframeValues(const nsTArray<Keyframe>& aKeyframes,
                          dom::Element* aElement,
                          const ServoStyleContext* aComputedValues);

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
DistributeRange(const Range<Keyframe>& aRange);

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

  if (!nsDocument::IsWebAnimationsEnabled(aCx, nullptr) &&
      RequiresAdditiveAnimation(keyframes, aDocument)) {
    keyframes.Clear();
    aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
  }

  return keyframes;
}

/* static */ void
KeyframeUtils::DistributeKeyframes(nsTArray<Keyframe>& aKeyframes)
{
  if (aKeyframes.IsEmpty()) {
    return;
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
  const Keyframe* const last = &aKeyframes.LastElement();
  const RangedPtr<Keyframe> begin(aKeyframes.Elements(), aKeyframes.Length());
  RangedPtr<Keyframe> keyframeA = begin;
  while (keyframeA != last) {
    // Find keyframe A and keyframe B *between* which we will apply spacing.
    RangedPtr<Keyframe> keyframeB = keyframeA + 1;
    while (keyframeB->mOffset.isNothing() && keyframeB != last) {
      ++keyframeB;
    }
    keyframeB->mComputedOffset = keyframeB->mOffset.valueOr(1.0);

    // Fill computed offsets in (keyframe A, keyframe B).
    DistributeRange(Range<Keyframe>(keyframeA, keyframeB + 1));
    keyframeA = keyframeB;
  }
}

template<typename StyleType>
/* static */ nsTArray<AnimationProperty>
KeyframeUtils::GetAnimationPropertiesFromKeyframes(
  const nsTArray<Keyframe>& aKeyframes,
  dom::Element* aElement,
  StyleType* aStyle,
  dom::CompositeOperation aEffectComposite)
{
  nsTArray<AnimationProperty> result;

  const nsTArray<ComputedKeyframeValues> computedValues =
    GetComputedKeyframeValues(aKeyframes, aElement, aStyle);
  if (computedValues.IsEmpty()) {
    // In rare cases GetComputedKeyframeValues might fail and return an empty
    // array, in which case we likewise return an empty array from here.
    return result;
  }

  MOZ_ASSERT(aKeyframes.Length() == computedValues.Length(),
             "Array length mismatch");

  nsTArray<KeyframeValueEntry> entries(aKeyframes.Length());

  const size_t len = aKeyframes.Length();
  for (size_t i = 0; i < len; ++i) {
    const Keyframe& frame = aKeyframes[i];
    for (auto& value : computedValues[i]) {
      MOZ_ASSERT(frame.mComputedOffset != Keyframe::kComputedOffsetNotSet,
                 "Invalid computed offset");
      KeyframeValueEntry* entry = entries.AppendElement();
      entry->mOffset = frame.mComputedOffset;
      entry->mProperty = value.mProperty;
      entry->mValue = value.mValue;
      entry->mTimingFunction = frame.mTimingFunction;
      entry->mComposite =
        frame.mComposite ? frame.mComposite.value() : aEffectComposite;
    }
  }

  BuildSegmentsFromValueEntries(entries, result);
  return result;
}

/* static */ bool
KeyframeUtils::IsAnimatableProperty(nsCSSPropertyID aProperty,
                                    StyleBackendType aBackend)
{
  // Regardless of the backend type, treat the 'display' property as not
  // animatable. (The Servo backend will report it as being animatable, since
  // it is in fact animatable by SMIL.)
  if (aProperty == eCSSProperty_display) {
    return false;
  }

  if (aBackend == StyleBackendType::Servo) {
    return Servo_Property_IsAnimatable(aProperty);
  }

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
    aResult.Clear();
    aRv.Throw(NS_ERROR_FAILURE);
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
  ErrorResult parseEasingResult;

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

    if (keyframeDict.mComposite.WasPassed()) {
      keyframe->mComposite.emplace(keyframeDict.mComposite.Value());
    }

    // Look for additional property-values pairs on the object.
    nsTArray<PropertyValuesPair> propertyValuePairs;
    if (value.isObject()) {
      JS::Rooted<JSObject*> object(aCx, &value.toObject());
      if (!GetPropertyValuesPairs(aCx, object,
                                  ListAllowance::eDisallow,
                                  aDocument->GetStyleBackendType(),
                                  propertyValuePairs)) {
        return false;
      }
    }

    if (!parseEasingResult.Failed()) {
      keyframe->mTimingFunction =
        TimingParams::ParseEasing(keyframeDict.mEasing,
                                  aDocument,
                                  parseEasingResult);
      // Even if the above fails, we still need to continue reading off all the
      // properties since checking the validity of easing should be treated as
      // a separate step that happens *after* all the other processing in this
      // loop since (since it is never likely to be handled by WebIDL unlike the
      // rest of this loop).
    }

    for (PropertyValuesPair& pair : propertyValuePairs) {
      MOZ_ASSERT(pair.mValues.Length() == 1);

      Maybe<PropertyValuePair> valuePair =
        MakePropertyValuePair(pair.mProperty, pair.mValues[0], parser,
                              aDocument);
      if (!valuePair) {
        continue;
      }
      keyframe->mPropertyValues.AppendElement(Move(valuePair.ref()));

#ifdef DEBUG
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
#endif
    }
  }

  // Throw any errors we encountered while parsing 'easing' properties.
  if (parseEasingResult.MaybeSetPendingException(aCx)) {
    return false;
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
 * @param aBackend The style backend in use. Used to determine which properties
 *   are animatable since only animatable properties are read.
 * @param aResult The array into which the enumerated property-values
 *   pairs will be stored.
 * @return false on failure or JS exception thrown while interacting
 *   with aObject; true otherwise.
 */
static bool
GetPropertyValuesPairs(JSContext* aCx,
                       JS::Handle<JSObject*> aObject,
                       ListAllowance aAllowLists,
                       StyleBackendType aBackend,
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
    if (KeyframeUtils::IsAnimatableProperty(property, aBackend)) {
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

static void
ReportInvalidPropertyValueToConsole(nsCSSPropertyID aProperty,
                                    const nsAString& aInvalidPropertyValue,
                                    nsIDocument* aDoc)
{
  const nsString& invalidValue = PromiseFlatString(aInvalidPropertyValue);
  const NS_ConvertASCIItoUTF16 propertyName(
    nsCSSProps::GetStringValue(aProperty));
  const char16_t* params[] = { invalidValue.get(), propertyName.get() };
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Animation"),
                                  aDoc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  "InvalidKeyframePropertyValue",
                                  params, ArrayLength(params));
}

/**
 * Construct a PropertyValuePair parsing the given string into a suitable
 * nsCSSValue object.
 *
 * @param aProperty The CSS property.
 * @param aStringValue The property value to parse.
 * @param aParser The CSS parser object to use.
 * @param aDocument The document to use when parsing.
 * @return The constructed PropertyValuePair, or Nothing() if |aStringValue| is
 *   an invalid property value.
 */
static Maybe<PropertyValuePair>
MakePropertyValuePair(nsCSSPropertyID aProperty, const nsAString& aStringValue,
                      nsCSSParser& aParser, nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);
  Maybe<PropertyValuePair> result;

  if (aDocument->GetStyleBackendType() == StyleBackendType::Servo) {
    ServoCSSParser::ParsingEnvironment env =
      ServoCSSParser::GetParsingEnvironment(aDocument);
    RefPtr<RawServoDeclarationBlock> servoDeclarationBlock =
      ServoCSSParser::ParseProperty(aProperty, aStringValue, env);

    if (servoDeclarationBlock) {
      result.emplace(aProperty, Move(servoDeclarationBlock));
    } else {
      ReportInvalidPropertyValueToConsole(aProperty, aStringValue, aDocument);
    }
    return result;
  }

  nsCSSValue value;
  if (!nsCSSProps::IsShorthand(aProperty)) {
    aParser.ParseLonghandProperty(aProperty,
                                  aStringValue,
                                  aDocument->GetDocumentURI(),
                                  aDocument->GetDocumentURI(),
                                  aDocument->NodePrincipal(),
                                  value);
    if (value.GetUnit() == eCSSUnit_Null) {
      // Invalid property value, so return Nothing.
      ReportInvalidPropertyValueToConsole(aProperty, aStringValue, aDocument);
      return result;
    }
  }

  if (value.GetUnit() == eCSSUnit_Null) {
    // If we have a shorthand, store the string value as a token stream.
    nsCSSValueTokenStream* tokenStream = new nsCSSValueTokenStream;
    tokenStream->mTokenStream = aStringValue;

    // We are about to convert a null value to a token stream value but
    // by leaving the mPropertyID as unknown, we will be able to
    // distinguish between shorthand values and valid token stream values
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
  }

  result.emplace(aProperty, Move(value));
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

#ifdef DEBUG
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

  aPair.mSimulateComputeValuesFailure = true;
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
         aPair.mSimulateComputeValuesFailure;
}
#endif

/**
 * Calculate the StyleAnimationValues of properties of each keyframe.
 * This involves expanding shorthand properties into longhand properties,
 * removing the duplicated properties for each keyframe, and creating an
 * array of |property:computed value| pairs for each keyframe.
 *
 * These computed values are used when computing the final set of
 * per-property animation values (see GetAnimationPropertiesFromKeyframes).
 *
 * @param aKeyframes The input keyframes.
 * @param aElement The context element.
 * @param aStyleContext The style context to use when computing values.
 * @return The set of ComputedKeyframeValues. The length will be the same as
 *   aFrames.
 */
static nsTArray<ComputedKeyframeValues>
GetComputedKeyframeValues(const nsTArray<Keyframe>& aKeyframes,
                          dom::Element* aElement,
                          GeckoStyleContext* aStyleContext)
{
  MOZ_ASSERT(aStyleContext);
  MOZ_ASSERT(aElement);

  const size_t len = aKeyframes.Length();
  nsTArray<ComputedKeyframeValues> result(len);

  for (const Keyframe& frame : aKeyframes) {
    nsCSSPropertyIDSet propertiesOnThisKeyframe;
    ComputedKeyframeValues* computedValues = result.AppendElement();
    for (const PropertyValuePair& pair :
           PropertyPriorityIterator(frame.mPropertyValues)) {
      MOZ_ASSERT(!pair.mServoDeclarationBlock,
                 "Animation values were parsed using Servo backend but target"
                 " element is not using Servo backend?");

      // Expand each value into the set of longhands and produce
      // a KeyframeValueEntry for each value.
      nsTArray<PropertyStyleAnimationValuePair> values;

      // For shorthands, we store the string as a token stream so we need to
      // extract that first.
      if (nsCSSProps::IsShorthand(pair.mProperty)) {
        nsCSSValueTokenStream* tokenStream = pair.mValue.GetTokenStreamValue();
        if (!StyleAnimationValue::ComputeValues(pair.mProperty,
              CSSEnabledState::eForAllContent, aElement, aStyleContext,
              tokenStream->mTokenStream, /* aUseSVGMode */ false, values)) {
          continue;
        }

#ifdef DEBUG
        if (IsComputeValuesFailureKey(pair)) {
          continue;
        }
#endif
      } else if (pair.mValue.GetUnit() == eCSSUnit_Null) {
        // An uninitialized nsCSSValue represents the underlying value which
        // we represent as an uninitialized AnimationValue so we just leave
        // neutralPair->mValue as-is.
        PropertyStyleAnimationValuePair* neutralPair = values.AppendElement();
        neutralPair->mProperty = pair.mProperty;
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

      for (auto& value : values) {
        // If we already got a value for this property on the keyframe,
        // skip this one.
        if (propertiesOnThisKeyframe.HasProperty(value.mProperty)) {
          continue;
        }
        propertiesOnThisKeyframe.AddProperty(value.mProperty);
        computedValues->AppendElement(Move(value));
      }
    }
  }

  MOZ_ASSERT(result.Length() == aKeyframes.Length(), "Array length mismatch");
  return result;
}

/**
 * The variation of the above function. This is for Servo backend.
 */
static nsTArray<ComputedKeyframeValues>
GetComputedKeyframeValues(const nsTArray<Keyframe>& aKeyframes,
                          dom::Element* aElement,
                          const ServoStyleContext* aStyleContext)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->IsStyledByServo());

  nsTArray<ComputedKeyframeValues> result;

  nsPresContext* presContext = nsContentUtils::GetContextForContent(aElement);
  if (!presContext) {
    // This has been reported to happen with some combinations of content
    // (particularly involving resize events and layout flushes? See bug 1407898
    // and bug 1408420) but no reproducible steps have been found.
    // For now we just return an empty array.
    return result;
  }

  result = presContext->StyleSet()->AsServo()
    ->GetComputedKeyframeValuesFor(aKeyframes, aElement, aStyleContext);
  return result;
}

static void
AppendInitialSegment(AnimationProperty* aAnimationProperty,
                     const KeyframeValueEntry& aFirstEntry)
{
  AnimationPropertySegment* segment =
    aAnimationProperty->mSegments.AppendElement();
  segment->mFromKey        = 0.0f;
  segment->mToKey          = aFirstEntry.mOffset;
  segment->mToValue        = aFirstEntry.mValue;
  segment->mToComposite    = aFirstEntry.mComposite;
}

static void
AppendFinalSegment(AnimationProperty* aAnimationProperty,
                   const KeyframeValueEntry& aLastEntry)
{
  AnimationPropertySegment* segment =
    aAnimationProperty->mSegments.AppendElement();
  segment->mFromKey        = aLastEntry.mOffset;
  segment->mFromValue      = aLastEntry.mValue;
  segment->mFromComposite  = aLastEntry.mComposite;
  segment->mToKey          = 1.0f;
  segment->mTimingFunction = aLastEntry.mTimingFunction;
}

// Returns a newly created AnimationProperty if one was created to fill-in the
// missing keyframe, nullptr otherwise (if we decided not to fill the keyframe
// becase we don't support additive animation).
static AnimationProperty*
HandleMissingInitialKeyframe(nsTArray<AnimationProperty>& aResult,
                             const KeyframeValueEntry& aEntry)
{
  MOZ_ASSERT(aEntry.mOffset != 0.0f,
             "The offset of the entry should not be 0.0");

  // If the preference of the core Web Animations API is not enabled, don't fill
  // in the missing keyframe since the missing keyframe requires support for
  // additive animation which is guarded by this pref.
  if (!nsContentUtils::AnimationsAPICoreEnabled()) {
    return nullptr;
  }

  AnimationProperty* result = aResult.AppendElement();
  result->mProperty = aEntry.mProperty;

  AppendInitialSegment(result, aEntry);

  return result;
}

static void
HandleMissingFinalKeyframe(nsTArray<AnimationProperty>& aResult,
                           const KeyframeValueEntry& aEntry,
                           AnimationProperty* aCurrentAnimationProperty)
{
  MOZ_ASSERT(aEntry.mOffset != 1.0f,
             "The offset of the entry should not be 1.0");

  // If the preference of the core Web Animations API is not enabled, don't fill
  // in the missing keyframe since the missing keyframe requires support for
  // additive animation which is guarded by this pref.
  if (!nsContentUtils::AnimationsAPICoreEnabled()) {
    // If we have already appended a new entry for the property so we have to
    // remove it.
    if (aCurrentAnimationProperty) {
      aResult.RemoveElementAt(aResult.Length() - 1);
    }
    return;
  }

  // If |aCurrentAnimationProperty| is nullptr, that means this is the first
  // entry for the property, we have to append a new AnimationProperty for this
  // property.
  if (!aCurrentAnimationProperty) {
    aCurrentAnimationProperty = aResult.AppendElement();
    aCurrentAnimationProperty->mProperty = aEntry.mProperty;

    // If we have only one entry whose offset is neither 1 nor 0 for this
    // property, we need to append the initial segment as well.
    if (aEntry.mOffset != 0.0f) {
      AppendInitialSegment(aCurrentAnimationProperty, aEntry);
    }
  }
  AppendFinalSegment(aCurrentAnimationProperty, aEntry);
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
  // Furthermore, if additive animation is disabled, the following loop takes
  // care to identify properties that lack a value at offset 0.0/1.0 and drops
  // those properties from |aResult|.

  nsCSSPropertyID lastProperty = eCSSProperty_UNKNOWN;
  AnimationProperty* animationProperty = nullptr;

  size_t i = 0, n = aEntries.Length();

  while (i < n) {
    // If we've reached the end of the array of entries, synthesize a final (and
    // initial) segment if necessary.
    if (i + 1 == n) {
      if (aEntries[i].mOffset != 1.0f) {
        HandleMissingFinalKeyframe(aResult, aEntries[i], animationProperty);
      } else if (aEntries[i].mOffset == 1.0f && !animationProperty) {
        // If the last entry with offset 1 and no animation property, that means
        // it is the only entry for this property so append a single segment
        // from 0 offset to |aEntry[i].offset|.
        Unused << HandleMissingInitialKeyframe(aResult, aEntries[i]);
      }
      animationProperty = nullptr;
      break;
    }

    MOZ_ASSERT(aEntries[i].mProperty != eCSSProperty_UNKNOWN &&
               aEntries[i + 1].mProperty != eCSSProperty_UNKNOWN,
               "Each entry should specify a valid property");

    // No keyframe for this property at offset 0.
    if (aEntries[i].mProperty != lastProperty &&
        aEntries[i].mOffset != 0.0f) {
      // If we don't support additive animation we can't fill in the missing
      // keyframes and we should just skip this property altogether. Since the
      // entries are sorted by offset for a given property, and since we don't
      // update |lastProperty|, we will keep hitting this condition until we
      // change property.
      animationProperty = HandleMissingInitialKeyframe(aResult, aEntries[i]);
      if (animationProperty) {
        lastProperty = aEntries[i].mProperty;
      } else {
        // Skip this entry if we did not handle the missing entry.
        ++i;
        continue;
      }
    }

    // Skip this entry if the next entry has the same offset except for initial
    // and final ones. We will handle missing keyframe in the next loop
    // if the property is changed on the next entry.
    if (aEntries[i].mProperty == aEntries[i + 1].mProperty &&
        aEntries[i].mOffset == aEntries[i + 1].mOffset &&
        aEntries[i].mOffset != 1.0f && aEntries[i].mOffset != 0.0f) {
      ++i;
      continue;
    }

    // No keyframe for this property at offset 1.
    if (aEntries[i].mProperty != aEntries[i + 1].mProperty &&
        aEntries[i].mOffset != 1.0f) {
      HandleMissingFinalKeyframe(aResult, aEntries[i], animationProperty);
      // Move on to new property.
      animationProperty = nullptr;
      ++i;
      continue;
    }

    // Starting from i + 1, determine the next [i, j] interval from which to
    // generate a segment. Basically, j is i + 1, but there are some special
    // cases for offset 0 and 1, so we need to handle them specifically.
    // Note: From this moment, we make sure [i + 1] is valid and
    //       there must be an initial entry (i.e. mOffset = 0.0) and
    //       a final entry (i.e. mOffset = 1.0). Besides, all the entries
    //       with the same offsets except for initial/final ones are filtered
    //       out already.
    size_t j = i + 1;
    if (aEntries[i].mOffset == 0.0f && aEntries[i + 1].mOffset == 0.0f) {
      // We need to generate an initial zero-length segment.
      MOZ_ASSERT(aEntries[i].mProperty == aEntries[i + 1].mProperty);
      while (j + 1 < n &&
             aEntries[j + 1].mOffset == 0.0f &&
             aEntries[j + 1].mProperty == aEntries[j].mProperty) {
        ++j;
      }
    } else if (aEntries[i].mOffset == 1.0f) {
      if (aEntries[i + 1].mOffset == 1.0f &&
          aEntries[i + 1].mProperty == aEntries[i].mProperty) {
        // We need to generate a final zero-length segment.
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
    segment->mFromKey        = aEntries[i].mOffset;
    segment->mToKey          = aEntries[j].mOffset;
    segment->mFromValue      = aEntries[i].mValue;
    segment->mToValue        = aEntries[j].mValue;
    segment->mTimingFunction = aEntries[i].mTimingFunction;
    segment->mFromComposite  = aEntries[i].mComposite;
    segment->mToComposite    = aEntries[j].mComposite;

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

  // Get all the property--value-list pairs off the object.
  JS::Rooted<JSObject*> object(aCx, &aValue.toObject());
  nsTArray<PropertyValuesPair> propertyValuesPairs;
  if (!GetPropertyValuesPairs(aCx, object, ListAllowance::eAllow,
                              aDocument->GetStyleBackendType(),
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

    // If we only have one value, we should animate from the underlying value
    // using additive animation--however, we don't support additive animation
    // when the core animation API pref is switched off.
    if (!nsContentUtils::AnimationsAPICoreEnabled() && count == 1) {
      aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
      return;
    }

    size_t n = pair.mValues.Length() - 1;
    size_t i = 0;

    for (const nsString& stringValue : pair.mValues) {
      // For single-valued lists, the single value should be added to a
      // keyframe with offset 1.
      double offset = n ? i++ / double(n) : 1;
      Keyframe* keyframe = processedKeyframes.LookupOrAdd(offset);
      if (keyframe->mPropertyValues.IsEmpty()) {
        keyframe->mComputedOffset = offset;
      }

      Maybe<PropertyValuePair> valuePair =
        MakePropertyValuePair(pair.mProperty, stringValue, parser, aDocument);
      if (!valuePair) {
        continue;
      }
      keyframe->mPropertyValues.AppendElement(Move(valuePair.ref()));
    }
  }

  aResult.SetCapacity(processedKeyframes.Count());
  for (auto iter = processedKeyframes.Iter(); !iter.Done(); iter.Next()) {
    aResult.AppendElement(Move(*iter.UserData()));
  }

  aResult.Sort(ComputedOffsetComparator());

  // Fill in any specified offsets
  //
  // This corresponds to step 5, "Otherwise," branch, substeps 5-6 of
  // https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument
  const FallibleTArray<Nullable<double>>* offsets = nullptr;
  AutoTArray<Nullable<double>, 1> singleOffset;
  auto& offset = keyframeDict.mOffset;
  if (offset.IsDouble()) {
    singleOffset.AppendElement(offset.GetAsDouble());
    // dom::Sequence is a fallible but AutoTArray is infallible and we need to
    // point to one or the other. Fortunately, fallible and infallible array
    // types can be implicitly converted provided they are const.
    const FallibleTArray<Nullable<double>>& asFallibleArray = singleOffset;
    offsets = &asFallibleArray;
  } else if (offset.IsDoubleOrNullSequence()) {
    offsets = &offset.GetAsDoubleOrNullSequence();
  }
  // If offset.IsNull() is true, then we want to leave the mOffset member of
  // each keyframe with its initialized value of null. By leaving |offsets|
  // as nullptr here, we skip updating mOffset below.

  size_t offsetsToFill =
    offsets ? std::min(offsets->Length(), aResult.Length()) : 0;
  for (size_t i = 0; i < offsetsToFill; i++) {
    if (!offsets->ElementAt(i).IsNull()) {
      aResult[i].mOffset.emplace(offsets->ElementAt(i).Value());
    }
  }

  // Check that the keyframes are loosely sorted and that any specified offsets
  // are between 0.0 and 1.0 inclusive.
  //
  // This corresponds to steps 6-7 of
  // https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument
  //
  // In the spec, TypeErrors arising from invalid offsets and easings are thrown
  // at the end of the procedure since it assumes we initially store easing
  // values as strings and then later parse them.
  //
  // However, we will parse easing members immediately when we process them
  // below. In order to maintain the relative order in which TypeErrors are
  // thrown according to the spec, namely exceptions arising from invalid
  // offsets are thrown before exceptions arising from invalid easings, we check
  // the offsets here.
  if (!HasValidOffsets(aResult)) {
    aRv.ThrowTypeError<dom::MSG_INVALID_KEYFRAME_OFFSETS>();
    aResult.Clear();
    return;
  }

  // Fill in any easings.
  //
  // This corresponds to step 5, "Otherwise," branch, substeps 7-11 of
  // https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument
  FallibleTArray<Maybe<ComputedTimingFunction>> easings;
  auto parseAndAppendEasing = [&](const nsString& easingString,
                                  ErrorResult& aRv) {
    auto easing = TimingParams::ParseEasing(easingString, aDocument, aRv);
    if (!aRv.Failed() && !easings.AppendElement(Move(easing), fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }
  };

  auto& easing = keyframeDict.mEasing;
  if (easing.IsString()) {
    parseAndAppendEasing(easing.GetAsString(), aRv);
    if (aRv.Failed()) {
      aResult.Clear();
      return;
    }
  } else {
    for (const nsString& easingString : easing.GetAsStringSequence()) {
      parseAndAppendEasing(easingString, aRv);
      if (aRv.Failed()) {
        aResult.Clear();
        return;
      }
    }
  }

  // If |easings| is empty, then we are supposed to fill it in with the value
  // "linear" and then repeat the list as necessary.
  //
  // However, for Keyframe.mTimingFunction we represent "linear" as a None
  // value. Since we have not assigned 'mTimingFunction' for any of the
  // keyframes in |aResult| they will already have their initial None value
  // (i.e. linear). As a result, if |easings| is empty, we don't need to do
  // anything.
  if (!easings.IsEmpty()) {
    for (size_t i = 0; i < aResult.Length(); i++) {
      aResult[i].mTimingFunction = easings[i % easings.Length()];
    }
  }

  // Fill in any composite operations.
  //
  // This corresponds to step 5, "Otherwise," branch, substep 12 of
  // https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument
  const FallibleTArray<dom::CompositeOperation>* compositeOps;
  AutoTArray<dom::CompositeOperation, 1> singleCompositeOp;
  auto& composite = keyframeDict.mComposite;
  if (composite.IsCompositeOperation()) {
    singleCompositeOp.AppendElement(composite.GetAsCompositeOperation());
    const FallibleTArray<dom::CompositeOperation>& asFallibleArray =
      singleCompositeOp;
    compositeOps = &asFallibleArray;
  } else {
    compositeOps = &composite.GetAsCompositeOperationSequence();
  }

  // Fill in and repeat as needed.
  if (!compositeOps->IsEmpty()) {
    for (size_t i = 0; i < aResult.Length(); i++) {
      aResult[i].mComposite.emplace(
        compositeOps->ElementAt(i % compositeOps->Length()));
    }
  }
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

    // We won't have called DistributeKeyframes when this is called so
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
 * Distribute the offsets of all keyframes in between the endpoints of the
 * given range.
 *
 * @param aRange The sequence of keyframes between whose endpoints we should
 * distribute offsets.
 */
static void
DistributeRange(const Range<Keyframe>& aRange)
{
  const Range<Keyframe> rangeToAdjust = Range<Keyframe>(aRange.begin() + 1,
                                                        aRange.end() - 1);
  const size_t n = aRange.length() - 1;
  const double startOffset = aRange[0].mComputedOffset;
  const double diffOffset = aRange[n].mComputedOffset - startOffset;
  for (auto iter = rangeToAdjust.begin(); iter != rangeToAdjust.end(); ++iter) {
    size_t index = iter - aRange.begin();
    iter->mComputedOffset = startOffset + double(index) / n * diffOffset;
  }
}

} // namespace mozilla
