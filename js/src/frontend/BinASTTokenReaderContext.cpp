/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTTokenReaderContext.h"

#include "mozilla/IntegerTypeTraits.h"  // mozilla::MaxValue
#include "mozilla/Result.h"             // MOZ_TRY*
#include "mozilla/ScopeExit.h"          // mozilla::MakeScopeExit

#include <string.h>  // memchr, memcmp, memmove

#include "ds/Sort.h"                 // MergeSort
#include "frontend/BinAST-macros.h"  // BINJS_TRY*, BINJS_MOZ_TRY*
#include "gc/Rooting.h"              // RootedAtom
#include "js/AllocPolicy.h"          // SystemAllocPolicy
#include "js/StableStringChars.h"    // Latin1Char
#include "js/Utility.h"              // js_free
#include "js/Vector.h"               // js::Vector
#include "util/StringBuffer.h"       // StringBuffer
#include "vm/JSAtom.h"               // AtomizeWTF8Chars
#include "vm/JSScript.h"             // ScriptSource

namespace js {
namespace frontend {

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
  //
  // `dictionary` is the dictionary we're currently constructing.
  // It MUST be empty.
  HuffmanPreludeReader(JSContext* cx, BinASTTokenReaderContext& reader,
                       HuffmanDictionary& dictionary)
      : cx_(cx),
        reader(reader),
        dictionary(dictionary),
        stack(cx_),
        auxStorageLength(cx_) {}

  // Start reading the prelude.
  MOZ_MUST_USE JS::Result<Ok> run(size_t initialCapacity);

 private:
  JSContext* cx_;
  BinASTTokenReaderContext& reader;

  // The dictionary we're currently constructing.
  HuffmanDictionary& dictionary;

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
    const NormalizedInterfaceAndField identity;

    explicit EntryBase(const NormalizedInterfaceAndField identity)
        : identity(identity) {}
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
    using SymbolType = JSAtom*;
    using Table = HuffmanTableIndexedSymbolsLiteralString;
    explicit String(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };
  using IdentifierName = String;
  using PropertyKey = String;

  // An optional string.
  // May be a literal string, identifier name or property key.
  struct MaybeString : EntryExplicit {
    using SymbolType = JSAtom*;
    using Table = HuffmanTableIndexedSymbolsOptionalLiteralString;
    explicit MaybeString(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };
  using MaybeIdentifierName = MaybeString;
  using MaybePropertyKey = MaybeString;

  // A JavaScript number. May not be null.
  struct Number : EntryExplicit {
    using SymbolType = double;
    using Table = HuffmanTableExplicitSymbolsF64;
    explicit Number(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };

  // A 32-bit integer. May not be null.
  struct UnsignedLong : EntryExplicit {
    using SymbolType = uint32_t;
    using Table = HuffmanTableExplicitSymbolsU32;
    explicit UnsignedLong(const NormalizedInterfaceAndField identity)
        : EntryExplicit(identity) {}
  };

  // A boolean. May not be null.
  struct Boolean : EntryIndexed {
    using SymbolType = bool;
    using Table = HuffmanTableIndexedSymbolsBool;

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
    using SymbolType = BinASTKind;

    // The kind of the interface.
    const BinASTKind kind;
    Interface(const NormalizedInterfaceAndField identity, BinASTKind kind)
        : EntryIndexed(identity), kind(kind) {}

    // Utility struct, used in macros to call the constructor as
    // `Interface::Maker(kind)(identity)`.
    struct Maker {
      const BinASTKind kind;
      explicit Maker(BinASTKind kind) : kind(kind) {}
      Interface operator()(const NormalizedInterfaceAndField identity) {
        return Interface(identity, kind);
      }
    };
  };

  // An optional value of a given interface.
  struct MaybeInterface : EntryIndexed {
    using SymbolType = BinASTKind;
    using Table = HuffmanTableIndexedSymbolsMaybeInterface;
    // The kind of the interface.
    const BinASTKind kind;

    // Comparing optional interfaces.
    //
    // As 0 == Null < _ == 1, we only compare indices.
    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      MOZ_ASSERT(aIndex <= 1);
      MOZ_ASSERT(bIndex <= 1);
      return aIndex < bIndex;
    }

    MaybeInterface(const NormalizedInterfaceAndField identity, BinASTKind kind)
        : EntryIndexed(identity), kind(kind) {}

    // The max number of symbols in a table for this type.
    size_t maxMumberOfSymbols() const { return 2; }

    // Utility struct, used in macros to call the constructor as
    // `MaybeInterface::Maker(kind)(identity)`.
    struct Maker {
      const BinASTKind kind;
      explicit Maker(BinASTKind kind) : kind(kind) {}
      MaybeInterface operator()(const NormalizedInterfaceAndField identity) {
        return MaybeInterface(identity, kind);
      }
    };
  };

  // A FrozenArray. May not be null.
  //
  // In practice, this type is used to represent the length of the list.
  // Once we have read the model for the length of the list, we push a
  // `ListContents` to read the model for the contents of the list.
  struct List : EntryExplicit {
    // The symbol type for the length of the list.
    using SymbolType = uint32_t;

    // The table for the length of the list.
    using Table = HuffmanTableExplicitSymbolsListLength;

    // The type of the list, e.g. list of numbers.
    // All lists with the same type share a model for their length.
    const BinASTList contents;

    List(const NormalizedInterfaceAndField identity, const BinASTList contents)
        : EntryExplicit(identity), contents(contents) {}

    // Utility struct, used in macros to call the constructor as
    // `List::Maker(kind)(identity)`.
    struct Maker {
      const BinASTList contents;
      explicit Maker(BinASTList contents) : contents(contents) {}
      List operator()(const NormalizedInterfaceAndField identity) {
        return {identity, contents};
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
    using SymbolType = BinASTKind;
    // The type of table used for this entry.
    using Table = HuffmanTableIndexedSymbolsSum;

    // The type of values in the sum.
    const BinASTSum contents;

    // Comparing sum entries alphabetically.
    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      MOZ_ASSERT(aIndex <= maxNumberOfSymbols());
      MOZ_ASSERT(bIndex <= maxNumberOfSymbols());
      const size_t aKey = getBinASTKindSortKey(interfaceAt(aIndex));
      const size_t bKey = getBinASTKindSortKey(interfaceAt(bIndex));
      return aKey < bKey;
    }

    Sum(const NormalizedInterfaceAndField identity, const BinASTSum contents)
        : EntryIndexed(identity), contents(contents) {}

    size_t maxNumberOfSymbols() const {
      return SUM_LIMITS[static_cast<size_t>(contents)];
    }

    BinASTKind interfaceAt(size_t index) const {
      MOZ_ASSERT(index < maxNumberOfSymbols());
      return SUM_RESOLUTIONS[static_cast<size_t>(contents)][index];
    }

    // Utility struct, used in macros to call the constructor as
    // `Sum::Maker(kind)(identity)`.
    struct Maker {
      const BinASTSum contents;
      explicit Maker(BinASTSum contents) : contents(contents) {}
      Sum operator()(const NormalizedInterfaceAndField identity) {
        return Sum(identity, contents);
      }
    };
  };

  // An optional choice between several interfaces.
  struct MaybeSum : EntryIndexed {
    // The type of symbols for this entry.
    // We use `BinASTKind::_Null` to represent the null case.
    using SymbolType = BinASTKind;
    // The type of table used for this entry.
    // We use `BinASTKind::_Null` to represent the null case.
    using Table = HuffmanTableIndexedSymbolsSum;

    // The type of values in the sum.
    const BinASTSum contents;

    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      return aIndex < bIndex;
    }

    MaybeSum(const NormalizedInterfaceAndField identity,
             const BinASTSum contents)
        : EntryIndexed(identity), contents(contents) {}

    size_t maxNumberOfSymbols() const {
      return SUM_LIMITS[static_cast<size_t>(contents)] + 1;
    }

    BinASTKind interfaceAt(size_t index) const {
      MOZ_ASSERT(index < maxNumberOfSymbols());
      if (index == 0) {
        return BinASTKind::_Null;
      }
      return SUM_RESOLUTIONS[static_cast<size_t>(contents)][index - 1];
    }

    // Utility struct, used in macros to call the constructor as
    // `MaybeSum::Maker(kind)(identity)`.
    struct Maker {
      const BinASTSum contents;
      explicit Maker(BinASTSum contents) : contents(contents) {}
      MaybeSum operator()(const NormalizedInterfaceAndField identity) {
        return MaybeSum(identity, contents);
      }
    };
  };

  // A choice between several strings. May not be null.
  struct StringEnum : EntryIndexed {
    using SymbolType = BinASTVariant;
    using Table = HuffmanTableIndexedSymbolsStringEnum;

    // Comparing string enums alphabetically.
    inline bool lessThan(uint32_t aIndex, uint32_t bIndex) {
      MOZ_ASSERT(aIndex <= maxNumberOfSymbols());
      MOZ_ASSERT(bIndex <= maxNumberOfSymbols());
      const size_t aKey = getBinASTVariantSortKey(variantAt(aIndex));
      const size_t bKey = getBinASTVariantSortKey(variantAt(bIndex));
      return aKey < bKey;
    }

    // The values in the enum.
    const BinASTStringEnum contents;
    StringEnum(const NormalizedInterfaceAndField identity,
               const BinASTStringEnum contents)
        : EntryIndexed(identity), contents(contents) {}

    size_t maxNumberOfSymbols() const {
      return STRING_ENUM_LIMITS[static_cast<size_t>(contents)];
    }

    BinASTVariant variantAt(size_t index) const {
      MOZ_ASSERT(index < maxNumberOfSymbols());
      return STRING_ENUM_RESOLUTIONS[static_cast<size_t>(contents)][index];
    }

    // Utility struct, used in macros to call the constructor as
    // `StringEnum::Maker(kind)(identity)`.
    struct Maker {
      const BinASTStringEnum contents;
      explicit Maker(BinASTStringEnum contents) : contents(contents) {}
      StringEnum operator()(const NormalizedInterfaceAndField identity) {
        return StringEnum(identity, contents);
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
              describeBinASTInterfaceAndField(entry.identity.identity));
    }
  };
#endif  // DEBUG

 private:
  // A stack of entries to process. We always process the latest entry added.
  Vector<Entry> stack;

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
    auto& table = dictionary.tableForListLength(list.contents);
    if (table.is<HuffmanTableUnreachable>()) {
      // Spec:
      // 2. Add the field to the set of visited fields
      table = {mozilla::VariantType<HuffmanTableInitializing>{}};

      // Read the lengths immediately.
      MOZ_TRY((readTable<HuffmanTableListLength, List>(table, list)));
    }

    // Spec:
    // 3. If the field has a FrozenArray type
    //   a. Determine if the array type is always empty
    //   b. If so, stop
    auto& lengthTable = table.as<HuffmanTableExplicitSymbolsListLength>();
    bool empty = true;
    for (auto iter : lengthTable) {
      if (*iter > 0) {
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
    switch (list.contents) {
#define WRAP_LIST_2(_, CONTENT) CONTENT
#define EMIT_CASE(LIST_NAME, _CONTENT_TYPE, _HUMAN_NAME, TYPE) \
  case BinASTList::LIST_NAME:                                  \
    return pushValue(list.identity, TYPE(list.identity));

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
    // Note: In this case, for compatibility, we do *not* check whether
    // the interface has already been visited.
    auto& table = dictionary.tableForField(identity);
    if (table.is<HuffmanTableUnreachable>()) {
      // Effectively, an `Interface` is a sum with a single entry.
      HuffmanTableIndexedSymbolsSum sum(cx_);
      MOZ_TRY(sum.initWithSingleValue(cx_, BinASTKind(interface.kind)));
      table = {mozilla::VariantType<HuffmanTableIndexedSymbolsSum>{},
               std::move(sum)};
    }

    // Spec:
    // 4. If the effective type is a monomorphic interface, push all of the
    // interface’s fields
    return pushFields(interface.kind);
  }

  // Generic implementation for other cases.
  template <class Entry>
  MOZ_MUST_USE JS::Result<Ok> pushValue(NormalizedInterfaceAndField identity,
                                        const Entry& entry) {
    // Spec:
    // 1. If the field is in the set of visited contexts, stop.
    auto& table = dictionary.tableForField(identity);
    if (!table.is<HuffmanTableUnreachable>()) {
      // Entry already initialized/initializing.
      return Ok();
    }

    // Spec:
    // 2. Add the field to the set of visited fields
    table = {mozilla::VariantType<HuffmanTableInitializing>{}};

    // Spec:
    // 5. Otherwise, push the field onto the stack
    BINJS_TRY(stack.append(entry));
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
  MOZ_MUST_USE JS::Result<typename Entry::SymbolType> readSymbol(const Entry&,
                                                                 size_t index);

  // Read the number of symbols in an entry.
  // For an indexed type, theis number is fetched from the grammar.
  // We have a guarantee that `index` is always in [0, numberOfSymbols)
  template <typename Entry>
  MOZ_MUST_USE JS::Result<uint32_t> readNumberOfSymbols(const Entry&);

  // Read a table in the optimized "only one value" format.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readSingleValueTable(typename Entry::Table&,
                                                   const Entry&);

  // Read a table in the non-optimized format.
  //
  // Entries in the table are represented in an order that is specific to each
  // type. See the documentation of each `ReadPoppedEntryMatcher::operator()`
  // for a discussion on each order.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readMultipleValuesTable(
      typename Entry::Table& table, Entry entry) {
    // Get the number of symbols.
    // If `Entry` is an indexed type, this is fetched from the grammar.
    BINJS_MOZ_TRY_DECL(numberOfSymbols, readNumberOfSymbols<Entry>(entry));

    MOZ_ASSERT(numberOfSymbols <= MAX_NUMBER_OF_SYMBOLS);

    if (numberOfSymbols == 1) {
      // Special case: only one symbol.
      BINJS_MOZ_TRY_DECL(bitLength, reader.readByte<Compression::No>());
      if (MOZ_UNLIKELY(bitLength != 0)) {
        // Since there is a single symbol, it doesn't make sense to have a non-0
        // bit length.
        return raiseInvalidTableData(entry.identity);
      }

      // Read the symbol.
      // If `Entry` is an indexed type, it is fetched directly from the grammar.
      BINJS_MOZ_TRY_DECL(
          symbol, readSymbol<Entry>(entry, /* First and only value */ 0));

      MOZ_TRY(table.initWithSingleValue(cx_, std::move(symbol)));
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
  readMultipleValuesTableAndAssignCode(typename Entry::Table& table,
                                       Entry entry, uint32_t numberOfSymbols) {
    // Explicit entry:
    // - All symbols must be read from disk.
    // - Lengths read from disk are bit lengths.
    // - Bit lengths are ranked by increasing order.
    // - Bit lengths MUST NOT be 0.

    // Data is presented in an order that doesn't match our memory
    // representation, so we need to copy `numberOfSymbols` entries.
    // We use an auxiliary vector to avoid allocating each time.
    MOZ_ASSERT(
        auxStorageLength.empty());  // We must have cleaned it up properly.
    BINJS_TRY(auxStorageLength.reserve(numberOfSymbols + 1));

    uint8_t largestBitLength = 0;

    // First read and store the bit lengths for all symbols.
    for (size_t i = 0; i < numberOfSymbols; ++i) {
      BINJS_MOZ_TRY_DECL(bitLength, reader.readByte<Compression::No>());
      if (MOZ_UNLIKELY(bitLength == 0)) {
        return raiseInvalidTableData(entry.identity);
      }
      if (bitLength > largestBitLength) {
        largestBitLength = bitLength;
      }
      BINJS_TRY(auxStorageLength.append(BitLengthAndIndex(bitLength, i)));
    }
    // Append a terminator.
    BINJS_TRY(auxStorageLength.append(
        BitLengthAndIndex(MAX_CODE_BIT_LENGTH, numberOfSymbols)));

    // Now read the symbols and assign bits.
    uint32_t code = 0;
    MOZ_TRY(table.initStart(cx_, numberOfSymbols, largestBitLength));

    for (size_t i = 0; i < numberOfSymbols; ++i) {
      const auto bitLength = auxStorageLength[i].bitLength;
      const auto nextBitLength =
          auxStorageLength[i + 1]
              .bitLength;  // Guaranteed to exist, as we have a terminator.

      if (MOZ_UNLIKELY(bitLength > nextBitLength)) {
        // By format invariant, bit lengths are always non-0
        // and always ranked by increasing order.
        return raiseInvalidTableData(entry.identity);
      }

      // Read and add symbol.
      BINJS_MOZ_TRY_DECL(
          symbol, readSymbol<Entry>(entry, i));  // Symbol is read from disk.
      MOZ_TRY(table.addSymbol(code, bitLength, std::move(symbol)));

      // Prepare next code.
      code = (code + 1) << (nextBitLength - bitLength);
    }

    MOZ_TRY(table.initComplete());
    auxStorageLength.clear();
    return Ok();
  }

  template <typename Entry>
  MOZ_MUST_USE JS::Result<typename Entry::Indexed>
  readMultipleValuesTableAndAssignCode(typename Entry::Table& table,
                                       Entry entry, uint32_t numberOfSymbols) {
    // Data is presented in an order that doesn't match our memory
    // representation, so we need to copy `numberOfSymbols` entries.
    // We use an auxiliary vector to avoid allocating each time.
    MOZ_ASSERT(
        auxStorageLength.empty());  // We must have cleaned it up properly.
    BINJS_TRY(auxStorageLength.reserve(numberOfSymbols + 1));

    // Implicit entry:
    // - Symbols are known statically.
    // - Lengths MAY be 0.
    // - Lengths are not sorted on disk.

    uint8_t largestBitLength = 0;

    // First read the length for all symbols, only store non-0 lengths.
    for (size_t i = 0; i < numberOfSymbols; ++i) {
      BINJS_MOZ_TRY_DECL(bitLength, reader.readByte<Compression::No>());
      if (MOZ_UNLIKELY(bitLength > MAX_CODE_BIT_LENGTH)) {
        MOZ_CRASH("FIXME: Implement error");
      }
      if (bitLength > 0) {
        BINJS_TRY(auxStorageLength.append(BitLengthAndIndex(bitLength, i)));
        if (bitLength > largestBitLength) {
          largestBitLength = bitLength;
        }
      }
    }

    // Sort by length then webidl order (which is also the index).
    std::sort(auxStorageLength.begin(), auxStorageLength.end(),
              [&entry](const BitLengthAndIndex& a,
                       const BitLengthAndIndex& b) -> bool {
                MOZ_ASSERT(a.index != b.index);
                // Compare first by bit length.
                if (a.bitLength < b.bitLength) {
                  return true;
                }
                if (a.bitLength > b.bitLength) {
                  return false;
                }
                // In case of equal bit length, compare by symbol value.
                return entry.lessThan(a.index, b.index);
              });

    // Append a terminator.
    BINJS_TRY(
        auxStorageLength.emplaceBack(MAX_CODE_BIT_LENGTH, numberOfSymbols));

    // Now read the symbols and assign bits.
    uint32_t code = 0;
    MOZ_TRY(
        table.initStart(cx_, auxStorageLength.length() - 1, largestBitLength));

    for (size_t i = 0; i < auxStorageLength.length() - 1; ++i) {
      const auto bitLength = auxStorageLength[i].bitLength;
      const auto nextBitLength =
          auxStorageLength[i + 1].bitLength;  // Guaranteed to exist, as we stop
                                              // before the terminator.
      MOZ_ASSERT(bitLength > 0);
      MOZ_ASSERT(bitLength <= nextBitLength);

      // Read symbol from memory and add it.
      BINJS_MOZ_TRY_DECL(
          symbol,
          readSymbol<Entry>(
              entry,
              auxStorageLength[i].index));  // Symbol is read from memory.

      MOZ_TRY(table.addSymbol(code, bitLength, std::move(symbol)));

      // Prepare next code.
      code = (code + 1) << (nextBitLength - bitLength);
    }

    MOZ_TRY(table.initComplete());

    auxStorageLength.clear();
    return Ok();
  }

  // Single-argument version: lookup the table using `dictionary.tableForField`
  // then proceed as two-arguments version.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readTable(Entry entry) {
    auto& table = dictionary.tableForField(entry.identity);
    return readTable<HuffmanTableValue, Entry>(table, entry);
  }

  // Two-arguments version: pass table explicitly. Generally called from single-
  // argument version, but may be called manually, e.g. for list lengths, as
  // their tables don't appear in `dictionary.tableForField`.
  template <typename HuffmanTable, typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readTable(HuffmanTable& table, Entry entry) {
    if (MOZ_UNLIKELY(!table.template is<HuffmanTableInitializing>())) {
      // We're attempting to re-read a table that has already been read.
      // FIXME: Shouldn't this be a MOZ_CRASH?
      return raiseDuplicateTableError(entry.identity);
    }

    uint8_t headerByte;
    MOZ_TRY_VAR(headerByte, reader.readByte<Compression::No>());
    switch (headerByte) {
      case TableHeader::SingleValue: {
        // Construct in-place.
        table = {mozilla::VariantType<typename Entry::Table>{}, cx_};
        auto& tableRef = table.template as<typename Entry::Table>();

        // The table contains a single value.
        MOZ_TRY((readSingleValueTable<Entry>(tableRef, entry)));
        return Ok();
      }
      case TableHeader::MultipleValues: {
        // Table contains multiple values.
        // Construct in-place.
        table = {mozilla::VariantType<typename Entry::Table>{}, cx_};
        auto& tableRef = table.template as<typename Entry::Table>();

        MOZ_TRY((readMultipleValuesTable<Entry>(tableRef, entry)));
        return Ok();
      }
      case TableHeader::Unreachable:
        // Table is unreachable, nothing to do.
        return Ok();
      default:
        return raiseInvalidTableData(entry.identity);
    }
  }

 private:
  // An auxiliary storage of bit lengths and indices, used when reading
  // a table with multiple entries. Used to avoid (re)allocating a
  // vector of lengths each time we read a table.
  struct BitLengthAndIndex {
    BitLengthAndIndex(uint8_t bitLength, size_t index)
        : bitLength(bitLength), index(index) {}

    // A bitLength, as read from disk.
    uint8_t bitLength;

    // The index of the entry in the table.
    size_t index;
  };
  Vector<BitLengthAndIndex> auxStorageLength;

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
      return owner.pushFields(entry.kind);
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
      const auto& table = owner.dictionary.tableForField(entry.identity);
      if (table.is<HuffmanTableUnreachable>()) {
        return Ok();
      }
      const auto& tableRef =
          table.as<HuffmanTableIndexedSymbolsMaybeInterface>();
      if (!tableRef.isAlwaysNull()) {
        MOZ_TRY(owner.pushFields(entry.kind));
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

      const auto& table = owner.dictionary.tableForField(entry.identity);
      if (table.is<HuffmanTableInitializing>()) {
        return Ok();
      }
      const auto& tableRef = table.as<HuffmanTableIndexedSymbolsSum>();

      for (auto iter : tableRef) {
        MOZ_TRY(
            owner.pushValue(entry.identity, Interface(entry.identity, *iter)));
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

      const auto& table = owner.dictionary.tableForField(entry.identity);
      if (table.is<HuffmanTableUnreachable>()) {
        return Ok();
      }
      const auto& tableRef = table.as<HuffmanTableIndexedSymbolsSum>();

      for (auto iter : tableRef) {
        MOZ_TRY(
            owner.pushValue(entry.identity, Interface(entry.identity, *iter)));
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
    return reader.raiseError("Duplicate table.");
  }

  MOZ_MUST_USE ErrorResult<JS::Error&> raiseInvalidTableData(
      const NormalizedInterfaceAndField identity) {
    return reader.raiseError("Invalid data while reading table.");
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
      dictionary(cx),
      posBeforeTree_(nullptr) {
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
  metadata_ = scriptSource->binASTSourceMetadata();
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

JS::Result<Ok> BinASTTokenReaderContext::readStringPrelude() {
  BINJS_MOZ_TRY_DECL(stringsNumberOfEntries, readVarU32<Compression::No>());

  const uint32_t MAX_NUMBER_OF_STRINGS = 32768;

  if (MOZ_UNLIKELY(stringsNumberOfEntries > MAX_NUMBER_OF_STRINGS)) {
    return raiseError("Too many entries in strings dictionary");
  }

  // FIXME: We don't use the global table like multipart format, but
  //        (interface, field)-local dictionary.
  //        Metadata should be fixed to store that.
  Vector<BinASTKind> binASTKinds(cx_);

  BinASTSourceMetadata* metadata =
      BinASTSourceMetadata::Create(binASTKinds, stringsNumberOfEntries);
  if (MOZ_UNLIKELY(!metadata)) {
    return raiseOOM();
  }

  // Free it if we don't make it out of here alive. Since we don't want to
  // calloc(), we need to avoid marking atoms that might not be there.
  auto se = mozilla::MakeScopeExit([metadata]() { js_free(metadata); });

  // Below, we read at most DECODED_BUFFER_SIZE bytes at once and look for
  // strings there. We can overrun to the model part, and in that case put
  // unused part back to `decodedBuffer_`.

  // This buffer holds partially-read string, as Latin1Char.  This doesn't match
  // to the actual encoding of the strings that is WTF-8, but the validation
  // should be done later by `AtomizeWTF8Chars`, not immediately.
  StringBuffer buf(cx_);

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

    // FIXME: We don't populate `slicesTable_` given we won't use it.
    //        Update BinASTSourceMetadata to reflect this.

    metadata->getAtom(stringIndex) = atom;

    // +1 for skipping 0x00.
    current_ = end + 1;
  }

  // Found all strings. The remaining data is kept in buffer and will be
  // used for later read.

  MOZ_ASSERT(!metadata_);
  MOZ_ASSERT(buf.empty());
  se.release();
  metadata_ = metadata;
  metadataOwned_ = MetadataOwnership::Owned;

  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::readHuffmanPrelude() {
  HuffmanPreludeReader reader{cx_, *this, dictionary};
  return reader.run(HUFFMAN_STACK_INITIAL_CAPACITY);
}

BinASTTokenReaderContext::BitBuffer::BitBuffer() : bits(0), bitLength(0) {
  static_assert(sizeof(bits) * 8 == BIT_BUFFER_SIZE,
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
  // are beyond `this->bitLength`

  if (this->bitLength <= MAX_PREFIX_BIT_LENGTH) {
    // Keys can be up to MAX_PREFIX_BIT_LENGTH bits long. If we have fewer bits
    // available, it's time to reload. We'll try and get as close to 64 bits as
    // possible.

    // Make an array to store the new data. We can read up to 8 bytes
    uint8_t bytes[8] = {};
    // Determine the number of bytes in `bits` rounded up to the nearest byte.
    uint32_t bytesInBits =
        (this->bitLength + BIT_BUFFER_READ_UNIT - 1) / BIT_BUFFER_READ_UNIT;
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

    this->bitLength += (BIT_BUFFER_READ_UNIT * readLen);
    // Make room for the new data by shifting `bits` to get
    // `0b_ABCD_EFGH__IJKL_MNOP__0000_0000__0000_0000`
    // check that we're not shifting by 64 since it's UB for a uint64_t
    if (readLen != 8) {
      this->bits <<= (BIT_BUFFER_READ_UNIT * readLen);
      // Finally, combine `newBits` with `bits` to get
      // `0b_ABCD_EFGH__IJKL_MNOP__QRST_UVWX__YZ12_3456`
      this->bits += newBits;
    }
    // If read 8 bytes just set `bits` to the new data
    else {
      this->bits = newBits;
    }
    // Now, we may prepare a `HuffmanLookup` with up to 32 bits.
    if (this->bitLength <= MAX_PREFIX_BIT_LENGTH) {
      return HuffmanLookup(this->bits, this->bitLength);
    }
  }

  // Keep only 32 bits. We perform the operation on 64 bits to avoid any
  // arithmetics surprise.
  const uint64_t bitPrefix =
      this->bits >> (this->bitLength - MAX_PREFIX_BIT_LENGTH);
  MOZ_ASSERT(bitPrefix <= uint32_t(-1));
  return HuffmanLookup(bitPrefix, MAX_PREFIX_BIT_LENGTH);
}

template <>
void BinASTTokenReaderContext::BitBuffer::advanceBitBuffer<Compression::No>(
    const uint8_t bitLength) {
  // It should be impossible to call `advanceBitBuffer`
  // with more bits than what we just handed out.
  MOZ_ASSERT(bitLength <= this->bitLength);

  this->bitLength -= bitLength;

  // Now zero out the bits that are beyond `this->bitLength`.
  const uint64_t mask =
      this->bitLength == 0
          ? 0  // >> 64 is UB for a uint64_t
          : uint64_t(-1) >> (BIT_BUFFER_SIZE - this->bitLength);
  this->bits &= mask;
  MOZ_ASSERT_IF(this->bitLength != BIT_BUFFER_SIZE,
                this->bits >> this->bitLength == 0);
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
    return context.position;
  }
  BinASTInterfaceAndField operator()(
      const BinASTTokenReaderBase::ListContext& context) {
    return context.position;
  }
  BinASTInterfaceAndField operator()(
      const BinASTTokenReaderBase::RootContext&) {
    MOZ_CRASH("The root context has no interface/field");
  }
};

JS::Result<BinASTKind> BinASTTokenReaderContext::readTagFromTable(
    const BinASTInterfaceAndField& identity) {
  // Extract the table.
  const auto& table =
      dictionary.tableForField(NormalizedInterfaceAndField(identity));
  BINJS_MOZ_TRY_DECL(bits,
                     (bitBuffer.getHuffmanLookup<Compression::No>(*this)));

  if (table.is<HuffmanTableIndexedSymbolsSum>()) {
    const auto& specialized = table.as<HuffmanTableIndexedSymbolsSum>();

    // We're entering either a single interface or a sum.
    const auto lookup = specialized.lookup(bits);
    bitBuffer.advanceBitBuffer<Compression::No>(lookup.key.bitLength);
    if (MOZ_UNLIKELY(!lookup.value)) {
      return raiseInvalidValue();
    }
    return *lookup.value;
  }

  MOZ_ASSERT(table.is<HuffmanTableIndexedSymbolsMaybeInterface>());
  const auto& specialized =
      table.as<HuffmanTableIndexedSymbolsMaybeInterface>();
  // We're entering an optional interface.
  const auto lookup = specialized.lookup(bits);
  bitBuffer.advanceBitBuffer<Compression::No>(lookup.key.bitLength);
  if (MOZ_UNLIKELY(!lookup.value)) {
    return raiseInvalidValue();
  }
  return *lookup.value;
}

template <typename Table>
JS::Result<typename Table::Contents>
BinASTTokenReaderContext::readFieldFromTable(
    const BinASTInterfaceAndField& identity) {
  // Extract the table.
  const auto& table =
      dictionary.tableForField(NormalizedInterfaceAndField(identity));
  if (MOZ_UNLIKELY(!table.is<Table>())) {
    return raiseNotInPrelude();
  }
  BINJS_MOZ_TRY_DECL(bits, bitBuffer.getHuffmanLookup<Compression::No>(*this));
  const auto lookup = table.as<Table>().lookup(bits);

  bitBuffer.advanceBitBuffer<Compression::No>(lookup.key.bitLength);
  if (MOZ_UNLIKELY(!lookup.value)) {
    return raiseInvalidValue();
  }
  return *lookup.value;
}

JS::Result<bool> BinASTTokenReaderContext::readBool(
    const FieldContext& context) {
  return readFieldFromTable<HuffmanTableIndexedSymbolsBool>(context.position);
}

JS::Result<double> BinASTTokenReaderContext::readDouble(
    const FieldContext& context) {
  return readFieldFromTable<HuffmanTableExplicitSymbolsF64>(context.position);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeAtom(
    const FieldContext& context) {
  return readFieldFromTable<HuffmanTableIndexedSymbolsOptionalLiteralString>(
      context.position);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readAtom(
    const FieldContext& context) {
  return readFieldFromTable<HuffmanTableIndexedSymbolsLiteralString>(
      context.position);
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
  BINJS_MOZ_TRY_DECL(result,
                     readFieldFromTable<HuffmanTableIndexedSymbolsStringEnum>(
                         context.position));
  return result;
}

JS::Result<BinASTVariant> BinASTTokenReaderContext::readVariant(
    const FieldContext& context) {
  BINJS_MOZ_TRY_DECL(result,
                     readFieldFromTable<HuffmanTableIndexedSymbolsStringEnum>(
                         context.position));
  return result;
}

JS::Result<uint32_t> BinASTTokenReaderContext::readUnsignedLong(
    const FieldContext& context) {
  return readFieldFromTable<HuffmanTableExplicitSymbolsU32>(context.position);
}

JS::Result<BinASTTokenReaderBase::SkippableSubTree>
BinASTTokenReaderContext::readSkippableSubTree(const FieldContext&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::enterSum(
    BinASTKind& tag, const FieldOrRootContext& context) {
  return context.match(
      [this, &tag](const BinASTTokenReaderBase::FieldContext& asFieldContext)
          -> JS::Result<Ok> {
        // This tuple is the value of the field we're currently reading.
        MOZ_TRY_VAR(tag, readTagFromTable(asFieldContext.position));
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
        MOZ_TRY_VAR(tag, readTagFromTable(asFieldContext.position));
        return Ok();
      },
      [this, &tag](const BinASTTokenReaderBase::ListContext& asListContext)
          -> JS::Result<Ok> {
        // This tuple is an element in a list we're currently reading.
        MOZ_TRY_VAR(tag, readTagFromTable(asListContext.position));
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
  MOZ_TRY_VAR(tag, readTagFromTable(context.position));
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::enterSum(BinASTKind& tag,
                                                  const FieldContext& context) {
  // This tuple is the value of the field we're currently reading.
  MOZ_TRY_VAR(tag, readTagFromTable(context.position));
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::enterList(uint32_t& items,
                                                   const ListContext& context) {
  const auto identity = context.content;
  const auto& table = dictionary.tableForListLength(identity);
  BINJS_MOZ_TRY_DECL(bits, bitBuffer.getHuffmanLookup<Compression::No>(*this));
  const auto& tableForLookup =
      table.as<HuffmanTableExplicitSymbolsListLength>();
  const auto lookup = tableForLookup.lookup(bits);
  bitBuffer.advanceBitBuffer<Compression::No>(lookup.key.bitLength);
  if (MOZ_UNLIKELY(!lookup.value)) {
    return raiseInvalidValue();
  }
  items = *lookup.value;
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
    : bits(bits), bitLength(bitLength) {
  MOZ_ASSERT(bitLength <= MAX_PREFIX_BIT_LENGTH);
  MOZ_ASSERT_IF(bitLength != 32 /* >> 32 is UB */, bits >> bitLength == 0);
}

FlatHuffmanKey::FlatHuffmanKey(HuffmanKey key)
    : representation((key.bitLength << MAX_CODE_BIT_LENGTH) | key.bits) {
  static_assert(MAX_CODE_BIT_LENGTH + MAX_BIT_LENGTH_BIT_LENGTH <= 32,
                "32 bits MUST be sufficient to store bits and bitLength");
  MOZ_ASSERT(key.bits >> MAX_CODE_BIT_LENGTH == 0);
  MOZ_ASSERT(key.bitLength >> MAX_BIT_LENGTH_BIT_LENGTH == 0);
}

FlatHuffmanKey::FlatHuffmanKey(const HuffmanKey* key)
    : representation((key->bitLength << MAX_CODE_BIT_LENGTH) | key->bits) {
  static_assert(MAX_CODE_BIT_LENGTH + MAX_BIT_LENGTH_BIT_LENGTH <= 32,
                "32 bits MUST be sufficient to store bits and bitLength");
  MOZ_ASSERT(key->bits >> MAX_CODE_BIT_LENGTH == 0);
  MOZ_ASSERT(key->bitLength >> MAX_BIT_LENGTH_BIT_LENGTH == 0);
}

// ---- Implementation of Huffman Tables

template <typename T>
GenericHuffmanTable<T>::Iterator::Iterator(
    typename SingleLookupHuffmanTable<T>::Iterator&& iterator)
    : implementation(std::move(iterator)) {}

template <typename T>
GenericHuffmanTable<T>::Iterator::Iterator(
    typename TwoLookupsHuffmanTable<T>::Iterator&& iterator)
    : implementation(std::move(iterator)) {}

template <typename T>
GenericHuffmanTable<T>::Iterator::Iterator(
    typename ThreeLookupsHuffmanTable<T>::Iterator&& iterator)
    : implementation(std::move(iterator)) {}

template <typename T>
void GenericHuffmanTable<T>::Iterator::operator++() {
  implementation.match(
      [](typename SingleLookupHuffmanTable<T>::Iterator& iterator) {
        iterator.operator++();
      },
      [](typename TwoLookupsHuffmanTable<T>::Iterator& iterator) {
        iterator.operator++();
      },
      [](typename ThreeLookupsHuffmanTable<T>::Iterator& iterator) {
        iterator.operator++();
      });
}

template <typename T>
bool GenericHuffmanTable<T>::Iterator::operator==(
    const GenericHuffmanTable<T>::Iterator& other) const {
  return implementation.match(
      [other](const typename SingleLookupHuffmanTable<T>::Iterator& iterator) {
        return iterator ==
               other.implementation.template as<
                   typename SingleLookupHuffmanTable<T>::Iterator>();
      },
      [other](const typename TwoLookupsHuffmanTable<T>::Iterator& iterator) {
        return iterator ==
               other.implementation
                   .template as<typename TwoLookupsHuffmanTable<T>::Iterator>();
      },
      [other](const typename ThreeLookupsHuffmanTable<T>::Iterator& iterator) {
        return iterator ==
               other.implementation.template as<
                   typename ThreeLookupsHuffmanTable<T>::Iterator>();
      });
}

template <typename T>
bool GenericHuffmanTable<T>::Iterator::operator!=(
    const GenericHuffmanTable<T>::Iterator& other) const {
  return implementation.match(
      [other](const typename SingleLookupHuffmanTable<T>::Iterator& iterator) {
        return iterator !=
               other.implementation.template as<
                   typename SingleLookupHuffmanTable<T>::Iterator>();
      },
      [other](const typename TwoLookupsHuffmanTable<T>::Iterator& iterator) {
        return iterator !=
               other.implementation
                   .template as<typename TwoLookupsHuffmanTable<T>::Iterator>();
      },
      [other](const typename ThreeLookupsHuffmanTable<T>::Iterator& iterator) {
        return iterator !=
               other.implementation.template as<
                   typename ThreeLookupsHuffmanTable<T>::Iterator>();
      });
}

template <typename T>
const T* GenericHuffmanTable<T>::Iterator::operator*() const {
  return implementation.match(
      [](const typename SingleLookupHuffmanTable<T>::Iterator& iterator) {
        return iterator.operator*();
      },
      [](const typename TwoLookupsHuffmanTable<T>::Iterator& iterator) {
        return iterator.operator*();
      },
      [](const typename ThreeLookupsHuffmanTable<T>::Iterator& iterator) {
        return iterator.operator*();
      });
}

template <typename T>
GenericHuffmanTable<T>::GenericHuffmanTable(JSContext*)
    : implementation(HuffmanTableUnreachable{}) {}

template <typename T>
JS::Result<Ok> GenericHuffmanTable<T>::initComplete() {
  return this->implementation.match(
      [](SingleLookupHuffmanTable<T>& implementation) -> JS::Result<Ok> {
        return implementation.initComplete();
      },
      [](TwoLookupsHuffmanTable<T>& implementation) -> JS::Result<Ok> {
        return implementation.initComplete();
      },
      [](ThreeLookupsHuffmanTable<T>& implementation) -> JS::Result<Ok> {
        return implementation.initComplete();
      },
      [](HuffmanTableUnreachable&) -> JS::Result<Ok> {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

template <typename T>
typename GenericHuffmanTable<T>::Iterator GenericHuffmanTable<T>::begin()
    const {
  return this->implementation.match(
      [](const SingleLookupHuffmanTable<T>& implementation)
          -> GenericHuffmanTable<T>::Iterator {
        return Iterator(implementation.begin());
      },
      [](const TwoLookupsHuffmanTable<T>& implementation)
          -> GenericHuffmanTable<T>::Iterator {
        return Iterator(implementation.begin());
      },
      [](const ThreeLookupsHuffmanTable<T>& implementation)
          -> GenericHuffmanTable<T>::Iterator {
        return Iterator(implementation.begin());
      },
      [](const HuffmanTableUnreachable&) -> GenericHuffmanTable<T>::Iterator {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

template <typename T>
typename GenericHuffmanTable<T>::Iterator GenericHuffmanTable<T>::end() const {
  return this->implementation.match(
      [](const SingleLookupHuffmanTable<T>& implementation)
          -> GenericHuffmanTable<T>::Iterator {
        return Iterator(implementation.end());
      },
      [](const TwoLookupsHuffmanTable<T>& implementation)
          -> GenericHuffmanTable<T>::Iterator {
        return Iterator(implementation.end());
      },
      [](const ThreeLookupsHuffmanTable<T>& implementation)
          -> GenericHuffmanTable<T>::Iterator {
        return Iterator(implementation.end());
      },
      [](const HuffmanTableUnreachable&) -> GenericHuffmanTable<T>::Iterator {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

template <typename T>
JS::Result<Ok> GenericHuffmanTable<T>::initWithSingleValue(JSContext* cx,
                                                           T&& value) {
  // Only one value: use HuffmanImplementationSaturated
  MOZ_ASSERT(this->implementation.template is<
             HuffmanTableUnreachable>());  // Make sure that we're initializing.

  this->implementation = {mozilla::VariantType<SingleLookupHuffmanTable<T>>{},
                          cx};

  MOZ_TRY(this->implementation.template as<SingleLookupHuffmanTable<T>>()
              .initWithSingleValue(cx, std::move(value)));
  return Ok();
}

template <typename T>
JS::Result<Ok> GenericHuffmanTable<T>::initStart(JSContext* cx,
                                                 size_t numberOfSymbols,
                                                 uint8_t largestBitLength) {
  // Make sure that we have a way to represent all legal bit lengths.
  static_assert(
      MAX_CODE_BIT_LENGTH <= ThreeLookupsHuffmanTable<T>::MAX_BIT_LENGTH,
      "ThreeLookupsHuffmanTable cannot hold all bit lengths");
  // Make sure that we're initializing.
  MOZ_ASSERT(this->implementation.template is<HuffmanTableUnreachable>());

  // Find the (hopefully) fastest implementation of HuffmanTable for
  // `largestBitLength`.
  // ...hopefully, only one lookup.
  if (largestBitLength <= SingleLookupHuffmanTable<T>::MAX_BIT_LENGTH) {
    this->implementation = {mozilla::VariantType<SingleLookupHuffmanTable<T>>{},
                            cx};
    return this->implementation.template as<SingleLookupHuffmanTable<T>>()
        .initStart(cx, numberOfSymbols, largestBitLength);
  }

  // ...if a single-lookup table would be too large, let's see if
  // we can fit in a two-lookup table.
  if (largestBitLength <= TwoLookupsHuffmanTable<T>::MAX_BIT_LENGTH) {
    this->implementation = {mozilla::VariantType<TwoLookupsHuffmanTable<T>>{},
                            cx};
    return this->implementation.template as<TwoLookupsHuffmanTable<T>>()
        .initStart(cx, numberOfSymbols, largestBitLength);
  }

  // ...otherwise, we'll need three lookups.
  this->implementation = {mozilla::VariantType<ThreeLookupsHuffmanTable<T>>{},
                          cx};
  return this->implementation.template as<ThreeLookupsHuffmanTable<T>>()
      .initStart(cx, numberOfSymbols, largestBitLength);
}

template <typename T>
JS::Result<Ok> GenericHuffmanTable<T>::addSymbol(uint32_t bits,
                                                 uint8_t bitLength, T&& value) {
  return this->implementation.match(
      [bits, bitLength, value = std::move(value)](
          SingleLookupHuffmanTable<T>&
              implementation) mutable /* discard implicit const */
      -> JS::Result<Ok> {
        return implementation.addSymbol(bits, bitLength, std::move(value));
      },
      [bits, bitLength, value = std::move(value)](
          TwoLookupsHuffmanTable<T>&
              implementation) mutable /* discard implicit const */
      -> JS::Result<Ok> {
        return implementation.addSymbol(bits, bitLength, std::move(value));
      },
      [bits, bitLength, value = std::move(value)](
          ThreeLookupsHuffmanTable<T>&
              implementation) mutable /* discard implicit const */
      -> JS::Result<Ok> {
        return implementation.addSymbol(bits, bitLength, std::move(value));
      },
      [](HuffmanTableUnreachable&) -> JS::Result<Ok> {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
        return Ok();
      });
}

template <typename T>
HuffmanEntry<const T*> GenericHuffmanTable<T>::lookup(
    HuffmanLookup lookup) const {
  return this->implementation.match(
      [lookup](const SingleLookupHuffmanTable<T>& implementation)
          -> HuffmanEntry<const T*> { return implementation.lookup(lookup); },
      [lookup](const TwoLookupsHuffmanTable<T>& implementation)
          -> HuffmanEntry<const T*> { return implementation.lookup(lookup); },
      [lookup](const ThreeLookupsHuffmanTable<T>& implementation)
          -> HuffmanEntry<const T*> { return implementation.lookup(lookup); },
      [](const HuffmanTableUnreachable&) -> HuffmanEntry<const T*> {
        MOZ_CRASH("GenericHuffmanTable is unitialized!");
      });
}

template <typename T, int N>
JS::Result<Ok> NaiveHuffmanTable<T, N>::initWithSingleValue(JSContext* cx,
                                                            T&& value) {
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  if (MOZ_UNLIKELY(!values.append(HuffmanEntry<T>(0, 0, std::move(value))))) {
    return cx->alreadyReportedError();
  }
  return Ok();
}

template <typename T, int N>
JS::Result<Ok> NaiveHuffmanTable<T, N>::initStart(JSContext* cx,
                                                  size_t numberOfSymbols,
                                                  uint8_t) {
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  if (MOZ_UNLIKELY(!values.initCapacity(numberOfSymbols))) {
    return cx->alreadyReportedError();
  }
  return Ok();
}

template <typename T, int N>
JS::Result<Ok> NaiveHuffmanTable<T, N>::initComplete() {
  MOZ_ASSERT(values.length() <= N);
  return Ok();
}

template <typename T, int N>
JS::Result<Ok> NaiveHuffmanTable<T, N>::addSymbol(uint32_t bits,
                                                  uint8_t bitLength,
                                                  T&& value) {
  MOZ_ASSERT(bitLength != 0,
             "Adding a symbol with a bitLength of 0 doesn't make sense.");
  MOZ_ASSERT(values.empty() || values.back().key.bitLength <= bitLength,
             "Symbols must be ranked by increasing bits length");
  MOZ_ASSERT_IF(bitLength != 32 /* >> 32 is UB */, bits >> bitLength == 0);
  // Memory was reserved in `init()`.
  MOZ_ALWAYS_TRUE(values.emplaceBack(bits, bitLength, std::move(value)));

  return Ok();
}

template <typename T, int N>
HuffmanEntry<const T*> NaiveHuffmanTable<T, N>::lookup(
    HuffmanLookup key) const {
  // This current implementation is O(length) and designed mostly for testing.
  // Future versions will presumably adapt the underlying data structure to
  // provide bounded-time lookup.
  for (const auto& iter : values) {
    if (iter.key.bitLength > key.bitLength) {
      // We can't find the entry.
      break;
    }

    const uint32_t keyBits = key.leadingBits(iter.key.bitLength);
    if (keyBits == iter.key.bits) {
      // Entry found.
      return HuffmanEntry<const T*>(iter.key.bits, iter.key.bitLength,
                                    &iter.value);
    }
  }

  // Error: no entry found.
  return HuffmanEntry<const T*>(0, 0, nullptr);
}

template <typename T>
JS::Result<Ok> MapBasedHuffmanTable<T>::initWithSingleValue(JSContext* cx,
                                                            T&& value) {
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  const HuffmanKey key(0, 0);
  if (MOZ_UNLIKELY(!values.put(FlatHuffmanKey(key), std::move(value)) ||
                   !keys.append(key))) {
    ReportOutOfMemory(cx);
    return cx->alreadyReportedError();
  }
  return Ok();
}

template <typename T>
JS::Result<Ok> MapBasedHuffmanTable<T>::initStart(JSContext* cx,
                                                  size_t numberOfSymbols,
                                                  uint8_t) {
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  if (MOZ_UNLIKELY(!values.reserve(numberOfSymbols) ||
                   !keys.reserve(numberOfSymbols))) {
    ReportOutOfMemory(cx);
    return cx->alreadyReportedError();
  }
  return Ok();
}

template <typename T>
JS::Result<Ok> MapBasedHuffmanTable<T>::initComplete() {
#if DEBUG
  // Check that there is a bijection between `keys` and `values`.
  // 1. Injection.
  for (const auto& key : keys) {
    MOZ_ALWAYS_TRUE(values.has(FlatHuffmanKey(key)));
  }
  // 2. Cardinality.
  MOZ_ASSERT(values.count() == keys.length());
#endif  // DEBUG
  return Ok();
}

template <typename T>
JS::Result<Ok> MapBasedHuffmanTable<T>::addSymbol(uint32_t bits,
                                                  uint8_t bitLength,
                                                  T&& value) {
  MOZ_ASSERT_IF(bitLength != 32 /* >> 32 is UB */, bits >> bitLength == 0);
  const HuffmanKey key(bits, bitLength);
  const FlatHuffmanKey flat(key);
  values.putNewInfallible(
      flat, std::move(value));  // Memory was reserved in `init()`.
  keys.infallibleAppend(std::move(key));

  return Ok();
}

template <typename T>
HuffmanEntry<const T*> MapBasedHuffmanTable<T>::lookup(
    HuffmanLookup lookup) const {
  for (auto bitLength = 0; bitLength < MAX_CODE_BIT_LENGTH; ++bitLength) {
    const uint32_t bits = lookup.leadingBits(bitLength);
    const HuffmanKey key(bits, bitLength);
    const FlatHuffmanKey flat(key);
    if (auto ptr = values.lookup(flat)) {
      // Entry found.
      return HuffmanEntry<const T*>(bits, bitLength, &ptr->value());
    }
  }

  // Error: no entry found.
  return HuffmanEntry<const T*>(0, 0, nullptr);
}

template <typename T>
SingleLookupHuffmanTable<T>::Iterator::Iterator(const HuffmanEntry<T>* position)
    : position(position) {}

template <typename T>
void SingleLookupHuffmanTable<T>::Iterator::operator++() {
  position++;
}

template <typename T>
const T* SingleLookupHuffmanTable<T>::Iterator::operator*() const {
  return &position->value;
}

template <typename T>
bool SingleLookupHuffmanTable<T>::Iterator::operator==(
    const Iterator& other) const {
  return position == other.position;
}

template <typename T>
bool SingleLookupHuffmanTable<T>::Iterator::operator!=(
    const Iterator& other) const {
  return position != other.position;
}

template <typename T>
JS::Result<Ok> SingleLookupHuffmanTable<T>::initWithSingleValue(JSContext* cx,
                                                                T&& value) {
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  if (MOZ_UNLIKELY(!values.emplaceBack(0, 0, std::move(value)))) {
    ReportOutOfMemory(cx);
    return cx->alreadyReportedError();
  }
  if (MOZ_UNLIKELY(!saturated.emplaceBack(0))) {
    ReportOutOfMemory(cx);
    return cx->alreadyReportedError();
  }
  this->largestBitLength = 0;
  return Ok();
}

template <typename T>
JS::Result<Ok> SingleLookupHuffmanTable<T>::initStart(
    JSContext* cx, size_t numberOfSymbols, uint8_t largestBitLength) {
  MOZ_ASSERT_IF(largestBitLength != 32,
                (uint32_t(1) << largestBitLength) - 1 <=
                    mozilla::MaxValue<InternalIndex>::value);
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.

  this->largestBitLength = largestBitLength;

  if (MOZ_UNLIKELY(!values.initCapacity(numberOfSymbols))) {
    return cx->alreadyReportedError();
  }
  const size_t saturatedLength = 1 << largestBitLength;
  if (MOZ_UNLIKELY(!saturated.initCapacity(saturatedLength))) {
    return cx->alreadyReportedError();
  }
  // Enlarge `saturated`, as we're going to fill it in random order.
  for (size_t i = 0; i < saturatedLength; ++i) {
    // Capacity reserved in this method.
    saturated.infallibleAppend(InternalIndex(-1));
  }
  return Ok();
}

template <typename T>
JS::Result<Ok> SingleLookupHuffmanTable<T>::initComplete() {
  // Double-check that we've initialized properly.
  MOZ_ASSERT(this->largestBitLength <= MAX_CODE_BIT_LENGTH);

  // We can end up with empty tables, if this `SingleLookupHuffmanTable`
  // is used to store suffixes in a `MultiLookupHuffmanTable` and
  // the corresponding prefix is never used.
  if (values.length() == 0) {
    MOZ_ASSERT(this->largestBitLength == 0);
    return Ok();
  }
#ifdef DEBUG
  bool foundMaxBitLength = false;
  for (size_t i = 0; i < saturated.length(); ++i) {
    const uint8_t index = saturated[i];
    MOZ_ASSERT(values[index].key.bitLength <= largestBitLength);
    if (values[index].key.bitLength == largestBitLength) {
      foundMaxBitLength = true;
    }
  }
  MOZ_ASSERT(foundMaxBitLength);
#endif  // DEBUG
  return Ok();
}

template <typename T>
JS::Result<Ok> SingleLookupHuffmanTable<T>::addSymbol(uint32_t bits,
                                                      uint8_t bitLength,
                                                      T&& value) {
  MOZ_ASSERT_IF(largestBitLength != 0, bitLength != 0);
  MOZ_ASSERT_IF(bitLength != 32 /* >> 32 is UB */, bits >> bitLength == 0);
  MOZ_ASSERT(bitLength <= largestBitLength);

  const size_t index = values.length();

  // First add the value to `values`.
  // Memory was reserved in `init()`.
  values.infallibleEmplaceBack(bits, bitLength, std::move(value));

  // Notation: in the following, unless otherwise specified, we consider
  // values with `largestBitLength` bits exactly.
  //
  // When we perform lookup, we will extract `largestBitLength` bits from the
  // key into a value `0bB...B`. We have a match for `value` if and only if
  // `0bB...B` may be decomposed into `0bC...CX...X` such that
  //    - `0bC...C` is `bitLength` bits long;
  //    - `0bC...C == bits`.
  //
  // To perform a fast lookup, we precompute all possible values of `0bB...B`
  // for which this condition is true. That's all the values of segment
  // `[0bC...C0...0, 0bC...C1...1]`.
  const HuffmanLookup base(bits, bitLength);
  for (size_t i : base.suffixes(largestBitLength)) {
    saturated[i] = index;
  }

  return Ok();
}

template <typename T>
HuffmanEntry<const T*> SingleLookupHuffmanTable<T>::lookup(
    HuffmanLookup key) const {
  if (values.length() == 0) {
    // If the table is empty, any lookup fails.
    return HuffmanEntry<const T*>(0, 0, nullptr);
  }
  // ...otherwise, all lookups succeed.

  // Take the `largestBitLength` highest weight bits of `key`.
  // In the documentation of `addSymbol`, this is
  // `0bB...B`.
  const uint32_t bits = key.leadingBits(largestBitLength);

  // Invariants: `saturated.length() == 1 << largestBitLength`
  // and `bits <= 1 << largestBitLength`.
  const size_t index = saturated[bits];

  // Invariants: `saturated[i] < values.length()`.
  const auto& entry = values[index];
  return HuffmanEntry<const T*>(entry.key.bits, entry.key.bitLength,
                                &entry.value);
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::Iterator::Iterator(
    const HuffmanEntry<T>* position)
    : position(position) {}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
void MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::Iterator::
operator++() {
  position++;
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
const T* MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::Iterator::
operator*() const {
  return &position->value;
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
bool MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::Iterator::
operator==(const Iterator& other) const {
  return position == other.position;
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
bool MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::Iterator::
operator!=(const Iterator& other) const {
  return position != other.position;
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
JS::Result<Ok> MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::initStart(
    JSContext* cx, size_t numberOfSymbols, uint8_t largestBitLength) {
  static_assert(PrefixBitLength < MAX_CODE_BIT_LENGTH,
                "Invalid PrefixBitLength");
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  MOZ_ASSERT(subTables.empty());
  this->largestBitLength = largestBitLength;
  if (MOZ_UNLIKELY(!values.initCapacity(numberOfSymbols))) {
    return cx->alreadyReportedError();
  }
  if (MOZ_UNLIKELY(!subTables.initCapacity(1 << PrefixBitLength))) {
    return cx->alreadyReportedError();
  }
  return Ok();
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
JS::Result<Ok> MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::addSymbol(
    uint32_t bits, uint8_t bitLength, T&& value) {
  MOZ_ASSERT_IF(largestBitLength != 0, bitLength != 0);
  MOZ_ASSERT(values.empty() || values.back().key.bitLength <= bitLength,
             "Symbols must be ranked by increasing bits length");
  MOZ_ASSERT_IF(bitLength != 32 /* >> 32 is UB */, bits >> bitLength == 0);

  values.infallibleEmplaceBack(bits, bitLength, std::move(value));

  return Ok();
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
JS::Result<Ok>
MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::initComplete() {
  // First, we need to collect the `largestBitLength`
  // and `numberofSymbols` for each subtable.
  struct Bucket {
    Bucket() : largestBitLength(0), numberOfSymbols(0){};
    uint8_t largestBitLength;
    uint32_t numberOfSymbols;
  };
  Vector<Bucket> buckets{cx_};
  BINJS_TRY(buckets.resize(1 << PrefixBitLength));

  for (const auto& entry : values) {
    const HuffmanLookup lookup(entry.key.bits, entry.key.bitLength);
    const auto split = lookup.split(PrefixBitLength);
    MOZ_ASSERT_IF(split.suffix.bitLength != 32,
                  split.suffix.bits >> split.suffix.bitLength == 0);

    // Entries that have a sufficient number of bits will be dispatched
    // to a single subtable (e.g. A, B, C, D, E, F in the documentation).
    // Other entries need to be dispatched to several subtables
    // (e.g. G, H in the documentation).
    for (const auto index : lookup.suffixes(PrefixBitLength)) {
      Bucket& bucket = buckets[index];
      if (split.suffix.bitLength >= bucket.largestBitLength) {
        bucket.largestBitLength = split.suffix.bitLength;
      }
      bucket.numberOfSymbols++;
    }
  }

  // We may now create the subtables.
  for (auto& bucket : buckets) {
    Subtable sub(cx_);
    MOZ_TRY(sub.initStart(cx_,
                          /* numberOfSymbols = */ bucket.numberOfSymbols,
                          /* largestBitLength = */ bucket.largestBitLength));
    BINJS_TRY(subTables.append(std::move(sub)));
  }

  // Now that the subtables are created, let's dispatch the values
  // among these tables.
  for (size_t i = 0; i < values.length(); ++i) {
    const auto& entry = values[i];

    // Find the relevant subtables.
    const HuffmanLookup lookup(entry.key.bits, entry.key.bitLength);
    const auto split = lookup.split(PrefixBitLength);
    MOZ_ASSERT_IF(split.suffix.bitLength != 32,
                  split.suffix.bits >> split.suffix.bitLength == 0);
    for (const auto index : lookup.suffixes(PrefixBitLength)) {
      auto& sub = subTables[index];

      // We may now add a reference to `entry` into the sybtable.
      MOZ_TRY(sub.addSymbol(split.suffix.bits, split.suffix.bitLength,
                            std::move(i)));
    }
  }

  // Finally, complete initialization of subtables.
  for (auto& sub : subTables) {
    MOZ_TRY(sub.initComplete());
  }

  return Ok();
}

template <typename T, typename Subtable, uint8_t PrefixBitLength>
HuffmanEntry<const T*>
MultiLookupHuffmanTable<T, Subtable, PrefixBitLength>::lookup(
    HuffmanLookup key) const {
  const auto split = key.split(PrefixBitLength);
  if (split.prefix.bits >= subTables.length()) {
    return HuffmanEntry<const T*>(0, 0, nullptr);
  }
  const Subtable& subtable = subTables[split.prefix.bits];

  auto found = subtable.lookup(split.suffix);

  if (found.value == nullptr) {
    // Propagate "not found".
    return {0, 0, nullptr};
  }

  // Otherwise, restore the entire `HuffmanEntry`.
  const auto& result = values[*found.value];

  return /* HuffmanEntry */ {/* bits */ result.key.bits,
                             /* bitLength */ result.key.bitLength,
                             /* value */ std::move(&result.value)};
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
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::run(size_t initialCapacity) {
  BINJS_TRY(stack.reserve(initialCapacity));

  // For the moment, the root node is hardcoded to be a BinASTKind::Script.
  // In future versions of the codec, we'll extend the format to handle
  // other possible roots (e.g. BinASTKind::Module).
  MOZ_TRY(pushFields(BinASTKind::Script));
  while (stack.length() > 0) {
    const Entry entry = stack.popCopy();
    MOZ_TRY(entry.match(ReadPoppedEntryMatcher(*this)));
  }
  return Ok();
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
MOZ_MUST_USE JS::Result<bool> HuffmanPreludeReader::readSymbol(const Boolean&,
                                                               size_t index) {
  MOZ_ASSERT(index < 2);
  return index != 0;
}

// Reading a single-value table of booleans
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<Boolean>(
    Boolean::Table& table, const Boolean& entry) {
  uint8_t indexByte;
  MOZ_TRY_VAR(indexByte, reader.readByte<Compression::No>());
  if (MOZ_UNLIKELY(indexByte >= 2)) {
    return raiseInvalidTableData(entry.identity);
  }

  MOZ_TRY(table.initWithSingleValue(cx_, indexByte != 0));
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
MOZ_MUST_USE JS::Result<BinASTKind> HuffmanPreludeReader::readSymbol(
    const MaybeInterface& entry, size_t index) {
  MOZ_ASSERT(index < 2);
  return index == 0 ? BinASTKind::_Null : entry.kind;
}

// Reading a single-value table of optional interfaces
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeInterface>(
    MaybeInterface::Table& table, const MaybeInterface& entry) {
  uint8_t indexByte;
  MOZ_TRY_VAR(indexByte, reader.readByte<Compression::No>());
  if (MOZ_UNLIKELY(indexByte >= 2)) {
    return raiseInvalidTableData(entry.identity);
  }

  MOZ_TRY(table.initWithSingleValue(
      cx_, indexByte == 0 ? BinASTKind::_Null : entry.kind));
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
MOZ_MUST_USE JS::Result<BinASTKind> HuffmanPreludeReader::readSymbol(
    const Sum& entry, size_t index) {
  MOZ_ASSERT(index < entry.maxNumberOfSymbols());
  return entry.interfaceAt(index);
}

// Reading a single-value table of sums of interfaces.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<Sum>(
    HuffmanTableIndexedSymbolsSum& table, const Sum& sum) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index >= sum.maxNumberOfSymbols())) {
    return raiseInvalidTableData(sum.identity);
  }

  MOZ_TRY(table.initWithSingleValue(cx_, sum.interfaceAt(index)));
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
MOZ_MUST_USE JS::Result<BinASTKind> HuffmanPreludeReader::readSymbol(
    const MaybeSum& sum, size_t index) {
  MOZ_ASSERT(index < sum.maxNumberOfSymbols());
  return sum.interfaceAt(index);
}

// Reading a single-value table of sums of interfaces.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeSum>(
    HuffmanTableIndexedSymbolsSum& table, const MaybeSum& sum) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index >= sum.maxNumberOfSymbols())) {
    return raiseInvalidTableData(sum.identity);
  }

  MOZ_TRY(table.initWithSingleValue(cx_, sum.interfaceAt(index)));
  return Ok();
}

// ------ Numbers
// 64 bits, IEEE 754, big endian

// Read the number of symbols from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const Number& number) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS)) {
    return raiseInvalidTableData(number.identity);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<double> HuffmanPreludeReader::readSymbol(
    const Number& number, size_t) {
  uint8_t bytes[8];
  MOZ_ASSERT(sizeof(bytes) == sizeof(double));

  uint32_t len = mozilla::ArrayLength(bytes);
  MOZ_TRY((reader.readBuf<Compression::No, EndOfFilePolicy::RaiseError>(
      reinterpret_cast<uint8_t*>(bytes), len)));

  // Decode big-endian.
  const uint64_t asInt = mozilla::BigEndian::readUint64(bytes);

  // Canonicalize NaN, just to make sure another form of signalling NaN
  // doesn't slip past us.
  return JS::CanonicalizeNaN(mozilla::BitwiseCast<double>(asInt));
}

// Reading a single-value table of numbers.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<Number>(
    HuffmanTableExplicitSymbolsF64& table, const Number& number) {
  BINJS_MOZ_TRY_DECL(value, readSymbol(number, 0 /* ignored */));
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  MOZ_TRY(table.initWithSingleValue(
      cx_,
      /* NOLINT(performance-move-const-arg) */ std::move(value)));
  return Ok();
}

// ------ List lengths
// varnum

// Read the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const List& list) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS)) {
    return raiseInvalidTableData(list.identity);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readSymbol(
    const List& list, size_t) {
  BINJS_MOZ_TRY_DECL(length, reader.readUnpackedLong());
  if (MOZ_UNLIKELY(length > MAX_LIST_LENGTH)) {
    return raiseInvalidTableData(list.identity);
  }
  return length;
}

// Reading a single-value table of list lengths.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<List>(
    HuffmanTableExplicitSymbolsListLength& table, const List& list) {
  BINJS_MOZ_TRY_DECL(length, reader.readUnpackedLong());
  if (MOZ_UNLIKELY(length > MAX_LIST_LENGTH)) {
    return raiseInvalidTableData(list.identity);
  }
  MOZ_TRY(table.initWithSingleValue(cx_, std::move(length)));
  return Ok();
}

// ------ Strings, non-nullable
// varnum (index)

// Read the number of symbols from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const String& string) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS ||
                   length > reader.metadata_->numStrings())) {
    return raiseInvalidTableData(string.identity);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<JSAtom*> HuffmanPreludeReader::readSymbol(
    const String& entry, size_t) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > reader.metadata_->numStrings())) {
    return raiseInvalidTableData(entry.identity);
  }
  return reader.metadata_->getAtom(index);
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<String>(
    HuffmanTableIndexedSymbolsLiteralString& table, const String& entry) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > reader.metadata_->numStrings())) {
    return raiseInvalidTableData(entry.identity);
  }
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  JSAtom* value = reader.metadata_->getAtom(index);
  MOZ_TRY(table.initWithSingleValue(
      cx_,
      /* NOLINT(performance-move-const-arg) */ std::move(value)));
  return Ok();
}

// ------ Optional strings
// varnum 0 -> null
// varnum i > 0 -> string at index i - 1

// Read the number of symbols from the metadata.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const MaybeString& entry) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS ||
                   length > reader.metadata_->numStrings() + 1)) {
    return raiseInvalidTableData(entry.identity);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<JSAtom*> HuffmanPreludeReader::readSymbol(
    const MaybeString& entry, size_t) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (index == 0) {
    return nullptr;
  }
  if (MOZ_UNLIKELY(index > reader.metadata_->numStrings() + 1)) {
    return raiseInvalidTableData(entry.identity);
  }
  return reader.metadata_->getAtom(index - 1);
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeString>(
    HuffmanTableIndexedSymbolsOptionalLiteralString& table,
    const MaybeString& entry) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > reader.metadata_->numStrings() + 1)) {
    return raiseInvalidTableData(entry.identity);
  }
  JSAtom* symbol = index == 0 ? nullptr : reader.metadata_->getAtom(index - 1);
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  MOZ_TRY(table.initWithSingleValue(
      cx_,
      /* NOLINT(performance-move-const-arg) */ std::move(symbol)));
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
MOZ_MUST_USE JS::Result<BinASTVariant> HuffmanPreludeReader::readSymbol(
    const StringEnum& entry, size_t index) {
  return entry.variantAt(index);
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<StringEnum>(
    HuffmanTableIndexedSymbolsStringEnum& table, const StringEnum& entry) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(index > entry.maxNumberOfSymbols())) {
    return raiseInvalidTableData(entry.identity);
  }

  BinASTVariant symbol = entry.variantAt(index);
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  MOZ_TRY(table.initWithSingleValue(
      cx_,
      /* NOLINT(performance-move-const-arg) */ std::move(symbol)));
  return Ok();
}

// ------ Unsigned Longs
// Unpacked 32-bit

// Read the number of symbols from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const UnsignedLong& entry) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (MOZ_UNLIKELY(length > MAX_NUMBER_OF_SYMBOLS)) {
    return raiseInvalidTableData(entry.identity);
  }
  return length;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readSymbol(
    const UnsignedLong& entry, size_t) {
  return reader.readUnpackedLong();
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<UnsignedLong>(
    HuffmanTableExplicitSymbolsU32& table, const UnsignedLong& entry) {
  BINJS_MOZ_TRY_DECL(index, reader.readUnpackedLong());
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  MOZ_TRY(table.initWithSingleValue(
      cx_,
      /* NOLINT(performance-move-const-arg) */ std::move(index)));
  return Ok();
}

HuffmanDictionary::HuffmanDictionary(JSContext* cx)
    : fields(BINAST_PARAM_NUMBER_OF_INTERFACE_AND_FIELD(
          mozilla::AsVariant(HuffmanTableUnreachable()))),
      listLengths(BINAST_PARAM_NUMBER_OF_LIST_TYPES(
          mozilla::AsVariant(HuffmanTableUnreachable()))) {}

HuffmanTableValue& HuffmanDictionary::tableForField(
    NormalizedInterfaceAndField index) {
  return fields[static_cast<size_t>(index.identity)];
}

HuffmanTableListLength& HuffmanDictionary::tableForListLength(BinASTList list) {
  return listLengths[static_cast<size_t>(list)];
}

uint32_t HuffmanLookup::leadingBits(const uint8_t aBitLength) const {
  MOZ_ASSERT(aBitLength <= this->bitLength);
  const uint32_t result =
      (aBitLength == 0) ? 0  // Shifting a uint32_t by 32 bits is UB.
                        : this->bits >> uint32_t(this->bitLength - aBitLength);
  return result;
}

Split<HuffmanLookup> HuffmanLookup::split(const uint8_t prefixLength) const {
  if (bitLength <= prefixLength) {
    // Not enough bits, pad with zeros.
    return {
        /* prefix: HuffmanLookup */ {bits << (prefixLength - bitLength),
                                     prefixLength},
        /* suffix: HuffmanLookup */ {0, 0},
    };
  }

  // Keep `prefixLength` bits from `bits`.
  // Pad the rest with 0s to build the suffix.
  const uint8_t shift = bitLength - prefixLength;
  switch (shift) {
    case 0:  // Special case, as we can't >> 32
      return {
          /* prefix: HuffmanLookup */ {bits, prefixLength},
          /* suffix: HuffmanLookup */ {0, 0},
      };
    case 32:  // Special case, as we can't >> 32
      return {
          /* prefix: HuffmanLookup */ {0, prefixLength},
          /* suffix: HuffmanLookup */ {bits, shift},
      };
  }
  return {
      /* prefix: HuffmanLookup */ {bits >> shift, prefixLength},
      /* suffix: HuffmanLookup */
      {bits & (mozilla::MaxValue<uint32_t>::value >> (32 - shift)), shift},
  };
}

mozilla::detail::IntegerRange<size_t> HuffmanLookup::suffixes(
    uint8_t expectedBitLength) const {
  if (expectedBitLength <= bitLength) {
    // We have too many bits, we need to truncate the HuffmanLookup,
    // then return a single element.
    const uint8_t shearing = bitLength - expectedBitLength;
    const size_t first = size_t(bits) >> shearing;
    const size_t last = first;
    return mozilla::IntegerRange<size_t>(first, last + 1);
  }

  // We need to pad with lower-weight 0s.
  const uint8_t padding = expectedBitLength - bitLength;
  const size_t first = bits << padding;
  const size_t last = first + (size_t(-1) >> (8 * sizeof(size_t) - padding));
  return mozilla::IntegerRange<size_t>(first, last + 1);
}

}  // namespace frontend

}  // namespace js
