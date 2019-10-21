/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinASTTokenReaderContext_h
#define frontend_BinASTTokenReaderContext_h

#include "mozilla/Array.h"         // mozilla::Array
#include "mozilla/Assertions.h"    // MOZ_ASSERT
#include "mozilla/Attributes.h"    // MOZ_MUST_USE, MOZ_STACK_CLASS
#include "mozilla/IntegerRange.h"  // mozilla::IntegerRange
#include "mozilla/Maybe.h"         // mozilla::Maybe
#include "mozilla/Variant.h"       // mozilla::Variant

#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint32_t

#include "frontend/BinASTRuntimeSupport.h"  // CharSlice, BinASTVariant, BinASTKind, BinASTField, BinASTSourceMetadata
#include "frontend/BinASTToken.h"
#include "frontend/BinASTTokenReaderBase.h"  // BinASTTokenReaderBase, SkippableSubTree
#include "js/AllocPolicy.h"                  // SystemAllocPolicy
#include "js/HashTable.h"                    // HashMap, DefaultHasher
#include "js/Result.h"                       // JS::Result, Ok, Error
#include "js/Vector.h"                       // js::Vector

class JSAtom;
class JSTracer;
struct JSContext;

namespace js {

class ScriptSource;

namespace frontend {

class ErrorReporter;

// The format treats several distinct models as the same.
//
// We use `NormalizedInterfaceAndField` as a proxy for `BinASTInterfaceAndField`
// to ensure that we always normalize into the canonical model.
struct NormalizedInterfaceAndField {
  const BinASTInterfaceAndField identity_;
  explicit NormalizedInterfaceAndField(BinASTInterfaceAndField identity)
      : identity_(
            identity == BinASTInterfaceAndField::
                            StaticMemberAssignmentTarget__Property
                ? BinASTInterfaceAndField::StaticMemberExpression__Property
                : identity) {}
};

template <typename T>
struct Split {
  T prefix_;
  T suffix_;
};

// A bunch of bits used to lookup a value in a Huffman table. In most cases,
// these are the 32 leading bits of the underlying bit stream.
//
// In a Huffman table, keys have variable bitlength. Consequently, we only know
// the bitlength of the key *after* we have performed the lookup. A
// `HuffmanLookup` is a data structure contained at least as many bits as
// needed to perform the lookup.
//
// Whenever a lookup is performed, the consumer MUST look at the `bitLength` of
// the returned `HuffmanKey` and consume as many bits from the bit stream.
struct HuffmanLookup {
  HuffmanLookup(const uint32_t bits, const uint8_t bitLength)
      // We zero out the highest `32 - bitLength_` bits.
      : bits_(bitLength == 0
                  ? 0  // >> 32 is UB
                  : (bits & (uint32_t(0xFFFFFFFF) >> (32 - bitLength)))),
        bitLength_(bitLength) {
    MOZ_ASSERT(bitLength_ <= 32);
    MOZ_ASSERT_IF(bitLength_ != 32 /* >> 32 is UB */, bits_ >> bitLength_ == 0);
  }

  // Return the `bitLength` leading bits of this superset, in the order
  // expected to compare to a `HuffmanKey`. The order of bits and bytes
  // is ensured by `BitBuffer`.
  //
  // Note: This only makes sense if `bitLength <= bitLength_`.
  //
  // So, for instance, if `leadingBits(4)` returns
  // `0b_0000_0000__0000_0000__0000_0000__0000_0100`, this is
  // equal to Huffman Key `0100`.
  uint32_t leadingBits(const uint8_t bitLength) const;

  // Split a HuffmanLookup into a prefix and a suffix.
  //
  // If the value holds at least `prefixLength` bits, the
  // prefix consists in the first `prefixLength` bits and the
  // suffix in the remaining bits.
  //
  // If the value holds fewer bits, the prefix consists in
  // all the bits, with 0 padding at the end to ensure that
  // the prefix contains exactly `prefixLength` bits.
  Split<HuffmanLookup> split(const uint8_t prefixLength) const;

  // The buffer holding the bits. At this stage, bits are stored
  // in the same order as `HuffmanKey`. See the implementation of
  // `BitBuffer` methods for more details about how this order
  // is implemented.
  //
  // If `bitLength_ < 32`, the unused highest bits are guaranteed
  // to be 0.
  const uint32_t bits_;

  // The actual length of buffer `bits_`.
  //
  // MUST be within `[0, 32]`.
  //
  // If `bitLength_ < 32`, it means that some of the highest bits are unused.
  const uint8_t bitLength_;

  // Return an iterable data structure representing all possible
  // suffixes of this `HuffmanLookup` with `expectedBitLength`
  // bits.
  //
  // If this `HuffmanLookup` is already at least `expectedBitLength`
  // bits long, we truncate the `HuffmanLookup` to `expectedBitLength`
  // bits and there is only one such suffix.
  mozilla::detail::IntegerRange<size_t> suffixes(
      uint8_t expectedBitLength) const;
};

// A Huffman Key.
struct HuffmanKey {
  // Construct the HuffmanKey.
  //
  // `bits` and `bitLength` define a buffer containing the standard Huffman
  // code for this key.
  //
  // For instance, if the Huffman code is `0100`,
  // - `bits = 0b0000_0000__0000_0000__0000_0000__0000_0100`;
  // - `bitLength = 4`.
  HuffmanKey(const uint32_t bits, const uint8_t bitLength);

  // The buffer holding the bits.
  //
  // For a Huffman code of `0100`
  // - `bits_ = 0b0000_0000__0000_0000__0000_0000__0000_0100`;
  //
  // If `bitLength_ < 32`, the unused highest bits are guaranteed
  // to be 0.
  const uint32_t bits_;

  // The actual length of buffer `bits_`.
  //
  // MUST be within `[0, 32]`.
  //
  // If `bitLength_ < 32`, it means that some of the highest bits are unused.
  const uint8_t bitLength_;
};

// Symbol appears in the table.
// This class is used to store symbols in `*HuffmanTable` classes without having
// multiple implementation or different generated code for each type.
//
// This class doesn't store any tag to determine which kind of symbol it is.
// The consumer MUST use the correct `from*`/`to*` pair.
class alignas(8) BinASTSymbol {
 private:
  uint64_t asBits_;

  explicit BinASTSymbol(uint64_t asBits) : asBits_(asBits) {}

  static BinASTSymbol fromRawBits(uint64_t asBits) {
    return BinASTSymbol(asBits);
  }

 public:
  static BinASTSymbol fromUnsignedLong(uint32_t i) { return fromRawBits(i); }
  static BinASTSymbol fromListLength(uint32_t i) { return fromRawBits(i); }
  static BinASTSymbol fromSubtableIndex(size_t i) { return fromRawBits(i); }
  static BinASTSymbol fromBool(bool b) { return fromRawBits(b); }
  static BinASTSymbol fromDouble(double d) {
    return fromRawBits(mozilla::BitwiseCast<uint64_t>(d));
  }
  static BinASTSymbol fromKind(BinASTKind k) {
    return fromRawBits(uint64_t(k));
  }
  static BinASTSymbol fromVariant(BinASTVariant v) {
    return fromRawBits(uint64_t(v));
  }
  static BinASTSymbol fromAtom(JSAtom* v) {
    return fromRawBits(reinterpret_cast<uint64_t>(v));
  }

  uint32_t toUnsignedLong() const { return uint32_t(asBits_); }
  uint32_t toListLength() const { return uint32_t(asBits_); }
  size_t toSubtableIndex() const { return size_t(asBits_); }
  bool toBool() const { return bool(asBits_); }
  double toDouble() const { return mozilla::BitwiseCast<double>(asBits_); }
  BinASTKind toKind() const { return BinASTKind(asBits_); }
  BinASTVariant toVariant() const { return BinASTVariant(asBits_); }
  JSAtom* toAtom() const { return reinterpret_cast<JSAtom*>(asBits_); }
};

// An entry in a Huffman table.
class HuffmanEntry {
  const HuffmanKey key_;
  const BinASTSymbol value_;

 public:
  HuffmanEntry(HuffmanKey key, const BinASTSymbol& value)
      : key_(key), value_(value) {}

  HuffmanEntry(uint32_t bits, uint8_t bitLength, const BinASTSymbol& value)
      : key_(bits, bitLength), value_(value) {}

  const HuffmanKey& key() const { return key_; };
  const BinASTSymbol& value() const { return value_; };
};

// The result of lookup in Huffman table.
class HuffmanLookupResult {
  uint8_t bitLength_;
  const BinASTSymbol* value_;

  HuffmanLookupResult(uint8_t bitLength, const BinASTSymbol* value)
      : bitLength_(bitLength), value_(value) {}

 public:
  static HuffmanLookupResult found(uint8_t bitLength,
                                   const BinASTSymbol* value) {
    MOZ_ASSERT(value);
    return HuffmanLookupResult(bitLength, value);
  }

  static HuffmanLookupResult notFound() {
    return HuffmanLookupResult(0, nullptr);
  }

  bool isFound() const { return !!value_; };

  uint8_t bitLength() const {
    MOZ_ASSERT(isFound());
    return bitLength_;
  }

  const BinASTSymbol& value() const {
    MOZ_ASSERT(isFound());
    return *value_;
  }
};

// The default inline buffer length for instances of HuffmanTableValue.
// Specific type (e.g. booleans) will override this to provide something
// more suited to their type.
const size_t HUFFMAN_TABLE_DEFAULT_INLINE_BUFFER_LENGTH = 8;

// A flag that determines only whether a value is `null`.
// Used for optional interface.
enum class Nullable {
  Null,
  NonNull,
};

// An implementation of Huffman Tables as a vector, with `O(entries)`
// lookup. Performance-wise, this implementation only makes sense for
// very short tables.
template <int N = HUFFMAN_TABLE_DEFAULT_INLINE_BUFFER_LENGTH>
class NaiveHuffmanTable {
 public:
  explicit NaiveHuffmanTable(JSContext* cx) : values_(cx) {}
  NaiveHuffmanTable(NaiveHuffmanTable&& other) noexcept
      : values_(std::move(other.values_)) {}

  // Initialize a Huffman table containing a single value.
  JS::Result<Ok> initWithSingleValue(JSContext* cx, const BinASTSymbol& value);

  // Initialize a Huffman table containing `numberOfSymbols`.
  // Symbols must be added with `addSymbol`.
  // If you initialize with `initStart`, you MUST call `initComplete()`
  // at the end of initialization.
  JS::Result<Ok> initStart(JSContext* cx, size_t numberOfSymbols,
                           uint8_t maxBitLength);

  JS::Result<Ok> initComplete();

  // Add a symbol to a value.
  JS::Result<Ok> addSymbol(uint32_t bits, uint8_t bitLength,
                           const BinASTSymbol& value);

  NaiveHuffmanTable() = delete;
  NaiveHuffmanTable(NaiveHuffmanTable&) = delete;

  // Lookup a value in the table.
  //
  // The return of this method contains:
  //
  // - the resulting value (`nullptr` if the value is not in the table);
  // - the number of bits in the entry associated to this value.
  //
  // Note that entries inside a single table are typically associated to
  // distinct bit lengths. The caller is responsible for checking
  // the result of this method and advancing the bitstream by
  // `result.key().bitLength_` bits.
  HuffmanLookupResult lookup(HuffmanLookup key) const;

  // The number of values in the table.
  size_t length() const { return values_.length(); }
  const HuffmanEntry* begin() const { return values_.begin(); }
  const HuffmanEntry* end() const { return values_.end(); }

 private:
  // The entries in this Huffman table.
  // Entries are always ranked by increasing bit_length, and within
  // a bitlength by increasing value of `bits`. This representation
  // is good for small tables, but in the future, we may adopt a
  // representation more optimized for larger tables.
  Vector<HuffmanEntry, N> values_;
  friend class HuffmanPreludeReader;
};

// An implementation of Huffman Tables for single-entry table.
class SingleEntryHuffmanTable {
 public:
  explicit SingleEntryHuffmanTable(const BinASTSymbol& value) : value_(value) {}
  SingleEntryHuffmanTable(SingleEntryHuffmanTable&& other) = default;

  SingleEntryHuffmanTable() = delete;
  SingleEntryHuffmanTable(SingleEntryHuffmanTable&) = delete;

  // Lookup a value in the table.
  // The key is 0-bit length and this always suceeds.
  HuffmanLookupResult lookup(HuffmanLookup key) const;

  // The number of values in the table.
  size_t length() const { return 1; }

  // Iterating in the order of insertion.
  struct Iterator {
    explicit Iterator(const BinASTSymbol* position);
    void operator++();
    const BinASTSymbol* operator*() const;
    const BinASTSymbol* operator->() const;
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

   private:
    const BinASTSymbol* position_;
  };
  Iterator begin() const { return Iterator(&value_); }
  Iterator end() const { return Iterator(nullptr); }

 private:
  BinASTSymbol value_;

  friend class HuffmanPreludeReader;
};

// An implementation of Huffman Tables as a vector designed to allow
// constant-time lookups at the expense of high space complexity.
//
// # Time complexity
//
// Lookups take constant time, which essentially consists in two
// simple vector lookups.
//
// # Space complexity
//
// After initialization, a `SingleLookupHuffmanTable`
// requires O(2 ^ max bit length in the table) space:
//
// - A vector `values_` containing one entry per symbol.
// - A vector `saturated_` containing exactly 2 ^ (max bit length in the
//   table) entries, which we use to map any combination of `largestBitLength_`
//   bits onto the only `HuffmanEntry` that may be reached by a prefix
//   of these `largestBitLength_` bits. See below for more details.
//
// # Algorithm
//
// Consider the following Huffman table
//
// Symbol | Binary Code  | Int value of Code | Bit Length
// ------ | ------------ | ----------------- | ----------
// A      | 11000        | 24                | 5
// B      | 11001        | 25                | 5
// C      | 1101         | 13                | 4
// D      | 100          | 4                 | 3
// E      | 101          | 5                 | 3
// F      | 111          | 7                 | 3
// G      | 00           | 0                 | 2
// H      | 01           | 1                 | 2
//
// By definition of a Huffman Table, the Binary Codes represent
// paths in a Huffman Tree. Consequently, padding these codes
// to the end would not change the result.
//
// Symbol | Binary Code  | Int value of Code | Bit Length
// ------ | ------------ | ----------------- | ----------
// A      | 11000        | 24                | 5
// B      | 11001        | 25                | 5
// C      | 1101?        | [26...27]         | 4
// D      | 100??        | [16...19]         | 3
// E      | 101??        | [20..23]          | 3
// F      | 111??        | [28..31]          | 3
// G      | 00???        | [0...7]           | 2
// H      | 01???        | [8...15]          | 2
//
// Row "Int value of Code" now contains all possible values
// that may be expressed in 5 bits. By using these values
// as array indices, we may therefore represent the
// Huffman table as an array:
//
// Index     |   Symbol   |   Bit Length
// --------- | ---------- | -------------
// [0...7]   |  G         | 2
// [8...15]  |  H         | 2
// [16...19] |  D         | 3
// [20...23] |  E         | 3
// 24        |  A         | 5
// 25        |  B         | 5
// [26...27] |  C         | 4
// [28...31] |  F         | 3
//
// By using the next 5 bits in the bit buffer, we may, in
// a single lookup, determine the symbol and the bit length.
//
// In the current implementation, to save some space, we have
// two distinct arrays, one (`values_`) with a single instance of each
// symbols bit length, and one (`saturated_`) with indices into that
// array.
class SingleLookupHuffmanTable {
 public:
  // An index into table `values_`.
  // We use `uint8_t` instead of `size_t` to limit the space
  // used by the table.
  using InternalIndex = uint8_t;

  // An enum used to represent how this table is used.
  // Used to perform additional DEBUG assertions.
  enum Use {
    // Used as a `Subtable` argument of a `MultiLookupHuffmanTable`.
    LeafOfMultiLookupHuffmanTable,
    // Used as its own table.
    ToplevelTable,
    // Used as a `shortKeys_` in a `MultiLookupHuffmanTable`.
    ShortKeys,
  };

  // The largest bit length that may be represented by this table.
  static const uint8_t MAX_BIT_LENGTH = sizeof(InternalIndex) * 8;

  explicit SingleLookupHuffmanTable(
      JSContext* cx, Use use = Use::LeafOfMultiLookupHuffmanTable)
      : values_(cx),
        saturated_(cx),
        largestBitLength_(-1)
#ifdef DEBUG
        ,
        use_(use)
#endif  // DEBUG
  {
  }
  SingleLookupHuffmanTable(SingleLookupHuffmanTable&& other) = default;

  // Initialize a Huffman table containing `numberOfSymbols`.
  // Symbols must be added with `addSymbol`.
  // If you initialize with `initStart`, you MUST call `initComplete()`
  // at the end of initialization.
  JS::Result<Ok> initStart(JSContext* cx, size_t numberOfSymbols,
                           uint8_t maxBitLength);

  JS::Result<Ok> initComplete();

  // Add a `(bit, bitLength) => value` mapping.
  JS::Result<Ok> addSymbol(uint32_t bits, uint8_t bitLength,
                           const BinASTSymbol& value);

  SingleLookupHuffmanTable() = delete;
  SingleLookupHuffmanTable(SingleLookupHuffmanTable&) = delete;

  // Lookup a value in the table.
  //
  // The return of this method contains:
  //
  // - the resulting value (`nullptr` if the value is not in the table);
  // - the number of bits in the entry associated to this value.
  //
  // Note that entries inside a single table are typically associated to
  // distinct bit lengths. The caller is responsible for checking
  // the result of this method and advancing the bitstream by
  // `result.key().bitLength_` bits.
  HuffmanLookupResult lookup(HuffmanLookup key) const;

  // The number of values in the table.
  size_t length() const { return values_.length(); }

  // Iterating in the order of insertion.
  struct Iterator {
    explicit Iterator(const HuffmanEntry* position);
    void operator++();
    const BinASTSymbol* operator*() const;
    const BinASTSymbol* operator->() const;
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

   private:
    const HuffmanEntry* position_;
  };
  Iterator begin() const { return Iterator(values_.begin()); }
  Iterator end() const { return Iterator(values_.end()); }

 private:
  // The entries in this Huffman Table, sorted in the order of insertion.
  //
  // Invariant (once `init*` has been called):
  // - Length is the number of values inserted in the table.
  // - for all i, `values_[i].bitLength_ <= largestBitLength_`.
  Vector<HuffmanEntry> values_;

  // The entries in this Huffman table, prepared for lookup.
  //
  // Invariant (once `init*` has been called):
  // - Length is `1 << largestBitLength_`.
  // - for all i, `saturated_[i] < values_.length()`
  Vector<InternalIndex> saturated_;

  // The maximal bitlength of a value in this table.
  //
  // Invariant (once `init*` has been called):
  // - `largestBitLength_ <= MAX_CODE_BIT_LENGTH`
  uint8_t largestBitLength_;

#ifdef DEBUG
  Use use_;
#endif  // DEBUG

  friend class HuffmanPreludeReader;
};

/// A table designed to support fast lookup in large sets of data.
/// In most cases, lookup will be slower than a `SingleLookupHuffmanTable`
/// but, particularly in heavily unbalanced trees, the table will
/// take ~2^prefix_len fewer internal entries than a `SingleLookupHuffmanTable`.
///
/// Typically, use this table whenever codes range between 10 and 20 bits.
///
/// # Time complexity
///
/// A lookup in `MultiLookupHuffmanTable` will also take constant time:

/// - a constant-time lookup in a `SingleLookupHuffmanTable`, in case we only
///   need to look for a small key;
/// - if the above lookup fails:
///   - a constant-time lookup to determine into which suffix table to perform
///     the lookup;
///   - a constant-time lookup into the suffix table;
/// - a constant-time lookup into the array of values.
///
///
/// # Space complexity
///
/// TBD. Highly dependent on the shape of the Huffman Tree.
///
///
/// # Algorithm
///
/// Consider the following Huffman table
///
/// Symbol | Binary Code  | Bit Length
/// ------ | ------------ | ----------
/// A      | 11000        | 5
/// B      | 11001        | 5
/// C      | 1101         | 4
/// D      | 100          | 3
/// E      | 101          | 3
/// F      | 111          | 3
/// G      | 00           | 2
/// H      | 01           | 2
///
/// Let us assume that we have somehow determined that:
///
/// - we wish to store all values with a bit length of 2
///   or less in a fast access table.
/// - we wish to use a prefix length of 4.
///
/// Note: These numbers of 2 and 4 are picked arbitrarily
/// for the example. Actual numbers used may vary.
///
/// We first extract all values with a bit length of <= 2:
///
/// Symbol | Binary Code  | Bit Length
/// ------ | ------------ | ----------
/// G      | 00           | 2
/// H      | 01           | 2
///
/// We store these values in a `SingleLookupHuffmanTable` for fast access.
/// We are now done with these values. Let's deal with the remaining values.
///
/// Now, as our prefix length is 4, we precompute all possible 3-bit
/// prefixes and split the table across such prefixes.
///
/// Prefix  | Int Value of Prefix | Symbols   | Max bit length
/// ------- | ------------------- | --------- | --------------
/// 0000    | 0                   |           | 0
/// 0001    | 1                   |           | 0
/// 0010    | 2                   |           | 0
/// 0011    | 3                   |           | 0
/// 0100    | 4                   |           | 0
/// 0101    | 5                   |           | 0
/// 0110    | 6                   |           | 0
/// 0111    | 7                   |           | 0
/// 1000    | 8                   | D         | 0
/// 1001    | 9                   | D         | 0
/// 1010    | 10                  | E         | 0
/// 1011    | 11                  | E         | 0
/// 1100    | 12                  | A, B      | 1
/// 1101    | 13                  | C         | 0
/// 1110    | 14                  | F         | 0
/// 1111    | 15                  | F         | 0
///
/// For each prefix, we build the table containing the Symbols,
/// stripping prefix from the Binary Code.
/// - Prefixes 0000-01111
///
/// Empty tables.
///
/// - Prefixes 1000, 1001
///
/// Symbol | Binary Code | Bit Length | Total Bit Length
/// ------ | ----------- | ---------- | --------------
/// D      | (none)      | 0          | 3
///
/// - Prefixes 1010, 1011
///
/// Symbol | Binary Code | Bit Length | Total Bit Length
/// ------ | ----------- | ---------- | --------------
/// E      | (none)      | 0          | 3
///
/// - Prefix 1100
///
/// Symbol | Binary Code | Bit Length | Total Bit Length
/// ------ | ----------- | ---------- | ----------------
/// A      | 0           | 1          | 5
/// B      | 1           | 1          | 5
///
/// - Prefix 1101
///
/// Symbol | Binary Code | Bit Length | Total Bit Length
/// ------ | ----------- | ---------- | ----------------
/// C      | (none)      | 0          | 4
///
/// - Prefixes 1110, 1111
///
/// Symbol | Binary Code | Bit Length | Total Bit Length
/// ------ | ----------- | ---------- | ----------------
/// F      | (none)      | 0          | 4
///
///
/// With this transformation, we have represented one table
/// with an initial max bit length of 5 as:
///
/// - 1 SingleLookupValue table with a max bit length of 3;
/// - 8 empty tables;
/// - 7 tables with a max bit length of 0;
/// - 1 table with a max bit length of 1;
///
/// Consequently, instead of storing 2^5 = 32 internal references,
/// as we would have done with a SingleLookupHuffmanTable, we only
/// need to store:
///
/// - 1 subtable with 2^3 = 8 references;
/// - 7 subtables with 1 reference each;
/// - 1 subtable with 2^1 = 2 references.
template <typename Subtable, uint8_t PrefixBitLength>
class MultiLookupHuffmanTable {
 public:
  // The largest bit length that may be represented by this table.
  static const uint8_t MAX_BIT_LENGTH =
      PrefixBitLength + Subtable::MAX_BIT_LENGTH;

  explicit MultiLookupHuffmanTable(JSContext* cx)
      : cx_(cx),
        shortKeys_(cx, SingleLookupHuffmanTable::Use::ShortKeys),
        values_(cx),
        suffixTables_(cx),
        largestBitLength_(-1) {}
  MultiLookupHuffmanTable(MultiLookupHuffmanTable&& other) = default;

  // Initialize a Huffman table containing `numberOfSymbols`.
  // Symbols must be added with `addSymbol`.
  // If you initialize with `initStart`, you MUST call `initComplete()`
  // at the end of initialization.
  JS::Result<Ok> initStart(JSContext* cx, size_t numberOfSymbols,
                           uint8_t largestBitLength);

  JS::Result<Ok> initComplete();

  // Add a `(bit, bitLength) => value` mapping.
  JS::Result<Ok> addSymbol(uint32_t bits, uint8_t bitLength,
                           const BinASTSymbol& value);

  MultiLookupHuffmanTable() = delete;
  MultiLookupHuffmanTable(MultiLookupHuffmanTable&) = delete;

  // Lookup a value in the table.
  //
  // The return of this method contains:
  //
  // - the resulting value (`nullptr` if the value is not in the table);
  // - the number of bits in the entry associated to this value.
  //
  // Note that entries inside a single table are typically associated to
  // distinct bit lengths. The caller is responsible for checking
  // the result of this method and advancing the bitstream by
  // `result.key().bitLength_` bits.
  HuffmanLookupResult lookup(HuffmanLookup key) const;

  // The number of values in the table.
  size_t length() const { return values_.length(); }

  // Iterating in the order of insertion.
  struct Iterator {
    explicit Iterator(const HuffmanEntry* position);
    void operator++();
    const BinASTSymbol* operator*() const;
    const BinASTSymbol* operator->() const;
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

   private:
    const HuffmanEntry* position_;
  };
  Iterator begin() const { return Iterator(values_.begin()); }
  Iterator end() const { return Iterator(values_.end()); }

 public:
  // An index into table `values_`.
  // We use `uint8_t` instead of `size_t` to limit the space
  // used by the table.
  using InternalIndex = uint8_t;

 private:
  JSContext* cx_;

  // Fast lookup for values whose keys fit within 8 bits.
  // Such values are not added to `suffixTables`.
  SingleLookupHuffmanTable shortKeys_;

  // The entries in this Huffman Table, sorted in the order of insertion.
  //
  // Invariant (once `init*` has been called):
  // - Length is the number of values inserted in the table.
  // - for all i, `values_[i].bitLength_ <= largestBitLength_`.
  //
  // FIXME: In a ThreeLookupsHuffmanTable, we currently store each value
  // three times. We could at least get down to twice.
  Vector<HuffmanEntry> values_;

  // A mapping from 0..2^prefixBitLen such that index `i`
  // maps to a subtable that holds all values associated
  // with a key that starts with `HuffmanKey(i, prefixBitLen)`.
  //
  // Note that, to allow the use of smaller tables, keys
  // inside the subtables have been stripped
  // from the prefix `HuffmanKey(i, prefixBitLen)`.
  Vector<Subtable> suffixTables_;

  // The maximal bitlength of a value in this table.
  //
  // Invariant (once `init*` has been called):
  // - `largestBitLength_ <= MAX_CODE_BIT_LENGTH`
  uint8_t largestBitLength_;

  friend class HuffmanPreludeReader;
};

/// A Huffman table suitable for max bit lengths in [8, 14]
using TwoLookupsHuffmanTable =
    MultiLookupHuffmanTable<SingleLookupHuffmanTable, 6>;

/// A Huffman table suitable for max bit lengths in [15, 20]
using ThreeLookupsHuffmanTable =
    MultiLookupHuffmanTable<TwoLookupsHuffmanTable, 6>;

// An empty Huffman table. Attempting to get a value from this table is a syntax
// error. This is the default value for `HuffmanTableValue` and represents all
// states that may not be reached.
//
// Part of variants `HuffmanTableValue`, `HuffmanTableListLength` and
// `GenericHuffmanTable::implementation`.
struct HuffmanTableUnreachable {};

// Generic implementation of Huffman tables.
//
//
struct GenericHuffmanTable {
  explicit GenericHuffmanTable(JSContext* cx);
  explicit GenericHuffmanTable() = delete;

  // Initialize a Huffman table containing a single value.
  JS::Result<Ok> initWithSingleValue(JSContext* cx, const BinASTSymbol& value);

  // Initialize a Huffman table containing `numberOfSymbols`.
  // Symbols must be added with `addSymbol`.
  // If you initialize with `initStart`, you MUST call `initComplete()`
  // at the end of initialization.
  JS::Result<Ok> initStart(JSContext* cx, size_t numberOfSymbols,
                           uint8_t maxBitLength);

  // Add a `(bit, bitLength) => value` mapping.
  JS::Result<Ok> addSymbol(uint32_t bits, uint8_t bitLength,
                           const BinASTSymbol& value);

  JS::Result<Ok> initComplete();

  // The number of values in the table.
  size_t length() const;

  struct Iterator {
    explicit Iterator(typename SingleEntryHuffmanTable::Iterator&&);
    explicit Iterator(typename SingleLookupHuffmanTable::Iterator&&);
    explicit Iterator(typename TwoLookupsHuffmanTable::Iterator&&);
    explicit Iterator(typename ThreeLookupsHuffmanTable::Iterator&&);
    Iterator(Iterator&&) = default;
    Iterator(const Iterator&) = default;
    void operator++();
    const BinASTSymbol* operator*() const;
    const BinASTSymbol* operator->() const;
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

   private:
    mozilla::Variant<typename SingleEntryHuffmanTable::Iterator,
                     typename SingleLookupHuffmanTable::Iterator,
                     typename TwoLookupsHuffmanTable::Iterator,
                     typename ThreeLookupsHuffmanTable::Iterator>
        implementation_;
  };

  // Iterating in the order of insertion.
  Iterator begin() const;
  Iterator end() const;

  // Lookup a value in the table.
  //
  // The return of this method contains:
  //
  // - the resulting value (`nullptr` if the value is not in the table);
  // - the number of bits in the entry associated to this value.
  //
  // Note that entries inside a single table are typically associated to
  // distinct bit lengths. The caller is responsible for checking
  // the result of this method and advancing the bitstream by
  // `result.key().bitLength_` bits.
  HuffmanLookupResult lookup(HuffmanLookup key) const;

 private:
  mozilla::Variant<SingleEntryHuffmanTable, SingleLookupHuffmanTable,
                   TwoLookupsHuffmanTable, ThreeLookupsHuffmanTable,
                   HuffmanTableUnreachable>
      implementation_;
};

// While reading the Huffman prelude, whenever we first encounter a
// `HuffmanTableUnreachable`, we replace it with a `HuffmanTableInitializing`
// to mark that we should not attempt to read/initialize it again.
//
// Attempting to get a value from this table is an internal error.
//
// Part of variants `HuffmanTableValue` and `HuffmanTableListLength`.
struct HuffmanTableInitializing {};

// These classes are all parts of variant `HuffmanTableValue`.

struct HuffmanTableExplicitSymbolsF64 : GenericHuffmanTable {
  explicit HuffmanTableExplicitSymbolsF64(JSContext* cx)
      : GenericHuffmanTable(cx) {}
};

struct HuffmanTableExplicitSymbolsU32 : GenericHuffmanTable {
  explicit HuffmanTableExplicitSymbolsU32(JSContext* cx)
      : GenericHuffmanTable(cx) {}
};

struct HuffmanTableIndexedSymbolsSum : GenericHuffmanTable {
  explicit HuffmanTableIndexedSymbolsSum(JSContext* cx)
      : GenericHuffmanTable(cx) {}
};

struct HuffmanTableIndexedSymbolsBool : NaiveHuffmanTable<2> {
  explicit HuffmanTableIndexedSymbolsBool(JSContext* cx)
      : NaiveHuffmanTable(cx) {}
};

// A Huffman table that may only ever contain two values:
// `BinASTKind::_Null` and another `BinASTKind`.
struct HuffmanTableIndexedSymbolsMaybeInterface : NaiveHuffmanTable<2> {
  explicit HuffmanTableIndexedSymbolsMaybeInterface(JSContext* cx)
      : NaiveHuffmanTable(cx) {}

  // `true` if this table only contains values for `null`.
  bool isAlwaysNull() const {
    MOZ_ASSERT(length() > 0);

    // By definition, we have either 1 or 2 values.
    // By definition, if we have 2 values, one of them is not null.
    if (length() != 1) {
      return false;
    }
    // Otherwise, check the single value.
    return begin()->value().toKind() == BinASTKind::_Null;
  }
};

struct HuffmanTableIndexedSymbolsStringEnum : GenericHuffmanTable {
  explicit HuffmanTableIndexedSymbolsStringEnum(JSContext* cx)
      : GenericHuffmanTable(cx) {}
};

struct HuffmanTableIndexedSymbolsLiteralString : GenericHuffmanTable {
  explicit HuffmanTableIndexedSymbolsLiteralString(JSContext* cx)
      : GenericHuffmanTable(cx) {}
};

struct HuffmanTableIndexedSymbolsOptionalLiteralString : GenericHuffmanTable {
  explicit HuffmanTableIndexedSymbolsOptionalLiteralString(JSContext* cx)
      : GenericHuffmanTable(cx) {}
};

// A single Huffman table, used for values.
using HuffmanTableValue = mozilla::Variant<
    HuffmanTableUnreachable,  // Default value.
    HuffmanTableInitializing, HuffmanTableExplicitSymbolsF64,
    HuffmanTableExplicitSymbolsU32, HuffmanTableIndexedSymbolsSum,
    HuffmanTableIndexedSymbolsMaybeInterface, HuffmanTableIndexedSymbolsBool,
    HuffmanTableIndexedSymbolsStringEnum,
    HuffmanTableIndexedSymbolsLiteralString,
    HuffmanTableIndexedSymbolsOptionalLiteralString>;

struct HuffmanTableExplicitSymbolsListLength : GenericHuffmanTable {
  explicit HuffmanTableExplicitSymbolsListLength(JSContext* cx)
      : GenericHuffmanTable(cx) {}
};

// A single Huffman table, specialized for list lengths.
using HuffmanTableListLength =
    mozilla::Variant<HuffmanTableUnreachable,  // Default value.
                     HuffmanTableInitializing,
                     HuffmanTableExplicitSymbolsListLength>;

// A Huffman dictionary for the current file.
//
// A Huffman dictionary consists in a (contiguous) set of Huffman tables
// to predict field values and a second (contiguous) set of Huffman tables
// to predict list lengths.
class HuffmanDictionary {
 public:
  explicit HuffmanDictionary(JSContext* cx);

  HuffmanTableValue& tableForField(NormalizedInterfaceAndField index);
  HuffmanTableListLength& tableForListLength(BinASTList list);

 private:
  // Huffman tables for `(Interface, Field)` pairs, used to decode the value of
  // `Interface::Field`. Some tables may be `HuffmanTableUnreacheable`
  // if they represent fields of interfaces that actually do not show up
  // in the file.
  //
  // The mapping from `(Interface, Field) -> index` is extracted statically from
  // the webidl specs.
  mozilla::Array<HuffmanTableValue, BINAST_INTERFACE_AND_FIELD_LIMIT> fields_;

  // Huffman tables for list lengths. Some tables may be
  // `HuffmanTableUnreacheable` if they represent lists that actually do not
  // show up in the file.
  //
  // The mapping from `List -> index` is extracted statically from the webidl
  // specs.
  mozilla::Array<HuffmanTableListLength, BINAST_NUMBER_OF_LIST_TYPES>
      listLengths_;
};

/**
 * A token reader implementing the "context" serialization format for BinAST.
 *
 * This serialization format, which is also supported by the reference
 * implementation of the BinAST compression suite, is designed to be
 * space- and time-efficient.
 *
 * As other token readers for the BinAST:
 *
 * - the reader does not support error recovery;
 * - the reader does not support lookahead or pushback.
 */
class MOZ_STACK_CLASS BinASTTokenReaderContext : public BinASTTokenReaderBase {
  using Base = BinASTTokenReaderBase;

 public:
  class AutoList;
  class AutoTaggedTuple;

  using CharSlice = BinaryASTSupport::CharSlice;
  using RootContext = BinASTTokenReaderBase::RootContext;
  using ListContext = BinASTTokenReaderBase::ListContext;
  using FieldContext = BinASTTokenReaderBase::FieldContext;
  using FieldOrRootContext = BinASTTokenReaderBase::FieldOrRootContext;
  using FieldOrListContext = BinASTTokenReaderBase::FieldOrListContext;
  using Chars = CharSlice;

 public:
  /**
   * Construct a token reader.
   *
   * Does NOT copy the buffer.
   */
  BinASTTokenReaderContext(JSContext* cx, ErrorReporter* er,
                           const uint8_t* start, const size_t length);

  /**
   * Construct a token reader.
   *
   * Does NOT copy the buffer.
   */
  BinASTTokenReaderContext(JSContext* cx, ErrorReporter* er,
                           const Vector<uint8_t>& chars);

  ~BinASTTokenReaderContext();

  // {readByte, readBuf, readVarU32} are implemented both for uncompressed
  // stream and brotli-compressed stream.
  //
  // Uncompressed variant is for reading the magic header, and compressed
  // variant is for reading the remaining part.
  //
  // Once compressed variant is called, the underlying uncompressed stream is
  // buffered and uncompressed variant cannot be called.
  enum class Compression { No, Yes };

  // Determine what to do if we reach the end of the file.
  enum class EndOfFilePolicy {
    // End of file was not expected, raise an error.
    RaiseError,
    // End of file is ok, read as many bytes as possible.
    BestEffort
  };

 protected:
  // A buffer of bits used to lookup data from the Huffman tables.
  // It may contain up to 64 bits.
  //
  // To interact with the buffer, see methods
  // - advanceBitBuffer()
  // - getHuffmanLookup()
  struct BitBuffer {
    BitBuffer();

    // Return the HuffmanLookup for the next lookup in a Huffman table.
    // After calling this method, do not forget to call `advanceBitBuffer`.
    //
    // If `result.bitLength_ == 0`, you have reached the end of the stream.
    template <Compression Compression>
    MOZ_MUST_USE JS::Result<HuffmanLookup> getHuffmanLookup(
        BinASTTokenReaderContext& owner);

    // Advance the bit buffer by `bitLength` bits.
    template <Compression Compression>
    void advanceBitBuffer(const uint8_t bitLength);

   private:
    // The contents of the buffer.
    //
    // - Bytes are added in the same order as the bytestream.
    // - Individual bits within bytes are mirrored.
    //
    // In other words, if the byte stream starts with
    // `0b_HGFE_DCBA`, `0b_PONM_LKJI`, `0b_0000_0000`,
    // .... `0b_0000_0000`, `bits_` will hold
    // `0b_0000_...0000__ABCD_EFGH__IJKL_MNOP`.
    //
    // Note: By opposition to `HuffmanKey` or `HuffmanLookup`,
    // the highest bits are NOT guaranteed to be `0`.
    uint64_t bits_;

    // The number of elements in `bits_`.
    //
    // Until we start lookup up into Huffman tables, `bitLength_ == 0`.
    // Once we do, we refill the buffer before any lookup, i.e.
    // `MAX_PREFIX_BIT_LENGTH = 32 <= bitLength <= BIT_BUFFER_SIZE = 64`
    // until we reach the last few bytes of the stream,
    // in which case `length` decreases monotonically to 0.
    //
    // If `bitLength_ < BIT_BUFFER_SIZE = 64`, some of the highest
    // bits of `bits_` are unused.
    uint8_t bitLength_;
  } bitBuffer;

  // Returns true if the brotli stream finished.
  bool isEOF() const;

  /**
   * Read a single byte.
   */
  template <Compression compression>
  MOZ_MUST_USE JS::Result<uint8_t> readByte();

  /**
   * Read several bytes.
   *
   * If the tokenizer has previously been poisoned, return an error.
   * If the end of file is reached, in the case of
   * EndOfFilePolicy::RaiseError, raise an error. Otherwise, update
   * `len` to indicate how many bytes have actually been read.
   */
  template <Compression compression, EndOfFilePolicy policy>
  MOZ_MUST_USE JS::Result<Ok> readBuf(uint8_t* bytes, uint32_t& len);

  enum class FillResult { EndOfStream, Filled };

 public:
  /**
   * Read the header of the file.
   */
  MOZ_MUST_USE JS::Result<Ok> readHeader();

  /**
   * Read the string dictionary from the header of the file.
   */
  MOZ_MUST_USE JS::Result<Ok> readStringPrelude();

  /**
   * Read the huffman dictionary from the header of the file.
   */
  MOZ_MUST_USE JS::Result<Ok> readHuffmanPrelude();

  // --- Primitive values.
  //
  // Note that the underlying format allows for a `null` value for primitive
  // values.
  //
  // Reading will return an error either in case of I/O error or in case of
  // a format problem. Reading if an exception in pending is an error and
  // will cause assertion failures. Do NOT attempt to read once an exception
  // has been cleared: the token reader does NOT support recovery, by design.

  /**
   * Read a single `true | false` value.
   */
  MOZ_MUST_USE JS::Result<bool> readBool(const FieldContext&);

  /**
   * Read a single `number` value.
   */
  MOZ_MUST_USE JS::Result<double> readDouble(const FieldContext&);

  /**
   * Read a single `string | null` value.
   *
   * Fails if that string is not valid UTF-8.
   */
  MOZ_MUST_USE JS::Result<JSAtom*> readMaybeAtom(const FieldContext&);
  MOZ_MUST_USE JS::Result<JSAtom*> readAtom(const FieldContext&);

  /**
   * Read a single IdentifierName value.
   */
  MOZ_MUST_USE JS::Result<JSAtom*> readMaybeIdentifierName(const FieldContext&);
  MOZ_MUST_USE JS::Result<JSAtom*> readIdentifierName(const FieldContext&);

  /**
   * Read a single PropertyKey value.
   */
  MOZ_MUST_USE JS::Result<JSAtom*> readPropertyKey(const FieldContext&);

  /**
   * Read a single `string | null` value.
   *
   * MAY check if that string is not valid UTF-8.
   */
  MOZ_MUST_USE JS::Result<Ok> readChars(Chars&, const FieldContext&);

  /**
   * Read a single `BinASTVariant | null` value.
   */
  MOZ_MUST_USE JS::Result<BinASTVariant> readVariant(const ListContext&);
  MOZ_MUST_USE JS::Result<BinASTVariant> readVariant(const FieldContext&);

  /**
   * Read over a single `[Skippable]` subtree value.
   *
   * This does *not* attempt to parse the subtree itself. Rather, the
   * returned `SkippableSubTree` contains the necessary information
   * to parse/tokenize the subtree at a later stage
   */
  MOZ_MUST_USE JS::Result<SkippableSubTree> readSkippableSubTree(
      const FieldContext&);

  // --- Composite values.
  //
  // The underlying format does NOT allows for a `null` composite value.
  //
  // Reading will return an error either in case of I/O error or in case of
  // a format problem. Reading from a poisoned tokenizer is an error and
  // will cause assertion failures.

  /**
   * Start reading a list.
   *
   * @param length (OUT) The number of elements in the list.
   * @param guard (OUT) A guard, ensuring that we read the list correctly.
   *
   * The `guard` is dedicated to ensuring that reading the list has consumed
   * exactly all the bytes from that list. The `guard` MUST therefore be
   * destroyed at the point where the caller has reached the end of the list.
   * If the caller has consumed too few/too many bytes, this will be reported
   * in the call go `guard.done()`.
   */
  MOZ_MUST_USE JS::Result<Ok> enterList(uint32_t& length, const ListContext&);

  /**
   * Start reading a tagged tuple.
   *
   * @param tag (OUT) The tag of the tuple.
   * @param fields Ignored, provided for API compatibility.
   * @param guard (OUT) A guard, ensuring that we read the tagged tuple
   * correctly.
   *
   * The `guard` is dedicated to ensuring that reading the list has consumed
   * exactly all the bytes from that tuple. The `guard` MUST therefore be
   * destroyed at the point where the caller has reached the end of the tuple.
   * If the caller has consumed too few/too many bytes, this will be reported
   * in the call go `guard.done()`.
   *
   * @return out If the header of the tuple is invalid.
   */
  MOZ_MUST_USE JS::Result<Ok> enterInterface(BinASTKind& tag) {
    // We're entering a monomorphic interface, so the tag is encoded as 0 bits.
    MOZ_ASSERT(tag != BinASTKind::_Uninitialized);
    return Ok();
  }
  MOZ_MUST_USE JS::Result<Ok> enterInterface(BinASTKind& tag,
                                             const FieldOrRootContext&) {
    return enterInterface(tag);
  }
  MOZ_MUST_USE JS::Result<Ok> enterInterface(BinASTKind& tag,
                                             const FieldOrListContext&) {
    return enterInterface(tag);
  }
  MOZ_MUST_USE JS::Result<Ok> enterInterface(BinASTKind& tag,
                                             const RootContext&) {
    return enterInterface(tag);
  }
  MOZ_MUST_USE JS::Result<Ok> enterInterface(BinASTKind& tag,
                                             const ListContext&) {
    return enterInterface(tag);
  }
  MOZ_MUST_USE JS::Result<Ok> enterInterface(BinASTKind& tag,
                                             const FieldContext&) {
    return enterInterface(tag);
  }
  MOZ_MUST_USE JS::Result<Ok> enterOptionalInterface(
      BinASTKind& tag, const FieldOrRootContext& context) {
    return enterSum(tag, context);
  }
  MOZ_MUST_USE JS::Result<Ok> enterOptionalInterface(
      BinASTKind& tag, const FieldOrListContext& context) {
    return enterSum(tag, context);
  }
  MOZ_MUST_USE JS::Result<Ok> enterOptionalInterface(
      BinASTKind& tag, const RootContext& context) {
    return enterSum(tag, context);
  }
  MOZ_MUST_USE JS::Result<Ok> enterOptionalInterface(
      BinASTKind& tag, const ListContext& context) {
    return enterSum(tag, context);
  }
  MOZ_MUST_USE JS::Result<Ok> enterOptionalInterface(
      BinASTKind& tag, const FieldContext& context) {
    return enterSum(tag, context);
  }
  MOZ_MUST_USE JS::Result<Ok> enterSum(BinASTKind& tag,
                                       const FieldOrRootContext&);
  MOZ_MUST_USE JS::Result<Ok> enterSum(BinASTKind& tag,
                                       const FieldOrListContext&);
  MOZ_MUST_USE JS::Result<Ok> enterSum(BinASTKind& tag, const RootContext&);
  MOZ_MUST_USE JS::Result<Ok> enterSum(BinASTKind& tag, const ListContext&);
  MOZ_MUST_USE JS::Result<Ok> enterSum(BinASTKind& tag, const FieldContext&);

  /**
   * Read a single unsigned long.
   */
  MOZ_MUST_USE JS::Result<uint32_t> readUnsignedLong(const FieldContext&);
  MOZ_MUST_USE JS::Result<uint32_t> readUnpackedLong();

 private:
  MOZ_MUST_USE JS::Result<BinASTKind> readTagFromTable(
      const BinASTInterfaceAndField&);

  template <typename Table>
  MOZ_MUST_USE JS::Result<BinASTSymbol> readFieldFromTable(
      const BinASTInterfaceAndField&);

  /**
   * Report an "invalid value error".
   */
  MOZ_MUST_USE ErrorResult<JS::Error&> raiseInvalidValue();

  /**
   * Report a "value not in prelude".
   */
  MOZ_MUST_USE ErrorResult<JS::Error&> raiseNotInPrelude();

 private:
  /**
   * Read a single uint32_t.
   */
  template <Compression compression>
  MOZ_MUST_USE JS::Result<uint32_t> readVarU32();

  template <EndOfFilePolicy policy>
  MOZ_MUST_USE JS::Result<Ok> handleEndOfStream();

  template <EndOfFilePolicy policy>
  MOZ_MUST_USE JS::Result<Ok> readBufCompressedAux(uint8_t* bytes,
                                                   uint32_t& len);

 private:
  enum class MetadataOwnership { Owned, Unowned };
  MetadataOwnership metadataOwned_ = MetadataOwnership::Owned;
  BinASTSourceMetadata* metadata_;

  class HuffmanDictionary dictionary_;

  const uint8_t* posBeforeTree_;

 public:
  BinASTTokenReaderContext(const BinASTTokenReaderContext&) = delete;
  BinASTTokenReaderContext(BinASTTokenReaderContext&&) = delete;
  BinASTTokenReaderContext& operator=(BinASTTokenReaderContext&) = delete;

 public:
  void traceMetadata(JSTracer* trc);
  BinASTSourceMetadata* takeMetadata();
  MOZ_MUST_USE JS::Result<Ok> initFromScriptSource(ScriptSource* scriptSource);

 protected:
  friend class HuffmanPreludeReader;

 public:
  // The following classes are used whenever we encounter a tuple/tagged
  // tuple/list to make sure that:
  //
  // - if the construct "knows" its byte length, we have exactly consumed all
  //   the bytes (otherwise, this means that the file is corrupted, perhaps on
  //   purpose, so we need to reject the stream);
  // - if the construct has a footer, once we are done reading it, we have
  //   reached the footer (this is to aid with debugging).
  //
  // In either case, the caller MUST call method `done()` of the guard once
  // it is done reading the tuple/tagged tuple/list, to report any pending
  // error.

  // Base class used by other Auto* classes.
  class MOZ_STACK_CLASS AutoBase {
   protected:
    explicit AutoBase(BinASTTokenReaderContext& reader)
#ifdef DEBUG
        : initialized_(false),
          reader_(reader)
#endif
    {
    }
    ~AutoBase() {
      // By now, the `AutoBase` must have been deinitialized by calling
      // `done()`. The only case in which we can accept not calling `done()` is
      // if we have bailed out because of an error.
      MOZ_ASSERT_IF(initialized_, reader_.hasRaisedError());
    }

    friend BinASTTokenReaderContext;

   public:
    inline void init() {
#ifdef DEBUG
      initialized_ = true;
#endif
    }

    inline MOZ_MUST_USE JS::Result<Ok> done() {
#ifdef DEBUG
      initialized_ = false;
#endif
      return Ok();
    }

   protected:
#ifdef DEBUG
    bool initialized_;
    BinASTTokenReaderContext& reader_;
#endif
  };

  // Guard class used to ensure that `enterList` is used properly.
  class MOZ_STACK_CLASS AutoList : public AutoBase {
   public:
    explicit AutoList(BinASTTokenReaderContext& reader) : AutoBase(reader) {}
  };

  // Guard class used to ensure that `enterTaggedTuple` is used properly.
  class MOZ_STACK_CLASS AutoTaggedTuple : public AutoBase {
   public:
    explicit AutoTaggedTuple(BinASTTokenReaderContext& reader)
        : AutoBase(reader) {}
  };

  // Compare a `Chars` and a string literal (ONLY a string literal).
  template <size_t N>
  static bool equals(const Chars& left, const char (&right)[N]) {
    MOZ_ASSERT(N > 0);
    MOZ_ASSERT(right[N - 1] == 0);
    if (left.byteLen_ + 1 /* implicit NUL */ != N) {
      return false;
    }

    if (!std::equal(left.start_, left.start_ + left.byteLen_, right)) {
      return false;
    }

    return true;
  }
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_BinASTTokenReaderContext_h
