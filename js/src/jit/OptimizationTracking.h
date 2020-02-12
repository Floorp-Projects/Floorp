/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_OptimizationTracking_h
#define jit_OptimizationTracking_h

#include "mozilla/Maybe.h"

#include "jit/CompactBuffer.h"
#include "jit/CompileInfo.h"
#include "jit/JitAllocPolicy.h"
#include "jit/JitSpewer.h"
#include "jit/shared/Assembler-shared.h"
#include "js/TrackedOptimizationInfo.h"
#include "vm/TypeInference.h"

namespace js {

namespace jit {

struct NativeToTrackedOptimizations {
  // [startOffset, endOffset]
  CodeOffset startOffset;
  CodeOffset endOffset;
  const TrackedOptimizations* optimizations;
};

class OptimizationAttempt {
  JS::TrackedStrategy strategy_;
  JS::TrackedOutcome outcome_;

 public:
  OptimizationAttempt(JS::TrackedStrategy strategy, JS::TrackedOutcome outcome)
      : strategy_(strategy), outcome_(outcome) {}

  void setOutcome(JS::TrackedOutcome outcome) { outcome_ = outcome; }
  bool succeeded() const {
    return outcome_ >= JS::TrackedOutcome::GenericSuccess;
  }
  bool failed() const { return outcome_ < JS::TrackedOutcome::GenericSuccess; }
  JS::TrackedStrategy strategy() const { return strategy_; }
  JS::TrackedOutcome outcome() const { return outcome_; }

  bool operator==(const OptimizationAttempt& other) const {
    return strategy_ == other.strategy_ && outcome_ == other.outcome_;
  }
  bool operator!=(const OptimizationAttempt& other) const {
    return strategy_ != other.strategy_ || outcome_ != other.outcome_;
  }
  HashNumber hash() const {
    return (HashNumber(strategy_) << 8) + HashNumber(outcome_);
  }

  void writeCompact(CompactBufferWriter& writer) const;
};

typedef Vector<OptimizationAttempt, 4, JitAllocPolicy>
    TempOptimizationAttemptsVector;
typedef Vector<TypeSet::Type, 1, JitAllocPolicy> TempTypeList;

class UniqueTrackedTypes;

class OptimizationTypeInfo {
  JS::TrackedTypeSite site_;
  MIRType mirType_;
  TempTypeList types_;

 public:
  OptimizationTypeInfo(OptimizationTypeInfo&& other)
      : site_(other.site_),
        mirType_(other.mirType_),
        types_(std::move(other.types_)) {}

  OptimizationTypeInfo(TempAllocator& alloc, JS::TrackedTypeSite site,
                       MIRType mirType)
      : site_(site), mirType_(mirType), types_(alloc) {}

  MOZ_MUST_USE bool trackTypeSet(TemporaryTypeSet* typeSet);
  MOZ_MUST_USE bool trackType(TypeSet::Type type);

  JS::TrackedTypeSite site() const { return site_; }
  MIRType mirType() const { return mirType_; }
  const TempTypeList& types() const { return types_; }

  bool operator==(const OptimizationTypeInfo& other) const;
  bool operator!=(const OptimizationTypeInfo& other) const;

  HashNumber hash() const;

  MOZ_MUST_USE bool writeCompact(CompactBufferWriter& writer,
                                 UniqueTrackedTypes& uniqueTypes) const;
};

typedef Vector<OptimizationTypeInfo, 1, JitAllocPolicy>
    TempOptimizationTypeInfoVector;

// Tracks the optimization attempts made at a bytecode location.
class TrackedOptimizations : public TempObject {
  friend class UniqueTrackedOptimizations;
  TempOptimizationTypeInfoVector types_;
  TempOptimizationAttemptsVector attempts_;
  uint32_t currentAttempt_;

 public:
  explicit TrackedOptimizations(TempAllocator& alloc)
      : types_(alloc), attempts_(alloc), currentAttempt_(UINT32_MAX) {}

  void clear() {
    types_.clear();
    attempts_.clear();
    currentAttempt_ = UINT32_MAX;
  }

  MOZ_MUST_USE bool trackTypeInfo(OptimizationTypeInfo&& ty);

  MOZ_MUST_USE bool trackAttempt(JS::TrackedStrategy strategy);
  void amendAttempt(uint32_t index);
  void trackOutcome(JS::TrackedOutcome outcome);
  void trackSuccess();

  bool matchTypes(const TempOptimizationTypeInfoVector& other) const;
  bool matchAttempts(const TempOptimizationAttemptsVector& other) const;

  void spew(JitSpewChannel channel) const;
};

// Assigns each unique sequence of optimization attempts an index; outputs a
// compact table.
class UniqueTrackedOptimizations {
 public:
  struct SortEntry {
    const TempOptimizationTypeInfoVector* types;
    const TempOptimizationAttemptsVector* attempts;
    uint32_t frequency;
  };
  typedef Vector<SortEntry, 4> SortedVector;

 private:
  struct Key {
    const TempOptimizationTypeInfoVector* types;
    const TempOptimizationAttemptsVector* attempts;

    typedef Key Lookup;
    static HashNumber hash(const Lookup& lookup);
    static bool match(const Key& key, const Lookup& lookup);
    static void rekey(Key& key, const Key& newKey) { key = newKey; }
  };

  struct Entry {
    uint8_t index;
    uint32_t frequency;
  };

  // Map of unique (TempOptimizationTypeInfoVector,
  // TempOptimizationAttemptsVector) pairs to indices.
  typedef HashMap<Key, Entry, Key> AttemptsMap;
  AttemptsMap map_;

  // TempOptimizationAttemptsVectors sorted by frequency.
  SortedVector sorted_;

 public:
  explicit UniqueTrackedOptimizations(JSContext* cx) : map_(cx), sorted_(cx) {}

  MOZ_MUST_USE bool add(const TrackedOptimizations* optimizations);

  MOZ_MUST_USE bool sortByFrequency(JSContext* cx);
  bool sorted() const { return !sorted_.empty(); }
  uint32_t count() const {
    MOZ_ASSERT(sorted());
    return sorted_.length();
  }
  const SortedVector& sortedVector() const {
    MOZ_ASSERT(sorted());
    return sorted_;
  }
  uint8_t indexOf(const TrackedOptimizations* optimizations) const;
};

}  // namespace jit
}  // namespace js

#endif  // jit_OptimizationTracking_h
