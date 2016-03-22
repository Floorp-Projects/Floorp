/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/KeyframeUtils.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Move.h"
#include "mozilla/TimingParams.h"
#include "mozilla/dom/BaseKeyframeTypesBinding.h" // For FastBaseKeyframe etc.
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "jsapi.h" // For ForOfIterator etc.
#include "nsClassHashtable.h"
#include "nsCSSParser.h"
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

// For the aAllowList parameter of AppendStringOrStringSequence and
// GetPropertyValuesPairs.
enum class ListAllowance { eDisallow, eAllow };

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
  nsCSSProperty mProperty;
  nsTArray<nsString> mValues;

  class PropertyPriorityComparator
  {
  public:
    PropertyPriorityComparator()
      : mSubpropertyCountInitialized(false) {}

    bool Equals(const PropertyValuesPair& aLhs,
                const PropertyValuesPair& aRhs) const
    {
      return aLhs.mProperty == aRhs.mProperty;
    }

    bool LessThan(const PropertyValuesPair& aLhs,
                  const PropertyValuesPair& aRhs) const
    {
      bool isShorthandLhs = nsCSSProps::IsShorthand(aLhs.mProperty);
      bool isShorthandRhs = nsCSSProps::IsShorthand(aRhs.mProperty);

      if (isShorthandLhs) {
        if (isShorthandRhs) {
          // First, sort shorthands by the number of longhands they have.
          uint32_t subpropCountLhs = SubpropertyCount(aLhs.mProperty);
          uint32_t subpropCountRhs = SubpropertyCount(aRhs.mProperty);
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
      return nsCSSProps::PropertyIDLNameSortPosition(aLhs.mProperty) <
             nsCSSProps::PropertyIDLNameSortPosition(aRhs.mProperty);
    }

    uint32_t SubpropertyCount(nsCSSProperty aProperty) const
    {
      if (!mSubpropertyCountInitialized) {
        PodZero(&mSubpropertyCount);
        mSubpropertyCountInitialized = true;
      }
      if (mSubpropertyCount[aProperty] == 0) {
        uint32_t count = 0;
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
            p, aProperty, nsCSSProps::eEnabledForAllContent) {
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
};

/**
 * The result of parsing a JS object as a BaseKeyframe dictionary
 * and getting its property-value pairs from its open-ended
 * properties.
 */
struct OffsetIndexedKeyframe
{
  dom::binding_detail::FastBaseKeyframe mKeyframeDict;
  nsTArray<PropertyValuesPair> mPropertyValuePairs;
};

/**
 * An additional property (for a property-values pair) found on a
 * BaseKeyframe or BasePropertyIndexedKeyframe object.
 */
struct AdditionalProperty
{
  nsCSSProperty mProperty;
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
 * A property and StyleAnimationValue pair.
 */
struct KeyframeValue
{
  nsCSSProperty mProperty;
  StyleAnimationValue mValue;
};

/**
 * Data for a segment in a keyframe animation of a given property
 * whose value is a StyleAnimationValue.
 *
 * KeyframeValueEntry is used in BuildAnimationPropertyListFromKeyframeSequence
 * to gather data for each individual segment described by an author-supplied
 * an IDL sequence<Keyframe> value so that they can be parsed into mProperties.
 */
struct KeyframeValueEntry : KeyframeValue
{
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
// Internal helper method declarations
//
// ------------------------------------------------------------------

static void
BuildAnimationPropertyListFromKeyframeSequence(
    JSContext* aCx,
    Element* aTarget,
    CSSPseudoElementType aPseudoType,
    JS::ForOfIterator& aIterator,
    nsTArray<AnimationProperty>& aResult,
    ErrorResult& aRv);

static void
GetKeyframeListFromKeyframeSequence(JSContext* aCx,
                                    JS::ForOfIterator& aIterator,
                                    nsTArray<Keyframe>& aResult,
                                    ErrorResult& aRv);

static bool
ConvertKeyframeSequence(JSContext* aCx,
                        JS::ForOfIterator& aIterator,
                        nsTArray<OffsetIndexedKeyframe>& aResult);

static bool
ConvertKeyframeSequence(JSContext* aCx,
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
MakePropertyValuePair(nsCSSProperty aProperty, const nsAString& aStringValue,
                      nsCSSParser& aParser, nsIDocument* aDocument);

static bool
HasValidOffsets(const nsTArray<OffsetIndexedKeyframe>& aKeyframes);

static bool
HasValidOffsets(const nsTArray<Keyframe>& aKeyframes);

static void
ApplyDistributeSpacing(nsTArray<OffsetIndexedKeyframe>& aKeyframes);

static void
GenerateValueEntries(Element* aTarget,
                     CSSPseudoElementType aPseudoType,
                     nsTArray<OffsetIndexedKeyframe>& aKeyframes,
                     nsTArray<KeyframeValueEntry>& aResult,
                     ErrorResult& aRv);

static void
BuildSegmentsFromValueEntries(nsTArray<KeyframeValueEntry>& aEntries,
                              nsTArray<AnimationProperty>& aResult);

static void
BuildAnimationPropertyListFromPropertyIndexedKeyframes(
    JSContext* aCx,
    Element* aTarget,
    CSSPseudoElementType aPseudoType,
    JS::Handle<JS::Value> aValue,
    InfallibleTArray<AnimationProperty>& aResult,
    ErrorResult& aRv);

static void
GetKeyframeListFromPropertyIndexedKeyframe(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           nsTArray<Keyframe>& aResult,
                                           ErrorResult& aRv);

static bool
RequiresAdditiveAnimation(const nsTArray<Keyframe>& aKeyframes,
                          nsIDocument* aDocument);


// ------------------------------------------------------------------
//
// Public API
//
// ------------------------------------------------------------------

/* static */ void
KeyframeUtils::BuildAnimationPropertyList(
    JSContext* aCx,
    Element* aTarget,
    CSSPseudoElementType aPseudoType,
    JS::Handle<JSObject*> aFrames,
    InfallibleTArray<AnimationProperty>& aResult,
    ErrorResult& aRv)
{
  MOZ_ASSERT(aResult.IsEmpty());

  // See the description of frame lists in the spec:
  //
  // https://w3c.github.io/web-animations/#processing-a-frames-argument
  //
  // We don't support SharedKeyframeList yet, but we do support the other
  // types of arguments.  We manually implement the parts of JS-to-IDL union
  // conversion algorithm from the Web IDL spec, since we have to represent
  // this as an object? so we can look at the open-ended set of properties
  // on these objects.

  if (!aFrames) {
    // The argument was explicitly null.  In this case, the default dictionary
    // value for PropertyIndexedKeyframe would result in no keyframes.
    return;
  }

  // At this point we know we have an object.  We try to convert it to a
  // sequence<Keyframe> first, and if that fails due to not being iterable,
  // we try to convert it to PropertyIndexedKeyframe.
  JS::Rooted<JS::Value> objectValue(aCx, JS::ObjectValue(*aFrames));
  JS::ForOfIterator iter(aCx);
  if (!iter.init(objectValue, JS::ForOfIterator::AllowNonIterable)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (iter.valueIsIterable()) {
    BuildAnimationPropertyListFromKeyframeSequence(aCx, aTarget, aPseudoType,
                                                   iter, aResult, aRv);
  } else {
    BuildAnimationPropertyListFromPropertyIndexedKeyframes(aCx, aTarget,
                                                           aPseudoType,
                                                           objectValue, aResult,
                                                           aRv);
  }
}

/* static */ nsTArray<Keyframe>
KeyframeUtils::GetKeyframesFromObject(JSContext* aCx,
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
    GetKeyframeListFromKeyframeSequence(aCx, iter, keyframes, aRv);
  } else {
    GetKeyframeListFromPropertyIndexedKeyframe(aCx, objectValue, keyframes,
                                               aRv);
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

  nsIDocument* doc = AnimationUtils::GetCurrentRealmDocument(aCx);
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    keyframes.Clear();
    return keyframes;
  }

  if (RequiresAdditiveAnimation(keyframes, doc)) {
    aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
    keyframes.Clear();
  }

  return keyframes;
}


// ------------------------------------------------------------------
//
// Internal helpers
//
// ------------------------------------------------------------------

/**
 * Converts a JS object to an IDL sequence<Keyframe> and builds an
 * array of AnimationProperty objects for the keyframe animation
 * that it specifies.
 *
 * @param aTarget The target of the animation.
 * @param aIterator An already-initialized ForOfIterator for the JS
 *   object to iterate over as a sequence.
 * @param aResult The array into which the resulting AnimationProperty
 *   objects will be appended.
 */
static void
BuildAnimationPropertyListFromKeyframeSequence(
    JSContext* aCx,
    Element* aTarget,
    CSSPseudoElementType aPseudoType,
    JS::ForOfIterator& aIterator,
    nsTArray<AnimationProperty>& aResult,
    ErrorResult& aRv)
{
  // Convert the object in aIterator to sequence<Keyframe>, producing
  // an array of OffsetIndexedKeyframe objects.
  AutoTArray<OffsetIndexedKeyframe,4> keyframes;
  if (!ConvertKeyframeSequence(aCx, aIterator, keyframes)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // If the sequence<> had zero elements, we won't generate any
  // keyframes.
  if (keyframes.IsEmpty()) {
    return;
  }

  // Check that the keyframes are loosely sorted and with values all
  // between 0% and 100%.
  if (!HasValidOffsets(keyframes)) {
    aRv.ThrowTypeError<dom::MSG_INVALID_KEYFRAME_OFFSETS>();
    return;
  }

  // Fill in 0%/100% values if the first/element keyframes don't have
  // a specified offset, and evenly space those that have a missing
  // offset.  (We don't support paced spacing yet.)
  ApplyDistributeSpacing(keyframes);

  // Convert the OffsetIndexedKeyframes into a list of KeyframeValueEntry
  // objects.
  nsTArray<KeyframeValueEntry> entries;
  GenerateValueEntries(aTarget, aPseudoType, keyframes, entries, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Finally, build an array of AnimationProperty objects in aResult
  // corresponding to the entries.
  BuildSegmentsFromValueEntries(entries, aResult);
}

/**
 * Converts a JS object to an IDL sequence<Keyframe>.
 *
 * @param aCx The JSContext corresponding to |aIterator|.
 * @param aIterator An already-initialized ForOfIterator for the JS
 *   object to iterate over as a sequence.
 * @param aResult The array into which the resulting Keyframe objects will be
 *   appended.
 * @param aRv Out param to store any errors thrown by this function.
 */
static void
GetKeyframeListFromKeyframeSequence(JSContext* aCx,
                                    JS::ForOfIterator& aIterator,
                                    nsTArray<Keyframe>& aResult,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(!aRv.Failed());
  MOZ_ASSERT(aResult.IsEmpty());

  // Convert the object in aIterator to a sequence of keyframes producing
  // an array of Keyframe objects.
  if (!ConvertKeyframeSequence(aCx, aIterator, aResult)) {
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
 * IDL sequence<Keyframe> and stores the resulting OffsetIndexedKeyframe
 * objects in aResult.
 */
static bool
ConvertKeyframeSequence(JSContext* aCx,
                        JS::ForOfIterator& aIterator,
                        nsTArray<OffsetIndexedKeyframe>& aResult)
{
  JS::Rooted<JS::Value> value(aCx);
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
                             "Element of sequence<Keyframes> argument");
      return false;
    }
    // Convert the JS value into a BaseKeyframe dictionary value.
    OffsetIndexedKeyframe* keyframe = aResult.AppendElement();
    if (!keyframe->mKeyframeDict.Init(
          aCx, value, "Element of sequence<Keyframes> argument")) {
      return false;
    }
    // Look for additional property-values pairs on the object.
    if (value.isObject()) {
      JS::Rooted<JSObject*> object(aCx, &value.toObject());
      if (!GetPropertyValuesPairs(aCx, object,
                                  ListAllowance::eDisallow,
                                  keyframe->mPropertyValuePairs)) {
        return false;
      }
    }
  }
  return true;
}

/**
 * Converts a JS object wrapped by the given JS::ForIfIterator to an
 * IDL sequence<Keyframe> and stores the resulting Keyframe objects in
 * aResult.
 */
static bool
ConvertKeyframeSequence(JSContext* aCx,
                        JS::ForOfIterator& aIterator,
                        nsTArray<Keyframe>& aResult)
{
  nsIDocument* doc = AnimationUtils::GetCurrentRealmDocument(aCx);
  if (!doc) {
    return false;
  }

  JS::Rooted<JS::Value> value(aCx);
  nsCSSParser parser(doc->CSSLoader());

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
      TimingParams::ParseEasing(keyframeDict.mEasing, doc, rv);
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
        MakePropertyValuePair(pair.mProperty, pair.mValues[0], parser, doc));
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
    nsCSSProperty property =
      nsCSSProps::LookupPropertyByIDLName(propName,
                                          nsCSSProps::eEnabledForAllContent);
    if (property != eCSSProperty_UNKNOWN &&
        (nsCSSProps::IsShorthand(property) ||
         nsCSSProps::kAnimTypeTable[property] != eStyleAnimType_None)) {
      // Only need to check for longhands being animatable, as the
      // StyleAnimationValue::ComputeValues calls later on will check for
      // a shorthand's components being animatable.
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
MakePropertyValuePair(nsCSSProperty aProperty, const nsAString& aStringValue,
                      nsCSSParser& aParser, nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);

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
    // By leaving mShorthandPropertyID as unknown, we ensure that when
    // we call nsCSSValue::AppendToString we get back the string stored
    // in mTokenStream.
    MOZ_ASSERT(tokenStream->mShorthandPropertyID == eCSSProperty_UNKNOWN,
               "The shorthand property of a token stream should be initialized"
               " to unknown");
    value.SetTokenStreamValue(tokenStream);
  }

  return { aProperty, value };
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
HasValidOffsets(const nsTArray<OffsetIndexedKeyframe>& aKeyframes)
{
  double offset = 0.0;
  for (const OffsetIndexedKeyframe& keyframe : aKeyframes) {
    if (!keyframe.mKeyframeDict.mOffset.IsNull()) {
      double thisOffset = keyframe.mKeyframeDict.mOffset.Value();
      if (thisOffset < offset || thisOffset > 1.0f) {
        return false;
      }
      offset = thisOffset;
    }
  }
  return true;
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
 * Fills in any null offsets for the given keyframes by applying the
 * "distribute" spacing algorithm.
 *
 * http://w3c.github.io/web-animations/#distribute-keyframe-spacing-mode
 */
static void
ApplyDistributeSpacing(nsTArray<OffsetIndexedKeyframe>& aKeyframes)
{
  // If the first or last keyframes have an unspecified offset,
  // fill them in with 0% and 100%.  If there is only a single keyframe,
  // then it gets 100%.
  if (aKeyframes.LastElement().mKeyframeDict.mOffset.IsNull()) {
    aKeyframes.LastElement().mKeyframeDict.mOffset.SetValue(1.0);
  }
  if (aKeyframes[0].mKeyframeDict.mOffset.IsNull()) {
    aKeyframes[0].mKeyframeDict.mOffset.SetValue(0.0);
  }

  // Fill in remaining missing offsets.
  size_t i = 0;
  while (i < aKeyframes.Length() - 1) {
    MOZ_ASSERT(!aKeyframes[i].mKeyframeDict.mOffset.IsNull());
    double start = aKeyframes[i].mKeyframeDict.mOffset.Value();
    size_t j = i + 1;
    while (aKeyframes[j].mKeyframeDict.mOffset.IsNull()) {
      ++j;
    }
    double end = aKeyframes[j].mKeyframeDict.mOffset.Value();
    size_t n = j - i;
    for (size_t k = 1; k < n; ++k) {
      double offset = start + double(k) / n * (end - start);
      aKeyframes[i + k].mKeyframeDict.mOffset.SetValue(offset);
    }
    i = j;
  }
}

/**
 * Splits out each property's keyframe animation segment information
 * from the OffsetIndexedKeyframe objects into an array of KeyframeValueEntry.
 *
 * The easing string value in OffsetIndexedKeyframe objects is parsed
 * into a ComputedTimingFunction value in the corresponding KeyframeValueEntry
 * objects.
 *
 * @param aTarget The target of the animation.
 * @param aPseudoType The pseudo type of the target if it is a pseudo element.
 * @param aKeyframes The keyframes to read.
 * @param aResult The array to append the resulting KeyframeValueEntry
 *   objects to.
 */
static void
GenerateValueEntries(Element* aTarget,
                     CSSPseudoElementType aPseudoType,
                     nsTArray<OffsetIndexedKeyframe>& aKeyframes,
                     nsTArray<KeyframeValueEntry>& aResult,
                     ErrorResult& aRv)
{
  nsCSSPropertySet properties;              // All properties encountered.
  nsCSSPropertySet propertiesWithFromValue; // Those with a defined 0% value.
  nsCSSPropertySet propertiesWithToValue;   // Those with a defined 100% value.

  for (OffsetIndexedKeyframe& keyframe : aKeyframes) {
    Maybe<ComputedTimingFunction> easing =
      TimingParams::ParseEasing(keyframe.mKeyframeDict.mEasing,
                                aTarget->OwnerDoc(), aRv);
    if (aRv.Failed()) {
      return;
    }
    float offset = float(keyframe.mKeyframeDict.mOffset.Value());
    // We ignore keyframe.mKeyframeDict.mComposite since we don't support
    // composite modes on keyframes yet.

    // keyframe.mPropertyValuePairs is currently sorted by CSS property IDL
    // name, since that was the order we read the properties from the JS
    // object.  Re-sort the list so that longhand properties appear before
    // shorthands, and with shorthands all appearing in increasing order of
    // number of components.  For two longhand properties, or two shorthands
    // with the same number of components, sort by IDL name.
    //
    // Example orderings that result from this:
    //
    //   margin-left, margin
    //
    // and:
    //
    //   border-top-color, border-color, border-top, border
    //
    // This allows us to prioritize values specified by longhands (or smaller
    // shorthand subsets) when longhands and shorthands are both specified
    // on the one keyframe.
    keyframe.mPropertyValuePairs.Sort(
        PropertyValuesPair::PropertyPriorityComparator());

    nsCSSPropertySet propertiesOnThisKeyframe;
    for (const PropertyValuesPair& pair : keyframe.mPropertyValuePairs) {
      MOZ_ASSERT(pair.mValues.Length() == 1,
                 "ConvertKeyframeSequence should have parsed single "
                 "DOMString values from the property-values pairs");
      // Parse the property's string value and produce a KeyframeValueEntry (or
      // more than one, for shorthands) for it.
      nsTArray<PropertyStyleAnimationValuePair> values;
      if (StyleAnimationValue::ComputeValues(pair.mProperty,
                                             nsCSSProps::eEnabledForAllContent,
                                             aTarget,
                                             aPseudoType,
                                             pair.mValues[0],
                                             /* aUseSVGMode */ false,
                                             values)) {
        for (auto& value : values) {
          // If we already got a value for this property on the keyframe,
          // skip this one.
          if (propertiesOnThisKeyframe.HasProperty(value.mProperty)) {
            continue;
          }

          KeyframeValueEntry* entry = aResult.AppendElement();
          entry->mOffset = offset;
          entry->mProperty = value.mProperty;
          entry->mValue = value.mValue;
          entry->mTimingFunction = easing;

          if (offset == 0.0) {
            propertiesWithFromValue.AddProperty(value.mProperty);
          } else if (offset == 1.0) {
            propertiesWithToValue.AddProperty(value.mProperty);
          }
          propertiesOnThisKeyframe.AddProperty(value.mProperty);
          properties.AddProperty(value.mProperty);
        }
      }
    }
  }

  // We don't support additive segments and so can't support missing properties
  // using their underlying value in 0% and 100% keyframes.  Throw an exception
  // until we do support this.
  if (!propertiesWithFromValue.Equals(properties) ||
      !propertiesWithToValue.Equals(properties)) {
    aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
    return;
  }
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

  MOZ_ASSERT(aEntries[0].mOffset == 0.0f);
  MOZ_ASSERT(aEntries.LastElement().mOffset == 1.0f);

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

  nsCSSProperty lastProperty = eCSSProperty_UNKNOWN;
  AnimationProperty* animationProperty = nullptr;

  size_t i = 0, n = aEntries.Length();

  while (i + 1 < n) {
    // Starting from i, determine the next [i, j] interval from which to
    // generate a segment.
    size_t j;
    if (aEntries[i].mOffset == 0.0f && aEntries[i + 1].mOffset == 0.0f) {
      // We need to generate an initial zero-length segment.
      MOZ_ASSERT(aEntries[i].mProperty == aEntries[i + 1].mProperty);
      j = i + 1;
      while (aEntries[j + 1].mOffset == 0.0f) {
        MOZ_ASSERT(aEntries[j].mProperty == aEntries[j + 1].mProperty);
        ++j;
      }
    } else if (aEntries[i].mOffset == 1.0f) {
      if (aEntries[i + 1].mOffset == 1.0f) {
        // We need to generate a final zero-length segment.
        MOZ_ASSERT(aEntries[i].mProperty == aEntries[i].mProperty);
        j = i + 1;
        while (j + 1 < n && aEntries[j + 1].mOffset == 1.0f) {
          MOZ_ASSERT(aEntries[j].mProperty == aEntries[j + 1].mProperty);
          ++j;
        }
      } else {
        // New property.
        MOZ_ASSERT(aEntries[i + 1].mOffset == 0.0f);
        MOZ_ASSERT(aEntries[i].mProperty != aEntries[i + 1].mProperty);
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
 * Converts a JS object to an IDL PropertyIndexedKeyframe and builds an
 * array of AnimationProperty objects for the keyframe animation
 * that it specifies.
 *
 * @param aTarget The target of the animation.
 * @param aValue The JS object.
 * @param aResult The array into which the resulting AnimationProperty
 *   objects will be appended.
 */
static void
BuildAnimationPropertyListFromPropertyIndexedKeyframes(
    JSContext* aCx,
    Element* aTarget,
    CSSPseudoElementType aPseudoType,
    JS::Handle<JS::Value> aValue,
    InfallibleTArray<AnimationProperty>& aResult,
    ErrorResult& aRv)
{
  MOZ_ASSERT(aValue.isObject());

  // Convert the object to a PropertyIndexedKeyframe dictionary to
  // get its explicit dictionary members.
  dom::binding_detail::FastBasePropertyIndexedKeyframe keyframes;
  if (!keyframes.Init(aCx, aValue, "BasePropertyIndexedKeyframe argument",
                      false)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  Maybe<ComputedTimingFunction> easing =
    TimingParams::ParseEasing(keyframes.mEasing, aTarget->OwnerDoc(), aRv);

  // We ignore easing.mComposite since we don't support composite modes on
  // keyframes yet.

  // Get all the property--value-list pairs off the object.
  JS::Rooted<JSObject*> object(aCx, &aValue.toObject());
  nsTArray<PropertyValuesPair> propertyValuesPairs;
  if (!GetPropertyValuesPairs(aCx, object, ListAllowance::eAllow,
                              propertyValuesPairs)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // We must keep track of which properties we've already generated
  // an AnimationProperty since the author could have specified both a
  // shorthand and one of its component longhands on the
  // PropertyIndexedKeyframe.
  nsCSSPropertySet properties;

  // Create AnimationProperty objects for each PropertyValuesPair, applying
  // the "distribute" spacing algorithm to the segments.
  for (const PropertyValuesPair& pair : propertyValuesPairs) {
    size_t count = pair.mValues.Length();
    if (count == 0) {
      // No animation values for this property.
      continue;
    }
    if (count == 1) {
      // We don't support additive segments and so can't support an
      // animation that goes from the underlying value to this
      // specified value.  Throw an exception until we do support this.
      aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
      return;
    }

    // If we find an invalid value, we don't create a segment for it, but
    // we adjust the surrounding segments so that the timing of the segments
    // is the same as if we did support it.  For example, animating with
    // values ["red", "green", "yellow", "invalid", "blue"] will generate
    // segments with this timing:
    //
    //   0.00 -> 0.25 : red -> green
    //   0.25 -> 0.50 : green -> yellow
    //   0.50 -> 1.00 : yellow -> blue
    //
    // With future spec clarifications we might decide to preserve the invalid
    // value on the segment and make the animation code deal with the invalid
    // value instead.
    nsTArray<PropertyStyleAnimationValuePair> fromValues;
    float fromKey = 0.0f;
    if (!StyleAnimationValue::ComputeValues(pair.mProperty,
                                            nsCSSProps::eEnabledForAllContent,
                                            aTarget,
                                            aPseudoType,
                                            pair.mValues[0],
                                            /* aUseSVGMode */ false,
                                            fromValues)) {
      // We need to throw for an invalid first value, since that would imply an
      // additive animation, which we don't support yet.
      aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
      return;
    }

    if (fromValues.IsEmpty()) {
      // All longhand components of a shorthand pair.mProperty must be disabled.
      continue;
    }

    // Create AnimationProperty objects for each property that had a
    // value computed.  When pair.mProperty is a longhand, it is just
    // that property.  When pair.mProperty is a shorthand, we'll have
    // one property per longhand component.
    nsTArray<size_t> animationPropertyIndexes;
    animationPropertyIndexes.SetLength(fromValues.Length());
    for (size_t i = 0, n = fromValues.Length(); i < n; ++i) {
      nsCSSProperty p = fromValues[i].mProperty;
      bool found = false;
      if (properties.HasProperty(p)) {
        // We have already dealt with this property.  Look up and
        // overwrite the old AnimationProperty object.
        for (size_t j = 0, m = aResult.Length(); j < m; ++j) {
          if (aResult[j].mProperty == p) {
            aResult[j].mSegments.Clear();
            animationPropertyIndexes[i] = j;
            found = true;
            break;
          }
        }
        MOZ_ASSERT(found, "properties is inconsistent with aResult");
      }
      if (!found) {
        // This is the first time we've encountered this property.
        animationPropertyIndexes[i] = aResult.Length();
        AnimationProperty* animationProperty = aResult.AppendElement();
        animationProperty->mProperty = p;
        properties.AddProperty(p);
      }
    }

    double portion = 1.0 / (count - 1);
    for (size_t i = 0; i < count - 1; ++i) {
      nsTArray<PropertyStyleAnimationValuePair> toValues;
      float toKey = (i + 1) * portion;
      if (!StyleAnimationValue::ComputeValues(pair.mProperty,
                                              nsCSSProps::eEnabledForAllContent,
                                              aTarget,
                                              aPseudoType,
                                              pair.mValues[i + 1],
                                              /* aUseSVGMode */ false,
                                              toValues)) {
        if (i + 1 == count - 1) {
          // We need to throw for an invalid last value, since that would
          // imply an additive animation, which we don't support yet.
          aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
          return;
        }
        // Otherwise, skip the segment.
        continue;
      }
      MOZ_ASSERT(toValues.Length() == fromValues.Length(),
                 "should get the same number of properties as the last time "
                 "we called ComputeValues for pair.mProperty");
      for (size_t j = 0, n = toValues.Length(); j < n; ++j) {
        size_t index = animationPropertyIndexes[j];
        AnimationPropertySegment* segment =
          aResult[index].mSegments.AppendElement();
        segment->mFromKey = fromKey;
        segment->mFromValue = fromValues[j].mValue;
        segment->mToKey = toKey;
        segment->mToValue = toValues[j].mValue;
        segment->mTimingFunction = easing;
      }
      fromValues = Move(toValues);
      fromKey = toKey;
    }
  }
}

/**
 * Converts a JS object representing a property-indexed keyframe into
 * an array of Keyframe objects.
 *
 * @param aCx The JSContext for |aValue|.
 * @param aValue The JS object.
 * @param aResult The array into which the resulting AnimationProperty
 *   objects will be appended.
 * @param aRv Out param to store any errors thrown by this function.
 */
static void
GetKeyframeListFromPropertyIndexedKeyframe(JSContext* aCx,
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

  // Get the document to use for parsing CSS properties.
  nsIDocument* doc = AnimationUtils::GetCurrentRealmDocument(aCx);
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  Maybe<ComputedTimingFunction> easing =
    TimingParams::ParseEasing(keyframeDict.mEasing, doc, aRv);
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
  nsCSSParser parser(doc->CSSLoader());
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
        MakePropertyValuePair(pair.mProperty, stringValue, parser, doc));
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

  nsCSSPropertySet properties;              // All properties encountered.
  nsCSSPropertySet propertiesWithFromValue; // Those with a defined 0% value.
  nsCSSPropertySet propertiesWithToValue;   // Those with a defined 100% value.

  auto addToPropertySets = [&](nsCSSProperty aProperty, double aOffset) {
    properties.AddProperty(aProperty);
    if (aOffset == 0.0) {
      propertiesWithFromValue.AddProperty(aProperty);
    } else if (aOffset == 1.0) {
      propertiesWithToValue.AddProperty(aProperty);
    }
  };

  for (size_t i = 0, len = aKeyframes.Length(); i < len; i++) {
    const Keyframe& frame = aKeyframes[i];

    // We won't have called ApplyDistributeSpacing when this is called so
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
        nsCSSValueTokenStream* tokenStream = pair.mValue.GetTokenStreamValue();
        nsCSSParser parser(aDocument->CSSLoader());
        if (!parser.IsValueValidForProperty(pair.mProperty,
                                            tokenStream->mTokenStream)) {
          continue;
        }
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
            prop, pair.mProperty, nsCSSProps::eEnabledForAllContent) {
          addToPropertySets(*prop, offsetToUse);
        }
      } else {
        if (pair.mValue.GetUnit() == eCSSUnit_TokenStream) {
          continue;
        }
        addToPropertySets(pair.mProperty, offsetToUse);
      }
    }
  }

  return !propertiesWithFromValue.Equals(properties) ||
         !propertiesWithToValue.Equals(properties);
}

} // namespace mozilla
