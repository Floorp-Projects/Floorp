/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/RegisterSets.h"

#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::jit;

static bool CoPrime(size_t a, size_t b) {
  if (b <= 1) {
    return a == 1 || b == 1;
  }
  return CoPrime(b, a % b);
}

// This macros are use to iterave over all registers in a large number of
// non-looping sequences, which does not rely on the getFirst / getLast
// functions.
#define BEGIN_INDEX_WALK(RegTotal)                      \
  static const size_t Total = RegTotal;                 \
  for (size_t walk = 1; walk < RegTotal; walk += 2) {   \
    if (!CoPrime(RegTotal, walk)) continue;             \
    for (size_t start = 0; start < RegTotal; start++) { \
      size_t index = start;

#define END_INDEX_WALK \
  }                    \
  }

#define BEGIN_All_WALK(RegTotal)        \
  static const size_t Total = RegTotal; \
  size_t walk = 1;                      \
  size_t start = 0;                     \
  size_t index = start;

#define FOR_ALL_REGISTERS(Register, reg) \
  do {                                   \
    Register reg = Register::FromCode(index);

#define END_FOR_ALL_REGISTERS     \
  index = (index + walk) % Total; \
  }                               \
  while (index != start)

BEGIN_TEST(testJitRegisterSet_GPR) {
  BEGIN_INDEX_WALK(Registers::Total)

  LiveGeneralRegisterSet liveRegs;
  AllocatableGeneralRegisterSet pool(GeneralRegisterSet::All());
  CHECK(liveRegs.empty());
  CHECK(pool.set() == GeneralRegisterSet::All());

  FOR_ALL_REGISTERS(Register, reg) {
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
    if (pool.has(reg)) {
      CHECK(!liveRegs.has(reg));
      pool.take(reg);
      liveRegs.add(reg);
      CHECK(liveRegs.has(reg));
      CHECK(!pool.has(reg));
    }
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
  }
  END_FOR_ALL_REGISTERS;

  CHECK(pool.empty());

  FOR_ALL_REGISTERS(Register, reg) {
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
    if (liveRegs.has(reg)) {
      CHECK(!pool.has(reg));
      liveRegs.take(reg);
      pool.add(reg);
      CHECK(pool.has(reg));
      CHECK(!liveRegs.has(reg));
    }
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
  }
  END_FOR_ALL_REGISTERS;

  CHECK(liveRegs.empty());
  CHECK(pool.set() == GeneralRegisterSet::All());

  END_INDEX_WALK
  return true;
}
END_TEST(testJitRegisterSet_GPR)

BEGIN_TEST(testJitRegisterSet_FPU) {
  BEGIN_INDEX_WALK(FloatRegisters::Total)

  LiveFloatRegisterSet liveRegs;
  AllocatableFloatRegisterSet pool(FloatRegisterSet::All());
  CHECK(liveRegs.empty());
  CHECK(pool.set() == FloatRegisterSet::All());

  FOR_ALL_REGISTERS(FloatRegister, reg) {
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
    if (pool.has(reg)) {
      CHECK(!liveRegs.has(reg));
      pool.take(reg);
      liveRegs.add(reg);
      CHECK(liveRegs.has(reg));
      CHECK(!pool.has(reg));
    }
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
  }
  END_FOR_ALL_REGISTERS;

  CHECK(pool.empty());

  FOR_ALL_REGISTERS(FloatRegister, reg) {
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
    if (liveRegs.has(reg)) {
      CHECK(!pool.has(reg));
      liveRegs.take(reg);
      pool.add(reg);
      CHECK(pool.has(reg));
      CHECK(!liveRegs.has(reg));
    }
    CHECK(!pool.has(reg) || !liveRegs.has(reg));
  }
  END_FOR_ALL_REGISTERS;

  CHECK(liveRegs.empty());
  CHECK(pool.set() == FloatRegisterSet::All());

  END_INDEX_WALK
  return true;
}
END_TEST(testJitRegisterSet_FPU)

void pullAllFpus(AllocatableFloatRegisterSet& set, uint32_t& max_bits,
                 uint32_t bits) {
  FloatRegisterSet allocSet(set.bits());
  FloatRegisterSet available_f32(
      allocSet.allAllocatable<RegTypeName::Float32>());
  FloatRegisterSet available_f64(
      allocSet.allAllocatable<RegTypeName::Float64>());
  FloatRegisterSet available_v128(
      allocSet.allAllocatable<RegTypeName::Vector128>());
  for (FloatRegisterIterator it(available_f32); it.more(); ++it) {
    FloatRegister tmp = *it;
    set.take(tmp);
    pullAllFpus(set, max_bits, bits + 32);
    set.add(tmp);
  }
  for (FloatRegisterIterator it(available_f64); it.more(); ++it) {
    FloatRegister tmp = *it;
    set.take(tmp);
    pullAllFpus(set, max_bits, bits + 64);
    set.add(tmp);
  }
  for (FloatRegisterIterator it(available_v128); it.more(); ++it) {
    FloatRegister tmp = *it;
    set.take(tmp);
    pullAllFpus(set, max_bits, bits + 128);
    set.add(tmp);
  }
  if (bits >= max_bits) {
    max_bits = bits;
  }
}

BEGIN_TEST(testJitRegisterSet_FPU_Aliases) {
  BEGIN_All_WALK(FloatRegisters::Total);
  FOR_ALL_REGISTERS(FloatRegister, reg) {
    AllocatableFloatRegisterSet pool;
    pool.add(reg);

    uint32_t alias_bits = 0;
    for (uint32_t i = 0; i < reg.numAlignedAliased(); i++) {
      FloatRegister alias = reg.alignedAliased(i);

      if (alias.isSingle()) {
        if (alias_bits <= 32) {
          alias_bits = 32;
        }
      } else if (alias.isDouble()) {
        if (alias_bits <= 64) {
          alias_bits = 64;
        }
      } else if (alias.isSimd128()) {
        if (alias_bits <= 128) {
          alias_bits = 128;
        }
      }
    }

    uint32_t max_bits = 0;
    pullAllFpus(pool, max_bits, 0);

    // By adding one register, we expect that we should not be able to pull
    // more than any of its aligned aliases.  This rule should hold for both
    // x64 and ARM.
    CHECK(max_bits <= alias_bits);

    // We added one register, we expect to be able to pull it back.
    CHECK(max_bits > 0);
  }
  END_FOR_ALL_REGISTERS;

  return true;
}
END_TEST(testJitRegisterSet_FPU_Aliases)
