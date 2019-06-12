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

// The magic header, at the start of every binjs file.
const char CX_MAGIC_HEADER[] =
    "\x89"
    "BJS\r\n\0\n";

// The latest format version understood by this tokenizer.
const uint32_t MAGIC_FORMAT_VERSION = 2;

// The maximal length of a single Huffman code, in bits.
//
// Hardcoded in the format.
const uint8_t MAX_CODE_BIT_LENGTH = 20;

// Hardcoded limit to avoid allocating too eagerly.
const uint32_t MAX_NUMBER_OF_SYMBOLS = 32768;

// The number of elements in each sum.
const size_t SUM_LIMITS[];
const BinASTKind* SUM_RESOLUTIONS[];

using AutoList = BinASTTokenReaderContext::AutoList;
using AutoTaggedTuple = BinASTTokenReaderContext::AutoTaggedTuple;
using CharSlice = BinaryASTSupport::CharSlice;
using Chars = BinASTTokenReaderContext::Chars;

BinASTTokenReaderContext::BinASTTokenReaderContext(JSContext* cx,
                                                   ErrorReporter* er,
                                                   const uint8_t* start,
                                                   const size_t length)
    : BinASTTokenReaderBase(cx, er, start, length),
      metadata_(nullptr),
      posBeforeTree_(nullptr) {
  MOZ_ASSERT(er);
}

BinASTTokenReaderContext::~BinASTTokenReaderContext() {
  if (metadata_ && metadataOwned_ == MetadataOwnership::Owned) {
    UniqueBinASTSourceMetadataPtr ptr(metadata_);
  }
  if (decoder_) {
    BrotliDecoderDestroyInstance(decoder_);
  }
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::readBuf<BinASTTokenReaderContext::Compression::No>(
    uint8_t* bytes, uint32_t len) {
  return Base::readBuf(bytes, len);
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::readBuf<BinASTTokenReaderContext::Compression::Yes>(
    uint8_t* bytes, uint32_t len) {
  while (availableDecodedLength() < len) {
    if (availableDecodedLength()) {
      memmove(bytes, decodedBufferBegin(), availableDecodedLength());
      bytes += availableDecodedLength();
      len -= availableDecodedLength();
    }

    MOZ_TRY(fillDecodedBuf());
  }

  memmove(bytes, decodedBufferBegin(), len);
  advanceDecodedBuffer(len);
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::fillDecodedBuf() {
  if (isEOF()) {
    return raiseError("Unexpected end of file");
  }

  MOZ_ASSERT(!availableDecodedLength());

  decodedBegin_ = 0;

  size_t inSize = stop_ - current_;
  size_t outSize = DECODED_BUFFER_SIZE;
  uint8_t* out = decodedBuffer_;

  BrotliDecoderResult result;
  result = BrotliDecoderDecompressStream(decoder_, &inSize, &current_, &outSize,
                                         &out,
                                         /* total_out = */ nullptr);
  if (result == BROTLI_DECODER_RESULT_ERROR) {
    return raiseError("Failed to decompress brotli stream");
  }

  decodedEnd_ = out - decodedBuffer_;

  return Ok();
}

void BinASTTokenReaderContext::advanceDecodedBuffer(uint32_t count) {
  MOZ_ASSERT(decodedBegin_ + count <= decodedEnd_);
  decodedBegin_ += count;
}

bool BinASTTokenReaderContext::isEOF() const {
  return BrotliDecoderIsFinished(decoder_);
}

template <>
JS::Result<uint8_t> BinASTTokenReaderContext::readByte<
    BinASTTokenReaderContext::Compression::No>() {
  return Base::readByte();
}

template <>
JS::Result<uint8_t> BinASTTokenReaderContext::readByte<
    BinASTTokenReaderContext::Compression::Yes>() {
  uint8_t buf;
  MOZ_TRY(readBuf<Compression::Yes>(&buf, 1));
  return buf;
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

  // Read global headers.
  MOZ_TRY(readConst(CX_MAGIC_HEADER));
  BINJS_MOZ_TRY_DECL(version, readVarU32<Compression::No>());

  if (version != MAGIC_FORMAT_VERSION) {
    return raiseError("Format version not implemented");
  }

  decoder_ = BrotliDecoderCreateInstance(/* alloc_func = */ nullptr,
                                         /* free_func = */ nullptr,
                                         /* opaque = */ nullptr);
  if (!decoder_) {
    return raiseError("Failed to create brotli decoder");
  }

  MOZ_TRY(readStringPrelude());

  // TODO: handle models here.

  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::readStringPrelude() {
  BINJS_MOZ_TRY_DECL(stringsNumberOfEntries, readVarU32<Compression::Yes>());

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
    size_t len;
    while (true) {
      if (availableDecodedLength() == 0) {
        MOZ_TRY(fillDecodedBuf());
      }

      MOZ_ASSERT(availableDecodedLength() > 0);

      const uint8_t* end = static_cast<const uint8_t*>(
          memchr(decodedBufferBegin(), '\0', availableDecodedLength()));
      if (end) {
        // Found the end of current string.
        len = end - decodedBufferBegin();
        break;
      }

      BINJS_TRY(buf.append(decodedBufferBegin(), availableDecodedLength()));
      advanceDecodedBuffer(availableDecodedLength());
    }

    // FIXME: handle 0x01-escaped string here.

    if (!buf.length()) {
      // If there's no partial string in the buffer, bypass it.
      const char* begin = reinterpret_cast<const char*>(decodedBufferBegin());
      BINJS_TRY_VAR(atom, AtomizeWTF8Chars(cx_, begin, len));
    } else {
      BINJS_TRY(
          buf.append(reinterpret_cast<const char*>(decodedBufferBegin()), len));

      const char* begin = reinterpret_cast<const char*>(buf.rawLatin1Begin());
      len = buf.length();

      // We cannot use `StringBuffer::finishAtom` because the string is WTF-8.
      BINJS_TRY_VAR(atom, AtomizeWTF8Chars(cx_, begin, len));

      buf.clear();
    }

    // FIXME: We don't populate `slicesTable_` given we won't use it.
    //        Update BinASTSourceMetadata to reflect this.

    metadata->getAtom(stringIndex) = atom;

    // +1 for skipping 0x00.
    advanceDecodedBuffer(len + 1);
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

void BinASTTokenReaderContext::traceMetadata(JSTracer* trc) {
  if (metadata_) {
    metadata_->trace(trc);
  }
}

JS::Result<bool> BinASTTokenReaderContext::readBool(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<double> BinASTTokenReaderContext::readDouble(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeAtom(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readAtom(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeIdentifierName(
    const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readIdentifierName(
    const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readPropertyKey(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::readChars(Chars& out, const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<BinASTVariant> BinASTTokenReaderContext::readVariant(
    const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<BinASTTokenReaderBase::SkippableSubTree>
BinASTTokenReaderContext::readSkippableSubTree(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::enterTaggedTuple(
    BinASTKind& tag, BinASTTokenReaderContext::BinASTFields&, const Context&,
    AutoTaggedTuple& guard) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::enterList(uint32_t& items,
                                                   const Context&,
                                                   AutoList& guard) {
  return raiseError("Not Yet Implemented");
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

template <BinASTTokenReaderContext::Compression compression>
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
  return readVarU32<Compression::Yes>();
}

BinASTTokenReaderContext::AutoTaggedTuple::AutoTaggedTuple(
    BinASTTokenReaderContext& reader)
    : AutoBase(reader) {}

JS::Result<Ok> BinASTTokenReaderContext::AutoTaggedTuple::done() {
  return reader_.raiseError("Not Yet Implemented");
}

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
  MOZ_MUST_USE JS::Result<Ok> run(size_t initialCapacity) {
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

 private:
  JSContext* cx_;
  BinASTTokenReaderContext& reader;

  // The dictionary we're currently constructing.
  HuffmanDictionary& dictionary;

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
    String(const NormalizedInterfaceAndField identity) : EntryBase(identity) {}
  };
  using IdentifierName = String;
  using PropertyKey = String;

  // An optional string.
  // May be a literal string, identifier name or property key.
  struct MaybeString : EntryBase {
    MaybeString(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };
  using MaybeIdentifierName = MaybeString;
  using MaybePropertyKey = MaybeString;

  // A JavaScript number. May not be null.
  struct Number : EntryBase {
    Number(const NormalizedInterfaceAndField identity) : EntryBase(identity) {}
  };

  // A 32-bit integer. May not be null.
  struct UnsignedLong : EntryBase {
    UnsignedLong(const NormalizedInterfaceAndField identity)
        : EntryBase(identity) {}
  };

  // A boolean. May not be null.
  struct Boolean : EntryIndexed {
    using SymbolType = bool;
    using Table = HuffmanTableIndexedSymbolsBool;

    Boolean(const NormalizedInterfaceAndField identity)
        : EntryIndexed(identity) {}
  };

  // A field encoding a lazy offset.
  struct Lazy : EntryBase {
    Lazy(const NormalizedInterfaceAndField identity) : EntryBase(identity) {}
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
      Maker(BinASTKind kind) : kind(kind) {}
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
      Maker(BinASTKind kind) : kind(kind) {}
      MaybeInterface operator()(const NormalizedInterfaceAndField identity) {
        return MaybeInterface(identity, kind);
      }
    };
  };

  // A FrozenArray. May not be null.
  struct List : EntryBase {
    // The type of the list, e.g. list of numbers.
    // All lists with the same type share a model/
    const BinASTList contents;
    List(const NormalizedInterfaceAndField identity, const BinASTList contents)
        : EntryBase(identity), contents(contents) {}

    // Utility struct, used in macros to call the constructor as
    // `List::Maker(kind)(identity)`.
    struct Maker {
      const BinASTList contents;
      Maker(BinASTList contents) : contents(contents) {}
      List operator()(const NormalizedInterfaceAndField identity) {
        return List(identity, contents);
      }
    };
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
      Maker(BinASTSum contents) : contents(contents) {}
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
      Maker(BinASTSum contents) : contents(contents) {}
      MaybeSum operator()(const NormalizedInterfaceAndField identity) {
        return MaybeSum(identity, contents);
      }
    };
  };

  // A choice between several strings. May not be null.
  struct StringEnum : EntryBase {
    // The values in the enum.
    const BinASTStringEnum contents;
    StringEnum(const NormalizedInterfaceAndField identity,
               const BinASTStringEnum contents)
        : EntryBase(identity), contents(contents) {}

    // Utility struct, used in macros to call the constructor as
    // `StringEnum::Maker(kind)(identity)`.
    struct Maker {
      const BinASTStringEnum contents;
      Maker(BinASTStringEnum contents) : contents(contents) {}
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
#define WRAP_INTERFACE(TYPE) Interface::Maker(BinASTKind::TYPE)
#define WRAP_MAYBE_INTERFACE(TYPE) MaybeInterface::Maker(BinASTKind::TYPE)
#define WRAP_PRIMITIVE(TYPE) TYPE
#define WRAP_LIST(TYPE, _) List::Maker(BinASTList::TYPE)
#define WRAP_SUM(TYPE) Sum::Maker(BinASTSum::TYPE)
#define WRAP_MAYBE_SUM(TYPE) MaybeSum::Maker(BinASTSum::TYPE)
#define WRAP_STRING_ENUM(TYPE) StringEnum::Maker(BinASTStringEnum::TYPE)
#define WRAP_MAYBE_STRING_ENUM(TYPE) \
  MaybeStringEnum::Maker(BinASTStringEnum::TYPE)
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
#undef WRAP_LIST
#undef WRAP_SUM
#undef WRAP_STRING_ENUM
#undef WRAP_PRIMITIVE
#undef WRAP_INTERFACE
#undef EMIT_FIELD
    }

    return Ok();
  }

  // Read a single symbol.
  // For an indexed type, the symbol is fetched from the grammar using `index`.
  // We have a guarantee that `index` is always in [0, numberOfSymbols).
  template <typename Entry>
  MOZ_MUST_USE JS::Result<typename Entry::SymbolType> readSymbol(
      const Entry& type, size_t index);

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
      BINJS_MOZ_TRY_DECL(bitLength, reader.readByte());
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

  template <typename Entry>
  MOZ_MUST_USE JS::Result<Ok> readTable(Entry entry) {
    HuffmanTable& table = dictionary.tableForField(entry.identity);
    if (!table.is<HuffmanTableUnreachable>()) {
      // We're attempting to re-read a table that has already been read.
      return raiseDuplicateTableError(entry.identity);
    }

    uint8_t headerByte;
    MOZ_TRY_VAR(headerByte, reader.readByte());
    switch (headerByte) {
      case TableHeader::SingleValue: {
        // Construct in-place.
        table = {mozilla::VariantType<typename Entry::Table>{}, cx_};
        auto& tableRef = table.as<typename Entry::Table>();

        // The table contains a single value.
        MOZ_TRY((readSingleValueTable<Entry>(tableRef, entry)));
        return Ok();
      }
      case TableHeader::MultipleValues: {
        // Table contains multiple values.
        // Construct in-place.
        table = {mozilla::VariantType<typename Entry::Table>{}, cx_};
        auto& tableRef = table.as<typename Entry::Table>();

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

  // ------ Reading booleans.
  // 0 -> False
  // 1 -> True

  // Extract the number of symbols from the grammar.
  template <>
  MOZ_MUST_USE JS::Result<uint32_t> readNumberOfSymbols(const Boolean&) {
    // Sadly, there are only two booleans known to this date.
    return 2;
  }

  // Extract symbol from the grammar.
  template <>
  MOZ_MUST_USE JS::Result<bool> readSymbol(const Boolean&, size_t index) {
    MOZ_ASSERT(index < 2);
    return index != 0;
  }

  // Reading a single-value table of booleans
  template <>
  MOZ_MUST_USE JS::Result<Ok> readSingleValueTable<Boolean>(
      Boolean::Table& table, const Boolean& entry) {
    uint8_t indexByte;
    MOZ_TRY_VAR(indexByte, reader.readByte());
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
  MOZ_MUST_USE JS::Result<uint32_t> readNumberOfSymbols(const MaybeInterface&) {
    // Null, NonNull
    return 2;
  }

  // Extract symbol from the grammar.
  template <>
  MOZ_MUST_USE JS::Result<Nullable> readSymbol(const MaybeInterface&,
                                               size_t index) {
    MOZ_ASSERT(index < 2);
    return index == 0 ? Nullable::Null : Nullable::NonNull;
  }

  // Reading a single-value table of optional interfaces
  template <>
  MOZ_MUST_USE JS::Result<Ok> readSingleValueTable<MaybeInterface>(
      MaybeInterface::Table& table, const MaybeInterface& entry) {
    uint8_t indexByte;
    MOZ_TRY_VAR(indexByte, reader.readByte());
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
  MOZ_MUST_USE JS::Result<uint32_t> readNumberOfSymbols(const Sum& sum) {
    return sum.maxNumberOfSymbols();
  }

  // Extract symbol from the grammar.
  template <>
  MOZ_MUST_USE JS::Result<BinASTKind> readSymbol(const Sum& sum, size_t index) {
    MOZ_ASSERT(index < sum.numberOfSymbols());
    return sum.interfaceAt(index);
  }

  // Reading a single-value table of sums of interfaces.
  template <>
  MOZ_MUST_USE JS::Result<Ok> readSingleValueTable<Sum>(
      HuffmanTableIndexedSymbolsSum& table, const Sum& sum) {
    BINJS_MOZ_TRY_DECL(index, reader.readVarU32());
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
  MOZ_MUST_USE JS::Result<uint32_t> readNumberOfSymbols(const MaybeSum& sum) {
    return sum.maxNumberOfSymbols();
  }

  // Extract symbol from the grammar.
  template <>
  MOZ_MUST_USE JS::Result<BinASTKind> readSymbol(const MaybeSum& sum,
                                                 size_t index) {
    MOZ_ASSERT(index < sum.numberOfSymbols());
    return sum.interfaceAt(index);
  }

  // Reading a single-value table of sums of interfaces.
  template <>
  BINJS_MOZ_TRY_DECL(index, reader.readVarU32());
  MOZ_MUST_USE JS::Result<Ok> readSingleValueTable<MaybeSum>(
      HuffmanTableIndexedSymbolsSum& table, const MaybeSum& sum) {
    if (index >= sum.maxNumberOfSymbols()) {
      return raiseInvalidTableData(sum.identity);
    }

    MOZ_TRY(table.impl.initWithSingleValue(cx_, sum.interfaceAt(index)));
    return Ok();
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

    EntryMatcher(HuffmanPreludeReader& owner) : owner(owner) {}

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
      owner.pushFields(entry.kind);
      return Ok();
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
        owner.pushFields(entry.kind);
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

    MOZ_MUST_USE JS::Result<Ok> operator()(const String& entry) {
      // FIXME: Read table.
      // FIXME: Initialize table.
      MOZ_CRASH("Unimplemented");
      return Ok();
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const MaybeString& entry) {
      // FIXME: Read table.
      // FIXME: Initialize table.
      MOZ_CRASH("Unimplemented");
      return Ok();
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const Number& entry) {
      // FIXME: Read table.
      // FIXME: Initialize table.
      MOZ_CRASH("Unimplemented");
      return Ok();
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const UnsignedLong& entry) {
      // FIXME: Read table.
      // FIXME: Initialize table.
      MOZ_CRASH("Unimplemented");
      return Ok();
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const List& entry) {
      // FIXME: Enqueue list length.
      // FIXME: Read table.
      // FIXME: Initialize table.
      MOZ_CRASH("Unimplemented");
      return Ok();
    }

    MOZ_MUST_USE JS::Result<Ok> operator()(const StringEnum& entry) {
      // FIXME: Enqueue symbols.
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
                                                 uint8_t bits_length,
                                                 T&& value) {
  MOZ_ASSERT(bits_lengths != 0,
             "Adding a symbol with a bits_length of 0 doesn't make sense.");
  MOZ_ASSERT(values.empty() || values.back()->bits_length <= bits_length,
             "Symbols must be ranked by increasing bits length");
  MOZ_ASSERT(bits >> bits_len == 0);
  if (!values.emplaceBack(bits, bits_length, std::move(value))) {
    MOZ_CRASH();  // Memory was reserved in `init()`.
  }

  return Ok();
}

// The number of possible interfaces in each sum, indexed by
// `static_cast<size_t>(BinASTSum)`.
const size_t SUM_LIMITS[] = {
#define WITH_SUM(_ENUM_NAME, _HUMAN_NAME, MACRO_NAME) \
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
#define WITH_SUM(_ENUM_NAME, _HUMAN_NAME, MACRO_NAME) \
  const BinASTKind SUM_RESOLUTION_##MACRO_NAME[]{     \
      FOR_EACH_BIN_INTERFACE_IN_SUM_##ARROW_EXPRESSION(WITH_SUM_CONTENTS)};
FOR_EACH_BIN_SUM(WITH_SUM)
#undef WITH_SUM
#undef WITH_SUM_CONTENTS

const BinASTKind* SUM_RESOLUTIONS[]{
#define WITH_SUM(_ENUM_NAME, _HUMAN_NAME, MACRO_NAME) \
  SUM_RESOLUTION_##MACRO_NAME,
    FOR_EACH_BIN_SUM(WITH_SUM)
#undef WITH_SUM
};

}  // namespace frontend

}  // namespace js
