/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ArenaList_h
#define gc_ArenaList_h

#include "gc/Heap.h"
#include "js/SliceBudget.h"
#include "threading/ProtectedData.h"

namespace JS {

struct Zone;

} /* namespace JS */

namespace js {

class Nursery;
class TenuringTracer;

namespace gc {

struct FinalizePhase;

/*
 * A single segment of a SortedArenaList. Each segment has a head and a tail,
 * which track the start and end of a segment for O(1) append and concatenation.
 */
struct SortedArenaListSegment
{
    Arena* head;
    Arena** tailp;

    void clear() {
        head = nullptr;
        tailp = &head;
    }

    bool isEmpty() const {
        return tailp == &head;
    }

    // Appends |arena| to this segment.
    void append(Arena* arena) {
        MOZ_ASSERT(arena);
        MOZ_ASSERT_IF(head, head->getAllocKind() == arena->getAllocKind());
        *tailp = arena;
        tailp = &arena->next;
    }

    // Points the tail of this segment at |arena|, which may be null. Note
    // that this does not change the tail itself, but merely which arena
    // follows it. This essentially turns the tail into a cursor (see also the
    // description of ArenaList), but from the perspective of a SortedArenaList
    // this makes no difference.
    void linkTo(Arena* arena) {
        *tailp = arena;
    }
};

/*
 * Arena lists have a head and a cursor. The cursor conceptually lies on arena
 * boundaries, i.e. before the first arena, between two arenas, or after the
 * last arena.
 *
 * Arenas are usually sorted in order of increasing free space, with the cursor
 * following the Arena currently being allocated from. This ordering should not
 * be treated as an invariant, however, as the free lists may be cleared,
 * leaving arenas previously used for allocation partially full. Sorting order
 * is restored during sweeping.

 * Arenas following the cursor should not be full.
 */
class ArenaList {
    // The cursor is implemented via an indirect pointer, |cursorp_|, to allow
    // for efficient list insertion at the cursor point and other list
    // manipulations.
    //
    // - If the list is empty: |head| is null, |cursorp_| points to |head|, and
    //   therefore |*cursorp_| is null.
    //
    // - If the list is not empty: |head| is non-null, and...
    //
    //   - If the cursor is at the start of the list: |cursorp_| points to
    //     |head|, and therefore |*cursorp_| points to the first arena.
    //
    //   - If cursor is at the end of the list: |cursorp_| points to the |next|
    //     field of the last arena, and therefore |*cursorp_| is null.
    //
    //   - If the cursor is at neither the start nor the end of the list:
    //     |cursorp_| points to the |next| field of the arena preceding the
    //     cursor, and therefore |*cursorp_| points to the arena following the
    //     cursor.
    //
    // |cursorp_| is never null.
    //
    Arena* head_;
    Arena** cursorp_;

    void copy(const ArenaList& other) {
        other.check();
        head_ = other.head_;
        cursorp_ = other.isCursorAtHead() ? &head_ : other.cursorp_;
        check();
    }

  public:
    ArenaList() {
        clear();
    }

    ArenaList(const ArenaList& other) {
        copy(other);
    }

    ArenaList& operator=(const ArenaList& other) {
        copy(other);
        return *this;
    }

    explicit ArenaList(const SortedArenaListSegment& segment) {
        head_ = segment.head;
        cursorp_ = segment.isEmpty() ? &head_ : segment.tailp;
        check();
    }

    // This does checking just of |head_| and |cursorp_|.
    void check() const {
#ifdef DEBUG
        // If the list is empty, it must have this form.
        MOZ_ASSERT_IF(!head_, cursorp_ == &head_);

        // If there's an arena following the cursor, it must not be full.
        Arena* cursor = *cursorp_;
        MOZ_ASSERT_IF(cursor, cursor->hasFreeThings());
#endif
    }

    void clear() {
        head_ = nullptr;
        cursorp_ = &head_;
        check();
    }

    ArenaList copyAndClear() {
        ArenaList result = *this;
        clear();
        return result;
    }

    bool isEmpty() const {
        check();
        return !head_;
    }

    // This returns nullptr if the list is empty.
    Arena* head() const {
        check();
        return head_;
    }

    bool isCursorAtHead() const {
        check();
        return cursorp_ == &head_;
    }

    bool isCursorAtEnd() const {
        check();
        return !*cursorp_;
    }

    void moveCursorToEnd() {
        while (!isCursorAtEnd())
            cursorp_ = &(*cursorp_)->next;
    }

    // This can return nullptr.
    Arena* arenaAfterCursor() const {
        check();
        return *cursorp_;
    }

    // This returns the arena after the cursor and moves the cursor past it.
    Arena* takeNextArena() {
        check();
        Arena* arena = *cursorp_;
        if (!arena)
            return nullptr;
        cursorp_ = &arena->next;
        check();
        return arena;
    }

    // This does two things.
    // - Inserts |a| at the cursor.
    // - Leaves the cursor sitting just before |a|, if |a| is not full, or just
    //   after |a|, if |a| is full.
    void insertAtCursor(Arena* a) {
        check();
        a->next = *cursorp_;
        *cursorp_ = a;
        // At this point, the cursor is sitting before |a|. Move it after |a|
        // if necessary.
        if (!a->hasFreeThings())
            cursorp_ = &a->next;
        check();
    }

    // Inserts |a| at the cursor, then moves the cursor past it.
    void insertBeforeCursor(Arena* a) {
        check();
        a->next = *cursorp_;
        *cursorp_ = a;
        cursorp_ = &a->next;
        check();
    }

    // This inserts |other|, which must be full, at the cursor of |this|.
    ArenaList& insertListWithCursorAtEnd(const ArenaList& other) {
        check();
        other.check();
        MOZ_ASSERT(other.isCursorAtEnd());
        if (other.isCursorAtHead())
            return *this;
        // Insert the full arenas of |other| after those of |this|.
        *other.cursorp_ = *cursorp_;
        *cursorp_ = other.head_;
        cursorp_ = other.cursorp_;
        check();
        return *this;
    }

    Arena* removeRemainingArenas(Arena** arenap);
    Arena** pickArenasToRelocate(size_t& arenaTotalOut, size_t& relocTotalOut);
    Arena* relocateArenas(Arena* toRelocate, Arena* relocated,
                          js::SliceBudget& sliceBudget, gcstats::Statistics& stats);
};

/*
 * A class that holds arenas in sorted order by appending arenas to specific
 * segments. Each segment has a head and a tail, which can be linked up to
 * other segments to create a contiguous ArenaList.
 */
class SortedArenaList
{
  public:
    // The minimum size, in bytes, of a GC thing.
    static const size_t MinThingSize = 16;

    static_assert(ArenaSize <= 4096, "When increasing the Arena size, please consider how"\
                                     " this will affect the size of a SortedArenaList.");

    static_assert(MinThingSize >= 16, "When decreasing the minimum thing size, please consider"\
                                      " how this will affect the size of a SortedArenaList.");

  private:
    // The maximum number of GC things that an arena can hold.
    static const size_t MaxThingsPerArena = (ArenaSize - ArenaHeaderSize) / MinThingSize;

    size_t thingsPerArena_;
    SortedArenaListSegment segments[MaxThingsPerArena + 1];

    // Convenience functions to get the nth head and tail.
    Arena* headAt(size_t n) { return segments[n].head; }
    Arena** tailAt(size_t n) { return segments[n].tailp; }

  public:
    explicit SortedArenaList(size_t thingsPerArena = MaxThingsPerArena) {
        reset(thingsPerArena);
    }

    void setThingsPerArena(size_t thingsPerArena) {
        MOZ_ASSERT(thingsPerArena && thingsPerArena <= MaxThingsPerArena);
        thingsPerArena_ = thingsPerArena;
    }

    // Resets the first |thingsPerArena| segments of this list for further use.
    void reset(size_t thingsPerArena = MaxThingsPerArena) {
        setThingsPerArena(thingsPerArena);
        // Initialize the segments.
        for (size_t i = 0; i <= thingsPerArena; ++i)
            segments[i].clear();
    }

    // Inserts an arena, which has room for |nfree| more things, in its segment.
    void insertAt(Arena* arena, size_t nfree) {
        MOZ_ASSERT(nfree <= thingsPerArena_);
        segments[nfree].append(arena);
    }

    // Remove all empty arenas, inserting them as a linked list.
    void extractEmpty(Arena** empty) {
        SortedArenaListSegment& segment = segments[thingsPerArena_];
        if (segment.head) {
            *segment.tailp = *empty;
            *empty = segment.head;
            segment.clear();
        }
    }

    // Links up the tail of each non-empty segment to the head of the next
    // non-empty segment, creating a contiguous list that is returned as an
    // ArenaList. This is not a destructive operation: neither the head nor tail
    // of any segment is modified. However, note that the Arenas in the
    // resulting ArenaList should be treated as read-only unless the
    // SortedArenaList is no longer needed: inserting or removing arenas would
    // invalidate the SortedArenaList.
    ArenaList toArenaList() {
        // Link the non-empty segment tails up to the non-empty segment heads.
        size_t tailIndex = 0;
        for (size_t headIndex = 1; headIndex <= thingsPerArena_; ++headIndex) {
            if (headAt(headIndex)) {
                segments[tailIndex].linkTo(headAt(headIndex));
                tailIndex = headIndex;
            }
        }
        // Point the tail of the final non-empty segment at null. Note that if
        // the list is empty, this will just set segments[0].head to null.
        segments[tailIndex].linkTo(nullptr);
        // Create an ArenaList with head and cursor set to the head and tail of
        // the first segment (if that segment is empty, only the head is used).
        return ArenaList(segments[0]);
    }
};

enum class ShouldCheckThresholds
{
    DontCheckThresholds = 0,
    CheckThresholds = 1
};

class ArenaLists
{
    JSRuntime* const runtime_;

    /*
     * For each arena kind its free list is represented as the first span with
     * free things. Initially all the spans are initialized as empty. After we
     * find a new arena with available things we move its first free span into
     * the list and set the arena as fully allocated. way we do not need to
     * update the arena after the initial allocation. When starting the
     * GC we only move the head of the of the list of spans back to the arena
     * only for the arena that was not fully allocated.
     */
    ZoneGroupData<AllAllocKindArray<FreeSpan*>> freeLists_;
    FreeSpan*& freeLists(AllocKind i) { return freeLists_.ref()[i]; }
    FreeSpan* freeLists(AllocKind i) const { return freeLists_.ref()[i]; }

    // Because the JITs can allocate from the free lists, they cannot be null.
    // We use a placeholder FreeSpan that is empty (and wihout an associated
    // Arena) so the JITs can fall back gracefully.
    static FreeSpan placeholder;

    ZoneGroupOrGCTaskData<AllAllocKindArray<ArenaList>> arenaLists_;
    ArenaList& arenaLists(AllocKind i) { return arenaLists_.ref()[i]; }
    const ArenaList& arenaLists(AllocKind i) const { return arenaLists_.ref()[i]; }

    enum BackgroundFinalizeStateEnum { BFS_DONE, BFS_RUN };

    typedef mozilla::Atomic<BackgroundFinalizeStateEnum, mozilla::SequentiallyConsistent>
        BackgroundFinalizeState;

    /* The current background finalization state, accessed atomically. */
    UnprotectedData<AllAllocKindArray<BackgroundFinalizeState>> backgroundFinalizeState_;
    BackgroundFinalizeState& backgroundFinalizeState(AllocKind i) { return backgroundFinalizeState_.ref()[i]; }
    const BackgroundFinalizeState& backgroundFinalizeState(AllocKind i) const { return backgroundFinalizeState_.ref()[i]; }

    /* For each arena kind, a list of arenas remaining to be swept. */
    ActiveThreadOrGCTaskData<AllAllocKindArray<Arena*>> arenaListsToSweep_;
    Arena*& arenaListsToSweep(AllocKind i) { return arenaListsToSweep_.ref()[i]; }
    Arena* arenaListsToSweep(AllocKind i) const { return arenaListsToSweep_.ref()[i]; }

    /* During incremental sweeping, a list of the arenas already swept. */
    ZoneGroupOrGCTaskData<AllocKind> incrementalSweptArenaKind;
    ZoneGroupOrGCTaskData<ArenaList> incrementalSweptArenas;

    // Arena lists which have yet to be swept, but need additional foreground
    // processing before they are swept.
    ZoneGroupData<Arena*> gcShapeArenasToUpdate;
    ZoneGroupData<Arena*> gcAccessorShapeArenasToUpdate;
    ZoneGroupData<Arena*> gcScriptArenasToUpdate;
    ZoneGroupData<Arena*> gcObjectGroupArenasToUpdate;

    // While sweeping type information, these lists save the arenas for the
    // objects which have already been finalized in the foreground (which must
    // happen at the beginning of the GC), so that type sweeping can determine
    // which of the object pointers are marked.
    ZoneGroupData<ObjectAllocKindArray<ArenaList>> savedObjectArenas_;
    ArenaList& savedObjectArenas(AllocKind i) { return savedObjectArenas_.ref()[i]; }
    ZoneGroupData<Arena*> savedEmptyObjectArenas;

  public:
    explicit ArenaLists(JSRuntime* rt, ZoneGroup* group);
    ~ArenaLists();

    const void* addressOfFreeList(AllocKind thingKind) const {
        return reinterpret_cast<const void*>(&freeLists_.refNoCheck()[thingKind]);
    }

    Arena* getFirstArena(AllocKind thingKind) const {
        return arenaLists(thingKind).head();
    }

    Arena* getFirstArenaToSweep(AllocKind thingKind) const {
        return arenaListsToSweep(thingKind);
    }

    Arena* getFirstSweptArena(AllocKind thingKind) const {
        if (thingKind != incrementalSweptArenaKind.ref())
            return nullptr;
        return incrementalSweptArenas.ref().head();
    }

    Arena* getArenaAfterCursor(AllocKind thingKind) const {
        return arenaLists(thingKind).arenaAfterCursor();
    }

    bool arenaListsAreEmpty() const {
        for (auto i : AllAllocKinds()) {
            /*
             * The arena cannot be empty if the background finalization is not yet
             * done.
             */
            if (backgroundFinalizeState(i) != BFS_DONE)
                return false;
            if (!arenaLists(i).isEmpty())
                return false;
        }
        return true;
    }

    void unmarkAll() {
        for (auto i : AllAllocKinds()) {
            /* The background finalization must have stopped at this point. */
            MOZ_ASSERT(backgroundFinalizeState(i) == BFS_DONE);
            for (Arena* arena = arenaLists(i).head(); arena; arena = arena->next)
                arena->unmarkAll();
        }
    }

    bool doneBackgroundFinalize(AllocKind kind) const {
        return backgroundFinalizeState(kind) == BFS_DONE;
    }

    bool needBackgroundFinalizeWait(AllocKind kind) const {
        return backgroundFinalizeState(kind) != BFS_DONE;
    }

    /*
     * Clear the free lists so we won't try to allocate from swept arenas.
     */
    void purge() {
        for (auto i : AllAllocKinds())
            freeLists(i) = &placeholder;
    }

    inline void prepareForIncrementalGC();

    /* Check if this arena is in use. */
    bool arenaIsInUse(Arena* arena, AllocKind kind) const {
        MOZ_ASSERT(arena);
        return arena == freeLists(kind)->getArenaUnchecked();
    }

    MOZ_ALWAYS_INLINE TenuredCell* allocateFromFreeList(AllocKind thingKind, size_t thingSize) {
        return freeLists(thingKind)->allocate(thingSize);
    }

    /*
     * Moves all arenas from |fromArenaLists| into |this|.
     */
    void adoptArenas(JSRuntime* runtime, ArenaLists* fromArenaLists, bool targetZoneIsCollecting);

    /* True if the Arena in question is found in this ArenaLists */
    bool containsArena(JSRuntime* runtime, Arena* arena);

    void checkEmptyFreeLists() {
#ifdef DEBUG
        for (auto i : AllAllocKinds())
            checkEmptyFreeList(i);
#endif
    }

    bool checkEmptyArenaLists() {
        bool empty = true;
#ifdef DEBUG
        for (auto i : AllAllocKinds()) {
            if (!checkEmptyArenaList(i))
                empty = false;
        }
#endif
        return empty;
    }

    void checkEmptyFreeList(AllocKind kind) {
        MOZ_ASSERT(freeLists(kind)->isEmpty());
    }

    bool checkEmptyArenaList(AllocKind kind);

    bool relocateArenas(JS::Zone* zone, Arena*& relocatedListOut, JS::gcreason::Reason reason,
                        js::SliceBudget& sliceBudget, gcstats::Statistics& stats);

    void queueForegroundObjectsForSweep(FreeOp* fop);
    void queueForegroundThingsForSweep(FreeOp* fop);

    void mergeForegroundSweptObjectArenas();

    bool foregroundFinalize(FreeOp* fop, AllocKind thingKind, js::SliceBudget& sliceBudget,
                            SortedArenaList& sweepList);
    static void backgroundFinalize(FreeOp* fop, Arena* listHead, Arena** empty);

    // When finalizing arenas, whether to keep empty arenas on the list or
    // release them immediately.
    enum KeepArenasEnum {
        RELEASE_ARENAS,
        KEEP_ARENAS
    };

  private:
    inline void queueForForegroundSweep(FreeOp* fop, const FinalizePhase& phase);
    inline void queueForBackgroundSweep(FreeOp* fop, const FinalizePhase& phase);
    inline void queueForForegroundSweep(FreeOp* fop, AllocKind thingKind);
    inline void queueForBackgroundSweep(FreeOp* fop, AllocKind thingKind);
    inline void mergeSweptArenas(AllocKind thingKind);

    TenuredCell* allocateFromArena(JS::Zone* zone, AllocKind thingKind,
                                   ShouldCheckThresholds checkThresholds);
    inline TenuredCell* allocateFromArenaInner(JS::Zone* zone, Arena* arena, AllocKind kind);

    friend class GCRuntime;
    friend class js::Nursery;
    friend class js::TenuringTracer;
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_ArenaList_h */

