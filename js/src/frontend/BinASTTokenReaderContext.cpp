/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTTokenReaderContext.h"

#include "mozilla/OperatorNewExtensions.h"  // mozilla::KnownNotNull
#include "mozilla/Result.h"                 // MOZ_TRY*
#include "mozilla/ScopeExit.h"              // mozilla::MakeScopeExit

#include <limits>    // std::numeric_limits
#include <string.h>  // memchr, memcmp, memmove

#include "ds/FixedLengthVector.h"    // FixedLengthVector
#include "ds/Sort.h"                 // MergeSort
#include "frontend/BinAST-macros.h"  // BINJS_TRY*, BINJS_MOZ_TRY*
#include "gc/Rooting.h"              // RootedAtom
#include "js/AllocPolicy.h"          // SystemAllocPolicy
#include "js/StableStringChars.h"    // Latin1Char
#include "js/UniquePtr.h"            // js::UniquePtr
#include "js/Utility.h"              // js_free
#include "js/Vector.h"               // js::Vector
#include "util/StringBuffer.h"       // StringBuffer
#include "vm/JSAtom.h"               // AtomizeWTF8Chars
#include "vm/JSScript.h"             // ScriptSource

namespace js::frontend {

#ifdef BINAST_CX_MAGIC_HEADER  // Context 0.1 doesn't implement the planned
                               // magic header.

// The magic header, at the start of every binjs file.
const char CX_MAGIC_HEADER[] =
    "\x89"
    "BJS\r\n\0\n";

// The latest format version understood by this tokenizer.
const uint32_t MAGIC_FORMAT_VERSION = 2;

#endif  // BINAST_CX_MAGIC_HEADER

// The maximal length of a single Huffman code, in bits.
//
// Hardcoded in the format.
const uint8_t MAX_CODE_BIT_LENGTH = 20;

// The maximal number of bits needed to represent bit lengths.
const uint8_t MAX_BIT_LENGTH_BIT_LENGTH = 5;
static_assert(1 << (MAX_BIT_LENGTH_BIT_LENGTH - 1) < MAX_CODE_BIT_LENGTH,
              "MAX_BIT_LENGTH_BIT_LENGTH - 1 bits MUST be insufficient to "
              "store MAX_CODE_BIT_LENGTH");
static_assert(
    1 << MAX_BIT_LENGTH_BIT_LENGTH >= MAX_CODE_BIT_LENGTH,
    "MAX_BIT_LENGTH bits MUST be sufficient to store MAX_CODE_BIT_LENGTH");

// The maximal length of a Huffman prefix.
//
// This is distinct from MAX_CODE_BIT_LENGTH as we commonly load
// more bites than necessary, just to be on the safe side.
const uint8_t MAX_PREFIX_BIT_LENGTH = 32;

// The length of the bit buffer, in bits.
const uint8_t BIT_BUFFER_SIZE = 64;

// Number of bits into the `bitBuffer` read at each step.
const uint8_t BIT_BUFFER_READ_UNIT = 8;

// Hardcoded limits to avoid allocating too eagerly.
const uint32_t MAX_NUMBER_OF_SYMBOLS = 32768;
const uint32_t MAX_LIST_LENGTH =
    std::min((uint32_t)32768, NativeObject::MAX_DENSE_ELEMENTS_COUNT);
const size_t HUFFMAN_STACK_INITIAL_CAPACITY = 1024;

// The number of elements in each sum.
// Note: `extern` is needed to forward declare.
extern const size_t SUM_LIMITS[BINAST_NUMBER_OF_SUM_TYPES];

// For each sum, an array of BinASTKind corresponding to the possible value.
// The length of `SUM_RESOLUTIONS[i]` is `SUM_LIMITS[i]`.
// Note: `extern` is needed to forward declare.
extern const BinASTKind* SUM_RESOLUTIONS[BINAST_NUMBER_OF_SUM_TYPES];

// The number of elements in each enum.
// Note: `extern` is needed to forward declare.
extern const size_t STRING_ENUM_LIMITS[BINASTSTRINGENUM_LIMIT];

// For each string enum, an array of BinASTVariant corresponding to the possible
// value. The length of `STRING_ENUM_RESOLUTIONS[i]` is `STRING_ENUM_LIMITS[i]`.
// Note: `extern` is needed to forward declare.
extern const BinASTVariant* STRING_ENUM_RESOLUTIONS[BINASTSTRINGENUM_LIMIT];

using Compression = BinASTTokenReaderContext::Compression;
using EndOfFilePolicy = BinASTTokenReaderContext::EndOfFilePolicy;

#define WRAP_INTERFACE(TYPE) Interface::Maker(BinASTKind::TYPE)
#define WRAP_MAYBE_INTERFACE(TYPE) MaybeInterface::Maker(BinASTKind::TYPE)
#define WRAP_PRIMITIVE(TYPE) TYPE
#define WRAP_LIST(TYPE, _) List::Maker(BinASTList::TYPE)
#define WRAP_SUM(TYPE) Sum::Maker(BinASTSum::TYPE)
#define WRAP_MAYBE_SUM(TYPE) MaybeSum::Maker(BinASTSum::TYPE)
#define WRAP_STRING_ENUM(TYPE) StringEnum::Maker(BinASTStringEnum::TYPE)
#define WRAP_MAYBE_STRING_ENUM(TYPE) \
  MaybeStringEnum::Maker(BinASTStringEnum::TYPE)

using CharSlice = BinaryASTSupport::CharSlice;
using Chars = BinASTTokenReaderContext::Chars;

// Read Huffman tables from the prelude.
class HuffmanPreludeReader {
 public:
  // Construct a prelude reader.
  HuffmanPreludeReader(JSContext* cx, BinASTTokenReaderContext& reader)
      : cx_(cx),
        reader_(reader),
        dictionary_(nullptr),
        stack_(cx_),
        auxStorageLength_(cx_) {}

  // Start reading the prelude.
  MOZ_MUST_USE JS::Result<HuffmanDictionaryForMetadata*> run(
      size_t initialCapacity);

 private:
  JSContext* cx_;
  BinASTTokenReaderContext& reader_;

  // The dictionary we're currently constructing.
  UniquePtr<HuffmanDictionary> dictionary_;

  // Temporary storage for items allocated while reading huffman prelude.
  // All items are freed after reading huffman prelude.
  TemporaryStorage tempStorage_;

 public:
  // Tables may have different representations in the stream.
  //
  // The value of each variant maps to the value inside the stream.
  enum TableHeader : uint8_t {
    // Table is optimized to represent a single value.
    SingleValue = 0x00,

    // General table, with any number of values.
    MultipleValues = 0x01,

    // A special table that contains no value because it is never used.
    Unreachable = 0x02,
  };

  // --- Representation of the grammar.
  //
  // We need to walk the grammar to read the Huffman tables. In this
  // implementation, we represent the grammar as a variant type `Entry`,
  // composed of subclasses of `EntryBase`.
  //
  // Note that, while some grammar constructions are theoretically possible
  // (e.g. `bool?`), we only implement constructions that actually appear in the
  // webidl. Future constructions may be implemented later.

  // Base class for grammar entries.
  struct EntryBase {
    // The field we are currently reading.
    const NormalizedInterfaceAndField identity_;

    explicit EntryBase(const NormalizedInterfaceAndField identity)
        : identity_(identity) {}
  };

  // Grammar entries for values that are represented by indexed in a set
  // specified by the grammar, e.g booleans, string enums, sums etc
  struct EntryIndexed : EntryBase {
    // We use `Foo::Indexed` during template substitution to accept
    // subclasses of `EntryIndexed`.
    using Indexed = Ok;
    explicit EntryIndexed(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  struct EntryExplicit : EntryBase {
    // We use `Foo::Explicit` during template substitution to accept
    // subclasses of `EntryExplicit`.
    using Explicit = Ok;
    explicit EntryExplicit(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  // A string.
  // May be a literal string, identifier name or property key. May not be null.
  struct String : EntryExplicit {
    explicit String(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };
  using IdentifierName = String;
  using PropertyKey = String;

  // An optional string.
  // May be a literal string, identifier name or property key.
  struct MaybeString : EntryExplicit {
    explicit MaybeString(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };
  using MaybeIdentifierName = MaybeString;
  using MaybePropertyKey = MaybeString;

  // A JavaScript number. May not be null.
  struct Number : EntryExplicit {
    explicit Number(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };

  // A 32-bit integer. May not be null.
  struct UnsignedLong : EntryExplicit {
    explicit UnsignedLong(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };

  // A boolean. May not be null.
  struct Boolean : EntryIndexed {
    explicit Boolean(const NormalizedInterfaceAndField identity)
        : EntryIndexed(identity) {}

    // Comparing booleans.
    //
    // As 0 == False < True == 1, we only compare indices.
    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      MOZ_ASSERT(aIndex <= 1);
      MOZ_ASSERT(bIndex <= 1);
      return aIndex < bIndex;
    }
  };

  // A field encoding a lazy offset.
  struct Lazy : EntryExplicit {
    explicit Lazy(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };

  // A value of a given interface. May not be null.
  struct Interface : EntryIndexed {
    // The kind of the interface.
    const BinASTKind kind_;
    Interface(const NormalizedInterfaceAndField identity, BinASTKind kind)
        : EntryIndexed(identity), kind_(kind) {}

    // Utility struct, used in macros to call the constructor as
    // `Interface::Maker(kind)(identity)`.
    struct Maker {
      const BinASTKind kind_;
      explicit Maker(BinASTKind kind) : kind_(kind) {}
      Interface operator()(const NormalizedInterfaceAndField identity) {
        return Interface(identity, kind_);
      }
    };
  };

  // An optional value of a given interface.
  struct MaybeInterface : EntryIndexed {
    // The kind of the interface.
    const BinASTKind kind_;

    // Comparing optional interfaces.
    //
    // As 0 == Null < _ == 1, we only compare indices.
    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      MOZ_ASSERT(aIndex <= 1);
      MOZ_ASSERT(bIndex <= 1);
      return aIndex < bIndex;
    }

    MaybeInterface(const NormalizedInterfaceAndField identity, BinASTKind kind)
        : EntryIndexed(identity), kind_(kind) {}

    // The max number of symbols in a table for this type.
    size_t maxMumberOfSymbols() const { return 2; }

    // Utility struct, used in macros to call the constructor as
    // `MaybeInterface::Maker(kind)(identity)`.
    struct Maker {
      const BinASTKind kind_;
      explicit Maker(BinASTKind kind) : kind_(kind) {}
      MaybeInterface operator()(const NormalizedInterfaceAndField identity) {
        return MaybeInterface(identity, kind_);
      }
    };
  };

  // A FrozenArray. May not be null.
  //
  // In practice, this type is used to represent the length of the list.
  // Once we have read the model for the length of the list, we push a
  // `ListContents` to read the model for the contents of the list.
  struct List : EntryExplicit {
    // The type of the list, e.g. list of numbers.
    // All lists with the same type share a model for their length.
    const BinASTList contents_;

    List(const NormalizedInterfaceAndField identity, const BinASTList contents)
        : EntryExplicit(identity), contents_(contents) {}

    // Utility struct, used in macros to call the constructor as
    // `List::Maker(kind)(identity)`.
    struct Maker {
      const BinASTList contents_;
      explicit Maker(BinASTList contents) : contents_(contents) {}
      List operator()(const NormalizedInterfaceAndField identity) {
        return {identity, contents_};
      }
    };
  };

  // A FrozenArray. May not be null.
  struct ListContents : EntryBase {
    explicit ListContents(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  // A choice between several interfaces. May not be null.
  struct Sum : EntryIndexed {
    // The type of values in the sum.
    const BinASTSum contents_;

    // Comparing sum entries alphabetically.
    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      MOZ_ASSERT(aIndex <= maxNumberOfSymbols());
      MOZ_ASSERT(bIndex <= maxNumberOfSymbols());
      const size_t aKey = getBinASTKindSortKey(interfaceAt(aIndex));
      const size_t bKey = getBinASTKindSortKey(interfaceAt(bIndex));
      return aKey < bKey;
    }

    Sum(const NormalizedInterfaceAndField identity, const BinASTSum contents)
        : EntryIndexed(identity), contents_(contents) {}

    size_t maxNumberOfSymbols() const {
      return SUM_LIMITS[static_cast<size_t>(contents_)];
    }

    BinASTKind interfaceAt(size_t index) const {
      MOZ_ASSERT(index < maxNumberOfSymbols());
      return SUM_RESOLUTIONS[static_cast<size_t>(contents_)][index];
    }

    // Utility struct, used in macros to call the constructor as
    // `Sum::Maker(kind)(identity)`.
    struct Maker {
      const BinASTSum contents_;
      explicit Maker(BinASTSum contents) : contents_(contents) {}
      Sum operator()(const NormalizedInterfaceAndField identity) {
        return Sum(identity, contents_);
      }
    };
  };

  // An optional choice between several interfaces.
  struct MaybeSum : EntryIndexed {
    // The type of values in the sum.
    // We use `BinASTKind::_Null` to represent the null case.
    const BinASTSum contents_;

    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      return aIndex < bIndex;
    }

    MaybeSum(const NormalizedInterfaceAndField identity,
             const BinASTSum contents)
        : EntryIndexed(identity), contents_(contents) {}

    size_t maxNumberOfSymbols() const {
      return SUM_LIMITS[static_cast<size_t>(contents_)] + 1;
    }

    BinASTKind interfaceAt(size_t index) const {
      MOZ_ASSERT(index < maxNumberOfSymbols());
      if (index == 0) {
        return BinASTKind::_Null;
      }
      return SUM_RESOLUTIONS[static_cast<size_t>(contents_)][index - 1];
    }

    // Utility struct, used in macros to call the constructor as
    // `MaybeSum::Maker(kind)(identity)`.
    struct Maker {
      const BinASTSum contents_;
      explicit Maker(BinASTSum contents) : contents_(contents) {}
      MaybeSum operator()(const NormalizedInterfaceAndField identity) {
        return MaybeSum(identity, contents_);
      }
    };
  };

  // A choice between several strings. May not be null.
  struct StringEnum : EntryIndexed {
    // Comparing string enums alphabetically.
    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      MOZ_ASSERT(aIndex <= maxNumberOfSymbols());
      MOZ_ASSERT(bIndex <= maxNumberOfSymbols());
      const size_t aKey = getBinASTVariantSortKey(variantAt(aIndex));
      const size_t bKey = getBinASTVariantSortKey(variantAt(bIndex));
      return aKey < bKey;
    }

    // The values in the enum.
    const BinASTStringEnum contents_;
    StringEnum(const NormalizedInterfaceAndField identity,
               const BinASTStringEnum contents)
        : EntryIndexed(identity), contents_(contents) {}

    size_t maxNumberOfSymbols() const {
      return STRING_ENUM_LIMITS[static_cast<size_t>(contents_)];
    }

    BinASTVariant variantAt(size_t index) const {
      MOZ_ASSERT(index < maxNumberOfSymbols());
      return STRING_ENUM_RESOLUTIONS[static_cast<size_t>(contents_)][index];
    }

    // Utility struct, used in macros to call the constructor as
    // `StringEnum::Maker(kind)(identity)`.
    struct Maker {
      const BinASTStringEnum contents_;
      explicit Maker(BinASTStringEnum contents) : contents_(contents) {}
      StringEnum operator()(const NormalizedInterfaceAndField identity) {
        return StringEnum(identity, contents_);
      }
    };
  };

 public:
  // The entries in the grammar.
  using Entry = mozilla::Variant<
      // Primitives
      Boolean, String, MaybeString, Number, UnsignedLong, Lazy,
      // Combinators
      Interface, MaybeInterface, List, Sum, MaybeSum, StringEnum>;

#ifdef DEBUG
  // Utility struct, designed to inspect an `Entry` while debugging.
  struct PrintEntry {
    static void print(const char* text, const Entry& entry) {
      fprintf(stderr, "%s ", text);
      entry.match(PrintEntry());
      fprintf(stderr, "\n");
    }
    void operator()(const EntryBase& entry) {
      fprintf(stderr, "%s",
              describeBinASTInterfaceAndField(entry.identity_.identity_));
    }
  };
#endif  // DEBUG

 private:
  // A stack of entries to process. We always process the latest entry added.
  Vector<Entry> stack_;

  /*
  Implementation of "To push a field" from the specs.

  As per the spec:

  1. If the field is in the set of visited contexts, stop
  2. Add the field to the set of visited fields
  3. If the field has a FrozenArray type
    a. Determine if the array type is always empty
    b. If so, stop
  4. If the effective type is a monomorphic interface, push all of the
  interface’s fields
  5. Otherwise, push the field onto the stack
  */

  // Enqueue an entry to the stack.
  MOZ_MUST_USE JS::Result<Ok> pushValue(NormalizedInterfaceAndField identity,
                                        const List& list) {
    const auto tableId = TableIdentity(list.contents_);
    if (dictionary_->isUnreachable(tableId)) {
      // Spec:
      // 2. Add the field to the set of visited fields
      dictionary_->setInitializing(tableId);

      // Read the lengths immediately.
      MOZ_TRY((readTable<List>(tableId, list)));
    }

    // Spec:
    // 3. If the field has a FrozenArray type
    //   a. Determine if the array type is always empty
    //   b. If so, stop
    const auto& table = dictionary_->getTable(tableId);
    bool empty = true;
    for (auto iter : table) {
      if (iter->toListLength() > 0) {
        empty = false;
        break;
      }
    }
    if (empty) {
      return Ok();
    }

    // Spec:
    // To determine a field’s effective type:
    // 1. If the field’s type is an array type, the effective type is the
    // array element type. Stop.

    // We now recurse with the contents of the list/array, *without checking
    // whether the field has already been visited*.
    switch (list.contents_) {
#define WRAP_LIST_2(_, CONTENT) CONTENT
#define EMIT_CASE(LIST_NAME, _CONTENT_TYPE, _HUMAN_NAME, TYPE) \
  case BinASTList::LIST_NAME:                                  \
    return pushValue(list.identity_, TYPE(list.identity_));

      FOR_EACH_BIN_LIST(EMIT_CASE, WRAP_PRIMITIVE, WRAP_INTERFACE,
                        WRAP_MAYBE_INTERFACE, WRAP_LIST_2, WRAP_SUM,
                        WRAP_MAYBE_SUM, WRAP_STRING_ENUM,
                        WRAP_MAYBE_STRING_ENUM)
#undef EMIT_CASE
#undef WRAP_LIST_2
    }
    return Ok();
  }

  MOZ_MUST_USE JS::Result<Ok> pushValue(NormalizedInterfaceAndField identity,
                                        const Interface& interface) {
    const auto tableId = TableIdentity(identity);
    if (dictionary_->isUnreachable(tableId)) {
      // Effectively, an `Interface` is a sum with a single entry.
      auto& table = dictionary_->createTable(tableId);

      MOZ_TRY(table.initWithSingleValue(
          cx_, BinASTSymbol::fromKind(BinASTKind(interface.kind_))));
    }

    // Spec:
    // 4. If the effective type is a monomorphic interface, push all of the
    // interface’s fields
    return pushFields(interface.kind_);
  }

  // Generic implementation for other cases.
  template <class Entry>
  MOZ_MUST_USE JS::Result<Ok> pushValue(NormalizedInterfaceAndField identity,
                                        const Entry& entry) {
    // Spec:
    // 1. If the field is in the set of visited contexts, stop.
    const auto tableId = TableIdentity(identity);
    if (!dictionary_->isUnreachable(tableId)) {
      // Entry already initialized/initializing.
      return Ok();
    }

    // Spec:
    // 2. Add the field to the set of visited fields
    dictionary_->setInitializing(tableId);

    // Spec:
    // 5. Otherwise, push the field onto the stack
    BINJS_TRY(stack_.append(entry));
    return Ok();
  }

  // Enqueue all the fields of an interface to the stack.
  JS::Result<Ok> pushFields(BinASTKind tag) {
    /*
    Generate a static implementation of pushing fields.

    switch (tag) {
      case BinASTKind::$TagName: {
        MOZ_TRY(pushValue(NormalizedInterfaceAndField(BinASTIterfaceAndField::$Name),
    Entry(FIELD_TYPE(NormalizedInterfaceAndField(BinASTInterfaceAndField::$Name)))));
        // ...
        break;
      }
      // ...
    }
    */

    switch (tag) {
#define EMIT_FIELD(TAG_NAME, FIELD_NAME, FIELD_INDEX, FIELD_TYPE, _)        \
  MOZ_TRY(pushValue(NormalizedInterfaceAndField(                            \
                        BinASTInterfaceAndField::TAG_NAME##__##FIELD_NAME), \
                    FIELD_TYPE(NormalizedInterfaceAndField(                 \
                        BinASTInterfaceAndField::TAG_NAME##__##FIELD_NAME))));
#define EMIT_CASE(TAG_ENUM_NAME, _2, TAG_MACRO_NAME)                      \
  case BinASTKind::TAG_ENUM_NAME: {                                       \
    FOR_EACH_BIN_FIELD_IN_INTERFACE_##TAG_MACRO_NAME(                     \
        EMIT_FIELD, WRAP_PRIMITIVE, WRAP_INTERFACE, WRAP_MAYBE_INTERFACE, \
        WRAP_LIST, WRAP_SUM, WRAP_MAYBE_SUM, WRAP_STRING_ENUM,            \
        WRAP_MAYBE_STRING_ENUM);                                          \
    break;                                                                \
  }
      FOR_EACH_BIN_KIND(EMIT_CASE)
#undef EMIT_CASE
#undef EMIT_FIELD
    }

    return Ok();
  }

  // Read a single symbol.
  // For an indexed type, the symbol is fetched from the grammar using `index`.
  // We have a guarantee that `index` is always in [0, numberOfSymbols).
  template <typename Entry>
  MOZ_MUST_USE JS::Result<BinASTSymbol> readSymbol(const Entry&, size_t index);

  // Read the number of symbols in an entry.
  // For an indexed type, theis number is fetched from the grammar.
  // We have a guarantee that `index` is always in [0, numberOfSymbols)
  template <typename Entry>
  MOZ_MUST_USE JS::Result<uint32_t> readNumberOfSymbols(const Entry&);

  // Read a table in the optimized "only one value" format.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readSingleValueTable(GenericHuffmanTable&,
                                                   const Entry&);

  // Read a table in the non-optimized format.
  //
  // Entries in the table are represented in an order that is specific to each
  // type. See the documentation of each `ReadPoppedEntryMatcher::operator()`
  // for a discussion on each order.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readMultipleValuesTable(
      GenericHuffmanTable& table, Entry entry) {
    // Get the number of symbols.
    // If `Entry` is an indexed type, this is fetched from the grammar.
    BINJS_MOZ_TRY_DECL(numberOfSymbols, readNumberOfSymbols<Entry>(entry));

    MOZ_ASSERT(numberOfSymbols <= MAX_NUMBER_OF_SYMBOLS);

    if (numberOfSymbols == 1) {
      // Special case: only one symbol.
      BINJS_MOZ_TRY_DECL(bitLength, reader_.readByte<Compression::No>());
      if (MOZ_UNLIKELY(bitLength != 0)) {
        // Since there is a single symbol, it doesn't make sense to have a non-0
        // bit length.
        return raiseInvalidTableData(entry.identity_);
      }

      // Read the symbol.
      // If `Entry` is an indexed type, it is fetched directly from the grammar.
      BINJS_MOZ_TRY_DECL(
          symbol, readSymbol<Entry>(entry, /* First and only value */ 0));

      MOZ_TRY(table.initWithSingleValue(cx_, symbol));
      return Ok();
    }

    MOZ_TRY(readMultipleValuesTableAndAssignCode<Entry>(table, entry,
                                                        numberOfSymbols));

    // Note that the table may be empty, in the case of a list that never has
    // any elements.
    return Ok();
  }

  template <typename Entry>
  MOZ_MUST_USE JS::Result<typename Entry::Explicit>
  readMultipleValuesTableAndAssignCode(GenericHuffmanTable& table, Entry entry,
                                       uint32_t numberOfSymbols) {
    // Explicit entry:
    // - All symbols must be read from disk.
    // - Lengths read from disk are bit lengths.
    // - Bit lengths are ranked by increasing order.
    // - Bit lengths MUST NOT be 0.

    // Data is presented in an order that doesn't match our memory
    // representation, so we need to copy `numberOfSymbols` entries.
    // We use an auxiliary vector to avoid allocating each time.
    MOZ_ASSERT(
        auxStorageLength_.empty());  // We must have cleaned it up properly.
    BINJS_TRY(auxStorageLength_.reserve(numberOfSymbols + 1));

    uint8_t largestBitLength = 0;

    // First read and store the bit lengths for all symbols.
    for (size_t i = 0; i < numberOfSymbols; ++i) {
      BINJS_MOZ_TRY_DECL(bitLength, reader_.readByte<Compression::No>());
      if (MOZ_UNLIKELY(bitLength == 0)) {
        return raiseInvalidTableData(entry.identity_);
      }
      if (bitLength > largestBitLength) {
        largestBitLength = bitLength;
      }
      // Already reserved sufficient space above.
      auxStorageLength_.infallibleAppend(BitLengthAndIndex(bitLength, i));
    }
    // Append a terminator.
    // Already reserved sufficient space above.
    auxStorageLength_.infallibleAppend(
        BitLengthAndIndex(MAX_CODE_BIT_LENGTH, numberOfSymbols));

    // Now read the symbols and assign bits.
    uint32_t code = 0;
    MOZ_TRY(
        table.initStart(cx_, &tempStorage_, numberOfSymbols, largestBitLength));

    for (size_t i = 0; i < numberOfSymbols; ++i) {
      const auto bitLength = auxStorageLength_[i].bitLength_;
      const auto nextBitLength =
          auxStorageLength_[i + 1]
              .bitLength_;  // Guaranteed to exist, as we have a terminator.

      if (MOZ_UNLIKELY(bitLength > nextBitLength)) {
        // By format invariant, bit lengths are always non-0
        // and always ranked by increasing order.
        return raiseInvalidTableData(entry.identity_);
      }

      // Read and add symbol.
      BINJS_MOZ_TRY_DECL(
          symbol, readSymbol<Entry>(entry, i));  // Symbol is read from disk.
      MOZ_TRY(table.addSymbol(i, code, bitLength, symbol));

      // Prepare next code.
      code = (code + 1) << (nextBitLength - bitLength);
    }

    MOZ_TRY(table.initComplete(cx_, &tempStorage_));
    auxStorageLength_.clear();
    return Ok();
  }

  template <typename Entry>
  MOZ_MUST_USE JS::Result<typename Entry::Indexed>
  readMultipleValuesTableAndAssignCode(GenericHuffmanTable& table, Entry entry,
                                       uint32_t numberOfSymbols) {
    // In this case, `numberOfSymbols` is actually an upper bound,
    // rather than an actual number of symbols.

    // Data is presented in an order that doesn't match our memory
    // representation, so we need to copy `numberOfSymbols` entries.
    // We use an auxiliary vector to avoid allocating each time.
    MOZ_ASSERT(
        auxStorageLength_.empty());  // We must have cleaned it up properly.
    BINJS_TRY(auxStorageLength_.reserve(numberOfSymbols + 1));

    // Implicit entry:
    // - Symbols are known statically.
    // - Lengths MAY be 0.
    // - Lengths are not sorted on disk.

    uint8_t largestBitLength = 0;

    // First read the length for all symbols, only store non-0 lengths.
    for (size_t i = 0; i < numberOfSymbols; ++i) {
      BINJS_MOZ_TRY_DECL(bitLength, reader_.readByte<Compression::No>());
      if (MOZ_UNLIKELY(bitLength > MAX_CODE_BIT_LENGTH)) {
        MOZ_CRASH("FIXME: Implement error");
      }
      if (bitLength > 0) {
        // Already reserved sufficient space above.
        auxStorageLength_.infallibleAppend(BitLengthAndIndex(bitLength, i));
        if (bitLength > largestBitLength) {
          largestBitLength = bitLength;
        }
      }
    }

    if (auxStorageLength_.length() == 1) {
      // We have only one symbol, so let's use an optimized table.
      BINJS_MOZ_TRY_DECL(symbol,
                         readSymbol<Entry>(entry, auxStorageLength_[0].index_));
      MOZ_TRY(table.initWithSingleValue(cx_, symbol));
      auxStorageLength_.clear();
      return Ok();
    }

    // Sort by length then webidl order (which is also the index).
    std::sort(auxStorageLength_.begin(), auxStorageLength_.end(),
              [&entry](const BitLengthAndIndex& a,
                       const BitLengthAndIndex& b) -> bool {
                MOZ_ASSERT(a.index_ != b.index_);
                // Compare first by bit length.
                if (a.bitLength_ < b.bitLength_) {
                  return true;
                }
                if (a.bitLength_ > b.bitLength_) {
                  return false;
                }
                // In case of equal bit length, compare by symbol value.
                return entry.lessThan(a.index_, b.index_);
              });

    // Append a terminator.
    // Already reserved sufficient space above.
    auxStorageLength_.infallibleEmplaceBack(MAX_CODE_BIT_LENGTH,
                                            numberOfSymbols);

    // Now read the symbols and assign bits.
    uint32_t code = 0;
    MOZ_TRY(table.initStart(cx_, &tempStorage_, auxStorageLength_.length() - 1,
                            largestBitLength));

    for (size_t i = 0; i < auxStorageLength_.length() - 1; ++i) {
      const auto bitLength = auxStorageLength_[i].bitLength_;
      const auto nextBitLength =
          auxStorageLength_[i + 1].bitLength_;  // Guaranteed to exist, as we
                                                // stop before the terminator.
      MOZ_ASSERT(bitLength > 0);
      MOZ_ASSERT(bitLength <= nextBitLength);

      // Read symbol from memory and add it.
      BINJS_MOZ_TRY_DECL(symbol,
                         readSymbol<Entry>(entry, auxStorageLength_[i].index_));

      MOZ_TRY(table.addSymbol(i, code, bitLength, symbol));

      // Prepare next code.
      code = (code + 1) << (nextBitLength - bitLength);
    }

    MOZ_TRY(table.initComplete(cx_, &tempStorage_));

    auxStorageLength_.clear();
    return Ok();
  }

  // Single-argument version: lookup the table using `dictionary_->table`
  // then proceed as three-arguments version.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readTable(Entry entry) {
    const auto tableId = TableIdentity(entry.identity_);
    if (MOZ_UNLIKELY(!dictionary_->isInitializing(tableId))) {
      // We're attempting to re-read a table that has already been read.
      // FIXME: Shouldn't this be a MOZ_CRASH?
      return raiseDuplicateTableError(entry.identity_);
    }

    return readTable<Entry>(tableId, entry);
  }

  // Two-arguments version: pass table identity explicitly. Generally called
  // from single-argument version, but may be called manually, e.g. for list
  // lengths.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readTable(TableIdentity tableId, Entry entry) {
    uint8_t headerByte;
    MOZ_TRY_VAR(headerByte, reader_.readByte<Compression::No>());
    switch (headerByte) {
      case TableHeader::SingleValue: {
        auto& table = dictionary_->createTable(tableId);

        // The table contains a single value.
        MOZ_TRY((readSingleValueTable<Entry>(table, entry)));
        return Ok();
      }
      case TableHeader::MultipleValues: {
        auto& table = dictionary_->createTable(tableId);

        // Table contains multiple values.
        MOZ_TRY((readMultipleValuesTable<Entry>(table, entry)));
        return Ok();
      }
      case TableHeader::Unreachable:
        // Table is unreachable, nothing to do.
        return Ok();
      default:
        return raiseInvalidTableData(entry.identity_);
    }
  }

 private:
  // An auxiliary storage of bit lengths and indices, used when reading
  // a table with multiple entries. Used to avoid (re)allocating a
  // vector of lengths each time we read a table.
  struct BitLengthAndIndex {
    BitLengthAndIndex(uint8_t bitLength, size_t index)
        : bitLength_(bitLength), index_(index) {}

    // A bitLength, as read from disk.
    uint8_t bitLength_;

    // The index of the entry in the table.
    size_t index_;
  };
  Vector<BitLengthAndIndex> auxStorageLength_;

  // Implementation of pattern-matching cases for `entry.match`.
  struct ReadPoppedEntryMatcher {
    // The owning HuffmanPreludeReader.
    HuffmanPreludeReader& owner;

    explicit ReadPoppedEntryMatcher(HuffmanPreludeReader& owner)
        : owner(owner) {}

    // Lazy.
    // Values: no value/
    MOZ_MUST_USE JS::Result<Ok> operator()(const Lazy& entry) {
      // Nothing to do.
      return Ok();
    }

    // Boolean.
    // Values: [false, true], in this order.
    MOZ_MUST_USE JS::Result<Ok> operator()(const Boolean& entry) {
      return owner.readTable<Boolean>(entry);
    }

    // Interface.
    // Values: no value.
    MOZ_MUST_USE JS::Result<Ok> operator()(const Interface& entry) {
      // No table here, just push fields.
      return owner.pushFields(entry.kind_);
    }

    // Optional Interface.
    // Values: [null, non-null].
    MOZ_MUST_USE JS::Result<Ok> operator()(const MaybeInterface& entry) {
      // First, we need a table to determine whether the value is `null`.
      MOZ_TRY((owner.readTable<MaybeInterface>(entry)));

      // Then, if the table contains `true`, we need the fields of the
      // interface.
      // FIXME: readTable could return a reference to the table, eliminating an
      // array lookup.
      const auto tableId = TableIdentity(entry.identity_);
      if (owner.dictionary_->isUnreachable(tableId)) {
        return Ok();
      }

      const auto& table = owner.dictionary_->getTable(tableId);
      if (!table.isMaybeInterfaceAlwaysNull()) {
        MOZ_TRY(owner.pushFields(entry.kind_));
      }
      return Ok();
    }

    // Sum of interfaces, non-nullable.
    // Values: interfaces by the order in `FOR_EACH_BIN_INTERFACE_IN_SUM_*`.
    MOZ_MUST_USE JS::Result<Ok> operator()(const Sum& entry) {
      // First, we need a table to determine which values are present.
      MOZ_TRY((owner.readTable<Sum>(entry)));

      // Now, walk the table to enqueue each value if necessary.
      // FIXME: readTable could return a reference to the table, eliminating an
      // array lookup.

      const auto tableId = TableIdentity(entry.identity_);
      if (owner.dictionary_->isInitializing(tableId)) {
        return Ok();
      }

      auto index = entry.identity_;
      const auto& table = owner.dictionary_->getTable(tableId);
      for (auto iter : table) {
        MOZ_TRY(owner.pushValue(index, Interface(index, iter->toKind())));
      }
      return Ok();
    }

    // Sum of interfaces, nullable.
    // Values: `null`, followed by interfaces by the order in
    // `FOR_EACH_BIN_INTERFACE_IN_SUM_*`.
    MOZ_MUST_USE JS::Result<Ok> operator()(const MaybeSum& entry) {
      // First, we need a table to determine which values are present.
      MOZ_TRY((owner.readTable<MaybeSum>(entry)));

      // Now, walk the table to enqueue each value if necessary.
      // FIXME: readTable could return a reference to the table, eliminating an
      // array lookup.
      const auto tableId = TableIdentity(entry.identity_);
      if (owner.dictionary_->isUnreachable(tableId)) {
        return Ok();
      }

      auto index = entry.identity_;
      const auto& table = owner.dictionary_->getTable(tableId);
      for (auto iter : table) {
        MOZ_TRY(owner.pushValue(index, Interface(index, iter->toKind())));
      }
      return Ok();
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const Number& entry) {
      return owner.readTable<Number>(entry);
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const String& entry) {
      return owner.readTable<String>(entry);
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const MaybeString& entry) {
      return owner.readTable<MaybeString>(entry);
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const StringEnum& entry) {
      return owner.readTable<StringEnum>(entry);
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const UnsignedLong& entry) {
      return owner.readTable<UnsignedLong>(entry);
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const List&) {
      MOZ_CRASH("Unreachable");
      return Ok();
    }
  };

  template <typename T>
  using ErrorResult = BinASTTokenReaderBase::ErrorResult<T>;

  MOZ_MUST_USE ErrorResult<JS::Error&> raiseDuplicateTableError(
      const NormalizedInterfaceAndField identity) {
    return reader_.raiseError("Duplicate table.");
  }

  MOZ_MUST_USE ErrorResult<JS::Error&> raiseInvalidTableData(
      const NormalizedInterfaceAndField identity) {
    return reader_.raiseError("Invalid data while reading table.");
  }
};

using Boolean = HuffmanPreludeReader::Boolean;
using Interface = HuffmanPreludeReader::Interface;
using List = HuffmanPreludeReader::List;
using MaybeInterface = HuffmanPreludeReader::MaybeInterface;
using MaybeString = HuffmanPreludeReader::MaybeString;
using MaybeSum = HuffmanPreludeReader::MaybeSum;
using Number = HuffmanPreludeReader::Number;
using String = HuffmanPreludeReader::String;
using StringEnum = HuffmanPreludeReader::StringEnum;
using Sum = HuffmanPreludeReader::Sum;
using UnsignedLong = HuffmanPreludeReader::UnsignedLong;

BinASTTokenReaderContext::BinASTTokenReaderContext(JSContext* cx,
                                                   ErrorReporter* er,
                                                   const uint8_t* start,
                                                   const size_t length)
    : BinASTTokenReaderBase(cx, er, start, length),
      metadata_(nullptr),
      posBeforeTree_(nullptr),
      lazyScripts_(cx) {
  MOZ_ASSERT(er);
}

BinASTTokenReaderContext::~BinASTTokenReaderContext() {
  if (metadata_ && metadataOwned_ == MetadataOwnership::Owned) {
    UniqueBinASTSourceMetadataPtr ptr(metadata_);
  }
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::readBuf<Compression::No, EndOfFilePolicy::RaiseError>(
    uint8_t* bytes, uint32_t& len) {
  return Base::readBuf(bytes, len);
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::readBuf<Compression::No, EndOfFilePolicy::BestEffort>(
    uint8_t* bytes, uint32_t& len) {
  len = std::min((uint32_t)(stop_ - current_), len);
  return Base::readBuf(bytes, len);
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::handleEndOfStream<EndOfFilePolicy::RaiseError>() {
  return raiseError("Unexpected end of stream");
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::handleEndOfStream<EndOfFilePolicy::BestEffort>() {
  return Ok();
}

template <>
JS::Result<uint8_t> BinASTTokenReaderContext::readByte<Compression::No>() {
  return Base::readByte();
}

BinASTSourceMetadata* BinASTTokenReaderContext::takeMetadata() {
  MOZ_ASSERT(metadataOwned_ == MetadataOwnership::Owned);
  metadataOwned_ = MetadataOwnership::Unowned;
  return metadata_;
}

JS::Result<Ok> BinASTTokenReaderContext::initFromScriptSource(
    ScriptSource* scriptSource) {
  metadata_ = scriptSource->binASTSourceMetadata()->asContext();
  metadataOwned_ = MetadataOwnership::Unowned;

  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::readHeader() {
  // Check that we don't call this function twice.
  MOZ_ASSERT(!posBeforeTree_);

#if BINAST_CX_MAGIC_HEADER
  // Read global headers.
  MOZ_TRY(readConst(CX_MAGIC_HEADER));
  BINJS_MOZ_TRY_DECL(version, readVarU32<Compression::No>());

  if (MOZ_UNLIKELY(version != MAGIC_FORMAT_VERSION)) {
    return raiseError("Format version not implemented");
  }
#endif  // BINAST_CX_MAGIC_HEADER

  MOZ_TRY(readStringPrelude());
  MOZ_TRY(readHuffmanPrelude());

  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::readTreeFooter() {
  flushBitStream();

  BINJS_MOZ_TRY_DECL(numLazy, readVarU32<Compression::No>());
  if (numLazy != lazyScripts_.length()) {
    return raiseError("The number of lazy functions does not match");
  }

  for (size_t i = 0; i < numLazy; i++) {
    BINJS_MOZ_TRY_DECL(len, readVarU32<Compression::No>());
    // Use sourceEnd as temporary space to store length of each script.
    lazyScripts_[i]->setPositions(0, len, 0, len);
  }

  for (size_t i = 0; i < numLazy; i++) {
    uint32_t begin = offset();
    uint32_t len = lazyScripts_[i]->sourceEnd();

    current_ += len;

    lazyScripts_[0]->setPositions(begin, begin + len, begin, begin + len);
    lazyScripts_[0]->setColumn(begin);
  }

  return Ok();
}

void BinASTTokenReaderContext::flushBitStream() {
  current_ -= bitBuffer.numUnusedBytes();
  bitBuffer.flush();
}

JS::Result<Ok> BinASTTokenReaderContext::readStringPrelude() {
  BINJS_MOZ_TRY_DECL(stringsNumberOfEntries, readVarU32<Compression::No>());

  const uint32_t MAX_NUMBER_OF_STRINGS = 32768;

  if (MOZ_UNLIKELY(stringsNumberOfEntries > MAX_NUMBER_OF_STRINGS)) {
    return raiseError("Too many entries in strings dictionary");
  }

  BinASTSourceMetadataContext* metadata =
      BinASTSourceMetadataContext::create(stringsNumberOfEntries);
  if (MOZ_UNLIKELY(!metadata)) {
    return raiseOOM();
  }

  // Free it if we don't make it out of here alive. Since we don't want to
  // calloc(), we need to avoid marking atoms that might not be there.
  auto se = mozilla::MakeScopeExit([metadata]() { js_free(metadata); });

  // Below, we read at most DECODED_BUFFER_SIZE bytes at once and look for
  // strings there. We can overrun to the model part, and in that case put
  // unused part back to `decodedBuffer_`.

  RootedAtom atom(cx_);

  for (uint32_t stringIndex = 0; stringIndex < stringsNumberOfEntries;
       stringIndex++) {
    if (MOZ_UNLIKELY(current_ >= stop_)) {
      return raiseError("End of file reached while reading strings table");
    }

    const uint8_t* end =
        static_cast<const uint8_t*>(memchr(current_, '\0', stop_ - current_));
    if (MOZ_UNLIKELY(!end)) {
      return raiseError("Invalid string, missing NUL");
    }

    // FIXME: handle 0x01-escaped string here.

    const char* start = reinterpret_cast<const char*>(current_);
    BINJS_TRY_VAR(atom, AtomizeWTF8Chars(cx_, start, end - current_));

    metadata->getAtom(stringIndex) = atom;

    // +1 for skipping 0x00.
    current_ = end + 1;
  }

  // Found all strings. The remaining data is kept in buffer and will be
  // used for later read.

  MOZ_ASSERT(!metadata_);
  se.release();
  metadata_ = metadata;
  metadataOwned_ = MetadataOwnership::Owned;

  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::readHuffmanPrelude() {
  HuffmanPreludeReader reader(cx_, *this);
  BINJS_MOZ_TRY_DECL(dictionary, reader.run(HUFFMAN_STACK_INITIAL_CAPACITY));
  metadata_->setDictionary(dictionary);
  return Ok();
}

BinASTTokenReaderContext::BitBuffer::BitBuffer() : bits_(0), bitLength_(0) {
  static_assert(sizeof(bits_) * 8 == BIT_BUFFER_SIZE,
                "Expecting bitBuffer to match BIT_BUFFER_SIZE");
}

template <Compression C>
JS::Result<HuffmanLookup> BinASTTokenReaderContext::BitBuffer::getHuffmanLookup(
    BinASTTokenReaderContext& owner) {
  // First, read data if necessary.

  // The algorithm is not intuitive, so consider an example, where the byte
  // stream starts with `0b_HGFE_DCBA`, `0b_PONM_LKJI`, `0b_XWVU_TRSQ`,
  // `0b_6543_21ZY`
  //
  // In each byte, bits are stored in the reverse order, so what we want
  // is `0b_ABCD_EFGH`, `0b_IJML_MNOP`, `0b_QRST_UVWX`, `0b_YZ12_3456`.
  // For the example, let's assume that we have already read
  // `0b_ABCD_EFGH`, `0b_IJKL_MNOP` into `bits`, so before the call to
  // `getHuffmanLookup`, `bits` initially contains
  // `0b_XXXX_XXXX__XXXX_XXXX__ABCD_EFGH__IJKL_MNOP`, where `X` are bits that
  // are beyond `bitLength_`

  if (bitLength_ <= MAX_PREFIX_BIT_LENGTH) {
    // Keys can be up to MAX_PREFIX_BIT_LENGTH bits long. If we have fewer bits
    // available, it's time to reload. We'll try and get as close to 64 bits as
    // possible.

    // Make an array to store the new data. We can read up to 8 bytes
    uint8_t bytes[8] = {};
    // Determine the number of bytes in `bits` rounded up to the nearest byte.
    uint32_t bytesInBits =
        (bitLength_ + BIT_BUFFER_READ_UNIT - 1) / BIT_BUFFER_READ_UNIT;
    // Determine the number of bytes we need to read to get `bitLength` as close
    // as possible to 64
    uint32_t readLen = sizeof(bytes) - bytesInBits;
    // Try to read `readLen` bytes. If we read less than 8 bytes, the last `8 -
    // readLen` indices contain zeros. Since the most significant bytes of the
    // data are stored in the lowest indices, `bytes` is big endian.
    MOZ_TRY((owner.readBuf<Compression::No, EndOfFilePolicy::BestEffort>(
        bytes, readLen)));
    // Combine `bytes` array into `newBits`
    uint64_t newBits = (static_cast<uint64_t>(bytes[0]) << 56) |
                       (static_cast<uint64_t>(bytes[1]) << 48) |
                       (static_cast<uint64_t>(bytes[2]) << 40) |
                       (static_cast<uint64_t>(bytes[3]) << 32) |
                       (static_cast<uint64_t>(bytes[4]) << 24) |
                       (static_cast<uint64_t>(bytes[5]) << 16) |
                       (static_cast<uint64_t>(bytes[6]) << 8) |
                       static_cast<uint64_t>(bytes[7]);
    static_assert(sizeof(bytes) == sizeof(newBits),
                  "Expecting bytes array to match size of newBits");
    // After reading `readLen` bytes in our example, `bytes` will contain
    // `{0b_XWSU_TSRQ, 0b_6543_21ZY, 0, 0, 0, 0, 0, 0}`
    // and `newBits` contains zeros in the lower 32 bits and
    // `0b_XWSU_TSRQ__6543_21ZY__0000_0000__0000_0000` in the upper 32 bits

    // Remove any zeros if we read less than 8 bytes
    newBits >>= (BIT_BUFFER_READ_UNIT * (sizeof(bytes) - readLen));
    // Now the upper 32 bits of `newBits` are all zero and the lower 32 bits
    // contain `0b_0000_0000__0000_0000__XWSU_TSRQ__6543_21ZY`

    // Let's reverse the bits in each of the 8 bytes in `newBits` in place
    // First swap alternating bits
    newBits = ((newBits >> 1) & 0x5555555555555555) |
              ((newBits & 0x5555555555555555) << 1);
    // Then swap alternating groups of 2 bits
    newBits = ((newBits >> 2) & 0x3333333333333333) |
              ((newBits & 0x3333333333333333) << 2);
    // Finally swap alternating nibbles
    newBits = ((newBits >> 4) & 0x0F0F0F0F0F0F0F0F) |
              ((newBits & 0x0F0F0F0F0F0F0F0F) << 4);
    // Now the lower 32 bits of `newBits` contain
    // `0b_0000_0000__0000_0000__QRST_UVWX__YZ12_3456`

    bitLength_ += (BIT_BUFFER_READ_UNIT * readLen);
    // Make room for the new data by shifting `bits` to get
    // `0b_ABCD_EFGH__IJKL_MNOP__0000_0000__0000_0000`
    // check that we're not shifting by 64 since it's UB for a uint64_t
    if (readLen != 8) {
      bits_ <<= (BIT_BUFFER_READ_UNIT * readLen);
      // Finally, combine `newBits` with `bits` to get
      // `0b_ABCD_EFGH__IJKL_MNOP__QRST_UVWX__YZ12_3456`
      bits_ += newBits;
    }
    // If read 8 bytes just set `bits` to the new data
    else {
      bits_ = newBits;
    }
    // Now, we may prepare a `HuffmanLookup` with up to 32 bits.
    if (bitLength_ <= MAX_PREFIX_BIT_LENGTH) {
      return HuffmanLookup(bits_, bitLength_);
    }
  }

  // Keep only 32 bits. We perform the operation on 64 bits to avoid any
  // arithmetics surprise.
  const uint64_t bitPrefix = bits_ >> (bitLength_ - MAX_PREFIX_BIT_LENGTH);
  MOZ_ASSERT(bitPrefix <= uint32_t(-1));
  return HuffmanLookup(bitPrefix, MAX_PREFIX_BIT_LENGTH);
}

template <>
void BinASTTokenReaderContext::BitBuffer::advanceBitBuffer<Compression::No>(
    const uint8_t bitLength) {
  // It should be impossible to call `advanceBitBuffer`
  // with more bits than what we just handed out.
  MOZ_ASSERT(bitLength <= bitLength_);

  bitLength_ -= bitLength;

  // Now zero out the bits that are beyond `bitLength_`.
  const uint64_t mask = bitLength_ == 0
                            ? 0  // >> 64 is UB for a uint64_t
                            : uint64_t(-1) >> (BIT_BUFFER_SIZE - bitLength_);
  bits_ &= mask;
  MOZ_ASSERT_IF(bitLength_ != BIT_BUFFER_SIZE, bits_ >> bitLength_ == 0);
}

void BinASTTokenReaderContext::traceMetadata(JSTracer* trc) {
  if (metadata_) {
    metadata_->trace(trc);
  }
}

MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&>
BinASTTokenReaderContext::raiseInvalidValue() {
  errorReporter_->errorNoOffset(JSMSG_BINAST, "Invalid value");
  return cx_->alreadyReportedError();
}

MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&>
BinASTTokenReaderContext::raiseNotInPrelude() {
  errorReporter_->errorNoOffset(JSMSG_BINAST, "Value is not in prelude");
  return cx_->alreadyReportedError();
}

struct ExtractBinASTInterfaceAndFieldMatcher {
  BinASTInterfaceAndField operator()(
      const BinASTTokenReaderBase::FieldContext& context) {
    return context.position_;
  }
  BinASTInterfaceAndField operator()(
      const BinASTTokenReaderBase::ListContext& context) {
    return context.position_;
  }
  BinASTInterfaceAndField operator()(
      const BinASTTokenReaderBase::RootContext&) {
    MOZ_CRASH("The root context has no interface/field");
  }
};

JS::Result<BinASTKind> BinASTTokenReaderContext::readTagFromTable(
    const BinASTInterfaceAndField& identity) {
  // Extract the table.
  const auto tableId = TableIdentity(NormalizedInterfaceAndField(identity));
  const auto& table = metadata_->dictionary()->getTable(tableId);
  BINJS_MOZ_TRY_DECL(bits_,
                     (bitBuffer.getHuffmanLookup<Compression::No>(*this)));

  // We're entering either a single interface or a sum.
  const auto result = table.lookup(bits_);
  if (MOZ_UNLIKELY(!result.isFound())) {
    return raiseInvalidValue();
  }
  bitBuffer.advanceBitBuffer<Compression::No>(result.bitLength());
  return result.value().toKind();
}

JS::Result<BinASTSymbol> BinASTTokenReaderContext::readFieldFromTable(
    const BinASTInterfaceAndField& identity) {
  const auto tableId = TableIdentity(NormalizedInterfaceAndField(identity));
  if (!metadata_->dictionary()->isReady(tableId)) {
    return raiseNotInPrelude();
  }

  const auto& table = metadata_->dictionary()->getTable(tableId);
  BINJS_MOZ_TRY_DECL(bits_, bitBuffer.getHuffmanLookup<Compression::No>(*this));

  const auto result = table.lookup(bits_);
  if (MOZ_UNLIKELY(!result.isFound())) {
    return raiseInvalidValue();
  }

  bitBuffer.advanceBitBuffer<Compression::No>(result.bitLength());
  return result.value();
}

JS::Result<bool> BinASTTokenReaderContext::readBool(
    const FieldContext& context) {
  BINJS_MOZ_TRY_DECL(result, readFieldFromTable(context.position_));
  return result.toBool();
}

JS::Result<double> BinASTTokenReaderContext::readDouble(
    const FieldContext& context) {
  BINJS_MOZ_TRY_DECL(result, readFieldFromTable(context.position_));
  return result.toDouble();
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeAtom(
    const FieldContext& context) {
  BINJS_MOZ_TRY_DECL(result, readFieldFromTable(context.position_));
  if (result.isNullAtom()) {
    return nullptr;
  }
  return metadata_->getAtom(result.toAtomIndex());
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readAtom(
    const FieldContext& context) {
  BINJS_MOZ_TRY_DECL(result, readFieldFromTable(context.position_));
  MOZ_ASSERT(!result.isNullAtom());
  return metadata_->getAtom(result.toAtomIndex());
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeIdentifierName(
    const FieldContext& context) {
  return readMaybeAtom(context);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readIdentifierName(
    const FieldContext& context) {
  return readAtom(context);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readPropertyKey(
    const FieldContext& context) {
  return readAtom(context);
}

JS::Result<Ok> BinASTTokenReaderContext::readChars(Chars& out,
                                                   const FieldContext&) {
  return raiseError("readChars is not implemented in BinASTTokenReaderContext");
}

JS::Result<BinASTVariant> BinASTTokenReaderContext::readVariant(
    const ListContext& context) {
  BINJS_MOZ_TRY_DECL(result, readFieldFromTable(context.position_));
  return result.toVariant();
}

JS::Result<BinASTVariant> BinASTTokenReaderContext::readVariant(
    const FieldContext& context) {
  BINJS_MOZ_TRY_DECL(result, readFieldFromTable(context.position_));
  return result.toVariant();
}

JS::Result<uint32_t> BinASTTokenReaderContext::readUnsignedLong(
    const FieldContext& context) {
  BINJS_MOZ_TRY_DECL(result, readFieldFromTable(context.position_));
  return result.toUnsignedLong();
}

JS::Result<BinASTTokenReaderBase::SkippableSubTree>
BinASTTokenReaderContext::readSkippableSubTree(const FieldContext&) {
  // Postions are set when reading lazy functions after the tree.
  return SkippableSubTree(0, 0);
}

JS::Result<Ok> BinASTTokenReaderContext::registerLazyScript(LazyScript* lazy) {
  BINJS_TRY(lazyScripts_.append(lazy));
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::enterSum(
    BinASTKind& tag, const FieldOrRootContext& context) {
  return context.match(
      [this, &tag](const BinASTTokenReaderBase::FieldContext& asFieldContext)
          -> JS::Result<Ok> {
        // This tuple is the value of the field we're currently reading.
        MOZ_TRY_VAR(tag, readTagFromTable(asFieldContext.position_));
        return Ok();
      },
      [&tag](const BinASTTokenReaderBase::RootContext&) -> JS::Result<Ok> {
        // For the moment, the format hardcodes `Script` as root.
        tag = BinASTKind::Script;
        return Ok();
      });
}

JS::Result<Ok> BinASTTokenReaderContext::enterSum(
    BinASTKind& tag, const FieldOrListContext& context) {
  return context.match(
      [this, &tag](const BinASTTokenReaderBase::FieldContext& asFieldContext)
          -> JS::Result<Ok> {
        // This tuple is the value of the field we're currently reading.
        MOZ_TRY_VAR(tag, readTagFromTable(asFieldContext.position_));
        return Ok();
      },
      [this, &tag](const BinASTTokenReaderBase::ListContext& asListContext)
          -> JS::Result<Ok> {
        // This tuple is an element in a list we're currently reading.
        MOZ_TRY_VAR(tag, readTagFromTable(asListContext.position_));
        return Ok();
      });
}

JS::Result<Ok> BinASTTokenReaderContext::enterSum(BinASTKind& tag,
                                                  const RootContext& context) {
  // For the moment, the format hardcodes `Script` as root.
  tag = BinASTKind::Script;
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::enterSum(BinASTKind& tag,
                                                  const ListContext& context) {
  // This tuple is an element in a list we're currently reading.
  MOZ_TRY_VAR(tag, readTagFromTable(context.position_));
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::enterSum(BinASTKind& tag,
                                                  const FieldContext& context) {
  // This tuple is the value of the field we're currently reading.
  MOZ_TRY_VAR(tag, readTagFromTable(context.position_));
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::enterList(uint32_t& items,
                                                   const ListContext& context) {
  const auto tableId = TableIdentity(context.content_);
  const auto& table = metadata_->dictionary()->getTable(tableId);
  BINJS_MOZ_TRY_DECL(bits_, bitBuffer.getHuffmanLookup<Compression::No>(*this));
  const auto result = table.lookup(bits_);
  if (MOZ_UNLIKELY(!result.isFound())) {
    return raiseInvalidValue();
  }
  bitBuffer.advanceBitBuffer<Compression::No>(result.bitLength());
  items = result.value().toListLength();
  return Ok();
}

// Internal uint32_t
// Note that this is different than varnum in multipart.
//
// Encoded as variable length number.

template <Compression compression>
JS::Result<uint32_t> BinASTTokenReaderContext::readVarU32() {
  uint32_t result = 0;
  uint32_t shift = 0;
  while (true) {
    MOZ_ASSERT(shift < 32);
    uint32_t byte;
    MOZ_TRY_VAR(byte, readByte<compression>());

    const uint32_t newResult = result | (byte & 0x7f) << shift;
    if (MOZ_UNLIKELY(newResult < result)) {
      return raiseError("Overflow during readVarU32");
    }

    result = newResult;
    shift += 7;

    if ((byte & 0x80) == 0) {
      return result;
    }

    if (MOZ_UNLIKELY(shift >= 32)) {
      return raiseError("Overflow during readVarU32");
    }
  }
}

JS::Result<uint32_t> BinASTTokenReaderContext::readUnpackedLong() {
  uint8_t bytes[4];
  uint32_t length = 4;
  MOZ_TRY(
      (readBuf<Compression::No, EndOfFilePolicy::RaiseError>(bytes, length)));
  const uint32_t result = uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 |
                          uint32_t(bytes[2]) << 8 | uint32_t(bytes[3]);
  return result;
}

HuffmanKey::HuffmanKey(const uint32_t bits, const uint8_t bitLength)
    : bits_(bits), bitLength_(bitLength) {
  MOZ_ASSERT(bitLength_ <= MAX_PREFIX_BIT_LENGTH);
  MOZ_ASSERT_IF(bitLength_ != 32 /* >> 32 is UB */, bits >> bitLength == 0);
}

template <typename T>
T* TemporaryStorageItem<T>::alloc(JSContext* cx, size_t count) {
  total_ += count;

  if (MOZ_LIKELY(head_)) {
    if (MOZ_LIKELY(head_->used_ + count < head_->size_)) {
      // This chunk still has sufficient space to allocate `count` bytes.
      T* ret = head_->entries_ + head_->used_;
      head_->used_ += count;
      return ret;
    }
  }

  size_t chunkSize = Chunk::DefaultSize;
  if (count > chunkSize) {
    chunkSize = count;
  }

  // Subtract `sizeof(T)` because `Chunk` already has `T entries_[1]`
  // and that we want `T entries_[chunkSize]`.
  Chunk* chunk =
      reinterpret_cast<Chunk*>(cx->template maybe_pod_malloc<uint8_t>(
          sizeof(Chunk) - sizeof(T) + chunkSize * sizeof(T)));
  if (!chunk) {
    ReportOutOfMemory(cx);
    return nullptr;
  }
  chunk->used_ = count;
  chunk->size_ = chunkSize;
  chunk->next_ = head_;

  head_ = chunk;

  return head_->entries_;
}

template <typename T>
JS::Result<mozilla::Span<T>> TemporaryStorage::alloc(JSContext* cx,
                                                     size_t count) {
  MOZ_CRASH("unsupported type");
  return nullptr;
}

template <>
JS::Result<mozilla::Span<HuffmanEntry>> TemporaryStorage::alloc<HuffmanEntry>(
    JSContext* cx, size_t count) {
  auto* items = huffmanEntries_.alloc(cx, count);
  if (!items) {
    return cx->alreadyReportedError();
  }
  return mozilla::MakeSpan(items, count);
}

template <>
JS::Result<mozilla::Span<TemporaryStorage::InternalIndex>>
TemporaryStorage::alloc<TemporaryStorage::InternalIndex>(JSContext* cx,
                                                         size_t count) {
  auto* items = internalIndices_.alloc(cx, count);
  if (!items) {
    return cx->alreadyReportedError();
  }
  return mozilla::MakeSpan(items, count);
}

template <>
JS::Result<mozilla::Span<SingleLookupHuffmanTable>>
TemporaryStorage::alloc<SingleLookupHuffmanTable>(JSContext* cx, size_t count) {
  auto* items = singleTables_.alloc(cx, count);
  if (!items) {
    return cx->alreadyReportedError();
  }
  return mozilla::MakeSpan(items, count);
}

template <>
JS::Result<mozilla::Span<TwoLookupsHuffmanTable>>
TemporaryStorage::alloc<TwoLookupsHuffmanTable>(JSContext* cx, size_t count) {
  auto* items = twoTables_.alloc(cx, count);
  if (!items) {
    return cx->alreadyReportedError();
  }
  return mozilla::MakeSpan(items, count);
}

// ---- Implementation of Huffman Tables

GenericHuffmanTable::Iterator::Iterator(
    typename SingleEntryHuffmanTable::Iterator&& iterator)
    : implementation_(std::move(iterator)) {}

GenericHuffmanTable::Iterator::Iterator(
    typename TwoEntriesHuffmanTable::Iterator&& iterator)
    : implementation_(std::move(iterator)) {}

GenericHuffmanTable::Iterator::Iterator(
    typename SingleLookupHuffmanTable::Iterator&& iterator)
    : implementation_(std::move(iterator)) {}

GenericHuffmanTable::Iterator::Iterator(
    typename TwoLookupsHuffmanTable::Iterator&& iterator)
    : implementation_(std::move(iterator)) {}

GenericHuffmanTable::Iterator::Iterator(
    typename ThreeLookupsHuffmanTable::Iterator&& iterator)
    : implementation_(std::move(iterator)) {}

void GenericHuffmanTable::Iterator::operator++() {
  implementation_.match(
      [](typename SingleEntryHuffmanTable::Iterator& iterator) {
        iterator.operator++();
      },
      [](typename TwoEntriesHuffmanTable::Iterator& iterator) {
        iterator.operator++();
      },
      [](typename SingleLookupHuffmanTable::Iterator& iterator) {
        iterator.operator++();
      },
      [](typename TwoLookupsHuffmanTable::Iterator& iterator) {
        iterator.operator++();
      },
      [](typename ThreeLookupsHuffmanTable::Iterator& iterator) {
        iterator.operator++();
      });
}

bool GenericHuffmanTable::Iterator::operator==(
    const GenericHuffmanTable::Iterator& other) const {
  return implementation_.match(
      [other](const typename SingleEntryHuffmanTable::Iterator& iterator) {
        return iterator ==
               other.implementation_
                   .template as<typename SingleEntryHuffmanTable::Iterator>();
      },
      [other](const typename TwoEntriesHuffmanTable::Iterator& iterator) {
        return iterator ==
               other.implementation_
                   .template as<typename TwoEntriesHuffmanTable::Iterator>();
      },
      [other](const typename SingleLookupHuffmanTable::Iterator& iterator) {
        return iterator ==
               other.implementation_
                   .template as<typename SingleLookupHuffmanTable::Iterator>();
      },
      [other](const typename TwoLookupsHuffmanTable::Iterator& iterator) {
        return iterator ==
               other.implementation_
                   .template as<typename TwoLookupsHuffmanTable::Iterator>();
      },
      [other](const typename ThreeLookupsHuffmanTable::Iterator& iterator) {
        return iterator ==
               other.implementation_
                   .template as<typename ThreeLookupsHuffmanTable::Iterator>();
      });
}

bool GenericHuffmanTable::Iterator::operator!=(
    const GenericHuffmanTable::Iterator& other) const {
  return implementation_.match(
      [other](const typename SingleEntryHuffmanTable::Iterator& iterator) {
        return iterator !=
               other.implementation_
                   .template as<typename SingleEntryHuffmanTable::Iterator>();
      },
      [other](const typename TwoEntriesHuffmanTable::Iterator& iterator) {
        return iterator !=
               other.implementation_
                   .template as<typename TwoEntriesHuffmanTable::Iterator>();
      },
      [other](const typename SingleLookupHuffmanTable::Iterator& iterator) {
        return iterator !=
               other.implementation_
                   .template as<typename SingleLookupHuffmanTable::Iterator>();
      },
      [other](const typename TwoLookupsHuffmanTable::Iterator& iterator) {
        return iterator !=
               other.implementation_
                   .template as<typename TwoLookupsHuffmanTable::Iterator>();
      },
      [other](const typename ThreeLookupsHuffmanTable::Iterator& iterator) {
        return iterator !=
               other.implementation_
                   .template as<typename ThreeLookupsHuffmanTable::Iterator>();
      });
}

const BinASTSymbol* GenericHuffmanTable::Iterator::operator*() const {
  return implementation_.match(
      [](const typename SingleEntryHuffmanTable::Iterator& iterator) {
        return iterator.operator*();
      },
      [](const typename TwoEntriesHuffmanTable::Iterator& iterator) {
        return iterator.operator*();
      },
      [](const typename SingleLookupHuffmanTable::Iterator& iterator) {
        return iterator.operator*();
      },
      [](const typename TwoLookupsHuffmanTable::Iterator& iterator) {
        return iterator.operator*();
      },
      [](const typename ThreeLookupsHuffmanTable::Iterator& iterator) {
        return iterator.operator*();
      });
}

const BinASTSymbol* GenericHuffmanTable::Iterator::operator->() const {
  return implementation_.match(
      [](const typename SingleEntryHuffmanTable::Iterator& iterator) {
        return iterator.operator->();
      },
      [](const typename TwoEntriesHuffmanTable::Iterator& iterator) {
        return iterator.operator->();
      },
      [](const typename SingleLookupHuffmanTable::Iterator& iterator) {
        return iterator.operator->();
      },
      [](const typename TwoLookupsHuffmanTable::Iterator& iterator) {
        return iterator.operator->();
      },
      [](const typename ThreeLookupsHuffmanTable::Iterator& iterator) {
        return iterator.operator->();
      });
}

GenericHuffmanTable::GenericHuffmanTable()
    : implementation_(TableImplementationUninitialized{}) {}

JS::Result<Ok> GenericHuffmanTable::initComplete(
    JSContext* cx, TemporaryStorage* tempStorage) {
  return implementation_.match(
      [](SingleEntryHuffmanTable& implementation) -> JS::Result<Ok> {
        MOZ_CRASH("SingleEntryHuffmanTable shouldn't have multiple entries!");
      },
      [cx,
       tempStorage](TwoEntriesHuffmanTable& implementation) -> JS::Result<Ok> {
        return implementation.initComplete(cx, tempStorage);
      },
      [cx, tempStorage](
          SingleLookupHuffmanTable& implementation) -> JS::Result<Ok> {
        return implementation.initComplete(cx, tempStorage);
      },
      [cx,
       tempStorage](TwoLookupsHuffmanTable& implementation) -> JS::Result<Ok> {
        return implementation.initComplete(cx, tempStorage);
      },
      [cx, tempStorage](
          ThreeLookupsHuffmanTable& implementation) -> JS::Result<Ok> {
        return implementation.initComplete(cx, tempStorage);
      },
      [](TableImplementationUninitialized&) -> JS::Result<Ok> {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

typename GenericHuffmanTable::Iterator GenericHuffmanTable::begin() const {
  return implementation_.match(
      [](const SingleEntryHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.begin());
      },
      [](const TwoEntriesHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.begin());
      },
      [](const SingleLookupHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.begin());
      },
      [](const TwoLookupsHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.begin());
      },
      [](const ThreeLookupsHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.begin());
      },
      [](const TableImplementationUninitialized&)
          -> GenericHuffmanTable::Iterator {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

typename GenericHuffmanTable::Iterator GenericHuffmanTable::end() const {
  return implementation_.match(
      [](const SingleEntryHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.end());
      },
      [](const TwoEntriesHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.end());
      },
      [](const SingleLookupHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.end());
      },
      [](const TwoLookupsHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.end());
      },
      [](const ThreeLookupsHuffmanTable& implementation)
          -> GenericHuffmanTable::Iterator {
        return Iterator(implementation.end());
      },
      [](const TableImplementationUninitialized&)
          -> GenericHuffmanTable::Iterator {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

JS::Result<Ok> GenericHuffmanTable::initWithSingleValue(
    JSContext* cx, const BinASTSymbol& value) {
  // Only one value: use HuffmanImplementationSaturated
  MOZ_ASSERT(implementation_.template is<
             TableImplementationUninitialized>());  // Make sure that we're
                                                    // initializing.

  implementation_ = {mozilla::VariantType<SingleEntryHuffmanTable>{}, value};
  return Ok();
}

JS::Result<Ok> GenericHuffmanTable::initStart(JSContext* cx,
                                              TemporaryStorage* tempStorage,
                                              size_t numberOfSymbols,
                                              uint8_t largestBitLength) {
  // Make sure that we have a way to represent all legal bit lengths.
  static_assert(MAX_CODE_BIT_LENGTH <= ThreeLookupsHuffmanTable::MAX_BIT_LENGTH,
                "ThreeLookupsHuffmanTable cannot hold all bit lengths");

  // Make sure that we're initializing.
  MOZ_ASSERT(implementation_.template is<TableImplementationUninitialized>());

  // Make sure we don't accidentally end up with only one symbol.
  MOZ_ASSERT(numberOfSymbols != 1,
             "Should have used `initWithSingleValue` instead");

  if (numberOfSymbols == 2) {
    implementation_ = {mozilla::VariantType<TwoEntriesHuffmanTable>{}};
    return implementation_.template as<TwoEntriesHuffmanTable>().initStart(
        cx, tempStorage, numberOfSymbols, largestBitLength);
  }

  // Find the (hopefully) fastest implementation of HuffmanTable for
  // `largestBitLength`.
  // ...hopefully, only one lookup.
  if (largestBitLength <= SingleLookupHuffmanTable::MAX_BIT_LENGTH) {
    implementation_ = {mozilla::VariantType<SingleLookupHuffmanTable>{},
                       SingleLookupHuffmanTable::Use::ToplevelTable};
    return implementation_.template as<SingleLookupHuffmanTable>().initStart(
        cx, tempStorage, numberOfSymbols, largestBitLength);
  }

  // ...if a single-lookup table would be too large, let's see if
  // we can fit in a two-lookup table.
  if (largestBitLength <= TwoLookupsHuffmanTable::MAX_BIT_LENGTH) {
    implementation_ = {mozilla::VariantType<TwoLookupsHuffmanTable>{}};
    return implementation_.template as<TwoLookupsHuffmanTable>().initStart(
        cx, tempStorage, numberOfSymbols, largestBitLength);
  }

  // ...otherwise, we'll need three lookups.
  implementation_ = {mozilla::VariantType<ThreeLookupsHuffmanTable>{}};
  return implementation_.template as<ThreeLookupsHuffmanTable>().initStart(
      cx, tempStorage, numberOfSymbols, largestBitLength);
}

JS::Result<Ok> GenericHuffmanTable::addSymbol(size_t index, uint32_t bits,
                                              uint8_t bitLength,
                                              const BinASTSymbol& value) {
  return implementation_.match(
      [](SingleEntryHuffmanTable&) -> JS::Result<Ok> {
        MOZ_CRASH("SingleEntryHuffmanTable shouldn't have multiple entries!");
        return Ok();
      },
      [index, bits, bitLength,
       value](TwoEntriesHuffmanTable&
                  implementation) mutable /* discard implicit const */
      -> JS::Result<Ok> {
        return implementation.addSymbol(index, bits, bitLength, value);
      },
      [index, bits, bitLength,
       value](SingleLookupHuffmanTable&
                  implementation) mutable /* discard implicit const */
      -> JS::Result<Ok> {
        return implementation.addSymbol(index, bits, bitLength, value);
      },
      [index, bits, bitLength,
       value](TwoLookupsHuffmanTable&
                  implementation) mutable /* discard implicit const */
      -> JS::Result<Ok> {
        return implementation.addSymbol(index, bits, bitLength, value);
      },
      [index, bits, bitLength,
       value = value](ThreeLookupsHuffmanTable&
                          implementation) mutable /* discard implicit const */
      -> JS::Result<Ok> {
        return implementation.addSymbol(index, bits, bitLength, value);
      },
      [](TableImplementationUninitialized&) -> JS::Result<Ok> {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
        return Ok();
      });
}

HuffmanLookupResult GenericHuffmanTable::lookup(HuffmanLookup key) const {
  return implementation_.match(
      [key](const SingleEntryHuffmanTable& implementation)
          -> HuffmanLookupResult { return implementation.lookup(key); },
      [key](const TwoEntriesHuffmanTable& implementation)
          -> HuffmanLookupResult { return implementation.lookup(key); },
      [key](const SingleLookupHuffmanTable& implementation)
          -> HuffmanLookupResult { return implementation.lookup(key); },
      [key](const TwoLookupsHuffmanTable& implementation)
          -> HuffmanLookupResult { return implementation.lookup(key); },
      [key](const ThreeLookupsHuffmanTable& implementation)
          -> HuffmanLookupResult { return implementation.lookup(key); },
      [](const TableImplementationUninitialized&) -> HuffmanLookupResult {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

size_t GenericHuffmanTable::length() const {
  return implementation_.match(
      [](const SingleEntryHuffmanTable& implementation) -> size_t {
        return implementation.length();
      },
      [](const TwoEntriesHuffmanTable& implementation) -> size_t {
        return implementation.length();
      },
      [](const SingleLookupHuffmanTable& implementation) -> size_t {
        return implementation.length();
      },
      [](const TwoLookupsHuffmanTable& implementation) -> size_t {
        return implementation.length();
      },
      [](const ThreeLookupsHuffmanTable& implementation) -> size_t {
        return implementation.length();
      },
      [](const TableImplementationUninitialized& implementation) -> size_t {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

SingleEntryHuffmanTable::Iterator::Iterator(const BinASTSymbol* position)
    : position_(position) {}

void SingleEntryHuffmanTable::Iterator::operator++() {
  // There's only one entry, and `nullptr` means `end`.
  position_ = nullptr;
}

const BinASTSymbol* SingleEntryHuffmanTable::Iterator::operator*() const {
  return position_;
}

const BinASTSymbol* SingleEntryHuffmanTable::Iterator::operator->() const {
  return position_;
}

bool SingleEntryHuffmanTable::Iterator::operator==(
    const Iterator& other) const {
  return position_ == other.position_;
}

bool SingleEntryHuffmanTable::Iterator::operator!=(
    const Iterator& other) const {
  return position_ != other.position_;
}

HuffmanLookupResult SingleEntryHuffmanTable::lookup(HuffmanLookup key) const {
  return HuffmanLookupResult::found(0, &value_);
}

TwoEntriesHuffmanTable::Iterator::Iterator(const BinASTSymbol* position)
    : position_(position) {}

void TwoEntriesHuffmanTable::Iterator::operator++() { position_++; }

const BinASTSymbol* TwoEntriesHuffmanTable::Iterator::operator*() const {
  return position_;
}

const BinASTSymbol* TwoEntriesHuffmanTable::Iterator::operator->() const {
  return position_;
}

bool TwoEntriesHuffmanTable::Iterator::operator==(const Iterator& other) const {
  return position_ == other.position_;
}

bool TwoEntriesHuffmanTable::Iterator::operator!=(const Iterator& other) const {
  return position_ != other.position_;
}

JS::Result<Ok> TwoEntriesHuffmanTable::initStart(JSContext* cx,
                                                 TemporaryStorage* tempStorage,
                                                 size_t numberOfSymbols,
                                                 uint8_t largestBitLength) {
  // Make sure that we're initializing.
  MOZ_ASSERT(numberOfSymbols == 2);
  MOZ_ASSERT(largestBitLength == 1);
  return Ok();
}

JS::Result<Ok> TwoEntriesHuffmanTable::initComplete(
    JSContext* cx, TemporaryStorage* tempStorage) {
  return Ok();
}

JS::Result<Ok> TwoEntriesHuffmanTable::addSymbol(size_t index, uint32_t bits,
                                                 uint8_t bitLength,
                                                 const BinASTSymbol& value) {
  // Symbols must be ranked by increasing bits length
  MOZ_ASSERT_IF(index == 0, bits == 0);
  MOZ_ASSERT_IF(index == 1, bits == 1);

  // FIXME: Throw soft error instead of assert.
  MOZ_ASSERT(bitLength == 1);

  values_[index] = value;
  return Ok();
}

HuffmanLookupResult TwoEntriesHuffmanTable::lookup(HuffmanLookup key) const {
  // By invariant, bit lengths are 1.
  const auto index = key.leadingBits(1);
  return HuffmanLookupResult::found(1, &values_[index]);
}

SingleLookupHuffmanTable::Iterator::Iterator(const HuffmanEntry* position)
    : position_(position) {}

void SingleLookupHuffmanTable::Iterator::operator++() { position_++; }

const BinASTSymbol* SingleLookupHuffmanTable::Iterator::operator*() const {
  return &position_->value();
}

const BinASTSymbol* SingleLookupHuffmanTable::Iterator::operator->() const {
  return &position_->value();
}

bool SingleLookupHuffmanTable::Iterator::operator==(
    const Iterator& other) const {
  return position_ == other.position_;
}

bool SingleLookupHuffmanTable::Iterator::operator!=(
    const Iterator& other) const {
  return position_ != other.position_;
}

JS::Result<Ok> SingleLookupHuffmanTable::initStart(
    JSContext* cx, TemporaryStorage* tempStorage, size_t numberOfSymbols,
    uint8_t largestBitLength) {
  MOZ_ASSERT_IF(largestBitLength != 32,
                (uint32_t(1) << largestBitLength) - 1 <=
                    std::numeric_limits<InternalIndex>::max());

  largestBitLength_ = largestBitLength;

  MOZ_TRY_VAR(values_, tempStorage->alloc<HuffmanEntry>(cx, numberOfSymbols));

  const size_t saturatedLength = 1 << largestBitLength_;
  MOZ_TRY_VAR(saturated_,
              tempStorage->alloc<InternalIndex>(cx, saturatedLength));

  // Enlarge `saturated_`, as we're going to fill it in random order.
  for (size_t i = 0; i < saturatedLength; ++i) {
    // Capacity reserved in this method.
    saturated_[i] = InternalIndex(-1);
  }
  return Ok();
}

JS::Result<Ok> SingleLookupHuffmanTable::initComplete(
    JSContext* cx, TemporaryStorage* tempStorage) {
  // Double-check that we've initialized properly.
  MOZ_ASSERT(largestBitLength_ <= MAX_CODE_BIT_LENGTH);

  // We can end up with empty tables, if this `SingleLookupHuffmanTable`
  // is used to store suffixes in a `MultiLookupHuffmanTable` and
  // the corresponding prefix is never used.
  if (values_.size() == 0) {
    MOZ_ASSERT(largestBitLength_ == 0);
    return Ok();
  }

#ifdef DEBUG
  bool foundMaxBitLength = false;
  for (size_t i = 0; i < saturated_.size(); ++i) {
    const uint8_t index = saturated_[i];
    if (use_ != Use::ToplevelTable) {
      // The table may not be full.
      if (index >= values_.size()) {
        continue;
      }
    }
    MOZ_ASSERT(values_[index].key().bitLength_ <= largestBitLength_);
    if (values_[index].key().bitLength_ == largestBitLength_) {
      foundMaxBitLength = true;
    }
  }
  MOZ_ASSERT(foundMaxBitLength);
#endif  // DEBUG

  return Ok();
}

JS::Result<Ok> SingleLookupHuffmanTable::addSymbol(size_t index, uint32_t bits,
                                                   uint8_t bitLength,
                                                   const BinASTSymbol& value) {
  MOZ_ASSERT_IF(largestBitLength_ != 0, bitLength != 0);
  MOZ_ASSERT_IF(bitLength != 32 /* >> 32 is UB */, bits >> bitLength == 0);
  MOZ_ASSERT(bitLength <= largestBitLength_);

  // First add the value to `values_`.
  new (mozilla::KnownNotNull, &values_[index])
      HuffmanEntry(bits, bitLength, value);

  // Notation: in the following, unless otherwise specified, we consider
  // values with `largestBitLength_` bits exactly.
  //
  // When we perform lookup, we will extract `largestBitLength_` bits from the
  // key into a value `0bB...B`. We have a match for `value` if and only if
  // `0bB...B` may be decomposed into `0bC...CX...X` such that
  //    - `0bC...C` is `bitLength` bits long;
  //    - `0bC...C == bits`.
  //
  // To perform a fast lookup, we precompute all possible values of `0bB...B`
  // for which this condition is true. That's all the values of segment
  // `[0bC...C0...0, 0bC...C1...1]`.
  const HuffmanLookup base(bits, bitLength);
  for (size_t i : base.suffixes(largestBitLength_)) {
    saturated_[i] = index;
  }

  return Ok();
}

HuffmanLookupResult SingleLookupHuffmanTable::lookup(HuffmanLookup key) const {
  if (values_.size() == 0) {
    // If the table is empty, any lookup fails.
    return HuffmanLookupResult::notFound();
  }
  // ...otherwise, all lookups succeed.

  // Take the `largestBitLength_` highest weight bits of `key`.
  // In the documentation of `addSymbol`, this is
  // `0bB...B`.
  const uint32_t bits = key.leadingBits(largestBitLength_);

  // Invariants: `saturated_.size() == 1 << largestBitLength_`
  // and `bits <= 1 << largestBitLength_`.
  const size_t index = saturated_[bits];
  if (index >= values_.size()) {
    // This is useful only when the `SingleLookupHuffmanTable`
    // is used as a cache inside a `MultiLookupHuffmanTable`.
    MOZ_ASSERT(use_ == Use::ShortKeys);
    return HuffmanLookupResult::notFound();
  }

  // Invariants: `saturated_[i] < values_.size()`.
  const auto& entry = values_[index];
  return HuffmanLookupResult::found(entry.key().bitLength_, &entry.value());
}

template <typename Subtable, uint8_t PrefixBitLength>
MultiLookupHuffmanTable<Subtable, PrefixBitLength>::Iterator::Iterator(
    const HuffmanEntry* position)
    : position_(position) {}

template <typename Subtable, uint8_t PrefixBitLength>
void MultiLookupHuffmanTable<Subtable,
                             PrefixBitLength>::Iterator::operator++() {
  position_++;
}

template <typename Subtable, uint8_t PrefixBitLength>
const BinASTSymbol* MultiLookupHuffmanTable<
    Subtable, PrefixBitLength>::Iterator::operator*() const {
  return &position_->value();
}

template <typename Subtable, uint8_t PrefixBitLength>
const BinASTSymbol* MultiLookupHuffmanTable<
    Subtable, PrefixBitLength>::Iterator::operator->() const {
  return &position_->value();
}

template <typename Subtable, uint8_t PrefixBitLength>
bool MultiLookupHuffmanTable<Subtable, PrefixBitLength>::Iterator::operator==(
    const Iterator& other) const {
  return position_ == other.position_;
}

template <typename Subtable, uint8_t PrefixBitLength>
bool MultiLookupHuffmanTable<Subtable, PrefixBitLength>::Iterator::operator!=(
    const Iterator& other) const {
  return position_ != other.position_;
}

template <typename Subtable, uint8_t PrefixBitLength>
JS::Result<Ok> MultiLookupHuffmanTable<Subtable, PrefixBitLength>::initStart(
    JSContext* cx, TemporaryStorage* tempStorage, size_t numberOfSymbols,
    uint8_t largestBitLength) {
  static_assert(PrefixBitLength < MAX_CODE_BIT_LENGTH,
                "Invalid PrefixBitLength");
  largestBitLength_ = largestBitLength;

  MOZ_TRY_VAR(values_, tempStorage->alloc<HuffmanEntry>(cx, numberOfSymbols));

  auto numTables = 1 << PrefixBitLength;
  MOZ_TRY_VAR(suffixTables_, tempStorage->alloc<Subtable>(cx, numTables));

  return Ok();
}

template <typename Subtable, uint8_t PrefixBitLength>
JS::Result<Ok> MultiLookupHuffmanTable<Subtable, PrefixBitLength>::addSymbol(
    size_t index, uint32_t bits, uint8_t bitLength, const BinASTSymbol& value) {
  MOZ_ASSERT_IF(largestBitLength_ != 0, bitLength != 0);
  MOZ_ASSERT(index == 0 || values_[index - 1].key().bitLength_ <= bitLength,
             "Symbols must be ranked by increasing bits length");
  MOZ_ASSERT_IF(bitLength != 32 /* >> 32 is UB */, bits >> bitLength == 0);

  new (mozilla::KnownNotNull, &values_[index])
      HuffmanEntry(bits, bitLength, value);

  return Ok();
}

template <typename Subtable, uint8_t PrefixBitLength>
JS::Result<Ok> MultiLookupHuffmanTable<Subtable, PrefixBitLength>::initComplete(
    JSContext* cx, TemporaryStorage* tempStorage) {
  // First, we need to collect the `largestBitLength_`
  // and `numberofSymbols` for each subtable.
  struct Bucket {
    Bucket() : largestBitLength_(0), numberOfSymbols_(0){};
    uint8_t largestBitLength_;
    uint32_t numberOfSymbols_;
    void addSymbol(uint8_t bitLength) {
      ++numberOfSymbols_;
      if (bitLength > largestBitLength_) {
        largestBitLength_ = bitLength;
      }
    }
  };
  FixedLengthVector<Bucket> buckets;
  if (!buckets.allocate(cx, 1 << PrefixBitLength)) {
    return cx->alreadyReportedError();
  }
  Bucket shortKeysBucket;

  for (const auto& entry : values_) {
    if (entry.key().bitLength_ <= SingleLookupHuffmanTable::MAX_BIT_LENGTH) {
      // If the key is short, we put it in `shortKeys_` instead of
      // `suffixTables`.
      shortKeysBucket.addSymbol(entry.key().bitLength_);
      continue;
    }
    const HuffmanLookup lookup(entry.key().bits_, entry.key().bitLength_);
    const auto split = lookup.split(PrefixBitLength);
    MOZ_ASSERT_IF(split.suffix_.bitLength_ != 32,
                  split.suffix_.bits_ >> split.suffix_.bitLength_ == 0);

    // Entries that have a sufficient number of bits will be dispatched
    // to a single subtable (e.g. A, B, C, D, E, F in the documentation).
    // Other entries need to be dispatched to several subtables
    // (e.g. G, H in the documentation).
    for (const auto index : lookup.suffixes(PrefixBitLength)) {
      Bucket& bucket = buckets[index];
      bucket.addSymbol(split.suffix_.bitLength_);
    }
  }

  FixedLengthVector<size_t> suffixTablesIndices;
  if (MOZ_UNLIKELY(!suffixTablesIndices.allocateUninitialized(
          cx, suffixTables_.size()))) {
    return cx->alreadyReportedError();
  }

  // We may now create the subtables.
  size_t i = 0;
  for (auto& bucket : buckets) {
    new (mozilla::KnownNotNull, &suffixTables_[i]) Subtable();
    suffixTablesIndices[i] = 0;

    if (bucket.numberOfSymbols_ != 0) {
      // Often, a subtable will end up empty because all the prefixes end up
      // in `shortKeys_`. In such a case, we want to avoid initializing the
      // table.
      MOZ_TRY(suffixTables_[i].initStart(
          cx, tempStorage,
          /* numberOfSymbols = */ bucket.numberOfSymbols_,
          /* maxBitLength = */ bucket.largestBitLength_));
    }

    i++;
  }

  // Also, create the shortKeys_ fast lookup.
  MOZ_TRY(shortKeys_.initStart(cx, tempStorage,
                               shortKeysBucket.numberOfSymbols_,
                               shortKeysBucket.largestBitLength_));

  // Now that all the subtables are created, let's dispatch the values
  // among these tables.
  size_t shortKeysIndex = 0;
  for (size_t i = 0; i < values_.size(); ++i) {
    const auto& entry = values_[i];
    if (entry.key().bitLength_ <= SingleLookupHuffmanTable::MAX_BIT_LENGTH) {
      // The key fits in `shortKeys_`, let's use this table.
      MOZ_TRY(shortKeys_.addSymbol(shortKeysIndex++, entry.key().bits_,
                                   entry.key().bitLength_,
                                   BinASTSymbol::fromSubtableIndex(i)));
      continue;
    }

    // Otherwise, use one of the suffix tables.
    const HuffmanLookup lookup(entry.key().bits_, entry.key().bitLength_);
    const auto split = lookup.split(PrefixBitLength);
    MOZ_ASSERT_IF(split.suffix_.bitLength_ != 32,
                  split.suffix_.bits_ >> split.suffix_.bitLength_ == 0);
    for (const auto index : lookup.suffixes(PrefixBitLength)) {
      auto& sub = suffixTables_[index];

      // We may now add a reference to `entry` into the sybtable.
      MOZ_TRY(sub.addSymbol(suffixTablesIndices[index]++, split.suffix_.bits_,
                            split.suffix_.bitLength_,
                            BinASTSymbol::fromSubtableIndex(i)));
    }
  }

  // Finally, complete initialization of shortKeys_ and subtables.
  MOZ_TRY(shortKeys_.initComplete(cx, tempStorage));
  for (size_t i = 0; i < buckets.length(); ++i) {
    if (buckets[i].numberOfSymbols_ == 0) {
      // Again, we don't want to initialize empty subtables.
      continue;
    }
    auto& sub = suffixTables_[i];
    MOZ_TRY(sub.initComplete(cx, tempStorage));
  }

  return Ok();
}

template <typename Subtable, uint8_t PrefixBitLength>
HuffmanLookupResult MultiLookupHuffmanTable<Subtable, PrefixBitLength>::lookup(
    HuffmanLookup key) const {
  {
    // Let's first look in shortkeys.
    auto subResult = shortKeys_.lookup(key);
    if (subResult.isFound()) {
      // We have found a result in the shortKeys_ fastpath.
      const auto& result = values_[subResult.value().toSubtableIndex()];

      return HuffmanLookupResult::found(result.key().bitLength_,
                                        &result.value());
    }
  }
  const auto split = key.split(PrefixBitLength);
  if (split.prefix_.bits_ >= suffixTables_.size()) {
    return HuffmanLookupResult::notFound();
  }
  const Subtable& subtable = suffixTables_[split.prefix_.bits_];

  auto subResult = subtable.lookup(split.suffix_);
  if (!subResult.isFound()) {
    // Propagate "not found".
    return HuffmanLookupResult::notFound();
  }

  // Otherwise, restore the entire `HuffmanEntry`.
  const auto& result = values_[subResult.value().toSubtableIndex()];

  return HuffmanLookupResult::found(result.key().bitLength_, &result.value());
}

// -----

// The number of possible interfaces in each sum, indexed by
// `static_cast<size_t>(BinASTSum)`.
const size_t SUM_LIMITS[]{
#define WITH_SUM(_ENUM_NAME, _HUMAN_NAME, MACRO_NAME, _TYPE_NAME) \
  BINAST_SUM_##MACRO_NAME##_LIMIT,
    FOR_EACH_BIN_SUM(WITH_SUM)
#undef WITH_SUM
};

// For each sum S, an array from [0, SUM_LIMITS[S]( to the BinASTKind of the ith
// interface of the sum, ranked by the same sum order as the rest of the .
//
// For instance, as we have
// ArrowExpression ::= EagerArrowExpressionWithExpression
//                 |   EagerArrowExpressionWithFunctionBody
//                 |   ...
//
// SUM_RESOLUTION_ARROW_EXPRESSION[0] ==
// BinASTKind::EagerArrowExpressionWithExpression
// SUM_RESOLUTION_ARROW_EXPRESSION[1] ==
// BinASTKind::EagerArrowExpressionWithFunctionBody
// ...
#define WITH_SUM_CONTENTS(_SUM_NAME, _INDEX, INTERFACE_NAME, _MACRO_NAME, \
                          _SPEC_NAME)                                     \
  BinASTKind::INTERFACE_NAME,
#define WITH_SUM(_ENUM_NAME, _HUMAN_NAME, MACRO_NAME, _TYPE_NAME) \
  const BinASTKind SUM_RESOLUTION_##MACRO_NAME[]{                 \
      FOR_EACH_BIN_INTERFACE_IN_SUM_##MACRO_NAME(WITH_SUM_CONTENTS)};
FOR_EACH_BIN_SUM(WITH_SUM)
#undef WITH_SUM
#undef WITH_SUM_CONTENTS

const BinASTKind* SUM_RESOLUTIONS[BINAST_NUMBER_OF_SUM_TYPES]{
#define WITH_SUM(_ENUM_NAME, _HUMAN_NAME, MACRO_NAME, _TYPE_NAME) \
  SUM_RESOLUTION_##MACRO_NAME,
    FOR_EACH_BIN_SUM(WITH_SUM)
#undef WITH_SUM
};

// The number of possible interfaces in each string enum, indexed by
// `static_cast<size_t>(BinASTStringEnum)`.
const size_t STRING_ENUM_LIMITS[]{
#define WITH_ENUM(name, _, MACRO_NAME) BIN_AST_STRING_ENUM_##MACRO_NAME##_LIMIT,
    FOR_EACH_BIN_STRING_ENUM(WITH_ENUM)
#undef WITH_ENUM
};

#define WITH_ENUM_CONTENTS(_ENUM_NAME, VARIANT_NAME, _HUMAN_NAME) \
  BinASTVariant::VARIANT_NAME,
#define WITH_ENUM(_ENUM_NAME, _, MACRO_NAME)                              \
  const BinASTVariant STRING_ENUM_RESOLUTION_##MACRO_NAME[]{              \
      FOR_EACH_BIN_VARIANT_IN_STRING_ENUM_##MACRO_NAME##_BY_WEBIDL_ORDER( \
          WITH_ENUM_CONTENTS)};
FOR_EACH_BIN_STRING_ENUM(WITH_ENUM)
#undef WITH_ENUM
#undef WITH_ENUM_CONTENTS

const BinASTVariant* STRING_ENUM_RESOLUTIONS[BINASTSTRINGENUM_LIMIT]{
#define WITH_ENUM(name, _, MACRO_NAME) STRING_ENUM_RESOLUTION_##MACRO_NAME,
    FOR_EACH_BIN_STRING_ENUM(WITH_ENUM)
#undef WITH_ENUM
};

// Start reading the prelude.
MOZ_MUST_USE JS::Result<HuffmanDictionaryForMetadata*>
HuffmanPreludeReader::run(size_t initialCapacity) {
  BINJS_TRY(stack_.reserve(initialCapacity));

  dictionary_.reset(cx_->new_<HuffmanDictionary>());
  BINJS_TRY(dictionary_);

  // For the moment, the root node is hardcoded to be a BinASTKind::Script.
  // In future versions of the codec, we'll extend the format to handle
  // other possible roots (e.g. BinASTKind::Module).
  MOZ_TRY(pushFields(BinASTKind::Script));
  while (stack_.length() > 0) {
    const Entry entry = stack_.popCopy();
    MOZ_TRY(entry.match(ReadPoppedEntryMatcher(*this)));
  }

  auto dictForMetadata = HuffmanDictionaryForMetadata::createFrom(
      dictionary_.get(), &tempStorage_);
  if (!dictForMetadata) {
    ReportOutOfMemory(cx_);
    return cx_->alreadyReportedError();
  }

  return dictForMetadata;
}

// ------ Reading booleans.
// 0 -> False
// 1 -> True

// Extract the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const Boolean&) {
  // Sadly, there are only two booleans known to this date.
  return 2;
}

// Extract symbol from the grammar.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const Boolean&, size_t index) {
  MOZ_ASSERT(index < 2);
  return BinASTSymbol::fromBool(index != 0);
}

// Reading a single-value table of booleans
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<Boolean>(
    GenericHuffmanTable& table, const Boolean& entry) {
  uint8_t indexByte;
  MOZ_TRY_VAR(indexByte, reader_.readByte<Compression::No>());
  if (MOZ_UNLIKELY(indexByte >= 2)) {
    return raiseInvalidTableData(entry.identity_);
  }

  MOZ_TRY(
      table.initWithSingleValue(cx_, BinASTSymbol::fromBool(indexByte != 0)));
  return Ok();
}

// ------ Optional interfaces.
// 0 -> Null
// 1 -> NonNull

// Extract the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const MaybeInterface&) {
  // Null, NonNull
  return 2;
}

// Extract symbol from the grammar.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const MaybeInterface& entry, size_t index) {
  MOZ_ASSERT(index < 2);
  return BinASTSymbol::fromKind(index == 0 ? BinASTKind::_Null : entry.kind_);
}

// Reading a single-value table of optional interfaces
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeInterface>(
    GenericHuffmanTable& table, const MaybeInterface& entry) {
  uint8_t indexByte;
  MOZ_TRY_VAR(indexByte, reader_.readByte<Compression::No>());
  if (MOZ_UNLIKELY(indexByte >= 2)) {
    return raiseInvalidTableData(entry.identity_);
  }

  MOZ_TRY(table.initWithSingleValue(
      cx_, BinASTSymbol::fromKind(indexByte == 0 ? BinASTKind::_Null
                                                 : entry.kind_)));
  return Ok();
}

// ------ Sums of interfaces
// varnum i -> index `i` in the order defined by
// `FOR_EACH_BIN_INTERFACE_IN_SUM_*`

// Extract the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const Sum& sum) {
  return sum.maxNumberOfSymbols();
}

// Extract symbol from the grammar.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const Sum& entry, size_t index) {
  MOZ_ASSERT(index < entry.maxNumberOfSymbols());
  return BinASTSymbol::fromKind(entry.interfaceAt(index));
}

// Reading a single-value table of sums of interfaces.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<Sum>(
    GenericHuffmanTable& table, const Sum& sum) {
  BINJS_MOZ_TRY_DECL(index, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index >= sum.maxNumberOfSymbols())) {
    return raiseInvalidTableData(sum.identity_);
  }

  MOZ_TRY(table.initWithSingleValue(
      cx_, BinASTSymbol::fromKind(sum.interfaceAt(index))));
  return Ok();
}

// ------ Optional sums of interfaces
// varnum 0 -> null
// varnum i > 0 -> index `i - 1` in the order defined by
// `FOR_EACH_BIN_INTERFACE_IN_SUM_*`

// Extract the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const MaybeSum& sum) {
  return sum.maxNumberOfSymbols();
}

// Extract symbol from the grammar.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const MaybeSum& sum, size_t index) {
  MOZ_ASSERT(index < sum.maxNumberOfSymbols());
  return BinASTSymbol::fromKind(sum.interfaceAt(index));
}

// Reading a single-value table of sums of interfaces.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeSum>(GenericHuffmanTable& table,
                                                     const MaybeSum& sum) {
  BINJS_MOZ_TRY_DECL(index, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index >= sum.maxNumberOfSymbols())) {
    return raiseInvalidTableData(sum.identity_);
  }

  MOZ_TRY(table.initWithSingleValue(
      cx_, BinASTSymbol::fromKind(sum.interfaceAt(index))));
  return Ok();
}

// ------ Numbers
// 64 bits, IEEE 754, big endian

// Read the number of symbols from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const Number& number) {
  BINJS_MOZ_TRY_DECL(length, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS)) {
    return raiseInvalidTableData(number.identity_);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const Number& number, size_t) {
  uint8_t bytes[8];
  MOZ_ASSERT(sizeof(bytes) == sizeof(double));

  uint32_t len = mozilla::ArrayLength(bytes);
  MOZ_TRY((reader_.readBuf<Compression::No, EndOfFilePolicy::RaiseError>(
      reinterpret_cast<uint8_t*>(bytes), len)));

  // Decode big-endian.
  const uint64_t asInt = mozilla::BigEndian::readUint64(bytes);

  // Canonicalize NaN, just to make sure another form of signalling NaN
  // doesn't slip past us.
  return BinASTSymbol::fromDouble(
      JS::CanonicalizeNaN(mozilla::BitwiseCast<double>(asInt)));
}

// Reading a single-value table of numbers.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<Number>(
    GenericHuffmanTable& table, const Number& number) {
  BINJS_MOZ_TRY_DECL(value, readSymbol(number, 0 /* ignored */));
  MOZ_TRY(table.initWithSingleValue(cx_, value));
  return Ok();
}

// ------ List lengths
// varnum

// Read the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const List& list) {
  BINJS_MOZ_TRY_DECL(length, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS)) {
    return raiseInvalidTableData(list.identity_);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const List& list, size_t) {
  BINJS_MOZ_TRY_DECL(length, reader_.readUnpackedLong());
  if (MOZ_UNLIKELY(length > MAX_LIST_LENGTH)) {
    return raiseInvalidTableData(list.identity_);
  }
  return BinASTSymbol::fromListLength(length);
}

// Reading a single-value table of list lengths.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<List>(
    GenericHuffmanTable& table, const List& list) {
  BINJS_MOZ_TRY_DECL(length, reader_.readUnpackedLong());
  if (MOZ_UNLIKELY(length > MAX_LIST_LENGTH)) {
    return raiseInvalidTableData(list.identity_);
  }
  MOZ_TRY(table.initWithSingleValue(cx_, BinASTSymbol::fromListLength(length)));
  return Ok();
}

// ------ Strings, non-nullable
// varnum (index)

// Read the number of symbols from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const String& string) {
  BINJS_MOZ_TRY_DECL(length, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS ||
                   length > reader_.metadata_->numStrings())) {
    return raiseInvalidTableData(string.identity_);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const String& entry, size_t) {
  BINJS_MOZ_TRY_DECL(index, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > reader_.metadata_->numStrings())) {
    return raiseInvalidTableData(entry.identity_);
  }
  return BinASTSymbol::fromAtomIndex(index);
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<String>(
    GenericHuffmanTable& table, const String& entry) {
  BINJS_MOZ_TRY_DECL(index, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > reader_.metadata_->numStrings())) {
    return raiseInvalidTableData(entry.identity_);
  }
  MOZ_TRY(table.initWithSingleValue(cx_, BinASTSymbol::fromAtomIndex(index)));
  return Ok();
}

// ------ Optional strings
// varnum 0 -> null
// varnum i > 0 -> string at index i - 1

// Read the number of symbols from the metadata.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const MaybeString& entry) {
  BINJS_MOZ_TRY_DECL(length, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS ||
                   length > reader_.metadata_->numStrings() + 1)) {
    return raiseInvalidTableData(entry.identity_);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const MaybeString& entry, size_t) {
  BINJS_MOZ_TRY_DECL(index, reader_.readVarU32<Compression::No>());
  // (index == 0) is specified as `null` value and
  // (index > 0) maps to (index - 1)-th atom.
  if (index == 0) {
    return BinASTSymbol::nullAtom();
  }
  if (MOZ_UNLIKELY(index > reader_.metadata_->numStrings() + 1)) {
    return raiseInvalidTableData(entry.identity_);
  }
  return BinASTSymbol::fromAtomIndex(index - 1);
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeString>(
    GenericHuffmanTable& table, const MaybeString& entry) {
  BINJS_MOZ_TRY_DECL(index, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > reader_.metadata_->numStrings() + 1)) {
    return raiseInvalidTableData(entry.identity_);
  }
  // (index == 0) is specified as `null` value and
  // (index > 0) maps to (index - 1)-th atom.
  if (index == 0) {
    MOZ_TRY(table.initWithSingleValue(cx_, BinASTSymbol::nullAtom()));
  } else {
    MOZ_TRY(
        table.initWithSingleValue(cx_, BinASTSymbol::fromAtomIndex(index - 1)));
  }
  return Ok();
}

// ------ String Enums
// varnum index in the enum

// Read the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const StringEnum& entry) {
  return entry.maxNumberOfSymbols();
}

// Read a single symbol from the grammar.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const StringEnum& entry, size_t index) {
  return BinASTSymbol::fromVariant(entry.variantAt(index));
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<StringEnum>(
    GenericHuffmanTable& table, const StringEnum& entry) {
  BINJS_MOZ_TRY_DECL(index, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > entry.maxNumberOfSymbols())) {
    return raiseInvalidTableData(entry.identity_);
  }

  BinASTVariant symbol = entry.variantAt(index);
  MOZ_TRY(table.initWithSingleValue(cx_, BinASTSymbol::fromVariant(symbol)));
  return Ok();
}

// ------ Unsigned Longs
// Unpacked 32-bit

// Read the number of symbols from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const UnsignedLong& entry) {
  BINJS_MOZ_TRY_DECL(length, reader_.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS)) {
    return raiseInvalidTableData(entry.identity_);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<BinASTSymbol> HuffmanPreludeReader::readSymbol(
    const UnsignedLong& entry, size_t) {
  BINJS_MOZ_TRY_DECL(result, reader_.readUnpackedLong());
  return BinASTSymbol::fromUnsignedLong(result);
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<UnsignedLong>(
    GenericHuffmanTable& table, const UnsignedLong& entry) {
  BINJS_MOZ_TRY_DECL(index, reader_.readUnpackedLong());
  MOZ_TRY(
      table.initWithSingleValue(cx_, BinASTSymbol::fromUnsignedLong(index)));
  return Ok();
}

HuffmanDictionaryForMetadata::~HuffmanDictionaryForMetadata() {
  // WARNING: If you change the code of this destructor,
  //          `clearFromIncompleteInitialization` needs to be synchronized.
  for (size_t i = 0; i < numTables_; i++) {
    tablesBase()[i].~GenericHuffmanTable();
  }
  for (size_t i = 0; i < numSingleTables_; i++) {
    singleTablesBase()[i].~SingleLookupHuffmanTable();
  }
  for (size_t i = 0; i < numTwoTables_; i++) {
    twoTablesBase()[i].~TwoLookupsHuffmanTable();
  }
}

/* static */
HuffmanDictionaryForMetadata* HuffmanDictionaryForMetadata::createFrom(
    HuffmanDictionary* dictionary, TemporaryStorage* tempStorage) {
  size_t numTables = dictionary->numTables();
  size_t numHuffmanEntries = tempStorage->numHuffmanEntries();
  size_t numInternalIndices = tempStorage->numInternalIndices();
  while (numInternalIndices * sizeof(InternalIndex) % sizeof(uintptr_t) != 0) {
    numInternalIndices++;
  }
  size_t numSingleTables = tempStorage->numSingleTables();
  size_t numTwoTables = tempStorage->numTwoTables();

  HuffmanDictionaryForMetadata* data =
      static_cast<HuffmanDictionaryForMetadata*>(
          js_malloc(totalSize(numTables, numHuffmanEntries, numInternalIndices,
                              numSingleTables, numTwoTables)));
  if (MOZ_UNLIKELY(!data)) {
    return nullptr;
  }

  new (mozilla::KnownNotNull, data) HuffmanDictionaryForMetadata(
      numTables, numHuffmanEntries, numInternalIndices, numSingleTables,
      numTwoTables);

  data->moveFrom(dictionary, tempStorage);
  return data;
}

/* static */
HuffmanDictionaryForMetadata* HuffmanDictionaryForMetadata::create(
    size_t numTables, size_t numHuffmanEntries, size_t numInternalIndices,
    size_t numSingleTables, size_t numTwoTables) {
  HuffmanDictionaryForMetadata* data =
      static_cast<HuffmanDictionaryForMetadata*>(
          js_malloc(totalSize(numTables, numHuffmanEntries, numInternalIndices,
                              numSingleTables, numTwoTables)));
  if (MOZ_UNLIKELY(!data)) {
    return nullptr;
  }

  new (mozilla::KnownNotNull, data) HuffmanDictionaryForMetadata(
      numTables, numHuffmanEntries, numInternalIndices, numSingleTables,
      numTwoTables);

  return data;
}

void HuffmanDictionaryForMetadata::clearFromIncompleteInitialization(
    size_t numInitializedTables, size_t numInitializedSingleTables,
    size_t numInitializedTwoTables) {
  // This is supposed to be called from AutoClearHuffmanDictionaryForMetadata.
  // See AutoClearHuffmanDictionaryForMetadata class comment for more details.

  // Call destructors for already-initialized tables.
  for (size_t i = 0; i < numInitializedTables; i++) {
    tablesBase()[i].~GenericHuffmanTable();
  }
  for (size_t i = 0; i < numInitializedSingleTables; i++) {
    singleTablesBase()[i].~SingleLookupHuffmanTable();
  }
  for (size_t i = 0; i < numInitializedTwoTables; i++) {
    twoTablesBase()[i].~TwoLookupsHuffmanTable();
  }

  // Set the following fields to 0 so that destructor doesn't call tables'
  // destructor.
  numTables_ = 0;
  numSingleTables_ = 0;
  numTwoTables_ = 0;
}

void HuffmanDictionaryForMetadata::moveFrom(HuffmanDictionary* dictionary,
                                            TemporaryStorage* tempStorage) {
  // Move tableIndices_ from HuffmanDictionary to the payload of
  // HuffmanDictionaryForMetadata.
  for (size_t i = 0; i < TableIdentity::Limit; i++) {
    // HuffmanDictionaryForMetadata.tableIndices_ is initialized to
    // UnreachableIndex, and we don't have to move if
    // dictionary->status_[i] == HuffmanDictionary::TableStatus::Unreachable.
    if (dictionary->status_[i] == HuffmanDictionary::TableStatus::Ready) {
      tableIndices_[i] = dictionary->tableIndices_[i];
    }
  }

  // Fill items of each array from the beginning.
  auto tablePtr = tablesBase();
  auto huffmanEntryPtr = huffmanEntriesBase();
  auto internalIndexPtr = internalIndicesBase();
  auto singleTablePtr = singleTablesBase();
  auto twoTablePtr = twoTablesBase();

  // Move the content of SingleLookupHuffmanTable from
  // TemporaryStorage to the payload of HuffmanDictionaryForMetadata.
  //
  // SingleLookupHuffmanTable itself should already be moved to
  // HuffmanDictionaryForMetadata.
  auto moveSingleTableContent =
      [&huffmanEntryPtr, &internalIndexPtr](SingleLookupHuffmanTable& table) {
        // table.{values_,saturated_} points the spans in TemporaryStorage.
        // Move those items to the payload and then update
        // table.{values_,saturated_} to point that range.

        {
          size_t size = table.values_.size();
          memmove(huffmanEntryPtr.get(), table.values_.data(),
                  sizeof(HuffmanEntry) * size);
          table.values_ = mozilla::MakeSpan(huffmanEntryPtr.get(), size);
          huffmanEntryPtr += size;
        }

        {
          size_t size = table.saturated_.size();
          memmove(internalIndexPtr.get(), table.saturated_.data(),
                  sizeof(InternalIndex) * size);
          table.saturated_ = mozilla::MakeSpan(internalIndexPtr.get(), size);
          internalIndexPtr += size;
        }
      };

  // Move the content of TwoLookupsHuffmanTable from
  // TemporaryStorage to the payload of HuffmanDictionaryForMetadata.
  auto moveTwoTableContent =
      [&huffmanEntryPtr, &singleTablePtr,
       moveSingleTableContent](TwoLookupsHuffmanTable& table) {
        // table.shortKeys_ instance itself is already moved.
        // Move the contents to the payload.
        moveSingleTableContent(table.shortKeys_);

        // table.{values_,suffixTables_} points the spans in TemporaryStorage.
        // Move those items to the payload and then update
        // table.{values_,suffixTables_} to point that range.
        // Also recursively move the content of suffixTables_.

        {
          size_t size = table.values_.size();
          memmove(huffmanEntryPtr.get(), table.values_.data(),
                  sizeof(HuffmanEntry) * size);
          table.values_ = mozilla::MakeSpan(huffmanEntryPtr.get(), size);
          huffmanEntryPtr += size;
        }

        {
          size_t size = table.suffixTables_.size();
          auto head = singleTablePtr.get();
          for (auto& fromSubTable : table.suffixTables_) {
            memmove(singleTablePtr.get(), &fromSubTable,
                    sizeof(SingleLookupHuffmanTable));
            auto& toSubTable = *singleTablePtr;
            singleTablePtr++;

            moveSingleTableContent(toSubTable);
          }
          table.suffixTables_ = mozilla::MakeSpan(head, size);
        }
      };

  // Move the content of ThreeLookupsHuffmanTable from
  // TemporaryStorage to the payload of HuffmanDictionaryForMetadata.
  auto moveThreeTableContent =
      [&huffmanEntryPtr, &twoTablePtr, moveSingleTableContent,
       moveTwoTableContent](ThreeLookupsHuffmanTable& table) {
        // table.shortKeys_ instance itself is already moved.
        // Move the contents to the payload.
        moveSingleTableContent(table.shortKeys_);

        // table.{values_,suffixTables_} points the spans in TemporaryStorage.
        // Move those items to the payload and then update
        // table.{values_,suffixTables_} to point that range.
        // Also recursively move the content of suffixTables_.

        {
          size_t size = table.values_.size();
          memmove(huffmanEntryPtr.get(), table.values_.data(),
                  sizeof(HuffmanEntry) * size);
          table.values_ = mozilla::MakeSpan(huffmanEntryPtr.get(), size);
          huffmanEntryPtr += size;
        }

        {
          size_t size = table.suffixTables_.size();
          auto head = twoTablePtr.get();
          for (auto& fromSubTable : table.suffixTables_) {
            memmove(twoTablePtr.get(), &fromSubTable,
                    sizeof(TwoLookupsHuffmanTable));
            auto& toSubTable = *twoTablePtr;
            twoTablePtr++;

            moveTwoTableContent(toSubTable);
          }
          table.suffixTables_ = mozilla::MakeSpan(head, size);
        }
      };

  // Move tables from HuffmanDictionary to the payload of
  // HuffmanDictionaryForMetadata, and then move contents of those tables
  // to the payload of HuffmanDictionaryForMetadata.
  for (size_t i = 0; i < numTables_; i++) {
    auto& fromTable = dictionary->tableAtIndex(i);

    if (fromTable.implementation_.is<SingleEntryHuffmanTable>() ||
        fromTable.implementation_.is<TwoEntriesHuffmanTable>()) {
      memmove(tablePtr.get(), &fromTable, sizeof(GenericHuffmanTable));
      tablePtr++;
    } else if (fromTable.implementation_.is<SingleLookupHuffmanTable>()) {
      memmove(tablePtr.get(), &fromTable, sizeof(GenericHuffmanTable));
      auto& specialized =
          tablePtr->implementation_.as<SingleLookupHuffmanTable>();
      tablePtr++;

      moveSingleTableContent(specialized);
    } else if (fromTable.implementation_.is<TwoLookupsHuffmanTable>()) {
      memmove(tablePtr.get(), &fromTable, sizeof(GenericHuffmanTable));
      auto& specialized =
          tablePtr->implementation_.as<TwoLookupsHuffmanTable>();
      tablePtr++;

      moveTwoTableContent(specialized);
    } else {
      MOZ_ASSERT(fromTable.implementation_.is<ThreeLookupsHuffmanTable>());

      memmove(tablePtr.get(), &fromTable, sizeof(GenericHuffmanTable));
      auto& specialized =
          tablePtr->implementation_.as<ThreeLookupsHuffmanTable>();
      tablePtr++;

      moveThreeTableContent(specialized);
    }
  }
}

/* static */
size_t HuffmanDictionaryForMetadata::totalSize(size_t numTables,
                                               size_t numHuffmanEntries,
                                               size_t numInternalIndices,
                                               size_t numSingleTables,
                                               size_t numTwoTables) {
  static_assert(alignof(GenericHuffmanTable) % sizeof(uintptr_t) == 0,
                "should be aligned to pointer size");
  static_assert(alignof(HuffmanEntry) % sizeof(uintptr_t) == 0,
                "should be aligned to pointer size");
  static_assert(alignof(SingleLookupHuffmanTable) % sizeof(uintptr_t) == 0,
                "should be aligned to pointer size");
  static_assert(alignof(TwoLookupsHuffmanTable) % sizeof(uintptr_t) == 0,
                "should be aligned to pointer size");

  // InternalIndex is not guaranteed to be aligned to pointer size.
  // Make sure `numInternalIndices` meets the requirement that
  // the entire block size is aligned to pointer size.
  MOZ_ASSERT(numInternalIndices * sizeof(InternalIndex) % sizeof(uintptr_t) ==
             0);

  return sizeof(HuffmanDictionaryForMetadata) +
         numTables * sizeof(GenericHuffmanTable) +
         numHuffmanEntries * sizeof(HuffmanEntry) +
         numInternalIndices * sizeof(InternalIndex) +
         numSingleTables * sizeof(SingleLookupHuffmanTable) +
         numTwoTables * sizeof(TwoLookupsHuffmanTable);
}

HuffmanDictionary::~HuffmanDictionary() {
  for (size_t i = 0; i < nextIndex_; i++) {
    tableAtIndex(i).~GenericHuffmanTable();
  }
}

uint32_t HuffmanLookup::leadingBits(const uint8_t aBitLength) const {
  MOZ_ASSERT(aBitLength <= bitLength_);
  const uint32_t result = (aBitLength == 0)
                              ? 0  // Shifting a uint32_t by 32 bits is UB.
                              : bits_ >> uint32_t(bitLength_ - aBitLength);
  return result;
}

Split<HuffmanLookup> HuffmanLookup::split(const uint8_t prefixLength) const {
  if (bitLength_ <= prefixLength) {
    // Not enough bits, pad with zeros.
    return {
        /* prefix: HuffmanLookup */ {bits_ << (prefixLength - bitLength_),
                                     prefixLength},
        /* suffix: HuffmanLookup */ {0, 0},
    };
  }

  // Keep `prefixLength` bits from `bits`.
  // Pad the rest with 0s to build the suffix.
  const uint8_t shift = bitLength_ - prefixLength;
  switch (shift) {
    case 0:  // Special case, as we can't >> 32
      return {
          /* prefix: HuffmanLookup */ {bits_, prefixLength},
          /* suffix: HuffmanLookup */ {0, 0},
      };
    case 32:  // Special case, as we can't >> 32
      return {
          /* prefix: HuffmanLookup */ {0, prefixLength},
          /* suffix: HuffmanLookup */ {bits_, shift},
      };
  }
  return {
      /* prefix: HuffmanLookup */ {bits_ >> shift, prefixLength},
      /* suffix: HuffmanLookup */
      {bits_ & (std::numeric_limits<uint32_t>::max() >> (32 - shift)), shift},
  };
}

mozilla::detail::IntegerRange<size_t> HuffmanLookup::suffixes(
    uint8_t expectedBitLength) const {
  if (expectedBitLength <= bitLength_) {
    // We have too many bits, we need to truncate the HuffmanLookup,
    // then return a single element.
    const uint8_t shearing = bitLength_ - expectedBitLength;
    const size_t first = size_t(bits_) >> shearing;
    const size_t last = first;
    return mozilla::IntegerRange<size_t>(first, last + 1);
  }

  // We need to pad with lower-weight 0s.
  const uint8_t padding = expectedBitLength - bitLength_;
  const size_t first = bits_ << padding;
  const size_t last = first + (size_t(-1) >> (8 * sizeof(size_t) - padding));
  return mozilla::IntegerRange<size_t>(first, last + 1);
}

}  // namespace js::frontend
