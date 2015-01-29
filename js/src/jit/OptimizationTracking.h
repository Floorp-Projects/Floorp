/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_OptimizationTracking_h
#define jit_OptimizationTracking_h

#include "mozilla/Maybe.h"

#include "jsinfer.h"
#include "jit/CompactBuffer.h"
#include "jit/CompileInfo.h"
#include "jit/JitAllocPolicy.h"
#include "jit/shared/CodeGenerator-shared.h"

namespace js {

namespace jit {

#define TRACKED_STRATEGY_LIST(_)                        \
    _(GetProp_ArgumentsLength,                          \
      "getprop arguments.length")                       \
    _(GetProp_ArgumentsCallee,                          \
      "getprop arguments.callee")                       \
    _(GetProp_InferredConstant,                         \
      "getprop inferred constant")                      \
    _(GetProp_Constant,                                 \
      "getprop constant")                               \
    _(GetProp_TypedObject,                              \
      "getprop TypedObject")                            \
    _(GetProp_DefiniteSlot,                             \
      "getprop definite slot")                          \
    _(GetProp_CommonGetter,                             \
      "getprop common getter")                          \
    _(GetProp_InlineAccess,                             \
      "getprop inline access")                          \
    _(GetProp_Innerize,                                 \
      "getprop innerize (access on global window)")     \
    _(GetProp_InlineCache,                              \
      "getprop IC")                                     \
                                                        \
    _(SetProp_CommonSetter,                             \
      "setprop common setter")                          \
    _(SetProp_TypedObject,                              \
      "setprop TypedObject")                            \
    _(SetProp_DefiniteSlot,                             \
      "setprop definite slot")                          \
    _(SetProp_InlineAccess,                             \
      "setprop inline access")                          \
                                                        \
    _(GetElem_TypedObject,                              \
      "getprop TypedObject")                            \
    _(GetElem_Dense,                                    \
      "getelem dense")                                  \
    _(GetElem_TypedStatic,                              \
      "getelem TypedArray static")                      \
    _(GetElem_TypedArray,                               \
      "getelem TypedArray")                             \
    _(GetElem_String,                                   \
      "getelem string")                                 \
    _(GetElem_Arguments,                                \
      "getelem arguments")                              \
    _(GetElem_ArgumentsInlined,                         \
      "getelem arguments inlined")                      \
    _(GetElem_InlineCache,                              \
      "getelem IC")                                     \
                                                        \
    _(Call_Inline,                                      \
      "call inline")


// Ordering is important below. All outcomes before GenericSuccess will be
// considered failures, and all outcomes after GenericSuccess will be
// considered successes.
#define TRACKED_OUTCOME_LIST(_)                                         \
    _(GenericFailure,                                                   \
      "failure")                                                        \
    _(Disabled,                                                         \
      "disabled")                                                       \
    _(NoTypeInfo,                                                       \
      "no type info")                                                   \
    _(NoAnalysisInfo,                                                   \
      "no newscript analysis")                                          \
    _(NoShapeInfo,                                                      \
      "cannot determine shape")                                         \
    _(UnknownObject,                                                    \
      "unknown object")                                                 \
    _(UnknownProperties,                                                \
      "unknown properties")                                             \
    _(Singleton,                                                        \
      "is singleton")                                                   \
    _(NotSingleton,                                                     \
      "is not singleton")                                               \
    _(NotFixedSlot,                                                     \
      "property not in fixed slot")                                     \
    _(NotObject,                                                        \
      "not definitely an object")                                       \
    _(NotStruct,                                                        \
      "not definitely a TypedObject struct")                            \
    _(StructNoField,                                                    \
      "struct doesn't definitely have field")                           \
    _(NeedsTypeBarrier,                                                 \
      "needs type barrier")                                             \
    _(InDictionaryMode,                                                 \
      "object in dictionary mode")                                      \
    _(NoProtoFound,                                                     \
      "no proto found")                                                 \
    _(MultiProtoPaths,                                                  \
      "not all paths to property go through same proto")                \
    _(NonWritableProperty,                                              \
      "non-writable property")                                          \
    _(ProtoIndexedProps,                                                \
      "prototype has indexed properties")                               \
    _(ArrayBadFlags,                                                    \
      "array observed to be sparse, overflowed .length, or has been iterated") \
    _(ArrayDoubleConversion,                                            \
      "array has ambiguous double conversion")                          \
    _(ArrayRange,                                                       \
      "array range issue (.length problems)")                           \
    _(ArraySeenNegativeIndex,                                           \
      "has seen array access with negative index")                      \
    _(TypedObjectNeutered,                                              \
      "TypedObject might have been neutered")                           \
    _(TypedObjectArrayRange,                                            \
      "TypedObject array of unknown length")                            \
    _(AccessNotDense,                                                   \
      "access not on dense native (check receiver, index, and result types)") \
    _(AccessNotTypedObject,                                             \
      "access not on typed array (check receiver and index types)")     \
    _(AccessNotTypedArray,                                              \
      "access not on typed array (check receiver, index, and result types)") \
    _(AccessNotString,                                                  \
      "getelem not on string (check receiver and index types)")         \
    _(StaticTypedArrayUint32,                                           \
      "static uint32 arrays currently cannot be optimized")             \
    _(StaticTypedArrayCantComputeMask,                                  \
      "can't compute mask for static typed array access (index isn't constant or not int32)") \
    _(OutOfBounds,                                                      \
      "observed out of bounds access")                                  \
    _(GetElemStringNotCached,                                           \
      "getelem on strings is not inline cached")                        \
    _(NonNativeReceiver,                                                \
      "observed non-native receiver")                                   \
    _(IndexType,                                                        \
      "index type must be int32, string, or symbol")                    \
                                                                        \
    _(CantInlineGeneric,                                                \
      "can't inline")                                                   \
    _(CantInlineNoTarget,                                               \
      "can't inline: no target")                                        \
    _(CantInlineNotInterpreted,                                         \
      "can't inline: not interpreted")                                  \
    _(CantInlineNoBaseline,                                             \
      "can't inline: no baseline code")                                 \
    _(CantInlineLazy,                                                   \
      "can't inline: lazy script")                                      \
    _(CantInlineNotConstructor,                                         \
      "can't inline: calling non-constructor with 'new'")               \
    _(CantInlineDisabledIon,                                            \
      "can't inline: ion disabled for callee")                          \
    _(CantInlineTooManyArgs,                                            \
      "can't inline: too many arguments")                               \
    _(CantInlineRecursive,                                              \
      "can't inline: recursive")                                        \
    _(CantInlineHeavyweight,                                            \
      "can't inline: heavyweight")                                      \
    _(CantInlineNeedsArgsObj,                                           \
      "can't inline: needs arguments object")                           \
    _(CantInlineDebuggee,                                               \
      "can't inline: debuggee")                                         \
    _(CantInlineUnknownProps,                                           \
      "can't inline: type has unknown properties")                      \
    _(CantInlineExceededDepth,                                          \
      "can't inline: exceeded inlining depth")                          \
    _(CantInlineBigLoop,                                                \
      "can't inline: big function with a loop")                         \
    _(CantInlineBigCaller,                                              \
      "can't inline: big caller")                                       \
    _(CantInlineBigCallee,                                              \
      "can't inline: big callee")                                       \
    _(CantInlineNotHot,                                                 \
      "can't inline: not hot enough")                                   \
    _(CantInlineNotInDispatch,                                          \
      "can't inline: not in dispatch table")                            \
    _(CantInlineNativeBadForm,                                          \
      "can't inline native: bad form (arity mismatch/constructing)")    \
    _(CantInlineNativeBadType,                                          \
      "can't inline native: bad argument or return type observed")      \
    _(CantInlineNativeNoTemplateObj,                                    \
      "can't inline native: no template object")                        \
    _(CantInlineBound,                                                  \
      "can't inline bound function invocation")                         \
                                                                        \
    _(GenericSuccess,                                                   \
      "success")                                                        \
    _(Inlined,                                                          \
      "inlined")                                                        \
    _(DOM,                                                              \
      "DOM")                                                            \
    _(Monomorphic,                                                      \
      "monomorphic")                                                    \
    _(Polymorphic,                                                      \
      "polymorphic")

#define TRACKED_TYPESITE_LIST(_)                \
    _(Receiver,                                 \
      "receiver object")                        \
    _(Index,                                    \
      "index")                                  \
    _(Value,                                    \
      "value")                                  \
    _(Call_Target,                              \
      "call target")                            \
    _(Call_This,                                \
      "call 'this'")                            \
    _(Call_Arg,                                 \
      "call argument")                          \
    _(Call_Return,                              \
      "call return")

enum class TrackedStrategy : uint32_t {
#define STRATEGY_OP(name, msg) name,
    TRACKED_STRATEGY_LIST(STRATEGY_OP)
#undef STRATEGY_OPT

    Count
};

enum class TrackedOutcome : uint32_t {
#define OUTCOME_OP(name, msg) name,
    TRACKED_OUTCOME_LIST(OUTCOME_OP)
#undef OUTCOME_OP

    Count
};

enum class TrackedTypeSite : uint32_t {
#define TYPESITE_OP(name, msg) name,
    TRACKED_TYPESITE_LIST(TYPESITE_OP)
#undef TYPESITE_OP

    Count
};

class OptimizationAttempt
{
    TrackedStrategy strategy_;
    TrackedOutcome outcome_;

  public:
    OptimizationAttempt(TrackedStrategy strategy, TrackedOutcome outcome)
      : strategy_(strategy),
        outcome_(outcome)
    { }

    void setOutcome(TrackedOutcome outcome) { outcome_ = outcome; }
    bool succeeded() const { return outcome_ >= TrackedOutcome::GenericSuccess; }
    bool failed() const { return outcome_ < TrackedOutcome::GenericSuccess; }
    TrackedStrategy strategy() const { return strategy_; }
    TrackedOutcome outcome() const { return outcome_; }

    bool operator ==(const OptimizationAttempt &other) const {
        return strategy_ == other.strategy_ && outcome_ == other.outcome_;
    }
    bool operator !=(const OptimizationAttempt &other) const {
        return strategy_ != other.strategy_ || outcome_ != other.outcome_;
    }
    HashNumber hash() const {
        return (HashNumber(strategy_) << 8) + HashNumber(outcome_);
    }

    explicit OptimizationAttempt(CompactBufferReader &reader);
    void writeCompact(CompactBufferWriter &writer) const;
};

typedef Vector<OptimizationAttempt, 4, JitAllocPolicy> TempAttemptsVector;
typedef Vector<OptimizationAttempt, 4, SystemAllocPolicy> AttemptsVector;

class UniqueTrackedTypes;

class TrackedTypeInfo
{
    TrackedTypeSite site_;
    MIRType mirType_;
    types::TypeSet::TypeList types_;

  public:
    TrackedTypeInfo(TrackedTypeInfo &&other)
      : site_(other.site_),
        mirType_(other.mirType_),
        types_(mozilla::Move(other.types_))
    { }

    TrackedTypeInfo(TrackedTypeSite site, MIRType mirType)
      : site_(site),
        mirType_(mirType)
    { }

    bool trackTypeSet(types::TemporaryTypeSet *typeSet);
    bool trackType(types::Type type);

    TrackedTypeSite site() const { return site_; }
    MIRType mirType() const { return mirType_; }
    const types::TypeSet::TypeList &types() const { return types_; }

    bool operator ==(const TrackedTypeInfo &other) const;
    bool operator !=(const TrackedTypeInfo &other) const;

    HashNumber hash() const;

    // This constructor is designed to be used in conjunction with readTypes
    // below it. The same reader must be passed to readTypes after
    // instantiating the TrackedTypeInfo.
    explicit TrackedTypeInfo(CompactBufferReader &reader);
    bool readTypes(CompactBufferReader &reader, const types::TypeSet::TypeList *allTypes);
    bool writeCompact(CompactBufferWriter &writer, UniqueTrackedTypes &uniqueTypes) const;
};

typedef Vector<TrackedTypeInfo, 1, JitAllocPolicy> TempTrackedTypeInfoVector;
typedef Vector<TrackedTypeInfo, 1, SystemAllocPolicy> TrackedTypeInfoVector;

// Tracks the optimization attempts made at a bytecode location.
class TrackedOptimizations : public TempObject
{
    friend class UniqueTrackedOptimizations;
    TempTrackedTypeInfoVector types_;
    TempAttemptsVector attempts_;
    uint32_t currentAttempt_;

  public:
    explicit TrackedOptimizations(TempAllocator &alloc)
      : types_(alloc),
        attempts_(alloc),
        currentAttempt_(UINT32_MAX)
    { }

    bool trackTypeInfo(TrackedTypeInfo &&ty);

    bool trackAttempt(TrackedStrategy strategy);
    void amendAttempt(uint32_t index);
    void trackOutcome(TrackedOutcome outcome);
    void trackSuccess();

    bool matchTypes(const TempTrackedTypeInfoVector &other) const;
    bool matchAttempts(const TempAttemptsVector &other) const;

    void spew() const;
};

// Assigns each unique sequence of optimization attempts an index; outputs a
// compact table.
class UniqueTrackedOptimizations
{
  public:
    struct SortEntry
    {
        const TempTrackedTypeInfoVector *types;
        const TempAttemptsVector *attempts;
        uint32_t frequency;
    };
    typedef Vector<SortEntry, 4> SortedVector;

  private:
    struct Key
    {
        const TempTrackedTypeInfoVector *types;
        const TempAttemptsVector *attempts;

        typedef Key Lookup;
        static HashNumber hash(const Lookup &lookup);
        static bool match(const Key &key, const Lookup &lookup);
        static void rekey(Key &key, const Key &newKey) {
            key = newKey;
        }
    };

    struct Entry
    {
        uint8_t index;
        uint32_t frequency;
    };

    // Map of unique (TempTrackedTypeInfoVector, TempAttemptsVector) pairs to
    // indices.
    typedef HashMap<Key, Entry, Key> AttemptsMap;
    AttemptsMap map_;

    // TempAttemptsVectors sorted by frequency.
    SortedVector sorted_;

  public:
    explicit UniqueTrackedOptimizations(JSContext *cx)
      : map_(cx),
        sorted_(cx)
    { }

    bool init() { return map_.init(); }
    bool add(const TrackedOptimizations *optimizations);

    bool sortByFrequency(JSContext *cx);
    bool sorted() const { return !sorted_.empty(); }
    uint32_t count() const { MOZ_ASSERT(sorted()); return sorted_.length(); }
    const SortedVector &sortedVector() const { MOZ_ASSERT(sorted()); return sorted_; }
    uint8_t indexOf(const TrackedOptimizations *optimizations) const;
};

// A compact table of tracked optimization information. Pictorially,
//
//    +------------------------------------------------+
//    |  Region 1                                      |  |
//    |------------------------------------------------|  |
//    |  Region 2                                      |  |
//    |------------------------------------------------|  |-- PayloadR of list-of-list of
//    |               ...                              |  |   range triples (see below)
//    |------------------------------------------------|  |
//    |  Region M                                      |  |
//    +================================================+ <- IonTrackedOptimizationsRegionTable
//    | uint32_t numRegions_ = M                       |  |
//    +------------------------------------------------+  |
//    | Region 1                                       |  |
//    |   uint32_t regionOffset = size(PayloadR)       |  |
//    +------------------------------------------------+  |-- Table
//    |   ...                                          |  |
//    +------------------------------------------------+  |
//    | Region M                                       |  |
//    |   uint32_t regionOffset                        |  |
//    +================================================+
//    |  Optimization type info 1                      |  |
//    |------------------------------------------------|  |
//    |  Optimization type info 2                      |  |-- PayloadT of list of
//    |------------------------------------------------|  |   IonTrackedOptimizationTypeInfo in
//    |               ...                              |  |   order of decreasing frequency
//    |------------------------------------------------|  |
//    |  Optimization type info N                      |  |
//    +================================================+ <- IonTrackedOptimizationsTypesTable
//    | uint32_t numEntries_ = N                       |  |
//    +------------------------------------------------+  |
//    | Optimization type info 1                       |  |
//    |   uint32_t entryOffset = size(PayloadT)        |  |
//    +------------------------------------------------+  |-- Table
//    |   ...                                          |  |
//    +------------------------------------------------+  |
//    | Optimization type info N                       |  |
//    |   uint32_t entryOffset                         |  |
//    +================================================+
//    |  Optimization attempts 1                       |  |
//    |------------------------------------------------|  |
//    |  Optimization attempts 2                       |  |-- PayloadA of list of
//    |------------------------------------------------|  |   IonTrackedOptimizationAttempts in
//    |               ...                              |  |   order of decreasing frequency
//    |------------------------------------------------|  |
//    |  Optimization attempts N                       |  |
//    +================================================+ <- IonTrackedOptimizationsAttemptsTable
//    | uint32_t numEntries_ = N                       |  |
//    +------------------------------------------------+  |
//    | Optimization attempts 1                        |  |
//    |   uint32_t entryOffset = size(PayloadA)        |  |
//    +------------------------------------------------+  |-- Table
//    |   ...                                          |  |
//    +------------------------------------------------+  |
//    | Optimization attempts N                        |  |
//    |   uint32_t entryOffset                         |  |
//    +------------------------------------------------+
//
// Abstractly, each region in the PayloadR section is a list of triples of the
// following, in order of ascending startOffset:
//
//     (startOffset, endOffset, optimization attempts index)
//
// The range of [startOffset, endOffset) is the native machine code offsets
// for which the optimization attempts referred to by the index applies.
//
// Concretely, each region starts with a header of:
//
//     { startOffset : 32, endOffset : 32 }
//
// followed by an (endOffset, index) pair, then by delta-encoded variants
// triples described below.
//
// Each list of type infos in the PayloadT section is a list of triples:
//
//     (kind, MIR type, type set)
//
// The type set is separately in another vector, and what is encoded instead
// is the (offset, length) pair needed to index into that vector.
//
// Each list of optimization attempts in the PayloadA section is a list of
// pairs:
//
//     (strategy, outcome)
//
// Both tail tables for PayloadR and PayloadA use reverse offsets from the
// table pointers.

class IonTrackedOptimizationsRegion
{
    const uint8_t *start_;
    const uint8_t *end_;

    // Unpacked state.
    uint32_t startOffset_;
    uint32_t endOffset_;
    const uint8_t *rangesStart_;

    void unpackHeader();

  public:
    IonTrackedOptimizationsRegion(const uint8_t *start, const uint8_t *end)
      : start_(start), end_(end),
        startOffset_(0), endOffset_(0), rangesStart_(nullptr)
    {
        MOZ_ASSERT(start < end);
        unpackHeader();
    }

    // Offsets for the entire range that this region covers.
    //
    // This, as well as the offsets for the deltas, is open at the ending
    // address: [startOffset, endOffset).
    uint32_t startOffset() const { return startOffset_; }
    uint32_t endOffset() const { return endOffset_; }

    class RangeIterator {
        const uint8_t *cur_;
        const uint8_t *start_;
        const uint8_t *end_;

        uint32_t firstStartOffset_;
        uint32_t prevEndOffset_;

      public:
        RangeIterator(const uint8_t *start, const uint8_t *end, uint32_t startOffset)
          : cur_(start), start_(start), end_(end),
            firstStartOffset_(startOffset), prevEndOffset_(0)
        { }

        bool more() const { return cur_ < end_; }
        void readNext(uint32_t *startOffset, uint32_t *endOffset, uint8_t *index);
    };

    RangeIterator ranges() const { return RangeIterator(rangesStart_, end_, startOffset_); }

    mozilla::Maybe<uint8_t> findAttemptsIndex(uint32_t offset) const;

    // For the variants below, S stands for startDelta, L for length, and I
    // for index. These were automatically generated from training on the
    // Octane benchmark.
    //
    // byte 1    byte 0
    // SSSS-SSSL LLLL-LII0
    //     startDelta max 127, length max 63, index max 3

    static const uint32_t ENC1_MASK = 0x1;
    static const uint32_t ENC1_MASK_VAL = 0x0;

    static const uint32_t ENC1_START_DELTA_MAX = 0x7f;
    static const uint32_t ENC1_START_DELTA_SHIFT = 9;

    static const uint32_t ENC1_LENGTH_MAX = 0x3f;
    static const uint32_t ENC1_LENGTH_SHIFT = 3;

    static const uint32_t ENC1_INDEX_MAX = 0x3;
    static const uint32_t ENC1_INDEX_SHIFT = 1;

    // byte 2    byte 1    byte 0
    // SSSS-SSSS SSSS-LLLL LLII-II01
    //     startDelta max 4095, length max 63, index max 15

    static const uint32_t ENC2_MASK = 0x3;
    static const uint32_t ENC2_MASK_VAL = 0x1;

    static const uint32_t ENC2_START_DELTA_MAX = 0xfff;
    static const uint32_t ENC2_START_DELTA_SHIFT = 12;

    static const uint32_t ENC2_LENGTH_MAX = 0x3f;
    static const uint32_t ENC2_LENGTH_SHIFT = 6;

    static const uint32_t ENC2_INDEX_MAX = 0xf;
    static const uint32_t ENC2_INDEX_SHIFT = 2;

    // byte 3    byte 2    byte 1    byte 0
    // SSSS-SSSS SSSL-LLLL LLLL-LIII IIII-I011
    //     startDelta max 2047, length max 1023, index max 255

    static const uint32_t ENC3_MASK = 0x7;
    static const uint32_t ENC3_MASK_VAL = 0x3;

    static const uint32_t ENC3_START_DELTA_MAX = 0x7ff;
    static const uint32_t ENC3_START_DELTA_SHIFT = 21;

    static const uint32_t ENC3_LENGTH_MAX = 0x3ff;
    static const uint32_t ENC3_LENGTH_SHIFT = 11;

    static const uint32_t ENC3_INDEX_MAX = 0xff;
    static const uint32_t ENC3_INDEX_SHIFT = 3;

    // byte 4    byte 3    byte 2    byte 1    byte 0
    // SSSS-SSSS SSSS-SSSL LLLL-LLLL LLLL-LIII IIII-I111
    //     startDelta max 32767, length max 16383, index max 255

    static const uint32_t ENC4_MASK = 0x7;
    static const uint32_t ENC4_MASK_VAL = 0x7;

    static const uint32_t ENC4_START_DELTA_MAX = 0x7fff;
    static const uint32_t ENC4_START_DELTA_SHIFT = 25;

    static const uint32_t ENC4_LENGTH_MAX = 0x3fff;
    static const uint32_t ENC4_LENGTH_SHIFT = 11;

    static const uint32_t ENC4_INDEX_MAX = 0xff;
    static const uint32_t ENC4_INDEX_SHIFT = 3;

    static bool IsDeltaEncodeable(uint32_t startDelta, uint32_t length) {
        MOZ_ASSERT(length != 0);
        return startDelta <= ENC4_START_DELTA_MAX && length <= ENC4_LENGTH_MAX;
    }

    static const uint32_t MAX_RUN_LENGTH = 100;

    typedef CodeGeneratorShared::NativeToTrackedOptimizations NativeToTrackedOptimizations;
    static uint32_t ExpectedRunLength(const NativeToTrackedOptimizations *start,
                                      const NativeToTrackedOptimizations *end);

    static void ReadDelta(CompactBufferReader &reader, uint32_t *startDelta, uint32_t *length,
                          uint8_t *index);
    static void WriteDelta(CompactBufferWriter &writer, uint32_t startDelta, uint32_t length,
                           uint8_t index);
    static bool WriteRun(CompactBufferWriter &writer,
                         const NativeToTrackedOptimizations *start,
                         const NativeToTrackedOptimizations *end,
                         const UniqueTrackedOptimizations &unique);
};

class IonTrackedOptimizationsAttempts
{
    const uint8_t *start_;
    const uint8_t *end_;

  public:
    IonTrackedOptimizationsAttempts(const uint8_t *start, const uint8_t *end)
      : start_(start), end_(end)
    {
        // Cannot be empty.
        MOZ_ASSERT(start < end);
    }

    template <class T>
    bool readVector(T *attempts) {
        CompactBufferReader reader(start_, end_);
        const uint8_t *cur = start_;
        while (cur != end_) {
            if (!attempts->append(OptimizationAttempt(reader)))
                return false;
            cur = reader.currentPosition();
            MOZ_ASSERT(cur <= end_);
        }
        return true;
    }
};

class IonTrackedOptimizationsTypeInfo
{
    const uint8_t *start_;
    const uint8_t *end_;

  public:
    IonTrackedOptimizationsTypeInfo(const uint8_t *start, const uint8_t *end)
      : start_(start), end_(end)
    {
        // Can be empty; i.e., no type info was tracked.
    }

    bool empty() const { return start_ == end_; }

    template <class T>
    bool readVector(T *types, const types::TypeSet::TypeList *allTypes) {
        CompactBufferReader reader(start_, end_);
        const uint8_t *cur = start_;
        while (cur != end_) {
            TrackedTypeInfo ty(reader);
            if (!ty.readTypes(reader, allTypes))
                return false;
            if (!types->append(mozilla::Move(ty)))
                return false;
            cur = reader.currentPosition();
            MOZ_ASSERT(cur <= end_);
        }
        return true;
    }
};

template <class Entry>
class IonTrackedOptimizationsOffsetsTable
{
    uint32_t padding_;
    uint32_t numEntries_;
    uint32_t entryOffsets_[1];

  protected:
    const uint8_t *payloadEnd() const {
        return (uint8_t *)(this) - padding_;
    }

  public:
    uint32_t numEntries() const { return numEntries_; }
    uint32_t entryOffset(uint32_t index) const {
        MOZ_ASSERT(index < numEntries());
        return entryOffsets_[index];
    }

    Entry entry(uint32_t index) const {
        const uint8_t *start = payloadEnd() - entryOffset(index);
        const uint8_t *end = payloadEnd();
        if (index < numEntries() - 1)
            end -= entryOffset(index + 1);
        return Entry(start, end);
    }
};

class IonTrackedOptimizationsRegionTable
  : public IonTrackedOptimizationsOffsetsTable<IonTrackedOptimizationsRegion>
{
  public:
    mozilla::Maybe<IonTrackedOptimizationsRegion> findRegion(uint32_t offset) const;

    const uint8_t *payloadStart() const { return payloadEnd() - entryOffset(0); }
};

typedef IonTrackedOptimizationsOffsetsTable<IonTrackedOptimizationsAttempts>
    IonTrackedOptimizationsAttemptsTable;

typedef IonTrackedOptimizationsOffsetsTable<IonTrackedOptimizationsTypeInfo>
    IonTrackedOptimizationsTypesTable;

bool
WriteIonTrackedOptimizationsTable(JSContext *cx, CompactBufferWriter &writer,
                                  const CodeGeneratorShared::NativeToTrackedOptimizations *start,
                                  const CodeGeneratorShared::NativeToTrackedOptimizations *end,
                                  const UniqueTrackedOptimizations &unique,
                                  uint32_t *numRegions, uint32_t *regionTableOffsetp,
                                  uint32_t *typesTableOffsetp, uint32_t *attemptsTableOffsetp,
                                  types::TypeSet::TypeList *allTypes);

} // namespace jit
} // namespace js

#endif // jit_OptimizationTracking_h
