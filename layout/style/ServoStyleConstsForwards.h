/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 * This file contains forward declarations and typedefs for types that cbindgen
 * cannot understand but renames / prefixes, and includes for some of the types
 * it needs.
 */

#ifndef mozilla_ServoStyleConsts_h
#  error "This file is only meant to be included from ServoStyleConsts.h"
#endif

#ifndef mozilla_ServoStyleConstsForwards_h
#  define mozilla_ServoStyleConstsForwards_h

#  include "nsColor.h"
#  include "nsCoord.h"
#  include "mozilla/AtomArray.h"
#  include "mozilla/IntegerRange.h"
#  include "mozilla/Span.h"
#  include "Units.h"
#  include "mozilla/gfx/Types.h"
#  include "mozilla/MemoryReporting.h"
#  include "mozilla/ServoTypes.h"
#  include "mozilla/ServoBindingTypes.h"
#  include "nsCSSPropertyID.h"
#  include "nsCompatibility.h"
#  include <atomic>

struct RawServoAnimationValueTable;
struct RawServoAnimationValueMap;

class nsAtom;
class nsIFrame;
class nsINode;
class nsCSSPropertyIDSet;
class nsPresContext;
class nsSimpleContentList;
struct nsCSSValueSharedList;
struct nsTimingFunction;

class gfxFontFeatureValueSet;
struct gfxFontFeature;
namespace mozilla {
namespace gfx {
struct FontVariation;
}
}  // namespace mozilla
typedef mozilla::gfx::FontVariation gfxFontVariation;

enum nsCSSUnit : uint32_t;
enum nsChangeHint : uint32_t;

namespace nsStyleTransformMatrix {
enum class MatrixTransformOperator : uint8_t;
}

template <typename T>
class nsMainThreadPtrHolder;

namespace mozilla {

class ComputedStyle;

using Matrix4x4Components = float[16];
using StyleMatrix4x4Components = Matrix4x4Components;

struct Keyframe;
struct PropertyStyleAnimationValuePair;

using ComputedKeyframeValues = nsTArray<PropertyStyleAnimationValuePair>;

class ComputedStyle;
class SeenPtrs;
class SharedFontList;
class StyleSheet;
class WritingMode;
class ServoElementSnapshotTable;
enum class StyleContentType : uint8_t;

template<typename T>
struct StyleForgottenArcSlicePtr;

struct AnimationPropertySegment;
struct ComputedTiming;
struct URLExtraData;

enum HalfCorner : uint8_t;
enum LogicalSide : uint8_t;
enum class PseudoStyleType : uint8_t;
enum class OriginFlags : uint8_t;

namespace css {
class Loader;
class LoaderReusableStyleSheets;
class SheetLoadData;
using SheetLoadDataHolder = nsMainThreadPtrHolder<SheetLoadData>;
struct URLValue;
enum SheetParsingMode : uint8_t;
}  // namespace css

namespace dom {
enum class IterationCompositeOperation : uint8_t;
enum class CallerType : uint32_t;

class Element;
class Document;
}  // namespace dom

// Work-around weird cbindgen renaming / avoiding moving stuff outside its
// namespace.

using StyleLoader = css::Loader;
using StyleLoaderReusableStyleSheets = css::LoaderReusableStyleSheets;
using StyleCallerType = dom::CallerType;
using StyleURLValue = css::URLValue;
using StyleSheetParsingMode = css::SheetParsingMode;
using StyleSheetLoadData = css::SheetLoadData;
using StyleSheetLoadDataHolder = css::SheetLoadDataHolder;
using StyleGeckoMallocSizeOf = MallocSizeOf;
using StyleDomStyleSheet = StyleSheet;

using StyleRawGeckoNode = nsINode;
using StyleRawGeckoElement = dom::Element;
using StyleDocument = dom::Document;
using StyleComputedValues = ComputedStyle;
using StyleIterationCompositeOperation = dom::IterationCompositeOperation;

using StyleMatrixTransformOperator =
    nsStyleTransformMatrix::MatrixTransformOperator;

#  define SERVO_ARC_TYPE(name_, type_) using Style##type_ = type_;
#  include "mozilla/ServoArcTypeList.h"
#  undef SERVO_ARC_TYPE

#  define SERVO_BOXED_TYPE(name_, type_) using Style##type_ = type_;
#  include "mozilla/ServoBoxedTypeList.h"
#  undef SERVO_BOXED_TYPE

using StyleAtomicUsize = std::atomic<size_t>;

}  // namespace mozilla

#endif
