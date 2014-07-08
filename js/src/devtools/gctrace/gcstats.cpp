/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Read and process GC trace logs.
 */

#include "gc/GCTraceFormat.h"

#define __STDC_FORMAT_MACROS

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <vector>

// State of the program

enum Heap
{
    Nursery,
    TenuredHeap,

    HeapKinds
};

enum State
{
    StateMutator,
    StateMinorGC,
    StateMajorGC
};

typedef uint64_t address;
typedef uint8_t AllocKind;
typedef uint8_t ClassId;

struct AllocInfo
{
    const uint64_t serial;
    const AllocKind kind;
    const Heap initialHeap;
    ClassId classId;

    AllocInfo(uint64_t allocCount, uint8_t kind, Heap loc)
      : serial(allocCount), kind(kind), initialHeap(loc), classId(0)
    {
        assert(kind < AllocKinds);
        assert(initialHeap < HeapKinds);
    }
};

struct ClassInfo
{
    const ClassId id;
    const char *name;
    const uint32_t flags;

    ClassInfo(ClassId id, const char *name, uint32_t flags)
      : id(id), name(name), flags(flags) {}
};

typedef std::unordered_map<address, AllocInfo> AllocMap;
typedef std::unordered_map<address, ClassId> ClassMap;
typedef std::vector<ClassInfo> ClassVector;

uint64_t thingSizes[AllocKinds];
AllocMap nurseryThings;
AllocMap tenuredThings;
ClassMap classMap;
ClassVector classes;
uint64_t allocCount = 0;

// Collected data

const unsigned MaxClasses = 128;
const unsigned LifetimeBinLog = 10;
const unsigned MaxLifetimeBins = 40;

const unsigned AugHeapKinds = HeapKinds + 1;
const unsigned HeapTotal = HeapKinds;
const unsigned AugAllocKinds = AllocKinds + 1;
const unsigned AllocKindTotal = AllocKinds;
const unsigned AugLifetimeBins = MaxLifetimeBins + 1;
const unsigned LifetimeBinTotal = MaxLifetimeBins;
const unsigned AugClasses = MaxClasses + 1;
const unsigned ClassTotal = MaxClasses;

template <typename T, size_t length>
struct Array
{
    Array() {}
    void zero() { memset(&elements, 0, sizeof(elements)); }
    T &operator[](size_t index) {
        assert(index < length);
        return elements[index];
    }
  private:
    T elements[length];
};

unsigned timesliceSize;
unsigned lifetimeBins;
std::vector<uint64_t> gcBytesAllocatedInSlice;
std::vector<uint64_t> gcBytesFreedInSlice;

Array<Array<uint64_t, AllocKinds>, HeapKinds> allocCountByHeapAndKind;
Array<Array<uint64_t, MaxLifetimeBins>, HeapKinds> allocCountByHeapAndLifetime;
Array<Array<Array<uint64_t, MaxLifetimeBins>, AllocKinds>, HeapKinds> allocCountByHeapKindAndLifetime;
Array<uint64_t, MaxClasses> objectCountByClass;
Array<Array<uint64_t, MaxClasses>, HeapKinds> objectCountByHeapAndClass;
Array<Array<Array<uint64_t, MaxLifetimeBins>, MaxClasses>, HeapKinds> objectCountByHeapClassAndLifetime;

static void
die(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    vfprintf(stderr, format, va);
    fprintf(stderr, "\n");
    va_end(va);
    exit(1);
}

const uint64_t FirstBinSize = 100;
const unsigned BinLog = 2;

static unsigned
getBin(uint64_t lifetime)
{
    /*
     * Calculate a bin number for a given lifetime.
     *
     * We use a logarithmic scale, starting with a bin size of 100 and doubling
     * from there.
     */
    static double logDivisor = log(BinLog);
    if (lifetime < FirstBinSize)
        return 0;
    return unsigned(log(lifetime / FirstBinSize) / logDivisor) + 1;
}

static unsigned
binLimit(unsigned bin)
{
    return unsigned(pow(BinLog, bin) * FirstBinSize);
}

static void
testBinning()
{
    assert(getBin(0) == 0);
    assert(getBin(FirstBinSize - 1) == 0);
    assert(getBin(FirstBinSize) == 1);
    assert(getBin(2 * FirstBinSize - 1) == 1);
    assert(getBin(2 * FirstBinSize) == 2);
    assert(getBin(4 * FirstBinSize - 1) == 2);
    assert(getBin(4 * FirstBinSize) == 3);
    assert(binLimit(0) == FirstBinSize);
    assert(binLimit(1) == 2 * FirstBinSize);
    assert(binLimit(2) == 4 * FirstBinSize);
    assert(binLimit(3) == 8 * FirstBinSize);
}

static const char *
allocKindName(AllocKind kind)
{
    static const char *AllocKindNames[] = {
        "Object0",
        "Object0Bg",
        "Object2",
        "Object2Bg",
        "Object4",
        "Object4Bg",
        "Object8",
        "Object8Bg",
        "Object12",
        "Object12Bg",
        "Object16",
        "Object16Bg",
        "Script",
        "LazyScript",
        "Shape",
        "BaseShape",
        "TypeObject",
        "FatInlineString",
        "String",
        "ExternalString",
        "Symbol",
        "JitCode",
        "Total"
    };
    assert(sizeof(AllocKindNames) / sizeof(const char *) == AugAllocKinds);
    assert(kind < AugAllocKinds);
    return AllocKindNames[kind];
}

static const char *
heapName(unsigned heap)
{
    static const char *HeapNames[] = {
        "nursery",
        "tenured heap",
        "all"
    };
    assert(heap < AugHeapKinds);
    return HeapNames[heap];
}


static const char *
heapLabel(unsigned heap)
{
    static const char *HeapLabels[] = {
        "Nursery",
        "Tenured heap",
        "Total"
    };
    assert(heap < AugHeapKinds);
    return HeapLabels[heap];
}

static void
outputGcBytesAllocated(FILE *file)
{
    fprintf(file, "# Total GC bytes allocated by timeslice\n");
    fprintf(file, "# Total allocations: %" PRIu64 "\n", allocCount);
    fprintf(file, "Time, GCBytesAllocated\n");

    uint64_t timesliceCount = allocCount / timesliceSize + 1;
    uint64_t total = 0;
    for (uint64_t i = 0; i < timesliceCount; ++i) {
        total += gcBytesAllocatedInSlice[i];
        fprintf(file, "%12" PRIu64 ", %12" PRIu64 "\n", i * timesliceSize, total);
    }
}

static void
outputGcBytesUsed(FILE *file)
{
    fprintf(file, "# Total GC bytes used by timeslice\n");
    fprintf(file, "# Total allocations: %" PRIu64 "\n", allocCount);
    fprintf(file, "Time, GCBytesUsed\n");

    uint64_t timesliceCount = allocCount / timesliceSize + 1;
    uint64_t total = 0;
    for (uint64_t i = 0; i < timesliceCount; ++i) {
        total += gcBytesAllocatedInSlice[i] - gcBytesFreedInSlice[i];
        fprintf(file, "%12" PRIu64 ", %12" PRIu64 "\n", i * timesliceSize, total);
    }
}

static void
outputThingCounts(FILE *file)
{
    fprintf(file, "# GC thing allocation count in nursery and tenured heap by kind\n");
    fprintf(file, "# This shows what kind of things we are allocating in the nursery\n");
    fprintf(file, "# Total allocations: %" PRIu64 "\n", allocCount);
    fprintf(file, "Kind, Nursery, Tenured heap\n");
    for (unsigned i = 0; i < AllocKinds; ++i) {
        fprintf(file, "%15s, %8" PRIu64 ", %8" PRIu64 "\n", allocKindName(i),
                allocCountByHeapAndKind[Nursery][i],
                allocCountByHeapAndKind[TenuredHeap][i]);
    }
}

static void
outputObjectCounts(FILE *file)
{
    fprintf(file, "# Object allocation count in nursery and tenured heap by class\n");
    fprintf(file, "# This shows what kind of objects we are allocating in the nursery\n");
    fprintf(file, "# Total allocations: %" PRIu64 "\n", allocCount);
    fprintf(file, "Class, Nursery, Tenured heap, Total\n");
    for (unsigned i = 0; i < classes.size(); ++i) {
        fprintf(file, "%30s, %8" PRIu64 ", %8" PRIu64 ", %8" PRIu64 "\n",
                classes[i].name,
                objectCountByHeapAndClass[Nursery][i],
                objectCountByHeapAndClass[TenuredHeap][i],
                objectCountByClass[i]);
    }
}

static void
outputLifetimeByHeap(FILE *file)
{
    fprintf(file, "# Lifetime of all things (in log2 bins) by initial heap\n");
    fprintf(file, "# NB invalid unless execution was traced with appropriate zeal\n");
    fprintf(file, "# Total allocations: %" PRIu64 "\n", allocCount);
    fprintf(file, "Lifetime");
    for (unsigned i = 0; i < HeapKinds; ++i)
        fprintf(file, ", %s", heapLabel(i));
    fprintf(file, "\n");

    for (unsigned i = 0; i < lifetimeBins; ++i) {
        fprintf(file, "%8d", binLimit(i));
        for (unsigned j = 0; j < HeapKinds; ++j)
            fprintf(file, ", %8" PRIu64, allocCountByHeapAndLifetime[j][i]);
        fprintf(file, "\n");
    }
}

static void
outputLifetimeByKind(FILE *file, unsigned initialHeap)
{
    assert(initialHeap < AugHeapKinds);

    fprintf(file, "# Lifetime of %s things (in log2 bins) by kind\n", heapName(initialHeap));
    fprintf(file, "# NB invalid unless execution was traced with appropriate zeal\n");
    fprintf(file, "# Total allocations: %" PRIu64 "\n", allocCount);
    fprintf(file, "Lifetime");
    for (unsigned i = 0; i < AllocKinds; ++i)
        fprintf(file, ", %15s", allocKindName(i));
    fprintf(file, "\n");

    for (unsigned i = 0; i < lifetimeBins; ++i) {
        fprintf(file, "%8d", binLimit(i));
        for (unsigned j = 0; j < AllocKinds; ++j)
            fprintf(file, ", %8" PRIu64,
                    allocCountByHeapKindAndLifetime[initialHeap][j][i]);
        fprintf(file, "\n");
    }
}

static void
outputLifetimeByClass(FILE *file, unsigned initialHeap)
{
    assert(initialHeap < AugHeapKinds);

    fprintf(file, "# Lifetime of %s things (in log2 bins) by class\n", heapName(initialHeap));
    fprintf(file, "# NB invalid unless execution was traced with appropriate zeal\n");
    fprintf(file, "# Total allocations: %" PRIu64 "\n", allocCount);
    fprintf(file, "Lifetime");
    for (unsigned i = 0; i < classes.size(); ++i)
        fprintf(file, ", %15s", classes[i].name);
    fprintf(file, "\n");

    for (unsigned i = 0; i < lifetimeBins; ++i) {
        fprintf(file, "%8d", binLimit(i));
        for (unsigned j = 0; j < classes.size(); ++j)
            fprintf(file, ", %8" PRIu64,
                    objectCountByHeapClassAndLifetime[initialHeap][j][i]);
        fprintf(file, "\n");
    }
}

static void
processAlloc(const AllocInfo &info, uint64_t finalizeTime)
{
    uint64_t lifetime = finalizeTime - info.serial;
    unsigned timeslice = info.serial / timesliceSize;

    unsigned lifetimeBin = getBin(lifetime);
    assert(lifetimeBin < lifetimeBins);

    ++allocCountByHeapAndKind[info.initialHeap][info.kind];
    ++allocCountByHeapAndLifetime[info.initialHeap][lifetimeBin];
    ++allocCountByHeapKindAndLifetime[info.initialHeap][info.kind][lifetimeBin];

    if (info.kind <= LastObjectAllocKind) {
        ++objectCountByClass[info.classId];
        ++objectCountByHeapAndClass[info.initialHeap][info.classId];
        ++objectCountByHeapClassAndLifetime[info.initialHeap][info.classId][lifetimeBin];
    }

    uint64_t size = thingSizes[info.kind];
    gcBytesAllocatedInSlice[timeslice] += size;
    gcBytesFreedInSlice[finalizeTime / timesliceSize] += size;
}

static bool
readTrace(FILE* file, uint64_t& trace)
{
    if (fread(&trace, sizeof(trace), 1, file) != 1) {
        if (feof(file))
            return false;
        else
            die("Error reading input");
    }
    return true;
}

static GCTraceEvent
getTraceEvent(uint64_t trace)
{
    uint64_t event = trace >> TraceEventShift;
    assert(event < GCTraceEventCount);
    return (GCTraceEvent)event;
}

static uint64_t
getTracePayload(uint64_t trace)
{
    return trace & ((1lu << TracePayloadBits) - 1);
}

static uint8_t
getTraceExtra(uint64_t trace)
{
    uint64_t extra = (trace >> TraceExtraShift) & ((1 << TraceExtraBits) - 1);
    assert(extra < 256);
    return (uint8_t)extra;
}

static uint64_t
expectTrace(FILE *file, GCTraceEvent event)
{
    uint64_t trace;
    if (!readTrace(file, trace))
        die("End of file while expecting trace %d", event);
    if (getTraceEvent(trace) != event)
        die("Expected trace %d but got trace %d", event, getTraceEvent(trace));
    return getTracePayload(trace);
}

static uint64_t
expectDataAddress(FILE *file)
{
    return expectTrace(file, TraceDataAddress);
}

static uint32_t
expectDataInt(FILE *file)
{
    return (uint32_t)expectTrace(file, TraceDataInt);
}

static char *
expectDataString(FILE *file)
{
    uint64_t length = expectTrace(file, TraceDataString);
    assert(length < 256); // Sanity check
    char *string = static_cast<char *>(malloc(length + 1));
    if (!string)
        die("Out of memory while reading string data");

    const unsigned charsPerWord = sizeof(uint64_t);
    unsigned wordCount = (length + charsPerWord - 1) / charsPerWord;
    for (unsigned i = 0; i < wordCount; ++i) {
        if (fread(&string[i * charsPerWord], sizeof(char), charsPerWord, file) != charsPerWord)
            die("Error or EOF while reading string data");
    }
    string[length] = 0;

    return string;
}

static void
createClassInfo(const char *name, uint32_t flags, address clasp = 0)
{
    ClassId id = classes.size();
    classes.push_back(ClassInfo(id, name, flags));
    if (clasp)
        classMap.emplace(clasp, id);
}

static void
readClassInfo(FILE *file, address clasp)
{
    assert(clasp);
    uint32_t flags = expectDataInt(file);
    char *name = expectDataString(file);
    createClassInfo(name, flags, clasp);
}

static void
allocHeapThing(address thing, AllocKind kind)
{
    uint64_t allocTime = allocCount++;
    tenuredThings.emplace(thing, AllocInfo(allocTime, kind, TenuredHeap));
}

static void
allocNurseryThing(address thing, AllocKind kind)
{
    uint64_t allocTime = allocCount++;
    nurseryThings.emplace(thing, AllocInfo(allocTime, kind, Nursery));
}

static void
setObjectClass(address obj, address clasp)
{
    auto i = classMap.find(clasp);
    assert(i != classMap.end());
    ClassId id = i->second;
    assert(id < classes.size());

    auto j = nurseryThings.find(obj);
    if (j == nurseryThings.end()) {
        j = tenuredThings.find(obj);
        if (j == tenuredThings.end())
            die("Can't find allocation for object %p", obj);
    }
    j->second.classId = id;
}

static void
promoteToTenured(address src, address dst)
{
    auto i = nurseryThings.find(src);
    assert(i != nurseryThings.end());
    AllocInfo alloc = i->second;
    tenuredThings.emplace(dst, alloc);
    nurseryThings.erase(i);
}

static void
finalizeThing(const AllocInfo &info)
{
    processAlloc(info, allocCount);
}

static void
sweepNursery()
{
    for (auto i = nurseryThings.begin(); i != nurseryThings.end(); ++i) {
        finalizeThing(i->second);
    }
    nurseryThings.clear();
}

static void
finalizeTenuredThing(address thing)
{
    auto i = tenuredThings.find(thing);
    assert(i != tenuredThings.end());
    finalizeThing(i->second);
    tenuredThings.erase(i);
}

static void
updateTimeslices(std::vector<uint64_t> &data, uint64_t lastTime, uint64_t currentTime, uint64_t value)
{
    unsigned firstSlice = (lastTime / timesliceSize) + 1;
    unsigned lastSlice = currentTime / timesliceSize;
    for (unsigned i = firstSlice; i <= lastSlice; ++i)
        data[i] = value;
}

static void
processTraceFile(const char *filename)
{
    FILE *file;
    file = fopen(filename, "r");
    if (!file)
        die("Can't read file: %s", filename);

    // Get a conservative estimate of the total number of allocations so we can
    // allocate buffers in advance.
    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);
    size_t maxTraces = length / sizeof(uint64_t);

    uint64_t trace;
    if (!readTrace(file, trace))
        die("Empty input file");
    if (getTraceEvent(trace) != TraceEventInit)
        die("Can't parse input file");
    if (getTraceExtra(trace) != TraceFormatVersion)
        die("Unexpected format version %d", getTraceExtra(trace));
    for (unsigned kind = 0; kind < AllocKinds; ++kind)
        thingSizes[kind] = expectTrace(file, TraceEventThingSize);

    timesliceSize = 1000;
    while ((maxTraces / timesliceSize ) > 1000)
        timesliceSize *= 2;

    size_t maxTimeslices = maxTraces / timesliceSize;
    gcBytesAllocatedInSlice.reserve(maxTimeslices);
    gcBytesFreedInSlice.reserve(maxTimeslices);
    lifetimeBins = getBin(maxTraces) + 1;
    assert(lifetimeBins <= MaxLifetimeBins);

    createClassInfo("unknown", 0);

    State state = StateMutator;
    while (readTrace(file, trace)) {
        GCTraceEvent event = getTraceEvent(trace);
        switch (event) {
      case TraceEventNurseryAlloc:
          assert(state == StateMutator);
          allocNurseryThing(getTracePayload(trace), getTraceExtra(trace));
          break;
      case TraceEventTenuredAlloc:
          assert(state == StateMutator);
          allocHeapThing(getTracePayload(trace), getTraceExtra(trace));
          break;
      case TraceEventClassInfo:
          assert(state == StateMutator);
          readClassInfo(file, getTracePayload(trace));
          break;
      case TraceEventCreateObject:
          assert(state == StateMutator);
          setObjectClass(getTracePayload(trace), expectDataAddress(file));
          break;
      case TraceEventMinorGCStart:
          assert(state == StateMutator);
          state = StateMinorGC;
          break;
      case TraceEventPromoteToTenured:
          assert(state == StateMinorGC);
          promoteToTenured(getTracePayload(trace), expectDataAddress(file));
          break;
      case TraceEventMinorGCEnd:
          assert(state == StateMinorGC);
          sweepNursery();
          state = StateMutator;
          break;
      case TraceEventMajorGCStart:
          assert(state == StateMutator);
          state = StateMajorGC;
          break;
      case TraceEventTenuredFinalize:
          assert(state == StateMajorGC);
          finalizeTenuredThing(getTracePayload(trace));
          break;
      case TraceEventMajorGCEnd:
          assert(state == StateMajorGC);
          state = StateMutator;
          break;
      default:
          assert(false);
          die("Unexpected trace event %d", event);
          break;
        }
    }

    // Correct number of lifetime bins now we know the real allocation count.
    assert(allocCount < maxTraces);
    lifetimeBins = getBin(allocCount) + 1;
    assert(lifetimeBins <= MaxLifetimeBins);

    fclose(file);
}

template <class func>
void withOutputFile(const char *base, const char *name, func f)
{
    const size_t bufSize = 256;
    char filename[bufSize];
    int r = snprintf(filename, bufSize, "%s-%s.csv", base, name);
    assert(r > 0 && r < bufSize);

    FILE *file = fopen(filename, "w");
    if (!file)
        die("Can't write to %s", filename);
    f(file);
    fclose(file);
}

int
main(int argc, const char *argv[])
{
    testBinning();

    if (argc != 3)
        die("usage: gctrace INPUT_FILE OUTPUT_BASE");
    const char* inputFile = argv[1];
    const char* outputBase = argv[2];

    processTraceFile(inputFile);

    using namespace std::placeholders;
    withOutputFile(outputBase, "bytesAllocatedBySlice", outputGcBytesAllocated);
    withOutputFile(outputBase, "bytesUsedBySlice", outputGcBytesUsed);
    withOutputFile(outputBase, "thingCounts", outputThingCounts);
    withOutputFile(outputBase, "objectCounts", outputObjectCounts);
    withOutputFile(outputBase, "lifetimeByClassForNursery",
                   std::bind(outputLifetimeByClass, _1, Nursery));
    withOutputFile(outputBase, "lifetimeByKindForHeap",
                   std::bind(outputLifetimeByKind, _1, TenuredHeap));
    withOutputFile(outputBase, "lifetimeByHeap", outputLifetimeByHeap);
    return 0;
}
