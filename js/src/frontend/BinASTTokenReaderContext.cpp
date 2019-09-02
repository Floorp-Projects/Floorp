/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTTokenReaderContext.h"

#include "mozilla/Result.h"     // MOZ_TRY*
#include "mozilla/ScopeExit.h"  // mozilla::MakeScopeExit

#include <string.h>  // memchr, memcmp, memmove

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

using AutoList = BinASTTokenReaderContext::AutoList;
using AutoTaggedTuple = BinASTTokenReaderContext::AutoTaggedTuple;
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
        auxStorageBitLengths(cx_) {}

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
    explicit EntryIndexed(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  // A string.
  // May be a literal string, identifier name or property key. May not be null.
  struct String : EntryBase {
    using SymbolType = JSAtom*;
    using Table = HuffmanTableIndexedSymbolsLiteralString;
    explicit String(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };
  using IdentifierName = String;
  using PropertyKey = String;

  // An optional string.
  // May be a literal string, identifier name or property key.
  struct MaybeString : EntryBase {
    using SymbolType = JSAtom*;
    using Table = HuffmanTableIndexedSymbolsOptionalLiteralString;
    explicit MaybeString(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };
  using MaybeIdentifierName = MaybeString;
  using MaybePropertyKey = MaybeString;

  // A JavaScript number. May not be null.
  struct Number : EntryBase {
    using SymbolType = double;
    using Table = HuffmanTableExplicitSymbolsF64;
    explicit Number(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  // A 32-bit integer. May not be null.
  struct UnsignedLong : EntryBase {
    explicit UnsignedLong(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  // A boolean. May not be null.
  struct Boolean : EntryIndexed {
    using SymbolType = bool;
    using Table = HuffmanTableIndexedSymbolsBool;

    explicit Boolean(const NormalizedInterfaceAndField identity)
        : EntryIndexed(identity) {}
  };

  // A field encoding a lazy offset.
  struct Lazy : EntryBase {
    explicit Lazy(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  // A value of a given interface. May not be null.
  struct Interface : EntryBase {
    // The kind of the interface.
    const BinASTKind kind;
    Interface(const NormalizedInterfaceAndField identity, BinASTKind kind)
        : EntryBase(identity), kind(kind) {}

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
    using SymbolType = Nullable;
    using Table = HuffmanTableIndexedSymbolsMaybeInterface;
    // The kind of the interface.
    const BinASTKind kind;

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
  struct List : EntryBase {
    // The symbol type for the length of the list.
    using SymbolType = uint32_t;

    // The table for the length of the list.
    using Table = HuffmanTableExplicitSymbolsListLength;

    // The type of the list, e.g. list of numbers.
    // All lists with the same type share a model for their length.
    const BinASTList contents;

    List(const NormalizedInterfaceAndField identity, const BinASTList contents)
        : EntryBase(identity), contents(contents) {}

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
  struct MaybeSum : EntryBase {
    // The type of symbols for this entry.
    // We use `BinASTKind::_Null` to represent the null case.
    using SymbolType = BinASTKind;
    // The type of table used for this entry.
    // We use `BinASTKind::_Null` to represent the null case.
    using Table = HuffmanTableIndexedSymbolsSum;

    // The type of values in the sum.
    const BinASTSum contents;

    MaybeSum(const NormalizedInterfaceAndField identity,
             const BinASTSum contents)
        : EntryBase(identity), contents(contents) {}

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
  struct StringEnum : EntryBase {
    using SymbolType = BinASTVariant;
    using Table = HuffmanTableIndexedSymbolsStringEnum;

    // The values in the enum.
    const BinASTStringEnum contents;
    StringEnum(const NormalizedInterfaceAndField identity,
               const BinASTStringEnum contents)
        : EntryBase(identity), contents(contents) {}

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

  // The entries in the grammar.
  using Entry = mozilla::Variant<
      // Primitives
      Boolean, String, MaybeString, Number, UnsignedLong, Lazy,
      // Combinators
      Interface, MaybeInterface, List, Sum, MaybeSum, StringEnum>;

  // A stack of entries to process. We always process the latest entry added.
  Vector<Entry> stack;

  // Enqueue an entry to the stack.
  MOZ_MUST_USE JS::Result<Ok> pushValue(NormalizedInterfaceAndField identity,
                                        const Entry& entry) {
    if (!dictionary.tableForField(identity).is<HuffmanTableUnreachable>()) {
      // Entry already initialized.
      return Ok();
    }
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
#define EMIT_FIELD(TAG_NAME, FIELD_NAME, FIELD_INDEX, FIELD_TYPE, _)    \
  MOZ_TRY(                                                              \
      pushValue(NormalizedInterfaceAndField(                            \
                    BinASTInterfaceAndField::TAG_NAME##__##FIELD_NAME), \
                Entry(FIELD_TYPE(NormalizedInterfaceAndField(           \
                    BinASTInterfaceAndField::TAG_NAME##__##FIELD_NAME)))));
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
  // type. See the documentation of each `EntryMatcher::operator()` for a
  // discussion on each order.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readMultipleValuesTable(
      typename Entry::Table& table, Entry entry) {
    // Get the number of symbols.
    // If `Entry` is an indexed type, this is fetched from the grammar.
    BINJS_MOZ_TRY_DECL(numberOfSymbols, readNumberOfSymbols<Entry>(entry));

    MOZ_ASSERT(numberOfSymbols <= MAX_NUMBER_OF_SYMBOLS);

    // Data is presented in an order that doesn't match our memory
    // representation, so we need to copy `numberOfSymbols` entries.
    // We use an auxiliary vector to avoid allocating each time.
    MOZ_ASSERT(
        auxStorageBitLengths.empty());  // We must have cleaned it up properly.
    BINJS_TRY(auxStorageBitLengths.reserve(numberOfSymbols + 1));

    // First read the bit lengths for all symbols.
    for (size_t i = 0; i < numberOfSymbols; ++i) {
      BINJS_MOZ_TRY_DECL(bitLength, reader.readByte<Compression::No>());
      if (bitLength == 0) {
        // A value with a bits length of 0? That makes no sense.
        return raiseInvalidTableData(entry.identity);
      }
      BINJS_TRY(auxStorageBitLengths.append(bitLength));
    }
    // Append a terminator.
    BINJS_TRY(auxStorageBitLengths.append(MAX_CODE_BIT_LENGTH));

    // Now read the symbols and assign bits.
    uint32_t code = 0;
    MOZ_TRY(table.impl.init(cx_, numberOfSymbols));

    for (size_t i = 0; i < numberOfSymbols; ++i) {
      const auto bitLength = auxStorageBitLengths[i];
      const auto nextBitLength =
          auxStorageBitLengths[i + 1];  // Valid thanks to the terminator.
      if (bitLength > nextBitLength) {
        // By format invariant, bit lengths are always ranked by increasing
        // order.
        return raiseInvalidTableData(entry.identity);
      }

      // Read the symbol.
      // If `Entry` is an indexed type, it is fetched directly from the grammar.
      BINJS_MOZ_TRY_DECL(symbol, readSymbol<Entry>(entry, i));

      MOZ_TRY(table.impl.addSymbol(code, bitLength, std::move(symbol)));

      code = (code + 1) << (nextBitLength - bitLength);
    }

    auxStorageBitLengths.clear();
    return Ok();
  }

  // Single-argument version: lookup the table using `dictionary.tableForField`
  // then proceed as two-arguments version.
  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readTable(Entry entry) {
    auto& table = dictionary.tableForField(entry.identity);
    return readTable<HuffmanTable, Entry>(table, entry);
  }

  // Two-arguments version: pass table explicitly. Generally called from single-
  // argument version, but may be called manually, e.g. for list lengths, as
  // their tables don't field in `dictionary.tableForField`.
  template <typename HuffmanTable, typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readTable(HuffmanTable& table, Entry entry) {
    if (!table.template is<HuffmanTableUnreachable>()) {
      // We're attempting to re-read a table that has already been read.
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
  // An auxiliary storage of bit lengths.
  // Used to avoid (re)allocating a vector of bit lengths each time we read a
  // table.
  Vector<uint8_t> auxStorageBitLengths;

  // Implementation of pattern-matching cases for `entry.match`.
  struct EntryMatcher {
    // The owning HuffmanPreludeReader.
    HuffmanPreludeReader& owner;

    explicit EntryMatcher(HuffmanPreludeReader& owner) : owner(owner) {}

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
      if (table.is<HuffmanTableUnreachable>()) {
        return Ok();
      }
      const auto& tableRef = table.as<HuffmanTableIndexedSymbolsSum>();

      for (const auto& kind : tableRef.impl) {
        MOZ_TRY(owner.pushValue(
            entry.identity,
            {mozilla::VariantType<Interface>(), entry.identity, kind.value}));
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

      for (const auto& kind : tableRef.impl) {
        MOZ_TRY(owner.pushValue(
            entry.identity,
            {mozilla::VariantType<Interface>(), entry.identity, kind.value}));
      }
      return Ok();
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const Number& entry) {
      return owner.readTable<Number>(entry);
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const List& entry) {
      // Read the model for the length of the list.
      MOZ_TRY((owner.readTable<HuffmanTableListLength, List>(
          owner.dictionary.tableForListLength(entry.contents), entry)));

      // Now read the model for the contents of the list.
      switch (entry.contents) {
#define EMIT_CASE(LIST_NAME, _CONTENT_NAME, _HUMAN_NAME, TYPE) \
  case BinASTList::LIST_NAME:                                  \
    return owner.pushValue(entry.identity, Entry(TYPE(entry.identity)));

        FOR_EACH_BIN_LIST(EMIT_CASE, WRAP_PRIMITIVE, WRAP_INTERFACE,
                          WRAP_MAYBE_INTERFACE, WRAP_LIST, WRAP_SUM,
                          WRAP_MAYBE_SUM, WRAP_STRING_ENUM,
                          WRAP_MAYBE_STRING_ENUM)
#undef EMIT_CASE
      }
      return Ok();
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
      // FIXME: Read table.
      // FIXME: Initialize table.
      MOZ_CRASH("Unimplemented");
      return Ok();
    }
  };

  template <typename T>
  using ErrorResult = BinASTTokenReaderBase::ErrorResult<T>;

  MOZ_MUST_USE ErrorResult<JS::Error&> raiseDuplicateTableError(
      const NormalizedInterfaceAndField identity) {
    MOZ_CRASH("FIXME: Implement");
    return reader.raiseError("Duplicate table.");
  }

  MOZ_MUST_USE ErrorResult<JS::Error&> raiseInvalidTableData(
      const NormalizedInterfaceAndField identity) {
    MOZ_CRASH("FIXME: Implement");
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

  if (version != MAGIC_FORMAT_VERSION) {
    return raiseError("Format version not implemented");
  }
#endif  // BINAST_CX_MAGIC_HEADER

  MOZ_TRY(readStringPrelude());
  MOZ_TRY(readHuffmanPrelude());

  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::readStringPrelude() {
  BINJS_MOZ_TRY_DECL(stringsNumberOfEntries, readVarU32<Compression::No>());

  const uint32_t MAX_NUMBER_OF_STRINGS = 32768;

  if (stringsNumberOfEntries > MAX_NUMBER_OF_STRINGS) {
    return raiseError("Too many entries in strings dictionary");
  }

  // FIXME: We don't use the global table like multipart format, but
  //        (interface, field)-local dictionary.
  //        Metadata should be fixed to store that.
  Vector<BinASTKind> binASTKinds(cx_);

  BinASTSourceMetadata* metadata =
      BinASTSourceMetadata::Create(binASTKinds, stringsNumberOfEntries);
  if (!metadata) {
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
    if (current_ >= stop_) {
      return raiseError("End of file reached while reading strings table");
    }

    const uint8_t* end =
        static_cast<const uint8_t*>(memchr(current_, '\0', stop_ - current_));
    if (!end) {
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
HuffmanLookup BinASTTokenReaderContext::BitBuffer::getHuffmanLookup() {
  // Only keep the leading 32 bits.
  const uint8_t bitLength =
      std::min<uint8_t>(this->bitLength, MAX_PREFIX_BIT_LENGTH);
  const uint32_t bitsPrefix = bits & (uint64_t)0x00000000FFFFFFFF;
  return HuffmanLookup(bitsPrefix, bitLength);
}

template <>
MOZ_MUST_USE JS::Result<Ok>
BinASTTokenReaderContext::BitBuffer::advanceBitBuffer<Compression::No>(
    BinASTTokenReaderContext& owner, const uint8_t bitLength) {
  // It should be impossible to call `advanceBitBuffer`
  // with more bits than what we just handed out.
  MOZ_ASSERT(bitLength <= this->bitLength);

  // The algorithm is not intuitive, so consider an example, where the byte
  // stream starts with `0b_HGFE_DCBA`, `0b_PONM_LKJI`, `0b_XWVU_TRSQ` (to keep
  // things concise, in the example, we won't use the entire 64 bits).
  //
  // In each byte, bits are stored in the reverse order, so what we want
  // is `0b_ABCD_EFGH`, `0b_IJML_MNOP`, `0b_QRST_UVWX`.
  // For the example, let's assume that we have already read
  // `0b_ABCD_EFGH`, `0b_IJKL_MNOP` into `bits`, so before the call to
  // `advanceBitBuffer`, `bits` initially contains
  // `0b_XXXX_XXXX__XXXX_XXXX__ABCD_EFGH__IJKL_MNOP`, where `X` are bits that
  // are beyond `this->bitLength`

  // 1. We have consumed a few bits from the bit buffer, say `ABC`.
  // `bits` is now `0b_XXXX_XXXX__XXXX_XXXX__XXXD_EFGH__IJKL_MNOP`.
  this->bitLength -= bitLength;

  if (this->bitLength <= MAX_PREFIX_BIT_LENGTH) {
    // Keys can be up to MAX_PREFIX_BIT_LENGTH bits long. If we have fewer bits
    // available, it's time to reload. We'll try and get as close to 64 bits as
    // possible.

    while (this->bitLength <= BIT_BUFFER_SIZE - BIT_BUFFER_READ_UNIT) {
      // Let's try and pull one byte.
      uint8_t byte;
      uint32_t readLen = 1;
      MOZ_TRY((owner.readBuf<Compression::No, EndOfFilePolicy::BestEffort>(
          &byte, readLen)));
      if (readLen < 1) {
        // Ok, nothing left to read.
        break;
      }

      // 2. We have just read to `byte`, here `0b_XWVU_TSRQ`. Let's reverse
      // `byte` into `0b_QRST_UVWX`.
      const uint8_t reversedByte =
          (byte & 0b10000000) >> 7 | (byte & 0b01000000) >> 5 |
          (byte & 0b00100000) >> 3 | (byte & 0b00010000) >> 1 |
          (byte & 0b00001000) << 1 | (byte & 0b00000100) << 3 |
          (byte & 0b00000010) << 5 | (byte & 0b00000001) << 7;

      // 3. Make space for these bits at the end of the stream
      // so shift `bits` into
      // `0b_XXXX_XXXX__XXXD_EFGH__IJKL_MNOP__0000_0000`.
      this->bits <<= BIT_BUFFER_READ_UNIT;

      // 4. Finally, combine into.
      // `0b_XXXX_XXXX__XXXD_EFGH__IJKL_MNOP__QRST_UVWX`.
      this->bits += reversedByte;
      this->bitLength += BIT_BUFFER_READ_UNIT;
      MOZ_ASSERT(bits >> this->bitLength == 0);

      // 4. Continue as long as we don't have enough bits.
    }
  }

  return Ok();
}

void BinASTTokenReaderContext::traceMetadata(JSTracer* trc) {
  if (metadata_) {
    metadata_->trace(trc);
  }
}

MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&>
BinASTTokenReaderContext::raiseInvalidValue(const Context&) {
  errorReporter_->errorNoOffset(JSMSG_BINAST, "Invalid value");
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

template <typename Table>
JS::Result<typename Table::Contents>
BinASTTokenReaderContext::readFieldFromTable(const Context& context) {
  // Extract the table.
  BinASTInterfaceAndField identity =
      context.match(ExtractBinASTInterfaceAndFieldMatcher());
  const auto& table =
      dictionary.tableForField(NormalizedInterfaceAndField(identity));
  const auto lookup = table.as<Table>().impl.lookup(
      bitBuffer.getHuffmanLookup<Compression::No>());
  MOZ_TRY(
      bitBuffer.advanceBitBuffer<Compression::No>(*this, lookup.key.bitLength));
  if (!lookup.value) {
    return raiseInvalidValue(context);
  }
  return *lookup.value;
}

JS::Result<bool> BinASTTokenReaderContext::readBool(const Context& context) {
  return readFieldFromTable<HuffmanTableIndexedSymbolsBool>(context);
}

JS::Result<double> BinASTTokenReaderContext::readDouble(
    const Context& context) {
  return readFieldFromTable<HuffmanTableExplicitSymbolsF64>(context);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeAtom(
    const Context& context) {
  return readFieldFromTable<HuffmanTableIndexedSymbolsOptionalLiteralString>(
      context);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readAtom(const Context& context) {
  return readFieldFromTable<HuffmanTableIndexedSymbolsLiteralString>(context);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeIdentifierName(
    const Context& context) {
  return readMaybeAtom(context);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readIdentifierName(
    const Context& context) {
  return readAtom(context);
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readPropertyKey(
    const Context& context) {
  return readAtom(context);
}

JS::Result<Ok> BinASTTokenReaderContext::readChars(Chars& out, const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<BinASTVariant> BinASTTokenReaderContext::readVariant(
    const Context& context) {
  return readFieldFromTable<HuffmanTableIndexedSymbolsStringEnum>(context);
}

JS::Result<BinASTTokenReaderBase::SkippableSubTree>
BinASTTokenReaderContext::readSkippableSubTree(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::enterTaggedTuple(
    BinASTKind& tag, BinASTTokenReaderContext::BinASTFields&,
    const Context& context, AutoTaggedTuple& guard) {
  if (context.is<BinASTTokenReaderBase::RootContext>()) {
    // For the moment, the format hardcodes `Script` as root.
    tag = BinASTKind::Script;
    return Ok();
  }
  MOZ_TRY_VAR(tag,
              (readFieldFromTable<HuffmanTableIndexedSymbolsSum>(context)));
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::enterList(uint32_t& items,
                                                   const Context& context,
                                                   AutoList& guard) {
  const auto identity =
      context.as<BinASTTokenReaderBase::ListContext>().content;
  const auto& table = dictionary.tableForListLength(identity);
  const auto lookup =
      table.as<HuffmanTableExplicitSymbolsListLength>().impl.lookup(
          bitBuffer.getHuffmanLookup<Compression::No>());
  MOZ_TRY(
      bitBuffer.advanceBitBuffer<Compression::No>(*this, lookup.key.bitLength));
  if (!lookup.value) {
    return raiseInvalidValue(context);
  }
  items = *lookup.value;
  return Ok();
}

void BinASTTokenReaderContext::AutoBase::init() { initialized_ = true; }

BinASTTokenReaderContext::AutoBase::AutoBase(BinASTTokenReaderContext& reader)
    : initialized_(false), reader_(reader) {}

BinASTTokenReaderContext::AutoBase::~AutoBase() {
  // By now, the `AutoBase` must have been deinitialized by calling `done()`.
  // The only case in which we can accept not calling `done()` is if we have
  // bailed out because of an error.
  MOZ_ASSERT_IF(initialized_, reader_.hasRaisedError());
}

JS::Result<Ok> BinASTTokenReaderContext::AutoBase::checkPosition(
    const uint8_t* expectedEnd) {
  return reader_.raiseError("Not Yet Implemented");
}

BinASTTokenReaderContext::AutoList::AutoList(BinASTTokenReaderContext& reader)
    : AutoBase(reader) {}

void BinASTTokenReaderContext::AutoList::init() { AutoBase::init(); }

JS::Result<Ok> BinASTTokenReaderContext::AutoList::done() {
  return reader_.raiseError("Not Yet Implemented");
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
    if (newResult < result) {
      return raiseError("Overflow during readVarU32");
    }

    result = newResult;
    shift += 7;

    if ((byte & 0x80) == 0) {
      return result;
    }

    if (shift >= 32) {
      return raiseError("Overflow during readVarU32");
    }
  }
}

JS::Result<uint32_t> BinASTTokenReaderContext::readUnsignedLong(
    const Context&) {
  return readVarU32<Compression::No>();
}

BinASTTokenReaderContext::AutoTaggedTuple::AutoTaggedTuple(
    BinASTTokenReaderContext& reader)
    : AutoBase(reader) {}

JS::Result<Ok> BinASTTokenReaderContext::AutoTaggedTuple::done() {
  return reader_.raiseError("Not Yet Implemented");
}

template <typename T, int N>
JS::Result<Ok> HuffmanTableImpl<T, N>::initWithSingleValue(JSContext* cx,
                                                           T&& value) {
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  if (!values.append(HuffmanEntry<T>(0, 0, std::move(value)))) {
    return cx->alreadyReportedError();
  }
  return Ok();
}

template <typename T, int N>
JS::Result<Ok> HuffmanTableImpl<T, N>::init(JSContext* cx,
                                            size_t numberOfSymbols) {
  MOZ_ASSERT(values.empty());  // Make sure that we're initializing.
  if (!values.initCapacity(numberOfSymbols)) {
    return cx->alreadyReportedError();
  }
  return Ok();
}

template <typename T, int N>
JS::Result<Ok> HuffmanTableImpl<T, N>::addSymbol(uint32_t bits,
                                                 uint8_t bitLength, T&& value) {
  MOZ_ASSERT(bitLength != 0,
             "Adding a symbol with a bitLength of 0 doesn't make sense.");
  MOZ_ASSERT(values.empty() || values.back().key.bitLength <= bitLength,
             "Symbols must be ranked by increasing bits length");
  MOZ_ASSERT(bits >> bitLength == 0);
  if (!values.emplaceBack(bits, bitLength, std::move(value))) {
    MOZ_CRASH();  // Memory was reserved in `init()`.
  }

  return Ok();
}

template <typename T, int N>
HuffmanEntry<const T*> HuffmanTableImpl<T, N>::lookup(HuffmanLookup key) const {
  // This current implementation is O(length) and designed mostly for testing.
  // Future versions will presumably adapt the underlying data structure to
  // provide bounded-time lookup.
  MOZ_ASSERT(length() > 0);
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

  return HuffmanEntry<const T*>(0, 0, nullptr);
}

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
      FOR_EACH_BIN_VARIANT_IN_STRING_ENUM_##MACRO_NAME##_BY_STRING_ORDER( \
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
    MOZ_TRY(entry.match(EntryMatcher(*this)));
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
  if (indexByte >= 2) {
    return raiseInvalidTableData(entry.identity);
  }

  MOZ_TRY(table.impl.initWithSingleValue(cx_, indexByte != 0));
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
MOZ_MUST_USE JS::Result<Nullable> HuffmanPreludeReader::readSymbol(
    const MaybeInterface&, size_t index) {
  MOZ_ASSERT(index < 2);
  return index == 0 ? Nullable::Null : Nullable::NonNull;
}

// Reading a single-value table of optional interfaces
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeInterface>(
    MaybeInterface::Table& table, const MaybeInterface& entry) {
  uint8_t indexByte;
  MOZ_TRY_VAR(indexByte, reader.readByte<Compression::No>());
  if (indexByte >= 2) {
    return raiseInvalidTableData(entry.identity);
  }

  MOZ_TRY(table.impl.initWithSingleValue(
      cx_, indexByte == 0 ? Nullable::Null : Nullable::NonNull));
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
  if (index >= sum.maxNumberOfSymbols()) {
    return raiseInvalidTableData(sum.identity);
  }

  MOZ_TRY(table.impl.initWithSingleValue(cx_, sum.interfaceAt(index)));
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
  if (index >= sum.maxNumberOfSymbols()) {
    return raiseInvalidTableData(sum.identity);
  }

  MOZ_TRY(table.impl.initWithSingleValue(cx_, sum.interfaceAt(index)));
  return Ok();
}

// ------ Numbers
// 64 bits, IEEE 754, big endian

// Read the number of symbols from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const Number& number) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (length > MAX_NUMBER_OF_SYMBOLS) {
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
  MOZ_TRY(table.impl.initWithSingleValue(
      cx_,
      /* NOLINT(performance-move-const-arg) */ std::move(value)));
  return Ok();
}

// ------ List lengths
// varnum

// Read the number of symbols from the grammar.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const List&) {
  return 1;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readSymbol(
    const List& list, size_t) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (length > MAX_LIST_LENGTH) {
    return raiseInvalidTableData(list.identity);
  }
  return length;
}

// Reading a single-value table of list lengths.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<List>(
    HuffmanTableExplicitSymbolsListLength& table, const List& list) {
  BINJS_MOZ_TRY_DECL(length, reader.readVarU32<Compression::No>());
  if (length > MAX_LIST_LENGTH) {
    return raiseInvalidTableData(list.identity);
  }
  MOZ_TRY(table.impl.initWithSingleValue(cx_, std::move(length)));
  return Ok();
}

// ------ Strings, non-nullable
// varnum (index)

// Read the number of symbols from the metadata.
template <>
MOZ_MUST_USE JS::Result<uint32_t> HuffmanPreludeReader::readNumberOfSymbols(
    const String&) {
  return reader.metadata_->numStrings();
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<JSAtom*> HuffmanPreludeReader::readSymbol(
    const String& entry, size_t) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (index > reader.metadata_->numStrings()) {
    return raiseInvalidTableData(entry.identity);
  }
  return reader.metadata_->getAtom(index);
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok> HuffmanPreludeReader::readSingleValueTable<String>(
    HuffmanTableIndexedSymbolsLiteralString& table, const String& entry) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (index > reader.metadata_->numStrings()) {
    return raiseInvalidTableData(entry.identity);
  }
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  JSAtom* value = reader.metadata_->getAtom(index);
  MOZ_TRY(table.impl.initWithSingleValue(
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
    const MaybeString&) {
  return reader.metadata_->numStrings() + 1;
}

// Read a single symbol from the stream.
template <>
MOZ_MUST_USE JS::Result<JSAtom*> HuffmanPreludeReader::readSymbol(
    const MaybeString& entry, size_t) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (index == 0) {
    return nullptr;
  } else if (index > reader.metadata_->numStrings() + 1) {
    return raiseInvalidTableData(entry.identity);
  } else {
    return reader.metadata_->getAtom(index - 1);
  }
}

// Reading a single-value table of string indices.
template <>
MOZ_MUST_USE JS::Result<Ok>
HuffmanPreludeReader::readSingleValueTable<MaybeString>(
    HuffmanTableIndexedSymbolsOptionalLiteralString& table,
    const MaybeString& entry) {
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32<Compression::No>());
  if (index > reader.metadata_->numStrings() + 1) {
    return raiseInvalidTableData(entry.identity);
  }
  JSAtom* symbol = index == 0 ? nullptr : reader.metadata_->getAtom(index - 1);
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  MOZ_TRY(table.impl.initWithSingleValue(
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
  if (index > entry.maxNumberOfSymbols()) {
    return raiseInvalidTableData(entry.identity);
  }

  BinASTVariant symbol = entry.variantAt(index);
  // Note: The `std::move` is useless for performance, but necessary to keep
  // a consistent API.
  MOZ_TRY(table.impl.initWithSingleValue(
      cx_,
      /* NOLINT(performance-move-const-arg) */ std::move(symbol)));
  return Ok();
}

HuffmanTable& HuffmanDictionary::tableForField(
    NormalizedInterfaceAndField index) {
  return fields[static_cast<size_t>(index.identity)];
}

HuffmanTableListLength& HuffmanDictionary::tableForListLength(BinASTList list) {
  return listLengths[static_cast<size_t>(list)];
}

uint32_t HuffmanLookup::leadingBits(const uint8_t bitLength) const {
  MOZ_ASSERT(bitLength <= this->bitLength);
  return this->bits >> (32 - bitLength);
}

}  // namespace frontend

}  // namespace js
