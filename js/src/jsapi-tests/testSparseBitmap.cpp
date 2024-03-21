/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PodOperations.h"

#include "ds/Bitmap.h"

#include "jsapi-tests/tests.h"

using namespace js;

BEGIN_TEST(testSparseBitmapBasics) {
  SparseBitmap bitmap;

  // Test bits in first block are initially zero.
  for (size_t i = 0; i < 100; i++) {
    CHECK(!bitmap.getBit(i));
  }

  // Test bits in different blocks are initially zero.
  for (size_t i = 0; i < 100; i++) {
    CHECK(!bitmap.getBit(i * 1000));
  }

  // Set some bits in the first block and check they are set.
  for (size_t i = 0; i < 100; i += 2) {
    bitmap.setBit(i);
  }
  for (size_t i = 0; i < 100; i++) {
    CHECK(bitmap.getBit(i) == ((i % 2) == 0));
  }

  // Set some bits in different blocks and check they are set.
  for (size_t i = 0; i < 100; i += 2) {
    bitmap.setBit(i * 1000);
  }
  for (size_t i = 0; i < 100; i++) {
    CHECK(bitmap.getBit(i * 1000) == ((i % 2) == 0));
  }

  // Create another bitmap with different bits set.
  SparseBitmap other;
  for (size_t i = 1; i < 100; i += 2) {
    other.setBit(i * 1000);
  }
  for (size_t i = 0; i < 100; i++) {
    CHECK(other.getBit(i * 1000) == ((i % 2) != 0));
  }

  // OR some bits into this bitmap and check the result.
  bitmap.bitwiseOrWith(other);
  for (size_t i = 0; i < 100; i++) {
    CHECK(bitmap.getBit(i * 1000));
  }

  // AND some bits into this bitmap and check the result.
  DenseBitmap dense;
  size_t wordCount = (100 * 1000) / JS_BITS_PER_WORD + 1;
  CHECK(dense.ensureSpace(wordCount));
  other.bitwiseOrInto(dense);
  bitmap.bitwiseAndWith(dense);
  for (size_t i = 0; i < 100; i++) {
    CHECK(bitmap.getBit(i * 1000) == ((i % 2) != 0));
  }

  return true;
}
END_TEST(testSparseBitmapBasics)

BEGIN_TEST(testSparseBitmapExternalOR) {
  // Testing ORing data into an external array.

  const size_t wordCount = 10;

  // Create a bitmap with one bit set per word so we can tell them apart.
  SparseBitmap bitmap;
  for (size_t i = 0; i < wordCount; i++) {
    bitmap.setBit(i * JS_BITS_PER_WORD + i);
  }

  // Copy a single word.
  uintptr_t target[wordCount];
  mozilla::PodArrayZero(target);
  bitmap.bitwiseOrRangeInto(0, 1, target);
  CHECK(target[0] == 1u << 0);
  CHECK(target[1] == 0);

  // Copy a word at an offset.
  mozilla::PodArrayZero(target);
  bitmap.bitwiseOrRangeInto(1, 1, target);
  CHECK(target[0] == 1u << 1);
  CHECK(target[1] == 0);

  // Check data is ORed with original target contents.
  mozilla::PodArrayZero(target);
  bitmap.bitwiseOrRangeInto(0, 1, target);
  bitmap.bitwiseOrRangeInto(1, 1, target);
  CHECK(target[0] == ((1u << 0) | (1u << 1)));

  // Copy multiple words at an offset.
  mozilla::PodArrayZero(target);
  bitmap.bitwiseOrRangeInto(2, wordCount - 2, target);
  for (size_t i = 0; i < wordCount - 2; i++) {
    CHECK(target[i] == (1u << (i + 2)));
  }
  CHECK(target[wordCount - 1] == 0);

  return true;
}
END_TEST(testSparseBitmapExternalOR)

BEGIN_TEST(testSparseBitmapExternalAND) {
  // Testing ANDing data from an external array.

  const size_t wordCount = 4;

  // Create a bitmap with two bits set per word based on the word index.
  SparseBitmap bitmap;
  for (size_t i = 0; i < wordCount; i++) {
    bitmap.setBit(i * JS_BITS_PER_WORD + i);
    bitmap.setBit(i * JS_BITS_PER_WORD + i + 1);
  }

  // Update a single word, clearing one of the bits.
  uintptr_t source[wordCount];
  CHECK(bitmap.getBit(0));
  CHECK(bitmap.getBit(1));
  source[0] = ~(1 << 1);
  bitmap.bitwiseAndRangeWith(0, 1, source);
  CHECK(bitmap.getBit(0));
  CHECK(!bitmap.getBit(1));

  // Update a word at an offset.
  CHECK(bitmap.getBit(JS_BITS_PER_WORD + 1));
  CHECK(bitmap.getBit(JS_BITS_PER_WORD + 2));
  source[0] = ~(1 << 2);
  bitmap.bitwiseAndRangeWith(1, 1, source);
  CHECK(bitmap.getBit(JS_BITS_PER_WORD + 1));
  CHECK(!bitmap.getBit(JS_BITS_PER_WORD + 2));

  // Update multiple words at an offset.
  CHECK(bitmap.getBit(JS_BITS_PER_WORD * 2 + 2));
  CHECK(bitmap.getBit(JS_BITS_PER_WORD * 2 + 3));
  CHECK(bitmap.getBit(JS_BITS_PER_WORD * 3 + 3));
  CHECK(bitmap.getBit(JS_BITS_PER_WORD * 3 + 4));
  source[0] = ~(1 << 3);
  source[1] = ~(1 << 4);
  bitmap.bitwiseAndRangeWith(2, 2, source);
  CHECK(bitmap.getBit(JS_BITS_PER_WORD * 2 + 2));
  CHECK(!bitmap.getBit(JS_BITS_PER_WORD * 2 + 3));
  CHECK(bitmap.getBit(JS_BITS_PER_WORD * 3 + 3));
  CHECK(!bitmap.getBit(JS_BITS_PER_WORD * 3 + 4));

  return true;
}
END_TEST(testSparseBitmapExternalAND)
