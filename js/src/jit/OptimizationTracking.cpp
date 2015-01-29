/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/OptimizationTracking.h"

#include "ds/Sort.h"
#include "jit/IonBuilder.h"
#include "jit/JitcodeMap.h"
#include "jit/JitSpewer.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;
using mozilla::Some;
using mozilla::Nothing;

typedef CodeGeneratorShared::NativeToTrackedOptimizations NativeToTrackedOptimizations;

bool
TrackedOptimizations::trackTypeInfo(TrackedTypeInfo &&ty)
{
    return types_.append(mozilla::Move(ty));
}

bool
TrackedOptimizations::trackAttempt(TrackedStrategy strategy)
{
    OptimizationAttempt attempt(strategy, TrackedOutcome::GenericFailure);
    currentAttempt_ = attempts_.length();
    return attempts_.append(attempt);
}

void
TrackedOptimizations::amendAttempt(uint32_t index)
{
    currentAttempt_ = index;
}

void
TrackedOptimizations::trackOutcome(TrackedOutcome outcome)
{
    attempts_[currentAttempt_].setOutcome(outcome);
}

void
TrackedOptimizations::trackSuccess()
{
    attempts_[currentAttempt_].setOutcome(TrackedOutcome::GenericSuccess);
}

template <class Vec>
static bool
VectorContentsMatch(const Vec *xs, const Vec *ys)
{
    if (xs->length() != ys->length())
        return false;
    for (auto x = xs->begin(), y = ys->begin(); x != xs->end(); x++, y++) {
        MOZ_ASSERT(y != ys->end());
        if (*x != *y)
            return false;
    }
    return true;
}

bool
TrackedOptimizations::matchTypes(const TempTrackedTypeInfoVector &other) const
{
    return VectorContentsMatch(&types_, &other);
}

bool
TrackedOptimizations::matchAttempts(const TempAttemptsVector &other) const
{
    return VectorContentsMatch(&attempts_, &other);
}

#ifdef DEBUG
static const char *
StrategyString(TrackedStrategy strategy)
{
    switch (strategy) {
#define STRATEGY_CASE(name, msg)                  \
      case TrackedStrategy::name:                 \
        return msg;
    TRACKED_STRATEGY_LIST(STRATEGY_CASE)
#undef STRATEGY_CASE

      default:
        MOZ_CRASH("bad strategy");
    }
}

static const char *
OutcomeString(TrackedOutcome outcome)
{
    switch (outcome) {
#define OUTCOME_CASE(name, msg)                   \
      case TrackedOutcome::name:                  \
        return msg;
      TRACKED_OUTCOME_LIST(OUTCOME_CASE)
#undef OUTCOME_CASE

      default:
        MOZ_CRASH("bad outcome");
    }
}

static const char *
TypeSiteString(TrackedTypeSite kind)
{
    switch (kind) {
#define TYPESITE_CASE(name, msg)                  \
      case TrackedTypeSite::name:                 \
        return msg;
      TRACKED_TYPESITE_LIST(TYPESITE_CASE)
#undef TYPESITE_CASE

      default:
        MOZ_CRASH("bad type site");
    }
}
#endif // DEBUG

void
SpewTempTrackedTypeInfoVector(const TempTrackedTypeInfoVector *types, const char *indent = nullptr)
{
#ifdef DEBUG
    for (const TrackedTypeInfo *t = types->begin(); t != types->end(); t++) {
        JitSpewStart(JitSpew_OptimizationTracking, "   %s%s of type %s, type set", indent ? indent : "",
                     TypeSiteString(t->site()), StringFromMIRType(t->mirType()));
        for (uint32_t i = 0; i < t->types().length(); i++)
            JitSpewCont(JitSpew_OptimizationTracking, " %s", types::TypeString(t->types()[i]));
        JitSpewFin(JitSpew_OptimizationTracking);
    }
#endif
}

void
SpewTempAttemptsVector(const TempAttemptsVector *attempts, const char *indent = nullptr)
{
#ifdef DEBUG
    for (const OptimizationAttempt *a = attempts->begin(); a != attempts->end(); a++) {
        JitSpew(JitSpew_OptimizationTracking, "   %s%s: %s", indent ? indent : "",
                StrategyString(a->strategy()), OutcomeString(a->outcome()));
    }
#endif
}

void
TrackedOptimizations::spew() const
{
#ifdef DEBUG
    SpewTempTrackedTypeInfoVector(&types_);
    SpewTempAttemptsVector(&attempts_);
#endif
}

bool
TrackedTypeInfo::trackTypeSet(types::TemporaryTypeSet *typeSet)
{
    if (!typeSet)
        return true;
    return typeSet->enumerateTypes(&types_);
}

bool
TrackedTypeInfo::trackType(types::Type type)
{
    return types_.append(type);
}

bool
TrackedTypeInfo::operator ==(const TrackedTypeInfo &other) const
{
    return site_ == other.site_ && mirType_ == other.mirType_ &&
           VectorContentsMatch(&types_, &other.types_);
}

bool
TrackedTypeInfo::operator !=(const TrackedTypeInfo &other) const
{
    return !(*this == other);
}

static inline HashNumber
CombineHash(HashNumber h, HashNumber n)
{
    h += n;
    h += (h << 10);
    h ^= (h >> 6);
    return h;
}

static inline HashNumber
HashType(types::Type ty)
{
    if (ty.isObjectUnchecked())
        return PointerHasher<types::TypeObjectKey *, 3>::hash(ty.objectKey());
    return HashNumber(ty.raw());
}

static HashNumber
HashTypeList(const types::TypeSet::TypeList &types)
{
    HashNumber h = 0;
    for (uint32_t i = 0; i < types.length(); i++)
        h = CombineHash(h, HashType(types[i]));
    return h;
}

HashNumber
TrackedTypeInfo::hash() const
{
    return ((HashNumber(site_) << 24) + (HashNumber(mirType_) << 16)) ^ HashTypeList(types_);
}

template <class Vec>
static HashNumber
HashVectorContents(const Vec *xs, HashNumber h)
{
    for (auto x = xs->begin(); x != xs->end(); x++)
        h = CombineHash(h, x->hash());
    return h;
}

/* static */ HashNumber
UniqueTrackedOptimizations::Key::hash(const Lookup &lookup)
{
    HashNumber h = HashVectorContents(lookup.types, 0);
    h = HashVectorContents(lookup.attempts, h);
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    return h;
}

/* static */ bool
UniqueTrackedOptimizations::Key::match(const Key &key, const Lookup &lookup)
{
    return VectorContentsMatch(key.attempts, lookup.attempts) &&
           VectorContentsMatch(key.types, lookup.types);
}

bool
UniqueTrackedOptimizations::add(const TrackedOptimizations *optimizations)
{
    MOZ_ASSERT(!sorted());
    Key key;
    key.types = &optimizations->types_;
    key.attempts = &optimizations->attempts_;
    AttemptsMap::AddPtr p = map_.lookupForAdd(key);
    if (p) {
        p->value().frequency++;
        return true;
    }
    Entry entry;
    entry.index = UINT8_MAX;
    entry.frequency = 1;
    return map_.add(p, key, entry);
}

struct FrequencyComparator
{
    bool operator()(const UniqueTrackedOptimizations::SortEntry &a,
                    const UniqueTrackedOptimizations::SortEntry &b,
                    bool *lessOrEqualp)
    {
        *lessOrEqualp = b.frequency <= a.frequency;
        return true;
    }
};

bool
UniqueTrackedOptimizations::sortByFrequency(JSContext *cx)
{
    MOZ_ASSERT(!sorted());

    JitSpew(JitSpew_OptimizationTracking, "=> Sorting unique optimizations by frequency");

    // Sort by frequency.
    Vector<SortEntry> entries(cx);
    for (AttemptsMap::Range r = map_.all(); !r.empty(); r.popFront()) {
        SortEntry entry;
        entry.types = r.front().key().types;
        entry.attempts = r.front().key().attempts;
        entry.frequency = r.front().value().frequency;
        if (!entries.append(entry))
            return false;
    }

    // The compact table stores indices as a max of uint8_t. In practice each
    // script has fewer unique optimization attempts than UINT8_MAX.
    if (entries.length() >= UINT8_MAX - 1)
        return false;

    Vector<SortEntry> scratch(cx);
    if (!scratch.resize(entries.length()))
        return false;

    FrequencyComparator comparator;
    MOZ_ALWAYS_TRUE(MergeSort(entries.begin(), entries.length(), scratch.begin(), comparator));

    // Update map entries' indices.
    for (size_t i = 0; i < entries.length(); i++) {
        Key key;
        key.types = entries[i].types;
        key.attempts = entries[i].attempts;
        AttemptsMap::Ptr p = map_.lookup(key);
        MOZ_ASSERT(p);
        p->value().index = sorted_.length();

        JitSpew(JitSpew_OptimizationTracking, "   Entry %u has frequency %u",
                sorted_.length(), p->value().frequency);

        if (!sorted_.append(entries[i]))
            return false;
    }

    return true;
}

uint8_t
UniqueTrackedOptimizations::indexOf(const TrackedOptimizations *optimizations) const
{
    MOZ_ASSERT(sorted());
    Key key;
    key.types = &optimizations->types_;
    key.attempts = &optimizations->attempts_;
    AttemptsMap::Ptr p = map_.lookup(key);
    MOZ_ASSERT(p);
    MOZ_ASSERT(p->value().index != UINT8_MAX);
    return p->value().index;
}

// Assigns each unique tracked type an index; outputs a compact list.
class jit::UniqueTrackedTypes
{
  public:
    struct TypeHasher
    {
        typedef types::Type Lookup;

        static HashNumber hash(const Lookup &ty) { return HashType(ty); }
        static bool match(const types::Type &ty1, const types::Type &ty2) { return ty1 == ty2; }
    };

  private:
    // Map of unique types::Types to indices.
    typedef HashMap<types::Type, uint8_t, TypeHasher> TypesMap;
    TypesMap map_;

    Vector<types::Type, 1> list_;

  public:
    explicit UniqueTrackedTypes(JSContext *cx)
      : map_(cx),
        list_(cx)
    { }

    bool init() { return map_.init(); }
    bool getIndexOf(types::Type ty, uint8_t *indexp);

    uint32_t count() const { MOZ_ASSERT(map_.count() == list_.length()); return list_.length(); }
    bool enumerate(types::TypeSet::TypeList *types) const;
};

bool
UniqueTrackedTypes::getIndexOf(types::Type ty, uint8_t *indexp)
{
    TypesMap::AddPtr p = map_.lookupForAdd(ty);
    if (p) {
        *indexp = p->value();
        return true;
    }

    // Store indices as max of uint8_t. In practice each script has fewer than
    // UINT8_MAX of unique observed types.
    if (count() >= UINT8_MAX)
        return false;

    uint8_t index = (uint8_t) count();
    if (!map_.add(p, ty, index))
        return false;
    if (!list_.append(ty))
        return false;
    *indexp = index;
    return true;
}

bool
UniqueTrackedTypes::enumerate(types::TypeSet::TypeList *types) const
{
    return types->append(list_.begin(), list_.end());
}

void
IonTrackedOptimizationsRegion::unpackHeader()
{
    CompactBufferReader reader(start_, end_);
    startOffset_ = reader.readUnsigned();
    endOffset_ = reader.readUnsigned();
    rangesStart_ = reader.currentPosition();
    MOZ_ASSERT(startOffset_ < endOffset_);
}

void
IonTrackedOptimizationsRegion::RangeIterator::readNext(uint32_t *startOffset, uint32_t *endOffset,
                                                       uint8_t *index)
{
    MOZ_ASSERT(more());

    CompactBufferReader reader(cur_, end_);

    // The very first entry isn't delta-encoded.
    if (cur_ == start_) {
        *startOffset = firstStartOffset_;
        *endOffset = prevEndOffset_ = reader.readUnsigned();
        *index = reader.readByte();
        cur_ = reader.currentPosition();
        MOZ_ASSERT(cur_ <= end_);
        return;
    }

    // Otherwise, read a delta.
    uint32_t startDelta, length;
    ReadDelta(reader, &startDelta, &length, index);
    *startOffset = prevEndOffset_ + startDelta;
    *endOffset = prevEndOffset_ = *startOffset + length;
    cur_ = reader.currentPosition();
    MOZ_ASSERT(cur_ <= end_);
}

bool
JitcodeGlobalEntry::IonEntry::optimizationAttemptsAtAddr(void *ptr,
                                                         Maybe<AttemptsVector> &attempts)
{
    MOZ_ASSERT(!attempts);
    MOZ_ASSERT(containsPointer(ptr));
    uint32_t ptrOffset = ((uint8_t *) ptr) - ((uint8_t *) nativeStartAddr());
    Maybe<IonTrackedOptimizationsRegion> region = optsRegionTable_->findRegion(ptrOffset);
    if (region.isNothing())
        return true;
    Maybe<uint8_t> attemptsIdx = region->findAttemptsIndex(ptrOffset);
    if (attemptsIdx.isNothing())
        return true;
    IonTrackedOptimizationsAttempts attemptsEntry = optsAttemptsTable_->entry(*attemptsIdx);
    attempts.emplace();
    if (!attemptsEntry.readVector(attempts.ptr()))
        return false;
    return true;
}

Maybe<uint8_t>
IonTrackedOptimizationsRegion::findAttemptsIndex(uint32_t offset) const
{
    if (offset < startOffset_ || offset >= endOffset_)
        return Nothing();

    // Linear search through the run.
    RangeIterator iter = ranges();
    while (iter.more()) {
        uint32_t startOffset, endOffset;
        uint8_t index;
        iter.readNext(&startOffset, &endOffset, &index);
        if (startOffset <= offset && offset < endOffset)
            return Some(index);
    }
    return Nothing();
}

Maybe<IonTrackedOptimizationsRegion>
IonTrackedOptimizationsRegionTable::findRegion(uint32_t offset) const
{
    static const uint32_t LINEAR_SEARCH_THRESHOLD = 8;
    uint32_t regions = numEntries();
    MOZ_ASSERT(regions > 0);

    // For small numbers of regions, do linear search.
    if (regions <= LINEAR_SEARCH_THRESHOLD) {
        for (uint32_t i = 0; i < regions; i++) {
            IonTrackedOptimizationsRegion region = entry(i);
            if (region.startOffset() <= offset && offset < region.endOffset()) {
                return Some(entry(i));
            }
        }
        return Nothing();
    }

    // Otherwise, do binary search.
    uint32_t i = 0;
    while (regions > 1) {
        uint32_t step = regions / 2;
        uint32_t mid = i + step;
        IonTrackedOptimizationsRegion region = entry(mid);

        if (offset < region.startOffset()) {
            // Entry is below mid.
            regions = step;
        } else if (offset >= region.endOffset()) {
            // Entry is above mid.
            i = mid;
            regions -= step;
        } else {
            // Entry is in mid.
            return Some(entry(i));
        }
    }
    return Nothing();
}

/* static */ uint32_t
IonTrackedOptimizationsRegion::ExpectedRunLength(const NativeToTrackedOptimizations *start,
                                                 const NativeToTrackedOptimizations *end)
{
    MOZ_ASSERT(start < end);

    // A run always has at least 1 entry, which is not delta encoded.
    uint32_t runLength = 1;
    uint32_t prevEndOffset = start->endOffset.offset();

    for (const NativeToTrackedOptimizations *entry = start + 1; entry != end; entry++) {
        uint32_t startOffset = entry->startOffset.offset();
        uint32_t endOffset = entry->endOffset.offset();
        uint32_t startDelta = startOffset - prevEndOffset;
        uint32_t length = endOffset - startOffset;

        if (!IsDeltaEncodeable(startDelta, length))
            break;

        runLength++;
        if (runLength == MAX_RUN_LENGTH)
            break;

        prevEndOffset = endOffset;
    }

    return runLength;
}

void
OptimizationAttempt::writeCompact(CompactBufferWriter &writer) const
{
    writer.writeUnsigned((uint32_t) strategy_);
    writer.writeUnsigned((uint32_t) outcome_);
}

bool
TrackedTypeInfo::writeCompact(CompactBufferWriter &writer,
                              UniqueTrackedTypes &uniqueTypes) const
{
    writer.writeUnsigned((uint32_t) site_);
    writer.writeUnsigned((uint32_t) mirType_);
    writer.writeUnsigned(types_.length());
    for (uint32_t i = 0; i < types_.length(); i++) {
        uint8_t index;
        if (!uniqueTypes.getIndexOf(types_[i], &index))
            return false;
        writer.writeByte(index);
    }
    return true;
}

/* static */ void
IonTrackedOptimizationsRegion::ReadDelta(CompactBufferReader &reader,
                                         uint32_t *startDelta, uint32_t *length,
                                         uint8_t *index)
{
    // 2 bytes
    // SSSS-SSSL LLLL-LII0
    const uint32_t firstByte = reader.readByte();
    const uint32_t secondByte = reader.readByte();
    if ((firstByte & ENC1_MASK) == ENC1_MASK_VAL) {
        uint32_t encVal = firstByte | secondByte << 8;
        *startDelta = encVal >> ENC1_START_DELTA_SHIFT;
        *length = (encVal >> ENC1_LENGTH_SHIFT) & ENC1_LENGTH_MAX;
        *index = (encVal >> ENC1_INDEX_SHIFT) & ENC1_INDEX_MAX;
        MOZ_ASSERT(length != 0);
        return;
    }

    // 3 bytes
    // SSSS-SSSS SSSS-LLLL LLII-II01
    const uint32_t thirdByte = reader.readByte();
    if ((firstByte & ENC2_MASK) == ENC2_MASK_VAL) {
        uint32_t encVal = firstByte | secondByte << 8 | thirdByte << 16;
        *startDelta = encVal >> ENC2_START_DELTA_SHIFT;
        *length = (encVal >> ENC2_LENGTH_SHIFT) & ENC2_LENGTH_MAX;
        *index = (encVal >> ENC2_INDEX_SHIFT) & ENC2_INDEX_MAX;
        MOZ_ASSERT(length != 0);
        return;
    }

    // 4 bytes
    // SSSS-SSSS SSSL-LLLL LLLL-LIII IIII-I011
    const uint32_t fourthByte = reader.readByte();
    if ((firstByte & ENC3_MASK) == ENC3_MASK_VAL) {
        uint32_t encVal = firstByte | secondByte << 8 | thirdByte << 16 | fourthByte << 24;
        *startDelta = encVal >> ENC3_START_DELTA_SHIFT;
        *length = (encVal >> ENC3_LENGTH_SHIFT) & ENC3_LENGTH_MAX;
        *index = (encVal >> ENC3_INDEX_SHIFT) & ENC3_INDEX_MAX;
        MOZ_ASSERT(length != 0);
        return;
    }

    // 5 bytes
    // SSSS-SSSS SSSS-SSSL LLLL-LLLL LLLL-LIII IIII-I111
    MOZ_ASSERT((firstByte & ENC4_MASK) == ENC4_MASK_VAL);
    uint64_t fifthByte = reader.readByte();
    uint64_t encVal = firstByte | secondByte << 8 | thirdByte << 16 | fourthByte << 24 |
                      fifthByte << 32;
    *startDelta = encVal >> ENC4_START_DELTA_SHIFT;
    *length = (encVal >> ENC4_LENGTH_SHIFT) & ENC4_LENGTH_MAX;
    *index = (encVal >> ENC4_INDEX_SHIFT) & ENC4_INDEX_MAX;
    MOZ_ASSERT(length != 0);
}

/* static */ void
IonTrackedOptimizationsRegion::WriteDelta(CompactBufferWriter &writer,
                                          uint32_t startDelta, uint32_t length,
                                          uint8_t index)
{
    // 2 bytes
    // SSSS-SSSL LLLL-LII0
    if (startDelta <= ENC1_START_DELTA_MAX &&
        length <= ENC1_LENGTH_MAX &&
        index <= ENC1_INDEX_MAX)
    {
        uint16_t val = ENC1_MASK_VAL |
                       (startDelta << ENC1_START_DELTA_SHIFT) |
                       (length << ENC1_LENGTH_SHIFT) |
                       (index << ENC1_INDEX_SHIFT);
        writer.writeByte(val & 0xff);
        writer.writeByte((val >> 8) & 0xff);
        return;
    }

    // 3 bytes
    // SSSS-SSSS SSSS-LLLL LLII-II01
    if (startDelta <= ENC2_START_DELTA_MAX &&
        length <= ENC2_LENGTH_MAX &&
        index <= ENC2_INDEX_MAX)
    {
        uint32_t val = ENC2_MASK_VAL |
                       (startDelta << ENC2_START_DELTA_SHIFT) |
                       (length << ENC2_LENGTH_SHIFT) |
                       (index << ENC2_INDEX_SHIFT);
        writer.writeByte(val & 0xff);
        writer.writeByte((val >> 8) & 0xff);
        writer.writeByte((val >> 16) & 0xff);
        return;
    }

    // 4 bytes
    // SSSS-SSSS SSSL-LLLL LLLL-LIII IIII-I011
    if (startDelta <= ENC3_START_DELTA_MAX &&
        length <= ENC3_LENGTH_MAX)
    {
        // index always fits because it's an uint8_t; change this if
        // ENC3_INDEX_MAX changes.
        MOZ_ASSERT(ENC3_INDEX_MAX == UINT8_MAX);
        uint32_t val = ENC3_MASK_VAL |
                       (startDelta << ENC3_START_DELTA_SHIFT) |
                       (length << ENC3_LENGTH_SHIFT) |
                       (index << ENC3_INDEX_SHIFT);
        writer.writeByte(val & 0xff);
        writer.writeByte((val >> 8) & 0xff);
        writer.writeByte((val >> 16) & 0xff);
        writer.writeByte((val >> 24) & 0xff);
        return;
    }

    // 5 bytes
    // SSSS-SSSS SSSS-SSSL LLLL-LLLL LLLL-LIII IIII-I111
    if (startDelta <= ENC4_START_DELTA_MAX &&
        length <= ENC4_LENGTH_MAX)
    {
        // index always fits because it's an uint8_t; change this if
        // ENC4_INDEX_MAX changes.
        MOZ_ASSERT(ENC4_INDEX_MAX == UINT8_MAX);
        uint64_t val = ENC4_MASK_VAL |
                       (((uint64_t) startDelta) << ENC4_START_DELTA_SHIFT) |
                       (((uint64_t) length) << ENC4_LENGTH_SHIFT) |
                       (((uint64_t) index) << ENC4_INDEX_SHIFT);
        writer.writeByte(val & 0xff);
        writer.writeByte((val >> 8) & 0xff);
        writer.writeByte((val >> 16) & 0xff);
        writer.writeByte((val >> 24) & 0xff);
        writer.writeByte((val >> 32) & 0xff);
        return;
    }

    MOZ_CRASH("startDelta,length,index triple too large to encode.");
}

/* static */ bool
IonTrackedOptimizationsRegion::WriteRun(CompactBufferWriter &writer,
                                        const NativeToTrackedOptimizations *start,
                                        const NativeToTrackedOptimizations *end,
                                        const UniqueTrackedOptimizations &unique)
{
    // Write the header, which is the range that this whole run encompasses.
    JitSpew(JitSpew_OptimizationTracking, "     Header: [%u, %u)",
            start->startOffset.offset(), (end - 1)->endOffset.offset());
    writer.writeUnsigned(start->startOffset.offset());
    writer.writeUnsigned((end - 1)->endOffset.offset());

    // Write the first entry of the run, which is not delta-encoded.
    JitSpew(JitSpew_OptimizationTracking,
            "     [%6u, %6u)                        vector %3u, offset %4u",
            start->startOffset.offset(), start->endOffset.offset(),
            unique.indexOf(start->optimizations), writer.length());
    uint32_t prevEndOffset = start->endOffset.offset();
    writer.writeUnsigned(prevEndOffset);
    writer.writeByte(unique.indexOf(start->optimizations));

    // Delta encode the run.
    for (const NativeToTrackedOptimizations *entry = start + 1; entry != end; entry++) {
        uint32_t startOffset = entry->startOffset.offset();
        uint32_t endOffset = entry->endOffset.offset();

        uint32_t startDelta = startOffset - prevEndOffset;
        uint32_t length = endOffset - startOffset;
        uint8_t index = unique.indexOf(entry->optimizations);

        JitSpew(JitSpew_OptimizationTracking,
                "     [%6u, %6u) delta [+%5u, +%5u) vector %3u, offset %4u",
                startOffset, endOffset, startDelta, length, index, writer.length());

        WriteDelta(writer, startDelta, length, index);

        prevEndOffset = endOffset;
    }

    if (writer.oom())
        return false;

    return true;
}

TrackedTypeInfo::TrackedTypeInfo(CompactBufferReader &reader)
  : site_(TrackedTypeSite(reader.readUnsigned())),
    mirType_(MIRType(reader.readUnsigned()))
{
    MOZ_ASSERT(site_ < TrackedTypeSite::Count);
}

bool
TrackedTypeInfo::readTypes(CompactBufferReader &reader, const types::TypeSet::TypeList *allTypes)
{
    uint32_t length = reader.readUnsigned();
    for (uint32_t i = 0; i < length; i++) {
        if (!types_.append((*allTypes)[reader.readByte()]))
            return false;
    }
    return true;
}

OptimizationAttempt::OptimizationAttempt(CompactBufferReader &reader)
  : strategy_(TrackedStrategy(reader.readUnsigned())),
    outcome_(TrackedOutcome(reader.readUnsigned()))
{
    MOZ_ASSERT(strategy_ < TrackedStrategy::Count);
    MOZ_ASSERT(outcome_ < TrackedOutcome::Count);
}

static bool
WriteOffsetsTable(CompactBufferWriter &writer, const Vector<uint32_t, 16> &offsets,
                  uint32_t *tableOffsetp)
{
    // 4-byte align for the uint32s.
    uint32_t padding = sizeof(uint32_t) - (writer.length() % sizeof(uint32_t));
    if (padding == sizeof(uint32_t))
        padding = 0;
    JitSpew(JitSpew_OptimizationTracking, "   Padding %u byte%s",
            padding, padding == 1 ? "" : "s");
    for (uint32_t i = 0; i < padding; i++)
        writer.writeByte(0);

    // Record the start of the table to compute reverse offsets for entries.
    uint32_t tableOffset = writer.length();

    // Write how many bytes were padded and numEntries.
    writer.writeNativeEndianUint32_t(padding);
    writer.writeNativeEndianUint32_t(offsets.length());

    // Write entry offset table.
    for (size_t i = 0; i < offsets.length(); i++) {
        JitSpew(JitSpew_OptimizationTracking, "   Entry %u reverse offset %u",
                i, tableOffset - padding - offsets[i]);
        writer.writeNativeEndianUint32_t(tableOffset - padding - offsets[i]);
    }

    if (writer.oom())
        return false;

    *tableOffsetp = tableOffset;
    return true;
}

bool
jit::WriteIonTrackedOptimizationsTable(JSContext *cx, CompactBufferWriter &writer,
                                       const NativeToTrackedOptimizations *start,
                                       const NativeToTrackedOptimizations *end,
                                       const UniqueTrackedOptimizations &unique,
                                       uint32_t *numRegions,
                                       uint32_t *regionTableOffsetp,
                                       uint32_t *typesTableOffsetp,
                                       uint32_t *optimizationTableOffsetp,
                                       types::TypeSet::TypeList *allTypes)
{
    MOZ_ASSERT(unique.sorted());

#ifdef DEBUG
    // Spew training data, which may be fed into a script to determine a good
    // encoding strategy.
    if (JitSpewEnabled(JitSpew_OptimizationTracking)) {
        JitSpewStart(JitSpew_OptimizationTracking, "=> Training data: ");
        for (const NativeToTrackedOptimizations *entry = start; entry != end; entry++) {
            JitSpewCont(JitSpew_OptimizationTracking, "%u,%u,%u ",
                        entry->startOffset.offset(), entry->endOffset.offset(),
                        unique.indexOf(entry->optimizations));
        }
        JitSpewFin(JitSpew_OptimizationTracking);
    }
#endif

    Vector<uint32_t, 16> offsets(cx);
    const NativeToTrackedOptimizations *entry = start;

    // Write out region offloads, partitioned into runs.
    JitSpew(JitSpew_Profiling, "=> Writing regions");
    while (entry != end) {
        uint32_t runLength = IonTrackedOptimizationsRegion::ExpectedRunLength(entry, end);
        JitSpew(JitSpew_OptimizationTracking, "   Run at entry %u, length %u, offset %u",
                entry - start, runLength, writer.length());

        if (!offsets.append(writer.length()))
            return false;

        if (!IonTrackedOptimizationsRegion::WriteRun(writer, entry, entry + runLength, unique))
            return false;

        entry += runLength;
    }

    // Write out the table indexing into the payloads. 4-byte align for the uint32s.
    if (!WriteOffsetsTable(writer, offsets, regionTableOffsetp))
        return false;

    *numRegions = offsets.length();

    // Clear offsets so that it may be reused below for the unique
    // optimizations table.
    offsets.clear();

    const UniqueTrackedOptimizations::SortedVector &vec = unique.sortedVector();
    JitSpew(JitSpew_OptimizationTracking, "=> Writing unique optimizations table with %u entr%s",
            vec.length(), vec.length() == 1 ? "y" : "ies");

    // Write out type info payloads.
    UniqueTrackedTypes uniqueTypes(cx);
    if (!uniqueTypes.init())
        return false;

    for (const UniqueTrackedOptimizations::SortEntry *p = vec.begin(); p != vec.end(); p++) {
        const TempTrackedTypeInfoVector *v = p->types;
        JitSpew(JitSpew_OptimizationTracking, "   Type info entry %u of length %u, offset %u",
                p - vec.begin(), v->length(), writer.length());
        SpewTempTrackedTypeInfoVector(v, "  ");

        if (!offsets.append(writer.length()))
            return false;

        for (const TrackedTypeInfo *t = v->begin(); t != v->end(); t++) {
            if (!t->writeCompact(writer, uniqueTypes))
                return false;
        }
    }

    // Copy the unique type list into the outparam TypeList.
    if (!uniqueTypes.enumerate(allTypes))
        return false;

    if (!WriteOffsetsTable(writer, offsets, typesTableOffsetp))
        return false;
    offsets.clear();

    // Write out attempts payloads.
    for (const UniqueTrackedOptimizations::SortEntry *p = vec.begin(); p != vec.end(); p++) {
        const TempAttemptsVector *v = p->attempts;
        JitSpew(JitSpew_OptimizationTracking, "   Attempts entry %u of length %u, offset %u",
                p - vec.begin(), v->length(), writer.length());
        SpewTempAttemptsVector(v, "  ");

        if (!offsets.append(writer.length()))
            return false;

        for (const OptimizationAttempt *a = v->begin(); a != v->end(); a++)
            a->writeCompact(writer);
    }

    return WriteOffsetsTable(writer, offsets, optimizationTableOffsetp);
}


BytecodeSite *
IonBuilder::maybeTrackedOptimizationSite(jsbytecode *pc)
{
    // BytecodeSites that track optimizations need to be 1-1 with the pc
    // when optimization tracking is enabled, so that all MIR generated by
    // a single pc are tracked at one place, even across basic blocks.
    //
    // Alternatively, we could make all BytecodeSites 1-1 with the pc, but
    // there is no real need as optimization tracking is a toggled
    // feature.
    //
    // Since sites that track optimizations should be sparse, just do a
    // reverse linear search, as we're most likely advancing in pc.
    MOZ_ASSERT(isOptimizationTrackingEnabled());
    for (size_t i = trackedOptimizationSites_.length(); i != 0; i--) {
        BytecodeSite *site = trackedOptimizationSites_[i - 1];
        if (site->pc() == pc) {
            MOZ_ASSERT(site->tree() == info().inlineScriptTree());
            return site;
        }
    }
    return nullptr;
}

void
IonBuilder::startTrackingOptimizations()
{
    if (isOptimizationTrackingEnabled()) {
        BytecodeSite *site = maybeTrackedOptimizationSite(current->trackedSite()->pc());

        if (!site) {
            site = current->trackedSite();
            site->setOptimizations(new(alloc()) TrackedOptimizations(alloc()));
            // OOMs are handled as if optimization tracking were turned off.
            if (!trackedOptimizationSites_.append(site))
                site = nullptr;
        }

        if (site)
            current->updateTrackedSite(site);
    }
}

void
IonBuilder::trackTypeInfoUnchecked(TrackedTypeSite kind, MIRType mirType,
                                   types::TemporaryTypeSet *typeSet)
{
    BytecodeSite *site = current->trackedSite();
    // OOMs are handled as if optimization tracking were turned off.
    TrackedTypeInfo typeInfo(kind, mirType);
    if (!typeInfo.trackTypeSet(typeSet)) {
        site->setOptimizations(nullptr);
        return;
    }
    if (!site->optimizations()->trackTypeInfo(mozilla::Move(typeInfo)))
        site->setOptimizations(nullptr);
}

void
IonBuilder::trackTypeInfoUnchecked(TrackedTypeSite kind, JSObject *obj)
{
    BytecodeSite *site = current->trackedSite();
    // OOMs are handled as if optimization tracking were turned off.
    TrackedTypeInfo typeInfo(kind, MIRType_Object);
    if (!typeInfo.trackType(types::Type::ObjectType(obj)))
        return;
    if (!site->optimizations()->trackTypeInfo(mozilla::Move(typeInfo)))
        site->setOptimizations(nullptr);
}

void
IonBuilder::trackTypeInfoUnchecked(CallInfo &callInfo)
{
    MDefinition *thisArg = callInfo.thisArg();
    trackTypeInfoUnchecked(TrackedTypeSite::Call_This, thisArg->type(), thisArg->resultTypeSet());

    for (uint32_t i = 0; i < callInfo.argc(); i++) {
        MDefinition *arg = callInfo.getArg(i);
        trackTypeInfoUnchecked(TrackedTypeSite::Call_Arg, arg->type(), arg->resultTypeSet());
    }

    types::TemporaryTypeSet *returnTypes = getInlineReturnTypeSet();
    trackTypeInfoUnchecked(TrackedTypeSite::Call_Return, returnTypes->getKnownMIRType(),
                           returnTypes);
}

void
IonBuilder::trackOptimizationAttemptUnchecked(TrackedStrategy strategy)
{
    BytecodeSite *site = current->trackedSite();
    // OOMs are handled as if optimization tracking were turned off.
    if (!site->optimizations()->trackAttempt(strategy))
        site->setOptimizations(nullptr);
}

void
IonBuilder::amendOptimizationAttemptUnchecked(uint32_t index)
{
    const BytecodeSite *site = current->trackedSite();
    site->optimizations()->amendAttempt(index);
}

void
IonBuilder::trackOptimizationOutcomeUnchecked(TrackedOutcome outcome)
{
    const BytecodeSite *site = current->trackedSite();
    site->optimizations()->trackOutcome(outcome);
}

void
IonBuilder::trackOptimizationSuccessUnchecked()
{
    const BytecodeSite *site = current->trackedSite();
    site->optimizations()->trackSuccess();
}

void
IonBuilder::trackInlineSuccessUnchecked(InliningStatus status)
{
    if (status == InliningStatus_Inlined)
        trackOptimizationOutcome(TrackedOutcome::Inlined);
}
