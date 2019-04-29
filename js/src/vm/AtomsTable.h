/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation details of the atoms table.
 */

#ifndef vm_AtomsTable_h
#define vm_AtomsTable_h

#include <type_traits>  // std::{enable_if,is_const}

#include "js/GCHashTable.h"
#include "js/TypeDecls.h"
#include "vm/JSAtom.h"

/*
 * The atoms table is a mapping from strings to JSAtoms that supports concurrent
 * access and incremental sweeping.
 *
 * The table is partitioned based on the key into multiple sub-tables. Each
 * sub-table is protected by a lock to ensure safety when accessed by helper
 * threads. Concurrent access improves performance of off-thread parsing which
 * frequently creates large numbers of atoms. Locking is only required when
 * off-thread parsing is running.
 */

namespace js {

// Take all atoms table locks to allow iterating over cells in the atoms zone.
class MOZ_RAII AutoLockAllAtoms {
  JSRuntime* runtime;

 public:
  explicit AutoLockAllAtoms(JSRuntime* rt);
  ~AutoLockAllAtoms();
};

struct AtomHasher {
  struct Lookup;
  static inline HashNumber hash(const Lookup& l);
  static MOZ_ALWAYS_INLINE bool match(const WeakHeapPtr<JSAtom*>& entry,
                                      const Lookup& lookup);
};

using AtomSet = JS::GCHashSet<WeakHeapPtr<JSAtom*>, AtomHasher,
                              SystemAllocPolicy>;

// This class is a wrapper for AtomSet that is used to ensure the AtomSet is
// not modified. It should only expose read-only methods from AtomSet.
// Note however that the atoms within the table can be marked during GC.
class FrozenAtomSet {
  AtomSet* mSet;

 public:
  // This constructor takes ownership of the passed-in AtomSet.
  explicit FrozenAtomSet(AtomSet* set) { mSet = set; }

  ~FrozenAtomSet() { js_delete(mSet); }

  MOZ_ALWAYS_INLINE AtomSet::Ptr readonlyThreadsafeLookup(
      const AtomSet::Lookup& l) const;

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mSet->shallowSizeOfIncludingThis(mallocSizeOf);
  }

  typedef AtomSet::Range Range;

  AtomSet::Range all() const { return mSet->all(); }
};

class AtomsTable {
  static const size_t PartitionShift = 5;
  static const size_t PartitionCount = 1 << PartitionShift;

  // Use a low initial capacity for atom hash tables to avoid penalizing
  // runtimes which create a small number of atoms.
  static const size_t InitialTableSize = 16;

  // A single partition, representing a subset of the atoms in the table.
  struct Partition {
    explicit Partition(uint32_t index);
    ~Partition();

    // Lock that must be held to access this set.
    Mutex lock;

    // The atoms in this set.
    AtomSet atoms;

    // Set of atoms added while the |atoms| set is being swept.
    AtomSet* atomsAddedWhileSweeping;
  };

  Partition* partitions[PartitionCount];

#ifdef DEBUG
  bool allPartitionsLocked = false;
#endif

 public:
  class AutoLock;

  // An iterator used for sweeping atoms incrementally.
  class SweepIterator {
    AtomsTable& atoms;
    size_t partitionIndex;
    mozilla::Maybe<AtomSet::Enum> atomsIter;

    void settle();
    void startSweepingPartition();
    void finishSweepingPartition();

   public:
    explicit SweepIterator(AtomsTable& atoms);
    bool empty() const;
    JSAtom* front() const;
    void removeFront();
    void popFront();
  };

  ~AtomsTable();
  bool init();

  template <typename Chars>
  MOZ_ALWAYS_INLINE JSAtom* atomizeAndCopyChars(
      JSContext* cx, Chars chars, size_t length, PinningBehavior pin,
      const mozilla::Maybe<uint32_t>& indexValue,
      const AtomHasher::Lookup& lookup);

  template <typename CharT, typename = typename std::enable_if<
                                !std::is_const<CharT>::value>::type>
  MOZ_ALWAYS_INLINE JSAtom* atomizeAndCopyChars(
      JSContext* cx, CharT* chars, size_t length, PinningBehavior pin,
      const mozilla::Maybe<uint32_t>& indexValue,
      const AtomHasher::Lookup& lookup) {
    return atomizeAndCopyChars(cx, const_cast<const CharT*>(chars), length, pin,
                               indexValue, lookup);
  }

  void pinExistingAtom(JSContext* cx, JSAtom* atom);

  void tracePinnedAtoms(JSTracer* trc, const AutoAccessAtomsZone& access);

  // Sweep all atoms non-incrementally.
  void sweepAll(JSRuntime* rt);

  bool startIncrementalSweep();

  // Sweep some atoms incrementally and return whether we finished.
  bool sweepIncrementally(SweepIterator& atomsToSweep, SliceBudget& budget);

#ifdef DEBUG
  bool mainThreadHasAllLocks() const { return allPartitionsLocked; }
#endif

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  // Map a key to a partition based on its hash.
  MOZ_ALWAYS_INLINE size_t getPartitionIndex(const AtomHasher::Lookup& lookup);

  void tracePinnedAtomsInSet(JSTracer* trc, AtomSet& atoms);
  void mergeAtomsAddedWhileSweeping(Partition& partition);

  friend class AutoLockAllAtoms;
  void lockAll();
  void unlockAll();
};

}  // namespace js

#endif /* vm_AtomsTable_h */
