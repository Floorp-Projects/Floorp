/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Cell_h
#define gc_Cell_h

#include "mozilla/Atomics.h"

#include <type_traits>

#include "gc/GCEnum.h"
#include "gc/Heap.h"
#include "js/GCAnnotations.h"
#include "js/shadow/Zone.h"  // JS::shadow::Zone
#include "js/TraceKind.h"
#include "js/TypeDecls.h"

namespace JS {
enum class TraceKind;
} /* namespace JS */

namespace js {

class GenericPrinter;

extern bool RuntimeFromMainThreadIsHeapMajorCollecting(
    JS::shadow::Zone* shadowZone);

#ifdef DEBUG

// Barriers can't be triggered during backend Ion compilation, which may run on
// a helper thread.
extern bool CurrentThreadIsIonCompiling();

extern bool CurrentThreadIsGCMarking();

#endif

extern void TraceManuallyBarrieredGenericPointerEdge(JSTracer* trc,
                                                     gc::Cell** thingp,
                                                     const char* name);

namespace gc {

class Arena;
enum class AllocKind : uint8_t;
struct Chunk;
class StoreBuffer;
class TenuredCell;

// Like gc::MarkColor but allows the possibility of the cell being unmarked.
//
// This class mimics an enum class, but supports operator overloading.
class CellColor {
 public:
  enum Color { White = 0, Gray = 1, Black = 2 };

  CellColor() : color(White) {}

  MOZ_IMPLICIT CellColor(MarkColor markColor)
      : color(markColor == MarkColor::Black ? Black : Gray) {}

  MOZ_IMPLICIT constexpr CellColor(Color c) : color(c) {}

  MarkColor asMarkColor() const {
    MOZ_ASSERT(color != White);
    return color == Black ? MarkColor::Black : MarkColor::Gray;
  }

  // Implement a total ordering for CellColor, with white being 'least marked'
  // and black being 'most marked'.
  bool operator<(const CellColor other) const { return color < other.color; }
  bool operator>(const CellColor other) const { return color > other.color; }
  bool operator<=(const CellColor other) const { return color <= other.color; }
  bool operator>=(const CellColor other) const { return color >= other.color; }
  bool operator!=(const CellColor other) const { return color != other.color; }
  bool operator==(const CellColor other) const { return color == other.color; }
  explicit operator bool() const { return color != White; }

#if defined(JS_GC_ZEAL) || defined(DEBUG)
  const char* name() const {
    switch (color) {
      case CellColor::White:
        return "white";
      case CellColor::Black:
        return "black";
      case CellColor::Gray:
        return "gray";
      default:
        MOZ_CRASH("Unexpected cell color");
    }
  }
#endif

 private:
  Color color;
};

// [SMDOC] GC Cell
//
// A GC cell is the ultimate base class for all GC things. All types allocated
// on the GC heap extend either gc::Cell or gc::TenuredCell. If a type is always
// tenured, prefer the TenuredCell class as base.
//
// The first word of Cell is a uintptr_t that reserves the low three bits for GC
// purposes. The remaining bits are available to sub-classes and can be used
// store a pointer to another gc::Cell. It can also be used for temporary
// storage (see setTemporaryGCUnsafeData). To make use of the remaining space,
// sub-classes derive from a helper class such as TenuredCellWithNonGCPointer.
//
// During moving GC operation a Cell may be marked as forwarded. This indicates
// that a gc::RelocationOverlay is currently stored in the Cell's memory and
// should be used to find the new location of the Cell.
struct Cell {
 protected:
  // Cell header word. Stores GC flags and derived class data.
  //
  // This is atomic since it can be read from and written to by different
  // threads during compacting GC, in a limited way. Specifically, writes that
  // update the derived class data can race with reads that check the forwarded
  // flag. The writes do not change the forwarded flag (which is always false in
  // this situation).
  mozilla::Atomic<uintptr_t, mozilla::MemoryOrdering::Relaxed> header_;

 public:
  static_assert(gc::CellFlagBitsReservedForGC >= 3,
                "Not enough flag bits reserved for GC");
  static constexpr uintptr_t RESERVED_MASK =
      BitMask(gc::CellFlagBitsReservedForGC);

  // Indicates whether the cell has been forwarded (moved) by generational or
  // compacting GC and is now a RelocationOverlay.
  static constexpr uintptr_t FORWARD_BIT = Bit(0);

  // Bits 1 and 2 are currently unused.

  bool isForwarded() const { return header_ & FORWARD_BIT; }
  uintptr_t flags() const { return header_ & RESERVED_MASK; }

  MOZ_ALWAYS_INLINE bool isTenured() const { return !IsInsideNursery(this); }
  MOZ_ALWAYS_INLINE const TenuredCell& asTenured() const;
  MOZ_ALWAYS_INLINE TenuredCell& asTenured();

  MOZ_ALWAYS_INLINE bool isMarkedAny() const;
  MOZ_ALWAYS_INLINE bool isMarkedBlack() const;
  MOZ_ALWAYS_INLINE bool isMarkedGray() const;
  MOZ_ALWAYS_INLINE bool isMarked(gc::MarkColor color) const;
  MOZ_ALWAYS_INLINE bool isMarkedAtLeast(gc::MarkColor color) const;

  MOZ_ALWAYS_INLINE CellColor color() const {
    return isMarkedBlack()
               ? CellColor::Black
               : isMarkedGray() ? CellColor::Gray : CellColor::White;
  }

  inline JSRuntime* runtimeFromMainThread() const;

  // Note: Unrestricted access to the runtime of a GC thing from an arbitrary
  // thread can easily lead to races. Use this method very carefully.
  inline JSRuntime* runtimeFromAnyThread() const;

  // May be overridden by GC thing kinds that have a compartment pointer.
  inline JS::Compartment* maybeCompartment() const { return nullptr; }

  // The StoreBuffer used to record incoming pointers from the tenured heap.
  // This will return nullptr for a tenured cell.
  inline StoreBuffer* storeBuffer() const;

  inline JS::TraceKind getTraceKind() const;

  static MOZ_ALWAYS_INLINE bool needPreWriteBarrier(JS::Zone* zone);

  template <typename T, typename = std::enable_if_t<JS::IsBaseTraceType_v<T>>>
  inline bool is() const {
    return getTraceKind() == JS::MapTypeToTraceKind<T>::kind;
  }

  template <typename T, typename = std::enable_if_t<JS::IsBaseTraceType_v<T>>>
  inline T* as() {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<T*>(this);
  }

  template <typename T, typename = std::enable_if_t<JS::IsBaseTraceType_v<T>>>
  inline const T* as() const {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<const T*>(this);
  }

  inline JS::Zone* zone() const;
  inline JS::Zone* zoneFromAnyThread() const;

  // Get the zone for a cell known to be in the nursery.
  inline JS::Zone* nurseryZone() const;
  inline JS::Zone* nurseryZoneFromAnyThread() const;

  // Default barrier implementations for nursery allocatable cells. These may be
  // overriden by derived classes.
  static MOZ_ALWAYS_INLINE void readBarrier(Cell* thing);
  static MOZ_ALWAYS_INLINE void preWriteBarrier(Cell* thing);

#ifdef DEBUG
  static inline void assertThingIsNotGray(Cell* cell);
  inline bool isAligned() const;
  void dump(GenericPrinter& out) const;
  void dump() const;
#endif

 protected:
  uintptr_t address() const;
  inline Chunk* chunk() const;

 private:
  // Cells are destroyed by the GC. Do not delete them directly.
  void operator delete(void*) = delete;
} JS_HAZ_GC_THING;

// A GC TenuredCell gets behaviors that are valid for things in the Tenured
// heap, such as access to the arena and mark bits.
class TenuredCell : public Cell {
 public:
  MOZ_ALWAYS_INLINE bool isTenured() const {
    MOZ_ASSERT(!IsInsideNursery(this));
    return true;
  }

  // Mark bit management.
  MOZ_ALWAYS_INLINE bool isMarkedAny() const;
  MOZ_ALWAYS_INLINE bool isMarkedBlack() const;
  MOZ_ALWAYS_INLINE bool isMarkedGray() const;

  // Same as Cell::color, but skips nursery checks.
  MOZ_ALWAYS_INLINE CellColor color() const {
    return isMarkedBlack()
               ? CellColor::Black
               : isMarkedGray() ? CellColor::Gray : CellColor::White;
  }

  // The return value indicates if the cell went from unmarked to marked.
  MOZ_ALWAYS_INLINE bool markIfUnmarked(
      MarkColor color = MarkColor::Black) const;
  MOZ_ALWAYS_INLINE void markBlack() const;
  MOZ_ALWAYS_INLINE void copyMarkBitsFrom(const TenuredCell* src);
  MOZ_ALWAYS_INLINE void unmark();

  // Access to the arena.
  inline Arena* arena() const;
  inline AllocKind getAllocKind() const;
  inline JS::TraceKind getTraceKind() const;
  inline JS::Zone* zone() const;
  inline JS::Zone* zoneFromAnyThread() const;
  inline bool isInsideZone(JS::Zone* zone) const;

  MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZone() const {
    return JS::shadow::Zone::from(zone());
  }
  MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZoneFromAnyThread() const {
    return JS::shadow::Zone::from(zoneFromAnyThread());
  }

  template <typename T, typename = std::enable_if_t<JS::IsBaseTraceType_v<T>>>
  inline bool is() const {
    return getTraceKind() == JS::MapTypeToTraceKind<T>::kind;
  }

  template <typename T, typename = std::enable_if_t<JS::IsBaseTraceType_v<T>>>
  inline T* as() {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<T*>(this);
  }

  template <typename T, typename = std::enable_if_t<JS::IsBaseTraceType_v<T>>>
  inline const T* as() const {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<const T*>(this);
  }

  static MOZ_ALWAYS_INLINE void readBarrier(TenuredCell* thing);
  static MOZ_ALWAYS_INLINE void preWriteBarrier(TenuredCell* thing);
  static MOZ_ALWAYS_INLINE void postWriteBarrier(void* cellp,
                                                 TenuredCell* prior,
                                                 TenuredCell* next);

  // Default implementation for kinds that don't require fixup.
  void fixupAfterMovingGC() {}

#ifdef DEBUG
  inline bool isAligned() const;
#endif
};

MOZ_ALWAYS_INLINE const TenuredCell& Cell::asTenured() const {
  MOZ_ASSERT(isTenured());
  return *static_cast<const TenuredCell*>(this);
}

MOZ_ALWAYS_INLINE TenuredCell& Cell::asTenured() {
  MOZ_ASSERT(isTenured());
  return *static_cast<TenuredCell*>(this);
}

MOZ_ALWAYS_INLINE bool Cell::isMarkedAny() const {
  return !isTenured() || asTenured().isMarkedAny();
}

MOZ_ALWAYS_INLINE bool Cell::isMarkedBlack() const {
  return !isTenured() || asTenured().isMarkedBlack();
}

MOZ_ALWAYS_INLINE bool Cell::isMarkedGray() const {
  return isTenured() && asTenured().isMarkedGray();
}

MOZ_ALWAYS_INLINE bool Cell::isMarked(gc::MarkColor color) const {
  return color == MarkColor::Gray ? isMarkedGray() : isMarkedBlack();
}

MOZ_ALWAYS_INLINE bool Cell::isMarkedAtLeast(gc::MarkColor color) const {
  return color == MarkColor::Gray ? isMarkedAny() : isMarkedBlack();
}

inline JSRuntime* Cell::runtimeFromMainThread() const {
  JSRuntime* rt = chunk()->trailer.runtime;
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  return rt;
}

inline JSRuntime* Cell::runtimeFromAnyThread() const {
  return chunk()->trailer.runtime;
}

inline uintptr_t Cell::address() const {
  uintptr_t addr = uintptr_t(this);
  MOZ_ASSERT(addr % CellAlignBytes == 0);
  MOZ_ASSERT(Chunk::withinValidRange(addr));
  return addr;
}

Chunk* Cell::chunk() const {
  uintptr_t addr = uintptr_t(this);
  MOZ_ASSERT(addr % CellAlignBytes == 0);
  addr &= ~ChunkMask;
  return reinterpret_cast<Chunk*>(addr);
}

inline StoreBuffer* Cell::storeBuffer() const {
  return chunk()->trailer.storeBuffer;
}

JS::Zone* Cell::zone() const {
  if (isTenured()) {
    return asTenured().zone();
  }

  return nurseryZone();
}

JS::Zone* Cell::zoneFromAnyThread() const {
  if (isTenured()) {
    return asTenured().zoneFromAnyThread();
  }

  return nurseryZoneFromAnyThread();
}

JS::Zone* Cell::nurseryZone() const {
  JS::Zone* zone = nurseryZoneFromAnyThread();
  MOZ_ASSERT(CurrentThreadIsGCMarking() || CurrentThreadCanAccessZone(zone));
  return zone;
}

JS::Zone* Cell::nurseryZoneFromAnyThread() const {
  return NurseryCellHeader::from(this)->zone();
}

#ifdef DEBUG
extern Cell* UninlinedForwarded(const Cell* cell);
#endif

inline JS::TraceKind Cell::getTraceKind() const {
  if (isTenured()) {
    MOZ_ASSERT_IF(isForwarded(), UninlinedForwarded(this)->getTraceKind() ==
                                     asTenured().getTraceKind());
    return asTenured().getTraceKind();
  }

  return NurseryCellHeader::from(this)->traceKind();
}

/* static */ MOZ_ALWAYS_INLINE bool Cell::needPreWriteBarrier(JS::Zone* zone) {
  return JS::shadow::Zone::from(zone)->needsIncrementalBarrier();
}

/* static */ MOZ_ALWAYS_INLINE void Cell::readBarrier(Cell* thing) {
  MOZ_ASSERT(!CurrentThreadIsGCMarking());
  if (thing->isTenured()) {
    TenuredCell::readBarrier(&thing->asTenured());
  }
}

/* static */ MOZ_ALWAYS_INLINE void Cell::preWriteBarrier(Cell* thing) {
  MOZ_ASSERT(!CurrentThreadIsGCMarking());
  if (thing && thing->isTenured()) {
    TenuredCell::preWriteBarrier(&thing->asTenured());
  }
}

bool TenuredCell::isMarkedAny() const {
  MOZ_ASSERT(arena()->allocated());
  return chunk()->bitmap.isMarkedAny(this);
}

bool TenuredCell::isMarkedBlack() const {
  MOZ_ASSERT(arena()->allocated());
  return chunk()->bitmap.isMarkedBlack(this);
}

bool TenuredCell::isMarkedGray() const {
  MOZ_ASSERT(arena()->allocated());
  return chunk()->bitmap.isMarkedGray(this);
}

bool TenuredCell::markIfUnmarked(MarkColor color /* = Black */) const {
  return chunk()->bitmap.markIfUnmarked(this, color);
}

void TenuredCell::markBlack() const { chunk()->bitmap.markBlack(this); }

void TenuredCell::copyMarkBitsFrom(const TenuredCell* src) {
  ChunkBitmap& bitmap = chunk()->bitmap;
  bitmap.copyMarkBit(this, src, ColorBit::BlackBit);
  bitmap.copyMarkBit(this, src, ColorBit::GrayOrBlackBit);
}

void TenuredCell::unmark() { chunk()->bitmap.unmark(this); }

inline Arena* TenuredCell::arena() const {
  MOZ_ASSERT(isTenured());
  uintptr_t addr = address();
  addr &= ~ArenaMask;
  return reinterpret_cast<Arena*>(addr);
}

AllocKind TenuredCell::getAllocKind() const { return arena()->getAllocKind(); }

JS::TraceKind TenuredCell::getTraceKind() const {
  return MapAllocToTraceKind(getAllocKind());
}

JS::Zone* TenuredCell::zone() const {
  JS::Zone* zone = arena()->zone;
  MOZ_ASSERT(CurrentThreadIsGCMarking() || CurrentThreadCanAccessZone(zone));
  return zone;
}

JS::Zone* TenuredCell::zoneFromAnyThread() const { return arena()->zone; }

bool TenuredCell::isInsideZone(JS::Zone* zone) const {
  return zone == arena()->zone;
}

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::readBarrier(
    TenuredCell* thing) {
  MOZ_ASSERT(!CurrentThreadIsIonCompiling());
  MOZ_ASSERT(!CurrentThreadIsGCMarking());
  MOZ_ASSERT(thing);
  MOZ_ASSERT(CurrentThreadCanAccessZone(thing->zoneFromAnyThread()));

  // Barriers should not be triggered on main thread while collecting.
  mozilla::DebugOnly<JSRuntime*> runtime = thing->runtimeFromAnyThread();
  MOZ_ASSERT_IF(CurrentThreadCanAccessRuntime(runtime),
                !JS::RuntimeHeapIsCollecting());

  JS::shadow::Zone* shadowZone = thing->shadowZoneFromAnyThread();
  if (shadowZone->needsIncrementalBarrier()) {
    // We should only observe barriers being enabled on the main thread.
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime));
    Cell* tmp = thing;
    TraceManuallyBarrieredGenericPointerEdge(shadowZone->barrierTracer(), &tmp,
                                             "read barrier");
    MOZ_ASSERT(tmp == thing);
  }

  if (thing->isMarkedGray()) {
    // There shouldn't be anything marked gray unless we're on the main thread.
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime));
    if (!JS::RuntimeHeapIsCollecting()) {
      JS::UnmarkGrayGCThingRecursively(
          JS::GCCellPtr(thing, thing->getTraceKind()));
    }
  }
}

void AssertSafeToSkipBarrier(TenuredCell* thing);

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::preWriteBarrier(
    TenuredCell* thing) {
  MOZ_ASSERT(!CurrentThreadIsIonCompiling());
  MOZ_ASSERT(!CurrentThreadIsGCMarking());

  if (!thing) {
    return;
  }

#ifdef JS_GC_ZEAL
  // When verifying pre barriers we need to switch on all barriers, even
  // those on the Atoms Zone. Normally, we never enter a parse task when
  // collecting in the atoms zone, so will filter out atoms below.
  // Unfortuantely, If we try that when verifying pre-barriers, we'd never be
  // able to handle off thread parse tasks at all as we switch on the verifier
  // any time we're not doing GC. This would cause us to deadlock, as off thread
  // parsing is meant to resume after GC work completes. Instead we filter out
  // any off thread barriers that reach us and assert that they would normally
  // not be possible.
  if (!CurrentThreadCanAccessRuntime(thing->runtimeFromAnyThread())) {
    AssertSafeToSkipBarrier(thing);
    return;
  }
#endif

  // Barriers can be triggered on the main thread while collecting, but are
  // disabled. For example, this happens when destroying HeapPtr wrappers.

  JS::shadow::Zone* shadowZone = thing->shadowZoneFromAnyThread();
  if (shadowZone->needsIncrementalBarrier()) {
    MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(shadowZone));
    Cell* tmp = thing;
    TraceManuallyBarrieredGenericPointerEdge(shadowZone->barrierTracer(), &tmp,
                                             "pre barrier");
    MOZ_ASSERT(tmp == thing);
  }
}

static MOZ_ALWAYS_INLINE void AssertValidToSkipBarrier(TenuredCell* thing) {
  MOZ_ASSERT(!IsInsideNursery(thing));
  MOZ_ASSERT_IF(thing, !IsNurseryAllocable(thing->getAllocKind()));
}

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::postWriteBarrier(
    void* cellp, TenuredCell* prior, TenuredCell* next) {
  AssertValidToSkipBarrier(next);
}

#ifdef DEBUG

/* static */ void Cell::assertThingIsNotGray(Cell* cell) {
  JS::AssertCellIsNotGray(cell);
}

bool Cell::isAligned() const {
  if (!isTenured()) {
    return true;
  }
  return asTenured().isAligned();
}

bool TenuredCell::isAligned() const {
  return Arena::isAligned(address(), arena()->getThingSize());
}

#endif

// Base class for nusery-allocatable GC things that have 32-bit length and
// 32-bit flags (currently JSString and BigInt).
//
// This tries to store both in Cell::header_, but if that isn't large enough the
// length is stored separately.
//
//          32       0
//  ------------------
//  | Length | Flags |
//  ------------------
//
// The low bits of the flags word (see CellFlagBitsReservedForGC) are reserved
// for GC. Derived classes must ensure they don't use these flags for non-GC
// purposes.
class alignas(gc::CellAlignBytes) CellWithLengthAndFlags : public Cell {
#if JS_BITS_PER_WORD == 32
  // Additional storage for length if |header_| is too small to fit both.
  uint32_t length_;
#endif

 protected:
  uint32_t headerLengthField() const {
#if JS_BITS_PER_WORD == 32
    return length_;
#else
    return uint32_t(header_ >> 32);
#endif
  }

  uint32_t headerFlagsField() const { return uint32_t(header_); }

  void setHeaderFlagBit(uint32_t flag) { header_ |= uintptr_t(flag); }
  void clearHeaderFlagBit(uint32_t flag) { header_ &= ~uintptr_t(flag); }
  void toggleHeaderFlagBit(uint32_t flag) { header_ ^= uintptr_t(flag); }

  void setHeaderLengthAndFlags(uint32_t len, uint32_t flags) {
#if JS_BITS_PER_WORD == 32
    header_ = flags;
    length_ = len;
#else
    header_ = (uint64_t(len) << 32) | uint64_t(flags);
#endif
  }

  // Sub classes can store temporary data in the flags word. This is not GC safe
  // and users must ensure flags/length are never checked (including by asserts)
  // while this data is stored. Use of this method is strongly discouraged!
  void setTemporaryGCUnsafeData(uintptr_t data) { header_ = data; }

  // To get back the data, values to safely re-initialize clobbered flags
  // must be provided.
  uintptr_t unsetTemporaryGCUnsafeData(uint32_t len, uint32_t flags) {
    uintptr_t data = header_;
    setHeaderLengthAndFlags(len, flags);
    return data;
  }

 public:
  // Returns the offset of header_. JIT code should use offsetOfFlags
  // below.
  static constexpr size_t offsetOfRawHeaderFlagsField() {
    return offsetof(CellWithLengthAndFlags, header_);
  }

  // Offsets for direct field from jit code. A number of places directly
  // access 32-bit length and flags fields so do endian trickery here.
#if JS_BITS_PER_WORD == 32
  static constexpr size_t offsetOfHeaderFlags() {
    return offsetof(CellWithLengthAndFlags, header_);
  }
  static constexpr size_t offsetOfHeaderLength() {
    return offsetof(CellWithLengthAndFlags, length_);
  }
#elif MOZ_LITTLE_ENDIAN()
  static constexpr size_t offsetOfHeaderFlags() {
    return offsetof(CellWithLengthAndFlags, header_);
  }
  static constexpr size_t offsetOfHeaderLength() {
    return offsetof(CellWithLengthAndFlags, header_) + sizeof(uint32_t);
  }
#else
  static constexpr size_t offsetOfHeaderFlags() {
    return offsetof(CellWithLengthAndFlags, header_) + sizeof(uint32_t);
  }
  static constexpr size_t offsetOfHeaderLength() {
    return offsetof(CellWithLengthAndFlags, header_);
  }
#endif
};

// Base class for non-nursery-allocatable GC things that allows storing a non-GC
// thing pointer in the first word.
//
// The low bits of the word (see CellFlagBitsReservedForGC) are reserved for GC.
template <class PtrT>
class alignas(gc::CellAlignBytes) TenuredCellWithNonGCPointer
    : public TenuredCell {
  static_assert(!std::is_pointer_v<PtrT>,
                "PtrT should be the type of the referent, not of the pointer");
  static_assert(
      !std::is_base_of_v<Cell, PtrT>,
      "Don't use TenuredCellWithNonGCPointer for pointers to GC things");

 protected:
  TenuredCellWithNonGCPointer() = default;
  explicit TenuredCellWithNonGCPointer(PtrT* initial) {
    uintptr_t data = uintptr_t(initial);
    MOZ_ASSERT((data & RESERVED_MASK) == 0);
    header_ = data;
  }

  PtrT* headerPtr() const {
    // Currently we never observe any flags set here because this base class is
    // only used for JSObject (for which the nursery kind flags are always
    // clear) or GC things that are always tenured (for which the nursery kind
    // flags are also always clear). This means we don't need to use masking to
    // get and set the pointer.
    MOZ_ASSERT(flags() == 0);
    return reinterpret_cast<PtrT*>(uintptr_t(header_));
  }

  void setHeaderPtr(PtrT* newValue) {
    // As above, no flags are expected to be set here.
    uintptr_t data = uintptr_t(newValue);
    MOZ_ASSERT(flags() == 0);
    MOZ_ASSERT((data & RESERVED_MASK) == 0);
    header_ = data;
  }

 public:
  static constexpr size_t offsetOfHeaderPtr() {
    return offsetof(TenuredCellWithNonGCPointer, header_);
  }
};

// Base class for GC things that have a tenured GC pointer as their first word.
//
// The low bits of the first word (see CellFlagBitsReservedForGC) are reserved
// for GC.
//
// This includes a pre write barrier when the pointer is update. No post barrier
// is necessary as the pointer is always tenured.
template <class BaseCell, class PtrT>
class alignas(gc::CellAlignBytes) CellWithTenuredGCPointer : public BaseCell {
  static void staticAsserts() {
    // These static asserts are not in class scope because the PtrT may not be
    // defined when this class template is instantiated.
    static_assert(
        std::is_same_v<BaseCell, Cell> || std::is_same_v<BaseCell, TenuredCell>,
        "BaseCell must be either Cell or TenuredCell");
    static_assert(
        !std::is_pointer_v<PtrT>,
        "PtrT should be the type of the referent, not of the pointer");
    static_assert(
        std::is_base_of_v<Cell, PtrT>,
        "Only use CellWithTenuredGCPointer for pointers to GC things");
  }

 protected:
  CellWithTenuredGCPointer() = default;
  explicit CellWithTenuredGCPointer(PtrT* initial) { initHeaderPtr(initial); }

  void initHeaderPtr(PtrT* initial) {
    MOZ_ASSERT(!IsInsideNursery(initial));
    uintptr_t data = uintptr_t(initial);
    MOZ_ASSERT((data & Cell::RESERVED_MASK) == 0);
    this->header_ = data;
  }

  void setHeaderPtr(PtrT* newValue) {
    // As above, no flags are expected to be set here.
    MOZ_ASSERT(!IsInsideNursery(newValue));
    PtrT::preWriteBarrier(headerPtr());
    unbarrieredSetHeaderPtr(newValue);
  }

 public:
  PtrT* headerPtr() const {
    // Currently we never observe any flags set here because this base class is
    // only used for GC things that are always tenured (for which the nursery
    // kind flags are also always clear). This means we don't need to use
    // masking to get and set the pointer.
    staticAsserts();
    MOZ_ASSERT(this->flags() == 0);
    return reinterpret_cast<PtrT*>(uintptr_t(this->header_));
  }

  void unbarrieredSetHeaderPtr(PtrT* newValue) {
    uintptr_t data = uintptr_t(newValue);
    MOZ_ASSERT(this->flags() == 0);
    MOZ_ASSERT((data & Cell::RESERVED_MASK) == 0);
    this->header_ = data;
  }

  static constexpr size_t offsetOfHeaderPtr() {
    return offsetof(CellWithTenuredGCPointer, header_);
  }
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_Cell_h */
