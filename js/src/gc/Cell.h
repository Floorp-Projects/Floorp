/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Cell_h
#define gc_Cell_h

#include "gc/GCEnum.h"
#include "gc/Heap.h"
#include "js/GCAnnotations.h"
#include "js/TraceKind.h"
#include "js/TypeDecls.h"

namespace JS {

namespace shadow {
struct Zone;
} /* namespace shadow */

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

// The cell header contains flags used by the GC. All GC cells must start with a
// CellHeader, or one of its derived classes that allows use of spare bits to
// store data.
class CellHeader {
 public:
  static_assert(gc::CellFlagBitsReservedForGC >= 3,
                "Not enough flag bits reserved for GC");

  static constexpr uintptr_t RESERVED_MASK =
      BitMask(gc::CellFlagBitsReservedForGC);

  // Indicates if the cell has been forwarded (moved) by generational or
  // compacting GC and is now a RelocationOverlay.
  static constexpr uintptr_t FORWARD_BIT = Bit(0);

  // When a Cell is in the nursery, this will indicate if it is a JSString (1)
  // or JSObject/BigInt (0). When not in nursery, this bit is still reserved for
  // JSString to use as JSString::NON_ATOM bit. This may be removed by Bug
  // 1376646.
  static constexpr uintptr_t JSSTRING_BIT = Bit(1);

  // When a Cell is in the nursery, this will indicate if it is a BigInt (1)
  // or JSObject/JSString (0).
  static constexpr uintptr_t BIGINT_BIT = Bit(2);

 protected:
  // NOTE: This word can also be used for temporary storage, see
  // setTemporaryGCUnsafeData.
  uintptr_t header_;
};

// [SMDOC] GC Cell
//
// A GC cell is the base class for all GC things. All types allocated on the GC
// heap extend either gc::Cell or gc::TenuredCell. If a type is always tenured,
// prefer the TenuredCell class as base.
//
// The first word (a pointer or uintptr_t) of each Cell must reserve the low
// three bits for GC purposes. The remaining bits are available to sub-classes
// and typically store a pointer to another gc::Cell.
//
// During moving GC operation a Cell may be marked as forwarded. This indicates
// that a gc::RelocationOverlay is currently stored in the Cell's memory and
// should be used to find the new location of the Cell.
struct alignas(gc::CellAlignBytes) Cell {
 public:
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

  static MOZ_ALWAYS_INLINE bool needWriteBarrierPre(JS::Zone* zone);

  inline bool isForwarded() const {
    uintptr_t firstWord = *reinterpret_cast<const uintptr_t*>(this);
    return firstWord & CellHeader::FORWARD_BIT;
  }

  inline bool nurseryCellIsString() const {
    MOZ_ASSERT(!isTenured());
    uintptr_t firstWord = *reinterpret_cast<const uintptr_t*>(this);
    return firstWord & CellHeader::JSSTRING_BIT;
  }

  inline bool nurseryCellIsBigInt() const {
    MOZ_ASSERT(!isTenured());
    uintptr_t firstWord = *reinterpret_cast<const uintptr_t*>(this);
    return firstWord & CellHeader::BIGINT_BIT;
  }

  template <class T>
  inline bool is() const {
    return getTraceKind() == JS::MapTypeToTraceKind<T>::kind;
  }

  template <class T>
  inline T* as() {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<T*>(this);
  }

  template <class T>
  inline const T* as() const {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<const T*>(this);
  }

#ifdef DEBUG
  static inline void assertThingIsNotGray(Cell* cell);
  inline bool isAligned() const;
  void dump(GenericPrinter& out) const;
  void dump() const;
#endif

 protected:
  uintptr_t address() const;
  inline Chunk* chunk() const;
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

  template <class T>
  inline bool is() const {
    return getTraceKind() == JS::MapTypeToTraceKind<T>::kind;
  }

  template <class T>
  inline T* as() {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<T*>(this);
  }

  template <class T>
  inline const T* as() const {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<const T*>(this);
  }

  static MOZ_ALWAYS_INLINE void readBarrier(TenuredCell* thing);
  static MOZ_ALWAYS_INLINE void writeBarrierPre(TenuredCell* thing);

  static void MOZ_ALWAYS_INLINE writeBarrierPost(void* cellp,
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

#ifdef DEBUG
extern Cell* UninlinedForwarded(const Cell* cell);
#endif

inline JS::TraceKind Cell::getTraceKind() const {
  if (isTenured()) {
    MOZ_ASSERT_IF(isForwarded(), UninlinedForwarded(this)->getTraceKind() ==
                                     asTenured().getTraceKind());
    return asTenured().getTraceKind();
  }
  if (nurseryCellIsString()) {
    MOZ_ASSERT_IF(isForwarded(), UninlinedForwarded(this)->getTraceKind() ==
                                     JS::TraceKind::String);
    return JS::TraceKind::String;
  }
  if (nurseryCellIsBigInt()) {
    MOZ_ASSERT_IF(isForwarded(), UninlinedForwarded(this)->getTraceKind() ==
                                     JS::TraceKind::BigInt);
    return JS::TraceKind::BigInt;
  }
  MOZ_ASSERT_IF(isForwarded(), UninlinedForwarded(this)->getTraceKind() ==
                                   JS::TraceKind::Object);
  return JS::TraceKind::Object;
}

/* static */ MOZ_ALWAYS_INLINE bool Cell::needWriteBarrierPre(JS::Zone* zone) {
  return JS::shadow::Zone::from(zone)->needsIncrementalBarrier();
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
  MOZ_ASSERT(thing);
  MOZ_ASSERT(CurrentThreadCanAccessZone(thing->zoneFromAnyThread()));
  // Barriers should not be triggered on main thread while collecting.
  MOZ_ASSERT_IF(CurrentThreadCanAccessRuntime(thing->runtimeFromAnyThread()),
                !JS::RuntimeHeapIsCollecting());

  JS::shadow::Zone* shadowZone = thing->shadowZoneFromAnyThread();
  if (shadowZone->needsIncrementalBarrier()) {
    // Barriers are only enabled on the main thread and are disabled while
    // collecting.
    MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(shadowZone));
    Cell* tmp = thing;
    TraceManuallyBarrieredGenericPointerEdge(shadowZone->barrierTracer(), &tmp,
                                             "read barrier");
    MOZ_ASSERT(tmp == thing);
  }

  if (thing->isMarkedGray()) {
    // There shouldn't be anything marked gray unless we're on the main thread.
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(thing->runtimeFromAnyThread()));
    if (!JS::RuntimeHeapIsCollecting()) {
      JS::UnmarkGrayGCThingRecursively(
          JS::GCCellPtr(thing, thing->getTraceKind()));
    }
  }
}

void AssertSafeToSkipBarrier(TenuredCell* thing);

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::writeBarrierPre(
    TenuredCell* thing) {
  MOZ_ASSERT(!CurrentThreadIsIonCompiling());
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
  MOZ_ASSERT_IF(
      thing,
      MapAllocToTraceKind(thing->getAllocKind()) != JS::TraceKind::Object &&
          MapAllocToTraceKind(thing->getAllocKind()) != JS::TraceKind::String);
}

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::writeBarrierPost(
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

// Cell header for GC things that have 32-bit length and 32-bit flags (currently
// JSString and BigInt).
//
// This tries to store both in CellHeader::header_, but if that isn't large
// enough the length is stored separately.
//
//          32       0
//  ------------------
//  | Length | Flags |
//  ------------------
//
// The low bits of the flags word (see CellFlagBitsReservedForGC) are reserved
// for GC. Derived classes must ensure they don't use these flags for non-GC
// purposes.
class CellHeaderWithLengthAndFlags : public CellHeader {
#if JS_BITS_PER_WORD == 32
  // Additional storage for length if |header_| is too small to fit both.
  uint32_t length_;
#endif

 public:
  uint32_t lengthField() const {
#if JS_BITS_PER_WORD == 32
    return length_;
#else
    return uint32_t(header_ >> 32);
#endif
  }

  uint32_t flagsField() const { return uint32_t(header_); }

  void setFlagBit(uint32_t flag) { header_ |= uintptr_t(flag); }
  void clearFlagBit(uint32_t flag) { header_ &= ~uintptr_t(flag); }
  void toggleFlagBit(uint32_t flag) { header_ ^= uintptr_t(flag); }

  void setLengthAndFlags(uint32_t len, uint32_t flags) {
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
    setLengthAndFlags(len, flags);
    return data;
  }

  // Returns the offset of header_. JIT code should use offsetOfFlags
  // below.
  static constexpr size_t offsetOfRawFlagsField() {
    return offsetof(CellHeaderWithLengthAndFlags, header_);
  }

  // Offsets for direct field from jit code. A number of places directly
  // access 32-bit length and flags fields so do endian trickery here.
#if JS_BITS_PER_WORD == 32
  static constexpr size_t offsetOfFlags() {
    return offsetof(CellHeaderWithLengthAndFlags, header_);
  }
  static constexpr size_t offsetOfLength() {
    return offsetof(CellHeaderWithLengthAndFlags, length_);
  }
#elif MOZ_LITTLE_ENDIAN()
  static constexpr size_t offsetOfFlags() {
    return offsetof(CellHeaderWithLengthAndFlags, header_);
  }
  static constexpr size_t offsetOfLength() {
    return offsetof(CellHeaderWithLengthAndFlags, header_) + sizeof(uint32_t);
  }
#else
  static constexpr size_t offsetOfFlags() {
    return offsetof(CellHeaderWithLengthAndFlags, header_) + sizeof(uint32_t);
  }
  static constexpr size_t offsetOfLength() {
    return offsetof(CellHeaderWithLengthAndFlags, header_);
  }
#endif
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_Cell_h */
