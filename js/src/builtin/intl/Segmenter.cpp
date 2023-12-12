/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.Segmenter implementation. */

#include "builtin/intl/Segmenter.h"

#include "mozilla/Assertions.h"
#include "mozilla/Range.h"
#include "mozilla/UniquePtr.h"

#if defined(MOZ_ICU4X)
#  include "mozilla/intl/ICU4XGeckoDataProvider.h"
#  include "ICU4XGraphemeClusterSegmenter.h"
#  include "ICU4XSentenceSegmenter.h"
#  include "ICU4XWordSegmenter.h"
#endif

#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/Array.h"
#include "builtin/intl/CommonFunctions.h"
#include "gc/AllocKind.h"
#include "gc/GCContext.h"
#include "js/CallArgs.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/StableStringChars.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "util/Unicode.h"
#include "vm/ArrayObject.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/PlainObject.h"
#include "vm/WellKnownAtom.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

const JSClass SegmenterObject::class_ = {
    "Intl.Segmenter",
    JSCLASS_HAS_RESERVED_SLOTS(SegmenterObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Segmenter),
    nullptr,
    &SegmenterObject::classSpec_,
};

const JSClass& SegmenterObject::protoClass_ = PlainObject::class_;

static bool segmenter_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().Segmenter);
  return true;
}

static const JSFunctionSpec segmenter_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_Segmenter_supportedLocalesOf",
                      1, 0),
    JS_FS_END,
};

static const JSFunctionSpec segmenter_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_Segmenter_resolvedOptions", 0,
                      0),
    JS_SELF_HOSTED_FN("segment", "Intl_Segmenter_segment", 1, 0),
    JS_FN("toSource", segmenter_toSource, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec segmenter_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Intl.Segmenter", JSPROP_READONLY),
    JS_PS_END,
};

static bool Segmenter(JSContext* cx, unsigned argc, Value* vp);

const ClassSpec SegmenterObject::classSpec_ = {
    GenericCreateConstructor<Segmenter, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<SegmenterObject>,
    segmenter_static_methods,
    nullptr,
    segmenter_methods,
    segmenter_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};

/**
 * Intl.Segmenter ([ locales [ , options ]])
 */
static bool Segmenter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Intl.Segmenter")) {
    return false;
  }

  // Steps 2-3 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Segmenter,
                                          &proto)) {
    return false;
  }

  Rooted<SegmenterObject*> segmenter(cx);
  segmenter = NewObjectWithClassProto<SegmenterObject>(cx, proto);
  if (!segmenter) {
    return false;
  }

  HandleValue locales = args.get(0);
  HandleValue options = args.get(1);

  // Steps 4-13.
  if (!intl::InitializeObject(cx, segmenter, cx->names().InitializeSegmenter,
                              locales, options)) {
    return false;
  }

  // Step 14.
  args.rval().setObject(*segmenter);
  return true;
}

const JSClass SegmentsObject::class_ = {
    "Intl.Segments",
    JSCLASS_HAS_RESERVED_SLOTS(SegmentsObject::SLOT_COUNT),
};

static const JSFunctionSpec segments_methods[] = {
    JS_SELF_HOSTED_FN("containing", "Intl_Segments_containing", 1, 0),
    JS_SELF_HOSTED_SYM_FN(iterator, "Intl_Segments_iterator", 0, 0),
    JS_FS_END,
};

bool GlobalObject::initSegmentsProto(JSContext* cx,
                                     Handle<GlobalObject*> global) {
  Rooted<JSObject*> proto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!proto) {
    return false;
  }

  if (!JS_DefineFunctions(cx, proto, segments_methods)) {
    return false;
  }

  global->initBuiltinProto(ProtoKind::SegmentsProto, proto);
  return true;
}

const JSClass SegmentIteratorObject::class_ = {
    "Intl.SegmentIterator",
    JSCLASS_HAS_RESERVED_SLOTS(SegmentIteratorObject::SLOT_COUNT),
};

static const JSFunctionSpec segment_iterator_methods[] = {
    JS_SELF_HOSTED_FN("next", "Intl_SegmentIterator_next", 0, 0),
    JS_FS_END,
};

static const JSPropertySpec segment_iterator_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Segmenter String Iterator", JSPROP_READONLY),
    JS_PS_END,
};

bool GlobalObject::initSegmentIteratorProto(JSContext* cx,
                                            Handle<GlobalObject*> global) {
  Rooted<JSObject*> iteratorProto(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!iteratorProto) {
    return false;
  }

  Rooted<JSObject*> proto(
      cx, GlobalObject::createBlankPrototypeInheriting<PlainObject>(
              cx, iteratorProto));
  if (!proto) {
    return false;
  }

  if (!JS_DefineFunctions(cx, proto, segment_iterator_methods)) {
    return false;
  }

  if (!JS_DefineProperties(cx, proto, segment_iterator_properties)) {
    return false;
  }

  global->initBuiltinProto(ProtoKind::SegmentIteratorProto, proto);
  return true;
}

struct Boundaries {
  // Start index of this segmentation boundary.
  int32_t startIndex = 0;

  // End index of this segmentation boundary.
  int32_t endIndex = 0;

  // |true| if the segment is word-like. (Only used for word segmentation.)
  bool isWordLike = false;
};

/**
 * Find the segmentation boundary for the string character whose position is
 * |index|.
 */
template <class T>
static Boundaries FindBoundaryFrom(const T& iter, int32_t index) {
  int32_t previous = 0;
  while (true) {
    // Find the next possible break index.
    int32_t next = iter.next();

    // If |next| is larger than the search index, we've found our segment end
    // index.
    if (next > index) {
      return {previous, next, iter.isWordLike()};
    }

    // Otherwise store |next| as the start index of the next segment,
    previous = next;
  }
}

// TODO: Consider switching to the ICU4X C++ headers when the C++ headers
// are in better shape: https://github.com/rust-diplomat/diplomat/issues/280

template <typename Interface>
class SegmenterBreakIteratorType {
  struct Deleter {
    void operator()(typename Interface::BreakIterator* ptr) const {
      Interface::destroy(ptr);
    }
  };
  mozilla::UniquePtr<typename Interface::BreakIterator, Deleter> impl_;

  static const auto* toUnsigned(const JS::Latin1Char* chars) {
    return reinterpret_cast<const uint8_t*>(chars);
  }

  static const auto* toUnsigned(const char16_t* chars) {
    return reinterpret_cast<const uint16_t*>(chars);
  }

 public:
  SegmenterBreakIteratorType(
      typename Interface::Segmenter* segmenter,
      mozilla::Span<const typename Interface::Char> chars) {
    impl_.reset(
        Interface::create(segmenter, toUnsigned(chars.data()), chars.size()));
  }

  int32_t next() const { return Interface::next(impl_.get()); }

  bool isWordLike() const { return Interface::isWordLike(impl_.get()); }
};

template <typename Interface>
class SegmenterType {
  struct Deleter {
    void operator()(typename Interface::Segmenter* ptr) const {
      Interface::destroy(ptr);
    }
  };
  mozilla::UniquePtr<typename Interface::Segmenter, Deleter> impl_;

 public:
  SegmenterType() {
    auto result = Interface::create(mozilla::intl::GetDataProvider());
    if (result.is_ok) {
      impl_.reset(result.ok);
    }
  }

  explicit operator bool() const { return bool(impl_); }

  Boundaries find(const JSLinearString* string, int32_t index) {
    // Tell the analysis ICU4X functions can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    if (string->hasLatin1Chars()) {
      auto chars = string->latin1Range(nogc);
      typename Interface::BreakIteratorLatin1 iter(impl_.get(), chars);
      return FindBoundaryFrom(iter, index);
    } else {
      auto chars = string->twoByteRange(nogc);
      typename Interface::BreakIteratorTwoByte iter(impl_.get(), chars);
      return FindBoundaryFrom(iter, index);
    }
  }
};

#if defined(MOZ_ICU4X)
// Each SegmenterBreakIterator interface contains the following definitions:
//
// - BreakIterator: Type of the ICU4X break iterator.
// - Segmenter: Type of the ICU4X segmenter.
// - Char: Character type, either `JS::Latin1Char` or `char16_t`.
// - create: Static method to create a new instance of `BreakIterator`.
// - destroy: Static method to destroy an instance of `BreakIterator`.
// - next: Static method to fetch the next break iteration index.
// - isWordLike: Static method to determine if the current segment is word-like.
//
//
// Each Segmenter interface contains the following definitions:
//
// - Segmenter: Type of the ICU4X segmenter.
// - BreakIteratorLatin1: SegmenterBreakIterator interface to Latin1 strings.
// - BreakIteratorTwoByte: SegmenterBreakIterator interface to TwoByte strings.
// - create: Static method to create a new instance of `Segmenter`.
// - destroy: Static method to destroy an instance of `Segmenter`.

struct GraphemeClusterSegmenterBreakIteratorLatin1 {
  using BreakIterator = capi::ICU4XGraphemeClusterBreakIteratorLatin1;
  using Segmenter = capi::ICU4XGraphemeClusterSegmenter;
  using Char = JS::Latin1Char;

  static constexpr auto& create =
      capi::ICU4XGraphemeClusterSegmenter_segment_latin1;
  static constexpr auto& destroy =
      capi::ICU4XGraphemeClusterBreakIteratorLatin1_destroy;
  static constexpr auto& next =
      capi::ICU4XGraphemeClusterBreakIteratorLatin1_next;

  static bool isWordLike(const BreakIterator*) { return false; }
};

struct GraphemeClusterSegmenterBreakIteratorTwoByte {
  using BreakIterator = capi::ICU4XGraphemeClusterBreakIteratorUtf16;
  using Segmenter = capi::ICU4XGraphemeClusterSegmenter;
  using Char = char16_t;

  static constexpr auto& create =
      capi::ICU4XGraphemeClusterSegmenter_segment_utf16;
  static constexpr auto& destroy =
      capi::ICU4XGraphemeClusterBreakIteratorUtf16_destroy;
  static constexpr auto& next =
      capi::ICU4XGraphemeClusterBreakIteratorUtf16_next;

  static bool isWordLike(const BreakIterator*) { return false; }
};

struct GraphemeClusterSegmenter {
  using Segmenter = capi::ICU4XGraphemeClusterSegmenter;
  using BreakIteratorLatin1 =
      SegmenterBreakIteratorType<GraphemeClusterSegmenterBreakIteratorLatin1>;
  using BreakIteratorTwoByte =
      SegmenterBreakIteratorType<GraphemeClusterSegmenterBreakIteratorTwoByte>;

  static constexpr auto& create = capi::ICU4XGraphemeClusterSegmenter_create;
  static constexpr auto& destroy = capi::ICU4XGraphemeClusterSegmenter_destroy;
};

struct WordSegmenterBreakIteratorLatin1 {
  using BreakIterator = capi::ICU4XWordBreakIteratorLatin1;
  using Segmenter = capi::ICU4XWordSegmenter;
  using Char = JS::Latin1Char;

  static constexpr auto& create = capi::ICU4XWordSegmenter_segment_latin1;
  static constexpr auto& destroy = capi::ICU4XWordBreakIteratorLatin1_destroy;
  static constexpr auto& next = capi::ICU4XWordBreakIteratorLatin1_next;
  static constexpr auto& isWordLike =
      capi::ICU4XWordBreakIteratorLatin1_is_word_like;
};

struct WordSegmenterBreakIteratorTwoByte {
  using BreakIterator = capi::ICU4XWordBreakIteratorUtf16;
  using Segmenter = capi::ICU4XWordSegmenter;
  using Char = char16_t;

  static constexpr auto& create = capi::ICU4XWordSegmenter_segment_utf16;
  static constexpr auto& destroy = capi::ICU4XWordBreakIteratorUtf16_destroy;
  static constexpr auto& next = capi::ICU4XWordBreakIteratorUtf16_next;
  static constexpr auto& isWordLike =
      capi::ICU4XWordBreakIteratorUtf16_is_word_like;
};

struct WordSegmenter {
  using Segmenter = capi::ICU4XWordSegmenter;
  using BreakIteratorLatin1 =
      SegmenterBreakIteratorType<WordSegmenterBreakIteratorLatin1>;
  using BreakIteratorTwoByte =
      SegmenterBreakIteratorType<WordSegmenterBreakIteratorTwoByte>;

  static constexpr auto& create = capi::ICU4XWordSegmenter_create_auto;
  static constexpr auto& destroy = capi::ICU4XWordSegmenter_destroy;
};

struct SentenceSegmenterBreakIteratorLatin1 {
  using BreakIterator = capi::ICU4XSentenceBreakIteratorLatin1;
  using Segmenter = capi::ICU4XSentenceSegmenter;
  using Char = JS::Latin1Char;

  static constexpr auto& create = capi::ICU4XSentenceSegmenter_segment_latin1;
  static constexpr auto& destroy =
      capi::ICU4XSentenceBreakIteratorLatin1_destroy;
  static constexpr auto& next = capi::ICU4XSentenceBreakIteratorLatin1_next;

  static bool isWordLike(const BreakIterator*) { return false; }
};

struct SentenceSegmenterBreakIteratorTwoByte {
  using BreakIterator = capi::ICU4XSentenceBreakIteratorUtf16;
  using Segmenter = capi::ICU4XSentenceSegmenter;
  using Char = char16_t;

  static constexpr auto& create = capi::ICU4XSentenceSegmenter_segment_utf16;
  static constexpr auto& destroy =
      capi::ICU4XSentenceBreakIteratorUtf16_destroy;
  static constexpr auto& next = capi::ICU4XSentenceBreakIteratorUtf16_next;

  static bool isWordLike(const BreakIterator*) { return false; }
};

struct SentenceSegmenter {
  using Segmenter = capi::ICU4XSentenceSegmenter;
  using BreakIteratorLatin1 =
      SegmenterBreakIteratorType<SentenceSegmenterBreakIteratorLatin1>;
  using BreakIteratorTwoByte =
      SegmenterBreakIteratorType<SentenceSegmenterBreakIteratorTwoByte>;

  static constexpr auto& create = capi::ICU4XSentenceSegmenter_create;
  static constexpr auto& destroy = capi::ICU4XSentenceSegmenter_destroy;
};
#endif

static bool EnsureInternalsResolved(JSContext* cx,
                                    Handle<SegmenterObject*> segmenter) {
  if (segmenter->getLocale()) {
    return true;
  }

  Rooted<JS::Value> value(cx);

  Rooted<JSObject*> internals(cx, intl::GetInternalsObject(cx, segmenter));
  if (!internals) {
    return false;
  }

  if (!GetProperty(cx, internals, internals, cx->names().locale, &value)) {
    return false;
  }
  Rooted<JSString*> locale(cx, value.toString());

  if (!GetProperty(cx, internals, internals, cx->names().granularity, &value)) {
    return false;
  }

  SegmenterGranularity granularity;
  {
    JSLinearString* linear = value.toString()->ensureLinear(cx);
    if (!linear) {
      return false;
    }

    if (StringEqualsLiteral(linear, "grapheme")) {
      granularity = SegmenterGranularity::Grapheme;
    } else if (StringEqualsLiteral(linear, "word")) {
      granularity = SegmenterGranularity::Word;
    } else {
      MOZ_ASSERT(StringEqualsLiteral(linear, "sentence"));
      granularity = SegmenterGranularity::Sentence;
    }
  }

  segmenter->setLocale(locale);
  segmenter->setGranularity(granularity);

  return true;
}

static bool GraphemeBoundaries(JSContext* cx, Handle<JSLinearString*> string,
                               int32_t index, Boundaries* boundaries) {
#if defined(MOZ_ICU4X)
  SegmenterType<GraphemeClusterSegmenter> segmenter{};
  if (!segmenter) {
    intl::ReportInternalError(cx);
    return false;
  }

  *boundaries = segmenter.find(string, index);
  return true;
#else
  MOZ_CRASH("ICU4X disabled");
#endif
}

static bool WordBoundaries(JSContext* cx, Handle<JSLinearString*> string,
                           int32_t index, Boundaries* boundaries) {
#if defined(MOZ_ICU4X)
  SegmenterType<WordSegmenter> segmenter{};
  if (!segmenter) {
    intl::ReportInternalError(cx);
    return false;
  }

  *boundaries = segmenter.find(string, index);
  return true;
#else
  MOZ_CRASH("ICU4X disabled");
#endif
}

static bool SentenceBoundaries(JSContext* cx, Handle<JSLinearString*> string,
                               int32_t index, Boundaries* boundaries) {
#if defined(MOZ_ICU4X)
  SegmenterType<SentenceSegmenter> segmenter{};
  if (!segmenter) {
    intl::ReportInternalError(cx);
    return false;
  }

  *boundaries = segmenter.find(string, index);
  return true;
#else
  MOZ_CRASH("ICU4X disabled");
#endif
}

/**
 * Create the boundaries result array for self-hosted code.
 */
static ArrayObject* CreateBoundaries(JSContext* cx, Boundaries boundaries,
                                     SegmenterGranularity granularity) {
  auto [startIndex, endIndex, isWordLike] = boundaries;

  auto* result = NewDenseFullyAllocatedArray(cx, 3);
  if (!result) {
    return nullptr;
  }
  result->setDenseInitializedLength(3);
  result->initDenseElement(0, Int32Value(startIndex));
  result->initDenseElement(1, Int32Value(endIndex));
  if (granularity == SegmenterGranularity::Word) {
    result->initDenseElement(2, BooleanValue(isWordLike));
  } else {
    result->initDenseElement(2, UndefinedValue());
  }
  return result;
}

bool js::intl_CreateSegmentsObject(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 2);

  Rooted<SegmenterObject*> segmenter(cx,
                                     &args[0].toObject().as<SegmenterObject>());
  Rooted<JSString*> string(cx, args[1].toString());

  // Ensure the internal properties are resolved.
  if (!EnsureInternalsResolved(cx, segmenter)) {
    return false;
  }

  Rooted<JSObject*> proto(
      cx, GlobalObject::getOrCreateSegmentsPrototype(cx, cx->global()));
  if (!proto) {
    return false;
  }

  auto* segments = NewObjectWithGivenProto<SegmentsObject>(cx, proto);
  if (!segments) {
    return false;
  }

  segments->setSegmenter(segmenter);
  segments->setString(string);

  args.rval().setObject(*segments);
  return true;
}

bool js::intl_CreateSegmentIterator(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);

  Rooted<SegmentsObject*> segments(cx,
                                   &args[0].toObject().as<SegmentsObject>());

  Rooted<JSObject*> proto(
      cx, GlobalObject::getOrCreateSegmentIteratorPrototype(cx, cx->global()));
  if (!proto) {
    return false;
  }

  auto* iterator = NewObjectWithGivenProto<SegmentIteratorObject>(cx, proto);
  if (!iterator) {
    return false;
  }

  iterator->setSegmenter(segments->getSegmenter());
  iterator->setString(segments->getString());
  iterator->setIndex(0);

  args.rval().setObject(*iterator);
  return true;
}

bool js::intl_FindSegmentBoundaries(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 2);

  Rooted<SegmentsObject*> segments(cx,
                                   &args[0].toObject().as<SegmentsObject>());

  Rooted<JSLinearString*> string(cx, segments->getString()->ensureLinear(cx));
  if (!string) {
    return false;
  }

  int32_t index = args[1].toInt32();
  MOZ_ASSERT(index >= 0);
  MOZ_ASSERT(uint32_t(index) < string->length());

  SegmenterGranularity granularity = segments->getSegmenter()->getGranularity();

  Boundaries boundaries{};
  switch (granularity) {
    case SegmenterGranularity::Grapheme:
      if (!GraphemeBoundaries(cx, string, index, &boundaries)) {
        return false;
      }
      break;
    case SegmenterGranularity::Word:
      if (!WordBoundaries(cx, string, index, &boundaries)) {
        return false;
      }
      break;
    case SegmenterGranularity::Sentence:
      if (!SentenceBoundaries(cx, string, index, &boundaries)) {
        return false;
      }
      break;
  }

  auto* result = CreateBoundaries(cx, boundaries, granularity);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

bool js::intl_FindNextSegmentBoundaries(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);

  Rooted<SegmentIteratorObject*> iterator(
      cx, &args[0].toObject().as<SegmentIteratorObject>());

  Rooted<JSLinearString*> string(cx, iterator->getString()->ensureLinear(cx));
  if (!string) {
    return false;
  }

  int32_t index = iterator->getIndex();
  MOZ_ASSERT(index >= 0);
  MOZ_ASSERT(uint32_t(index) < string->length());

  SegmenterGranularity granularity = iterator->getSegmenter()->getGranularity();

  Boundaries boundaries{};
  switch (granularity) {
    case SegmenterGranularity::Grapheme:
      if (!GraphemeBoundaries(cx, string, index, &boundaries)) {
        return false;
      }
      break;
    case SegmenterGranularity::Word:
      if (!WordBoundaries(cx, string, index, &boundaries)) {
        return false;
      }
      break;
    case SegmenterGranularity::Sentence:
      if (!SentenceBoundaries(cx, string, index, &boundaries)) {
        return false;
      }
      break;
  }

  iterator->setIndex(boundaries.endIndex);

  auto* result = CreateBoundaries(cx, boundaries, granularity);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}
