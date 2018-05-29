/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinTokenReaderMultipart_h
#define frontend_BinTokenReaderMultipart_h

#include "mozilla/Maybe.h"

#include "frontend/BinToken.h"
#include "frontend/BinTokenReaderBase.h"

#include "js/Result.h"

namespace js {
namespace frontend {

/**
 * A token reader implementing the "multipart" serialization format for BinAST.
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
class MOZ_STACK_CLASS BinTokenReaderMultipart: public BinTokenReaderBase
{
  public:
    class AutoList;
    class AutoTuple;
    class AutoTaggedTuple;

    using CharSlice = BinaryASTSupport::CharSlice;

    // This implementation of `BinFields` is effectively `void`, as the format
    // does not embed field information.
    class BinFields {
      public:
        explicit BinFields(JSContext*) {}
    };
    struct Chars: public CharSlice {
        explicit Chars(JSContext*)
          : CharSlice(nullptr, 0)
        { }
        Chars(const char* start, const uint32_t byteLen)
          : CharSlice(start, byteLen)
        { }
        Chars(const Chars& other) = default;
    };

  public:
    /**
     * Construct a token reader.
     *
     * Does NOT copy the buffer.
     */
    BinTokenReaderMultipart(JSContext* cx, const uint8_t* start, const size_t length);

    /**
     * Construct a token reader.
     *
     * Does NOT copy the buffer.
     */
    BinTokenReaderMultipart(JSContext* cx, const Vector<uint8_t>& chars);

    /**
     * Read the header of the file.
     */
    MOZ_MUST_USE JS::Result<Ok> readHeader();

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
    MOZ_MUST_USE JS::Result<bool> readBool();

    /**
     * Read a single `number` value.
     */
    MOZ_MUST_USE JS::Result<double> readDouble();

    /**
     * Read a single `string | null` value.
     *
     * Fails if that string is not valid UTF-8.
     */
    MOZ_MUST_USE JS::Result<JSAtom*> readMaybeAtom();
    MOZ_MUST_USE JS::Result<JSAtom*> readAtom();


    /**
     * Read a single `string | null` value.
     *
     * MAY check if that string is not valid UTF-8.
     */
    MOZ_MUST_USE JS::Result<Ok> readChars(Chars&);

    /**
     * Read a single `BinVariant | null` value.
     */
    MOZ_MUST_USE JS::Result<mozilla::Maybe<BinVariant>> readMaybeVariant();
    MOZ_MUST_USE JS::Result<BinVariant> readVariant();

    /**
     * Read over a single `[Skippable]` subtree value.
     *
     * This does *not* attempt to parse the subtree itself. Rather, the
     * returned `SkippableSubTree` contains the necessary information
     * to parse/tokenize the subtree at a later stage
     */
    MOZ_MUST_USE JS::Result<SkippableSubTree> readSkippableSubTree();

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
    MOZ_MUST_USE JS::Result<Ok> enterList(uint32_t& length, AutoList& guard);

    /**
     * Start reading a tagged tuple.
     *
     * @param tag (OUT) The tag of the tuple.
     * @param fields Ignored, provided for API compatibility.
     * @param guard (OUT) A guard, ensuring that we read the tagged tuple correctly.
     *
     * The `guard` is dedicated to ensuring that reading the list has consumed
     * exactly all the bytes from that tuple. The `guard` MUST therefore be
     * destroyed at the point where the caller has reached the end of the tuple.
     * If the caller has consumed too few/too many bytes, this will be reported
     * in the call go `guard.done()`.
     *
     * @return out If the header of the tuple is invalid.
     */
    MOZ_MUST_USE JS::Result<Ok> enterTaggedTuple(BinKind& tag, BinTokenReaderMultipart::BinFields& fields, AutoTaggedTuple& guard);

    /**
     * Start reading an untagged tuple.
     *
     * @param guard (OUT) A guard, ensuring that we read the tuple correctly.
     *
     * The `guard` is dedicated to ensuring that reading the list has consumed
     * exactly all the bytes from that tuple. The `guard` MUST therefore be
     * destroyed at the point where the caller has reached the end of the tuple.
     * If the caller has consumed too few/too many bytes, this will be reported
     * in the call go `guard.done()`.
     */
    MOZ_MUST_USE JS::Result<Ok> enterUntaggedTuple(AutoTuple& guard);

  private:
    /**
     * Read a single uint32_t.
     */
    MOZ_MUST_USE JS::Result<uint32_t> readInternalUint32();

  private:
    // A mapping grammar index => BinKind, as defined by the [GRAMMAR]
    // section of the file. Populated during readHeader().
    Vector<BinKind> grammarTable_;

    // A mapping string index => BinVariant as extracted from the [STRINGS]
    // section of the file. Populated lazily.
    js::HashMap<uint32_t, BinVariant, DefaultHasher<uint32_t>, SystemAllocPolicy> variantsTable_;

    // A mapping index => JSAtom, as defined by the [STRINGS]
    // section of the file. Populated during readHeader().
    using AtomVector = GCVector<JSAtom*>;
    JS::Rooted<AtomVector> atomsTable_;

    // The same mapping, but as CharSlice. Populated during readHeader().
    // The slices are into the source buffer.
    Vector<Chars> slicesTable_;

    const uint8_t* posBeforeTree_;

    BinTokenReaderMultipart(const BinTokenReaderMultipart&) = delete;
    BinTokenReaderMultipart(BinTokenReaderMultipart&&) = delete;
    BinTokenReaderMultipart& operator=(BinTokenReaderMultipart&) = delete;

  public:
    // The following classes are used whenever we encounter a tuple/tagged tuple/list
    // to make sure that:
    //
    // - if the construct "knows" its byte length, we have exactly consumed all
    //   the bytes (otherwise, this means that the file is corrupted, perhaps on
    //   purpose, so we need to reject the stream);
    // - if the construct has a footer, once we are done reading it, we have
    //   reached the footer (this is to aid with debugging).
    //
    // In either case, the caller MUST call method `done()` of the guard once
    // it is done reading the tuple/tagged tuple/list, to report any pending error.

    // Base class used by other Auto* classes.
    class MOZ_STACK_CLASS AutoBase
    {
      protected:
        explicit AutoBase(BinTokenReaderMultipart& reader);
        ~AutoBase();

        // Raise an error if we are not in the expected position.
        MOZ_MUST_USE JS::Result<Ok> checkPosition(const uint8_t* expectedPosition);

        friend BinTokenReaderMultipart;
        void init();

        // Set to `true` if `init()` has been called. Reset to `false` once
        // all conditions have been checked.
        bool initialized_;
        BinTokenReaderMultipart& reader_;
    };

    // Guard class used to ensure that `enterList` is used properly.
    class MOZ_STACK_CLASS AutoList : public AutoBase
    {
      public:
        explicit AutoList(BinTokenReaderMultipart& reader);

        // Check that we have properly read to the end of the list.
        MOZ_MUST_USE JS::Result<Ok> done();

      protected:
        friend BinTokenReaderMultipart;
        void init();
    };

    // Guard class used to ensure that `enterTaggedTuple` is used properly.
    class MOZ_STACK_CLASS AutoTaggedTuple : public AutoBase
    {
      public:
        explicit AutoTaggedTuple(BinTokenReaderMultipart& reader);

        // Check that we have properly read to the end of the tuple.
        MOZ_MUST_USE JS::Result<Ok> done();
    };

    // Guard class used to ensure that `readTuple` is used properly.
    class MOZ_STACK_CLASS AutoTuple : public AutoBase
    {
      public:
        explicit AutoTuple(BinTokenReaderMultipart& reader);

        // Check that we have properly read to the end of the tuple.
        MOZ_MUST_USE JS::Result<Ok> done();
    };

    // Compare a `Chars` and a string literal (ONLY a string literal).
    template <size_t N>
    static bool equals(const Chars& left, const char (&right)[N]) {
        MOZ_ASSERT(N > 0);
        MOZ_ASSERT(right[N - 1] == 0);
        if (left.byteLen_ + 1 /* implicit NUL */ != N)
            return false;

        if (!std::equal(left.start_, left.start_ + left.byteLen_, right))
          return false;

        return true;
    }

    template<size_t N>
    static JS::Result<Ok, JS::Error&>
    checkFields(const BinKind kind, const BinFields& actual, const BinField (&expected)[N]) {
      // Not implemented in this tokenizer.
      return Ok();
    }

    static JS::Result<Ok, JS::Error&>
    checkFields0(const BinKind kind, const BinFields& actual) {
      // Not implemented in this tokenizer.
      return Ok();
    }
};

} // namespace frontend
} // namespace js

#endif // frontend_BinTokenReaderMultipart_h
