/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/OptimizationTracking.h"

#include "ds/Sort.h"
#include "jit/IonBuilder.h"
#include "jit/JitcodeMap.h"
#include "jit/JitSpewer.h"
#include "js/TrackedOptimizationInfo.h"
#include "util/Text.h"

#include "vm/ObjectGroup-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

using JS::TrackedOutcome;
using JS::TrackedStrategy;
using JS::TrackedTypeSite;

bool TrackedOptimizations::trackTypeInfo(OptimizationTypeInfo&& ty) {
  return types_.append(std::move(ty));
}

bool TrackedOptimizations::trackAttempt(TrackedStrategy strategy) {
  OptimizationAttempt attempt(strategy, TrackedOutcome::GenericFailure);
  currentAttempt_ = attempts_.length();
  return attempts_.append(attempt);
}

void TrackedOptimizations::amendAttempt(uint32_t index) {
  currentAttempt_ = index;
}

void TrackedOptimizations::trackOutcome(TrackedOutcome outcome) {
  attempts_[currentAttempt_].setOutcome(outcome);
}

void TrackedOptimizations::trackSuccess() {
  attempts_[currentAttempt_].setOutcome(TrackedOutcome::GenericSuccess);
}

template <class Vec>
static bool VectorContentsMatch(const Vec* xs, const Vec* ys) {
  if (xs->length() != ys->length()) {
    return false;
  }
  for (auto x = xs->begin(), y = ys->begin(); x != xs->end(); x++, y++) {
    MOZ_ASSERT(y != ys->end());
    if (*x != *y) {
      return false;
    }
  }
  return true;
}

bool TrackedOptimizations::matchTypes(
    const TempOptimizationTypeInfoVector& other) const {
  return VectorContentsMatch(&types_, &other);
}

bool TrackedOptimizations::matchAttempts(
    const TempOptimizationAttemptsVector& other) const {
  return VectorContentsMatch(&attempts_, &other);
}

JS_PUBLIC_API const char* JS::TrackedStrategyString(TrackedStrategy strategy) {
  switch (strategy) {
#define STRATEGY_CASE(name)   \
  case TrackedStrategy::name: \
    return #name;
    TRACKED_STRATEGY_LIST(STRATEGY_CASE)
#undef STRATEGY_CASE

    default:
      MOZ_CRASH("bad strategy");
  }
}

JS_PUBLIC_API const char* JS::TrackedOutcomeString(TrackedOutcome outcome) {
  switch (outcome) {
#define OUTCOME_CASE(name)   \
  case TrackedOutcome::name: \
    return #name;
    TRACKED_OUTCOME_LIST(OUTCOME_CASE)
#undef OUTCOME_CASE

    default:
      MOZ_CRASH("bad outcome");
  }
}

JS_PUBLIC_API const char* JS::TrackedTypeSiteString(TrackedTypeSite site) {
  switch (site) {
#define TYPESITE_CASE(name)   \
  case TrackedTypeSite::name: \
    return #name;
    TRACKED_TYPESITE_LIST(TYPESITE_CASE)
#undef TYPESITE_CASE

    default:
      MOZ_CRASH("bad type site");
  }
}

void SpewTempOptimizationTypeInfoVector(
    JitSpewChannel channel, const TempOptimizationTypeInfoVector* types,
    const char* indent = nullptr) {
#ifdef JS_JITSPEW
  for (const OptimizationTypeInfo* t = types->begin(); t != types->end(); t++) {
    JitSpewStart(channel, "   %s%s of type %s, type set", indent ? indent : "",
                 TrackedTypeSiteString(t->site()),
                 StringFromMIRType(t->mirType()));
    for (uint32_t i = 0; i < t->types().length(); i++) {
      JitSpewCont(channel, " %s", TypeSet::TypeString(t->types()[i]).get());
    }
    JitSpewFin(channel);
  }
#endif
}

void SpewTempOptimizationAttemptsVector(
    JitSpewChannel channel, const TempOptimizationAttemptsVector* attempts,
    const char* indent = nullptr) {
#ifdef JS_JITSPEW
  for (const OptimizationAttempt* a = attempts->begin(); a != attempts->end();
       a++) {
    JitSpew(channel, "   %s%s: %s", indent ? indent : "",
            TrackedStrategyString(a->strategy()),
            TrackedOutcomeString(a->outcome()));
  }
#endif
}

void TrackedOptimizations::spew(JitSpewChannel channel) const {
#ifdef JS_JITSPEW
  SpewTempOptimizationTypeInfoVector(channel, &types_);
  SpewTempOptimizationAttemptsVector(channel, &attempts_);
#endif
}

bool OptimizationTypeInfo::trackTypeSet(TemporaryTypeSet* typeSet) {
  if (!typeSet) {
    return true;
  }
  return typeSet->enumerateTypes(&types_);
}

bool OptimizationTypeInfo::trackType(TypeSet::Type type) {
  return types_.append(type);
}

bool OptimizationTypeInfo::operator==(const OptimizationTypeInfo& other) const {
  return site_ == other.site_ && mirType_ == other.mirType_ &&
         VectorContentsMatch(&types_, &other.types_);
}

bool OptimizationTypeInfo::operator!=(const OptimizationTypeInfo& other) const {
  return !(*this == other);
}

static inline HashNumber CombineHash(HashNumber h, HashNumber n) {
  h += n;
  h += (h << 10);
  h ^= (h >> 6);
  return h;
}

static inline HashNumber HashType(TypeSet::Type ty) {
  if (ty.isObjectUnchecked()) {
    return PointerHasher<TypeSet::ObjectKey*>::hash(ty.objectKey());
  }
  return mozilla::HashGeneric(ty.raw());
}

static HashNumber HashTypeList(const TempTypeList& types) {
  HashNumber h = 0;
  for (uint32_t i = 0; i < types.length(); i++) {
    h = CombineHash(h, HashType(types[i]));
  }
  return h;
}

HashNumber OptimizationTypeInfo::hash() const {
  return ((HashNumber(site_) << 24) + (HashNumber(mirType_) << 16)) ^
         HashTypeList(types_);
}

template <class Vec>
static HashNumber HashVectorContents(const Vec* xs, HashNumber h) {
  for (auto x = xs->begin(); x != xs->end(); x++) {
    h = CombineHash(h, x->hash());
  }
  return h;
}

/* static */
HashNumber UniqueTrackedOptimizations::Key::hash(const Lookup& lookup) {
  HashNumber h = HashVectorContents(lookup.types, 0);
  h = HashVectorContents(lookup.attempts, h);
  h += (h << 3);
  h ^= (h >> 11);
  h += (h << 15);
  return h;
}

/* static */
bool UniqueTrackedOptimizations::Key::match(const Key& key,
                                            const Lookup& lookup) {
  return VectorContentsMatch(key.attempts, lookup.attempts) &&
         VectorContentsMatch(key.types, lookup.types);
}

bool UniqueTrackedOptimizations::add(
    const TrackedOptimizations* optimizations) {
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

struct FrequencyComparator {
  bool operator()(const UniqueTrackedOptimizations::SortEntry& a,
                  const UniqueTrackedOptimizations::SortEntry& b,
                  bool* lessOrEqualp) {
    *lessOrEqualp = b.frequency <= a.frequency;
    return true;
  }
};

bool UniqueTrackedOptimizations::sortByFrequency(JSContext* cx) {
  MOZ_ASSERT(!sorted());

  JitSpew(JitSpew_OptimizationTrackingExtended,
          "=> Sorting unique optimizations by frequency");

  // Sort by frequency.
  Vector<SortEntry> entries(cx);
  for (AttemptsMap::Range r = map_.all(); !r.empty(); r.popFront()) {
    SortEntry entry;
    entry.types = r.front().key().types;
    entry.attempts = r.front().key().attempts;
    entry.frequency = r.front().value().frequency;
    if (!entries.append(entry)) {
      return false;
    }
  }

  // The compact table stores indices as a max of uint8_t. In practice each
  // script has fewer unique optimization attempts than UINT8_MAX.
  if (entries.length() >= UINT8_MAX - 1) {
    return false;
  }

  Vector<SortEntry> scratch(cx);
  if (!scratch.resize(entries.length())) {
    return false;
  }

  FrequencyComparator comparator;
  MOZ_ALWAYS_TRUE(MergeSort(entries.begin(), entries.length(), scratch.begin(),
                            comparator));

  // Update map entries' indices.
  for (size_t i = 0; i < entries.length(); i++) {
    Key key;
    key.types = entries[i].types;
    key.attempts = entries[i].attempts;
    AttemptsMap::Ptr p = map_.lookup(key);
    MOZ_ASSERT(p);
    p->value().index = sorted_.length();

    JitSpew(JitSpew_OptimizationTrackingExtended,
            "   Entry %zu has frequency %" PRIu32, sorted_.length(),
            p->value().frequency);

    if (!sorted_.append(entries[i])) {
      return false;
    }
  }

  return true;
}

uint8_t UniqueTrackedOptimizations::indexOf(
    const TrackedOptimizations* optimizations) const {
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
class jit::UniqueTrackedTypes {
 public:
  struct TypeHasher {
    typedef TypeSet::Type Lookup;

    static HashNumber hash(const Lookup& ty) { return HashType(ty); }
    static bool match(const TypeSet::Type& ty1, const TypeSet::Type& ty2) {
      return ty1 == ty2;
    }
  };

 private:
  // Map of unique TypeSet::Types to indices.
  typedef HashMap<TypeSet::Type, uint8_t, TypeHasher> TypesMap;
  TypesMap map_;

  Vector<TypeSet::Type, 1> list_;

 public:
  explicit UniqueTrackedTypes(JSContext* cx) : map_(cx), list_(cx) {}

  bool getIndexOf(TypeSet::Type ty, uint8_t* indexp);

  uint32_t count() const {
    MOZ_ASSERT(map_.count() == list_.length());
    return list_.length();
  }
  bool enumerate(TypeSet::TypeList* types) const;
};

bool UniqueTrackedTypes::getIndexOf(TypeSet::Type ty, uint8_t* indexp) {
  TypesMap::AddPtr p = map_.lookupForAdd(ty);
  if (p) {
    *indexp = p->value();
    return true;
  }

  // Store indices as max of uint8_t. In practice each script has fewer than
  // UINT8_MAX of unique observed types.
  if (count() >= UINT8_MAX) {
    return false;
  }

  uint8_t index = (uint8_t)count();
  if (!map_.add(p, ty, index)) {
    return false;
  }
  if (!list_.append(ty)) {
    return false;
  }
  *indexp = index;
  return true;
}

bool UniqueTrackedTypes::enumerate(TypeSet::TypeList* types) const {
  return types->append(list_.begin(), list_.end());
}

BytecodeSite* IonBuilder::maybeTrackedOptimizationSite(jsbytecode* pc) {
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
  MOZ_ASSERT(mirGen_.isOptimizationTrackingEnabled());
  for (size_t i = trackedOptimizationSites_.length(); i != 0; i--) {
    BytecodeSite* site = trackedOptimizationSites_[i - 1];
    if (site->pc() == pc) {
      MOZ_ASSERT(site->tree() == info().inlineScriptTree());
      return site;
    }
  }
  return nullptr;
}

void IonBuilder::startTrackingOptimizations() {
  if (mirGen_.isOptimizationTrackingEnabled()) {
    BytecodeSite* site =
        maybeTrackedOptimizationSite(current->trackedSite()->pc());

    if (!site) {
      site = current->trackedSite();
      site->setOptimizations(new (alloc()) TrackedOptimizations(alloc()));
      // OOMs are handled as if optimization tracking were turned off.
      if (!trackedOptimizationSites_.append(site)) {
        site = nullptr;
      }
    } else if (site->hasOptimizations()) {
      // The same bytecode may be visited multiple times (see
      // restartLoop). Only the last time matters, so clear any previous
      // tracked optimizations.
      site->optimizations()->clear();
    }

    // The case of !site->hasOptimizations() means we had an OOM when
    // previously attempting to track optimizations. Leave
    // site->optimizations_ nullptr to leave optimization tracking off.

    if (site) {
      current->updateTrackedSite(site);
    }
  }
}

void IonBuilder::trackTypeInfoUnchecked(TrackedTypeSite kind, MIRType mirType,
                                        TemporaryTypeSet* typeSet) {
  BytecodeSite* site = current->trackedSite();
  // OOMs are handled as if optimization tracking were turned off.
  OptimizationTypeInfo typeInfo(alloc(), kind, mirType);
  if (!typeInfo.trackTypeSet(typeSet)) {
    site->setOptimizations(nullptr);
    return;
  }
  if (!site->optimizations()->trackTypeInfo(std::move(typeInfo))) {
    site->setOptimizations(nullptr);
  }
}

void IonBuilder::trackTypeInfoUnchecked(TrackedTypeSite kind, JSObject* obj) {
  BytecodeSite* site = current->trackedSite();
  // OOMs are handled as if optimization tracking were turned off.
  OptimizationTypeInfo typeInfo(alloc(), kind, MIRType::Object);
  if (!typeInfo.trackType(TypeSet::ObjectType(obj))) {
    return;
  }
  if (!site->optimizations()->trackTypeInfo(std::move(typeInfo))) {
    site->setOptimizations(nullptr);
  }
}

void IonBuilder::trackTypeInfoUnchecked(CallInfo& callInfo) {
  MDefinition* thisArg = callInfo.thisArg();
  trackTypeInfoUnchecked(TrackedTypeSite::Call_This, thisArg->type(),
                         thisArg->resultTypeSet());

  for (uint32_t i = 0; i < callInfo.argc(); i++) {
    MDefinition* arg = callInfo.getArg(i);
    trackTypeInfoUnchecked(TrackedTypeSite::Call_Arg, arg->type(),
                           arg->resultTypeSet());
  }

  TemporaryTypeSet* returnTypes = getInlineReturnTypeSet();
  trackTypeInfoUnchecked(TrackedTypeSite::Call_Return,
                         returnTypes->getKnownMIRType(), returnTypes);
}

void IonBuilder::trackOptimizationAttemptUnchecked(TrackedStrategy strategy) {
  BytecodeSite* site = current->trackedSite();
  // OOMs are handled as if optimization tracking were turned off.
  if (!site->optimizations()->trackAttempt(strategy)) {
    site->setOptimizations(nullptr);
  }
}

void IonBuilder::amendOptimizationAttemptUnchecked(uint32_t index) {
  const BytecodeSite* site = current->trackedSite();
  site->optimizations()->amendAttempt(index);
}

void IonBuilder::trackOptimizationOutcomeUnchecked(TrackedOutcome outcome) {
  const BytecodeSite* site = current->trackedSite();
  site->optimizations()->trackOutcome(outcome);
}

void IonBuilder::trackOptimizationSuccessUnchecked() {
  const BytecodeSite* site = current->trackedSite();
  site->optimizations()->trackSuccess();
}

void IonBuilder::trackInlineSuccessUnchecked(InliningStatus status) {
  if (status == InliningStatus_Inlined) {
    trackOptimizationOutcome(TrackedOutcome::Inlined);
  }
}
