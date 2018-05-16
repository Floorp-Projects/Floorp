/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoBindingTypes_h
#define mozilla_ServoBindingTypes_h

#include "mozilla/RefPtr.h"
#include "mozilla/ServoComputedData.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/SheetType.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/Types.h"
#include "nsCSSPropertyID.h"
#include "nsStyleAutoArray.h"
#include "nsTArray.h"

struct RawServoAuthorStyles;
struct RawServoStyleSet;
struct RawServoSelectorList;
struct RawServoSourceSizeList;
struct RawServoAnimationValueMap;
struct RustString;

#define SERVO_ARC_TYPE(name_, type_) struct type_;
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

namespace mozilla {
class ServoElementSnapshot;
class ComputedStyle;
struct StyleAnimation;
struct URLExtraData;
namespace dom {
class Element;
class StyleChildrenIterator;
} // namespace dom
struct AnimationPropertySegment;
struct ComputedTiming;
struct Keyframe;
struct PropertyValuePair;
struct PropertyStyleAnimationValuePair;
enum class OriginFlags : uint8_t;
using ComputedKeyframeValues = nsTArray<PropertyStyleAnimationValuePair>;
} // namespace mozilla
namespace nsStyleTransformMatrix {
enum class MatrixTransformOperator: uint8_t;
}

class nsCSSPropertyIDSet;
class nsCSSValue;
struct nsFontFaceRuleContainer;
class nsIDocument;
class nsINode;
class nsPresContext;
class nsSimpleContentList;
struct nsTimingFunction;
class nsXBLBinding;

using mozilla::dom::StyleChildrenIterator;
using mozilla::ServoElementSnapshot;

typedef void* RawServoAnimationValueTableBorrowed;

typedef nsINode RawGeckoNode;
typedef mozilla::dom::Element RawGeckoElement;
typedef nsIDocument RawGeckoDocument;
typedef nsPresContext RawGeckoPresContext;
typedef nsXBLBinding RawGeckoXBLBinding;
typedef mozilla::URLExtraData RawGeckoURLExtraData;
typedef nsTArray<RefPtr<RawServoAnimationValue>> RawGeckoServoAnimationValueList;
typedef nsTArray<mozilla::Keyframe> RawGeckoKeyframeList;
typedef nsTArray<mozilla::PropertyValuePair> RawGeckoPropertyValuePairList;
typedef nsTArray<mozilla::ComputedKeyframeValues> RawGeckoComputedKeyframeValuesList;
typedef nsStyleAutoArray<mozilla::StyleAnimation> RawGeckoStyleAnimationList;
typedef nsTArray<nsFontFaceRuleContainer> RawGeckoFontFaceRuleList;
typedef mozilla::AnimationPropertySegment RawGeckoAnimationPropertySegment;
typedef mozilla::ComputedTiming RawGeckoComputedTiming;
typedef nsTArray<const RawServoStyleRule*> RawGeckoServoStyleRuleList;
typedef nsTArray<nsCSSPropertyID> RawGeckoCSSPropertyIDList;
typedef mozilla::gfx::Float RawGeckoGfxMatrix4x4[16];
typedef mozilla::dom::StyleChildrenIterator RawGeckoStyleChildrenIterator;

// We have these helper types so that we can directly generate
// things like &T or Borrowed<T> on the Rust side in the function, providing
// additional safety benefits.
//
// FFI has a problem with templated types, so we just use raw pointers here.
//
// The "Borrowed" types generate &T or Borrowed<T> in the nullable case.
//
// The "Owned" types generate Owned<T> or OwnedOrNull<T>. Some of these
// are Servo-managed and can be converted to Box<ServoType> on the
// Servo side.
//
// The "Arc" types are Servo-managed Arc<ServoType>s, which are passed
// over FFI as Strong<T> (which is nullable).
// Note that T != ServoType, rather T is ArcInner<ServoType>
#define DECL_BORROWED_REF_TYPE_FOR(type_) typedef type_ const* type_##Borrowed;
#define DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_) typedef type_ const* type_##BorrowedOrNull;
#define DECL_BORROWED_MUT_REF_TYPE_FOR(type_) typedef type_* type_##BorrowedMut;
#define DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR(type_) typedef type_* type_##BorrowedMutOrNull;

#define SERVO_ARC_TYPE(name_, type_)         \
  DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_) \
  DECL_BORROWED_REF_TYPE_FOR(type_)          \
  DECL_BORROWED_MUT_REF_TYPE_FOR(type_)      \
  struct MOZ_MUST_USE_TYPE type_##Strong     \
  {                                          \
    type_* mPtr;                             \
    already_AddRefed<type_> Consume();       \
  };
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

typedef mozilla::ComputedStyle const* ComputedStyleBorrowed;
typedef mozilla::ComputedStyle const* ComputedStyleBorrowedOrNull;
typedef ServoComputedData const* ServoComputedDataBorrowed;

struct MOZ_MUST_USE_TYPE ComputedStyleStrong
{
  mozilla::ComputedStyle* mPtr;
  already_AddRefed<mozilla::ComputedStyle> Consume();
};

#define DECL_OWNED_REF_TYPE_FOR(type_)    \
  typedef type_* type_##Owned;            \
  DECL_BORROWED_REF_TYPE_FOR(type_)       \
  DECL_BORROWED_MUT_REF_TYPE_FOR(type_)

#define DECL_NULLABLE_OWNED_REF_TYPE_FOR(type_)    \
  typedef type_* type_##OwnedOrNull;               \
  DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_)       \
  DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR(type_)

// This is a reference to a reference of RawServoDeclarationBlock, which
// corresponds to Option<&Arc<RawServoDeclarationBlock>> in Servo side.
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawServoDeclarationBlockStrong)
DECL_OWNED_REF_TYPE_FOR(RawServoAuthorStyles)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawServoAuthorStyles)
DECL_OWNED_REF_TYPE_FOR(RawServoStyleSet)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawServoStyleSet)
DECL_NULLABLE_OWNED_REF_TYPE_FOR(StyleChildrenIterator)
DECL_OWNED_REF_TYPE_FOR(StyleChildrenIterator)
DECL_OWNED_REF_TYPE_FOR(ServoElementSnapshot)
DECL_OWNED_REF_TYPE_FOR(RawServoAnimationValueMap)

// We don't use BorrowedMut because the nodes may alias
// Servo itself doesn't directly read or mutate these;
// it only asks Gecko to do so. In case we wish to in
// the future, we should ensure that things being mutated
// are protected from noalias violations by a cell type
DECL_BORROWED_REF_TYPE_FOR(RawGeckoNode)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoNode)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoElement)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoElement)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoDocument)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoDocument)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoXBLBinding)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoXBLBinding)
DECL_BORROWED_MUT_REF_TYPE_FOR(StyleChildrenIterator)
DECL_BORROWED_MUT_REF_TYPE_FOR(ServoElementSnapshot)
DECL_BORROWED_REF_TYPE_FOR(nsCSSValue)
DECL_BORROWED_MUT_REF_TYPE_FOR(nsCSSValue)
DECL_OWNED_REF_TYPE_FOR(RawGeckoPresContext)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoPresContext)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoServoAnimationValueList)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoServoAnimationValueList)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoKeyframeList)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoKeyframeList)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoPropertyValuePairList)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoPropertyValuePairList)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoComputedKeyframeValuesList)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoStyleAnimationList)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoStyleAnimationList)
DECL_BORROWED_MUT_REF_TYPE_FOR(nsTimingFunction)
DECL_BORROWED_REF_TYPE_FOR(nsTimingFunction)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoFontFaceRuleList)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoAnimationPropertySegment)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoComputedTiming)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoServoStyleRuleList)
DECL_BORROWED_MUT_REF_TYPE_FOR(nsCSSPropertyIDSet)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoCSSPropertyIDList)
DECL_BORROWED_REF_TYPE_FOR(nsXBLBinding)
DECL_BORROWED_MUT_REF_TYPE_FOR(RawGeckoStyleChildrenIterator)
DECL_OWNED_REF_TYPE_FOR(RawServoSelectorList)
DECL_BORROWED_REF_TYPE_FOR(RawServoSelectorList)
DECL_OWNED_REF_TYPE_FOR(RawServoSourceSizeList)
DECL_BORROWED_REF_TYPE_FOR(RawServoSourceSizeList)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawServoSourceSizeList)

#undef DECL_ARC_REF_TYPE_FOR
#undef DECL_OWNED_REF_TYPE_FOR
#undef DECL_NULLABLE_OWNED_REF_TYPE_FOR
#undef DECL_BORROWED_REF_TYPE_FOR
#undef DECL_NULLABLE_BORROWED_REF_TYPE_FOR
#undef DECL_BORROWED_MUT_REF_TYPE_FOR
#undef DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR

#define SERVO_ARC_TYPE(name_, type_)                 \
  extern "C" {                                       \
  void Servo_##name_##_AddRef(type_##Borrowed ptr);  \
  void Servo_##name_##_Release(type_##Borrowed ptr); \
  }                                                  \
  namespace mozilla {                                \
  template<> struct RefPtrTraits<type_> {            \
    static void AddRef(type_* aPtr) {                \
      Servo_##name_##_AddRef(aPtr);                  \
    }                                                \
    static void Release(type_* aPtr) {               \
      Servo_##name_##_Release(aPtr);                 \
    }                                                \
  };                                                 \
  }
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

#define DEFINE_BOXED_TYPE(name_, type_)                     \
  extern "C" void Servo_##name_##_Drop(type_##Owned ptr);   \
  namespace mozilla {                                       \
  template<>                                                \
  class DefaultDelete<type_>                                \
  {                                                         \
  public:                                                   \
    void operator()(type_* aPtr) const                      \
    {                                                       \
      Servo_##name_##_Drop(aPtr);                           \
    }                                                       \
  };                                                        \
  }

DEFINE_BOXED_TYPE(StyleSet, RawServoStyleSet);
DEFINE_BOXED_TYPE(AuthorStyles, RawServoAuthorStyles);
DEFINE_BOXED_TYPE(SelectorList, RawServoSelectorList);
DEFINE_BOXED_TYPE(SourceSizeList, RawServoSourceSizeList);

#undef DEFINE_BOXED_TYPE

// used for associating sheet type with specific @font-face rules
struct nsFontFaceRuleContainer
{
  RefPtr<RawServoFontFaceRule> mRule;
  mozilla::SheetType mSheetType;
};

#endif // mozilla_ServoBindingTypes_h
