/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ArenaList_inl_h
#define gc_ArenaList_inl_h

#include "gc/ArenaList.h"

#include "gc/Heap.h"
#include "gc/Zone.h"

inline js::gc::ArenaList::ArenaList() { clear(); }

inline js::gc::ArenaList::ArenaList(ArenaList&& other) { moveFrom(other); }

inline js::gc::ArenaList::~ArenaList() { MOZ_ASSERT(isEmpty()); }

void js::gc::ArenaList::moveFrom(ArenaList& other) {
  other.check();

  head_ = other.head_;
  cursorp_ = other.isCursorAtHead() ? &head_ : other.cursorp_;
  other.clear();

  check();
}

js::gc::ArenaList& js::gc::ArenaList::operator=(ArenaList&& other) {
  MOZ_ASSERT(isEmpty());
  moveFrom(other);
  return *this;
}

inline js::gc::ArenaList::ArenaList(Arena* head, Arena* arenaBeforeCursor)
    : head_(head),
      cursorp_(arenaBeforeCursor ? &arenaBeforeCursor->next : &head_) {
  check();
}

// This does checking just of |head_| and |cursorp_|.
void js::gc::ArenaList::check() const {
#ifdef DEBUG
  // If the list is empty, it must have this form.
  MOZ_ASSERT_IF(!head_, cursorp_ == &head_);

  // If there's an arena following the cursor, it must not be full.
  Arena* cursor = *cursorp_;
  MOZ_ASSERT_IF(cursor, cursor->hasFreeThings());
#endif
}

void js::gc::ArenaList::clear() {
  head_ = nullptr;
  cursorp_ = &head_;
  check();
}

bool js::gc::ArenaList::isEmpty() const {
  check();
  return !head_;
}

js::gc::Arena* js::gc::ArenaList::head() const {
  check();
  return head_;
}

bool js::gc::ArenaList::isCursorAtHead() const {
  check();
  return cursorp_ == &head_;
}

bool js::gc::ArenaList::isCursorAtEnd() const {
  check();
  return !*cursorp_;
}

js::gc::Arena* js::gc::ArenaList::arenaAfterCursor() const {
  check();
  return *cursorp_;
}

js::gc::Arena* js::gc::ArenaList::takeNextArena() {
  check();
  Arena* arena = *cursorp_;
  if (!arena) {
    return nullptr;
  }
  cursorp_ = &arena->next;
  check();
  return arena;
}

void js::gc::ArenaList::insertAtCursor(Arena* a) {
  check();
  a->next = *cursorp_;
  *cursorp_ = a;
  // At this point, the cursor is sitting before |a|. Move it after |a|
  // if necessary.
  if (!a->hasFreeThings()) {
    cursorp_ = &a->next;
  }
  check();
}

void js::gc::ArenaList::insertBeforeCursor(Arena* a) {
  check();
  a->next = *cursorp_;
  *cursorp_ = a;
  cursorp_ = &a->next;
  check();
}

js::gc::ArenaList& js::gc::ArenaList::insertListWithCursorAtEnd(
    ArenaList& other) {
  check();
  other.check();
  MOZ_ASSERT(other.isCursorAtEnd());

  if (other.isEmpty()) {
    return *this;
  }

  // Insert the full arenas of |other| after those of |this|.
  *other.cursorp_ = *cursorp_;
  *cursorp_ = other.head_;
  cursorp_ = other.cursorp_;
  check();

  other.clear();
  return *this;
}

js::gc::Arena* js::gc::ArenaList::takeFirstArena() {
  check();
  Arena* arena = head_;
  if (!arena) {
    return nullptr;
  }

  head_ = arena->next;
  arena->next = nullptr;
  if (cursorp_ == &arena->next) {
    cursorp_ = &head_;
  }

  check();

  return arena;
}

js::gc::SortedArenaList::SortedArenaList(size_t thingsPerArena)
    : thingsPerArena_(thingsPerArena) {
  MOZ_ASSERT(thingsPerArena < SegmentCount);
}

void js::gc::SortedArenaList::insertAt(Arena* arena, size_t nfree) {
  MOZ_ASSERT(!isConvertedToArenaList);
  MOZ_ASSERT(nfree <= thingsPerArena_);
  segments[nfree].pushBack(arena);
}

void js::gc::SortedArenaList::extractEmptyTo(Arena** destListHeadPtr) {
  MOZ_ASSERT(!isConvertedToArenaList);
  check();

  Segment& segment = segments[thingsPerArena_];
  if (!segment.isEmpty()) {
    Arena* tail = *destListHeadPtr;
    Arena* segmentLast = segment.last();
    *destListHeadPtr = segment.release();
    segmentLast->next = tail;
  }
  MOZ_ASSERT(segment.isEmpty());
}

js::gc::ArenaList js::gc::SortedArenaList::convertToArenaList(
    Arena* maybeSegmentLastOut[SegmentCount]) {
#ifdef DEBUG
  MOZ_ASSERT(!isConvertedToArenaList);
  isConvertedToArenaList = true;
  check();
#endif

  if (maybeSegmentLastOut) {
    for (size_t i = 0; i < SegmentCount; i++) {
      maybeSegmentLastOut[i] = segments[i].last();
    }
  }

  // The cursor of the returned ArenaList needs to be between the last full
  // arena and the first arena with space. Record that here.
  Arena* lastFullArena = segments[0].last();

  Segment result;
  for (size_t i = 0; i <= thingsPerArena_; ++i) {
    result.append(std::move(segments[i]));
  }

  return ArenaList(result.release(), lastFullArena);
}

void js::gc::SortedArenaList::restoreFromArenaList(
    ArenaList& list, Arena* segmentLast[SegmentCount]) {
#ifdef DEBUG
  MOZ_ASSERT(isConvertedToArenaList);
  isConvertedToArenaList = false;
#endif

  // Group the ArenaList elements into SinglyLinkedList buckets, where the
  // boundaries between buckets are retrieved from |segmentLast|.

  Arena* remaining = list.head();
  list.clear();

  for (size_t i = 0; i <= thingsPerArena_; i++) {
    MOZ_ASSERT(segments[i].isEmpty());
    if (segmentLast[i]) {
      Arena* first = remaining;
      Arena* last = segmentLast[i];
      remaining = last->next;
      new (&segments[i]) Segment(first, last);
    }
  }

  check();
}

void js::gc::SortedArenaList::check() const {
#ifdef DEBUG
  AllocKind kind = AllocKind::LIMIT;
  for (size_t i = 0; i < SegmentCount; i++) {
    const auto& segment = segments[i];
    if (kind == AllocKind::LIMIT && !segment.isEmpty()) {
      kind = segment.first()->getAllocKind();
      MOZ_ASSERT(thingsPerArena_ == Arena::thingsPerArena(kind));
    }
    if (i > thingsPerArena_) {
      MOZ_ASSERT(segment.isEmpty());
    }
    for (auto arena = segment.iter(); !arena.done(); arena.next()) {
      MOZ_ASSERT(arena->getAllocKind() == kind);
      MOZ_ASSERT(arena->countFreeCells() == i);
    }
  }
#endif
}

#ifdef DEBUG

bool js::gc::FreeLists::allEmpty() const {
  for (auto i : AllAllocKinds()) {
    if (!isEmpty(i)) {
      return false;
    }
  }
  return true;
}

bool js::gc::FreeLists::isEmpty(AllocKind kind) const {
  return freeLists_[kind]->isEmpty();
}

#endif

void js::gc::FreeLists::clear() {
  for (auto i : AllAllocKinds()) {
#ifdef DEBUG
    auto old = freeLists_[i];
    if (!old->isEmpty()) {
      old->getArena()->checkNoMarkedFreeCells();
    }
#endif
    freeLists_[i] = &emptySentinel;
  }
}

js::gc::TenuredCell* js::gc::FreeLists::allocate(AllocKind kind) {
  return freeLists_[kind]->allocate(Arena::thingSize(kind));
}

void js::gc::FreeLists::unmarkPreMarkedFreeCells(AllocKind kind) {
  FreeSpan* freeSpan = freeLists_[kind];
  if (!freeSpan->isEmpty()) {
    freeSpan->getArena()->unmarkPreMarkedFreeCells();
  }
}

JSRuntime* js::gc::ArenaLists::runtime() {
  return zone_->runtimeFromMainThread();
}

JSRuntime* js::gc::ArenaLists::runtimeFromAnyThread() {
  return zone_->runtimeFromAnyThread();
}

js::gc::Arena* js::gc::ArenaLists::getFirstArena(AllocKind thingKind) const {
  return arenaList(thingKind).head();
}

js::gc::Arena* js::gc::ArenaLists::getFirstCollectingArena(
    AllocKind thingKind) const {
  return collectingArenaList(thingKind).head();
}

js::gc::Arena* js::gc::ArenaLists::getArenaAfterCursor(
    AllocKind thingKind) const {
  return arenaList(thingKind).arenaAfterCursor();
}

bool js::gc::ArenaLists::arenaListsAreEmpty() const {
  for (auto i : AllAllocKinds()) {
    /*
     * The arena cannot be empty if the background finalization is not yet
     * done.
     */
    if (concurrentUse(i) == ConcurrentUse::BackgroundFinalize) {
      return false;
    }
    if (!arenaList(i).isEmpty()) {
      return false;
    }
  }
  return true;
}

bool js::gc::ArenaLists::doneBackgroundFinalize(AllocKind kind) const {
  return concurrentUse(kind) != ConcurrentUse::BackgroundFinalize;
}

bool js::gc::ArenaLists::needBackgroundFinalizeWait(AllocKind kind) const {
  return concurrentUse(kind) == ConcurrentUse::BackgroundFinalize;
}

void js::gc::ArenaLists::clearFreeLists() { freeLists().clear(); }

MOZ_ALWAYS_INLINE js::gc::TenuredCell* js::gc::ArenaLists::allocateFromFreeList(
    AllocKind thingKind) {
  return freeLists().allocate(thingKind);
}

void js::gc::ArenaLists::unmarkPreMarkedFreeCells() {
  for (auto i : AllAllocKinds()) {
    freeLists().unmarkPreMarkedFreeCells(i);
  }
}

void js::gc::ArenaLists::checkEmptyFreeLists() {
  MOZ_ASSERT(freeLists().allEmpty());
}

void js::gc::ArenaLists::checkEmptyArenaLists() {
#ifdef DEBUG
  for (auto i : AllAllocKinds()) {
    checkEmptyArenaList(i);
  }
#endif
}

#endif  // gc_ArenaList_inl_h
