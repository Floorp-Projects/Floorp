/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinTokenReaderTester_h
#define frontend_BinTokenReaderTester_h

#include "mozilla/Maybe.h"

#include "frontend/BinToken.h"
#include "frontend/BinTokenReaderBase.h"

#include "js/TypeDecls.h"

#if !defined(NIGHTLY_BUILD)
#error "BinTokenReaderTester.* is designed to help test implementations of successive versions of JS BinaryAST. It is available only on Nightly."
#endif // !defined(NIGHTLY_BUILD)

namespace js {
namespace frontend {

using namespace mozilla;
using namespace JS;

/**
 * A token reader for a simple, alternative serialization format for BinAST.
 *
 * This serialization format, which is also supported by the reference
 * implementation of the BinAST compression suite, is designed to be
 * mostly human-readable and easy to check for all sorts of deserialization
 * errors. While this format is NOT designed to be shipped to end-users, it
 * is nevertheless a very useful tool for implementing and testing parsers.
 *
 * Both the format and the implementation are ridiculously inefficient:
 *
 * - the underlying format tags almost all its data with e.g. `<tuple>`, `</tuple>`
 *   to aid with detecting offset errors or format error;
 * - the underlying format copies list of fields into every single node, instead
 *   of keeping them once in the header;
 * - every kind/field extraction requires memory allocation and plenty of string
 *   comparisons;
 * - ...
 *
 * This token reader is designed to be API-compatible with the standard, shipped,
 * token reader. For these reasons:
 *
 * - it does not support any form of look ahead, push back;
 * - it does not support any form of error recovery.
 */
class MOZ_STACK_CLASS BinTokenReaderTester: public BinTokenReaderBase
{
  public:
    // A list of fields, in the order in which they appear in the stream.
    using BinFields = Vector<BinField, 8>;

    // A bunch of characters. At this stage, there is no guarantee on whether
    // they are valid UTF-8. Future versions may replace this by slice into
    // the buffer.
    using Chars     = Vector<uint8_t, 32>;

    class AutoList;
    class AutoTuple;
    class AutoTaggedTuple;

  public:
    /**
     * Construct a token reader.
     *
     * Does NOT copy the buffer.
     */
    BinTokenReaderTester(JSContext* cx, const uint8_t* start, const size_t length);

    /**
     * Construct a token reader.
     *
     * Does NOT copy the buffer.
     */
    BinTokenReaderTester(JSContext* cx, const Vector<uint8_t>& chars);

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
     * @return false If a double could not be read. In this case, an error
     * has been raised.
     */
    MOZ_MUST_USE JS::Result<double> readDouble();

    /**
     * Read a single `string | null` value.
     *
     * Fails if that string is not valid UTF-8.
     *
     * The returned `JSAtom*` may be `nullptr`.
     */
    MOZ_MUST_USE JS::Result<JSAtom*> readMaybeAtom();

    /**
     * Read a single `string` value.
     *
     * Fails if that string is not valid UTF-8 or in case of `null` string.
     *
     * The returned `JSAtom*` is never `nullptr`.
     */
    MOZ_MUST_USE JS::Result<JSAtom*> readAtom();


    /**
     * Read a single `string | null` value.
     *
     * There is no guarantee that the string is valid UTF-8.
     */
    MOZ_MUST_USE JS::Result<Ok> readChars(Chars&);

    /**
     * Read a single `BinVariant | null` value.
     */
    MOZ_MUST_USE JS::Result<Maybe<BinVariant>> readMaybeVariant();
    MOZ_MUST_USE JS::Result<BinVariant> readVariant();

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
     *
     * @return out If the header of the list is invalid.
     */
    MOZ_MUST_USE JS::Result<Ok> enterList(uint32_t& length, AutoList& guard);

    /**
     * Start reading a tagged tuple.
     *
     * @param tag (OUT) The tag of the tuple.
     * @param fields (OUT) The ORDERED list of fields encoded in this tuple.
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
    MOZ_MUST_USE JS::Result<Ok> enterTaggedTuple(BinKind& tag, BinTokenReaderTester::BinFields& fields, AutoTaggedTuple& guard);

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
     *
     * @return out If the header of the tuple is invalid.
     */
    MOZ_MUST_USE JS::Result<Ok> enterUntaggedTuple(AutoTuple& guard);

    /**
     * Read a single uint32_t.
     */
    MOZ_MUST_USE JS::Result<uint32_t> readInternalUint32();

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
        explicit AutoBase(BinTokenReaderTester& reader);
        ~AutoBase();

        // Raise an error if we are not in the expected position.
        MOZ_MUST_USE JS::Result<Ok> checkPosition(const uint8_t* expectedPosition);

        friend BinTokenReaderTester;
        void init();

        // Set to `true` if `init()` has been called. Reset to `false` once
        // all conditions have been checked.
        bool initialized_;
        BinTokenReaderTester& reader_;
    };

    // Guard class used to ensure that `enterList` is used properly.
    class MOZ_STACK_CLASS AutoList : public AutoBase
    {
      public:
        explicit AutoList(BinTokenReaderTester& reader);

        // Check that we have properly read to the end of the list.
        MOZ_MUST_USE JS::Result<Ok> done();
      protected:
        friend BinTokenReaderTester;
        void init();
    };

    // Guard class used to ensure that `enterTaggedTuple` is used properly.
    class MOZ_STACK_CLASS AutoTaggedTuple : public AutoBase
    {
      public:
        explicit AutoTaggedTuple(BinTokenReaderTester& reader);

        // Check that we have properly read to the end of the tuple.
        MOZ_MUST_USE JS::Result<Ok> done();
    };

    // Guard class used to ensure that `readTuple` is used properly.
    class MOZ_STACK_CLASS AutoTuple : public AutoBase
    {
      public:
        explicit AutoTuple(BinTokenReaderTester& reader);

        // Check that we have properly read to the end of the tuple.
        MOZ_MUST_USE JS::Result<Ok> done();
    };

    // Compare a `Chars` and a string literal (ONLY a string literal).
    template <size_t N>
    static bool equals(const Chars& left, const char (&right)[N]) {
        MOZ_ASSERT(N > 0);
        MOZ_ASSERT(right[N - 1] == 0);
        if (left.length() + 1 /* implicit NUL */ != N)
            return false;

        if (!std::equal(left.begin(), left.end(), right))
          return false;

        return true;
    }

    // Ensure that we are visiting the right fields.
    template<size_t N>
    JS::Result<Ok, JS::Error&> checkFields(const BinKind kind, const BinFields& actual,
                                                  const BinField (&expected)[N])
    {
        if (actual.length() != N)
            return raiseInvalidNumberOfFields(kind, N, actual.length());

        for (size_t i = 0; i < N; ++i) {
            if (actual[i] != expected[i])
                return raiseInvalidField(describeBinKind(kind), actual[i]);
        }

        return Ok();
    }

    // Special case for N=0, as empty arrays are not permitted in C++
    JS::Result<Ok, JS::Error&> checkFields0(const BinKind kind, const BinFields& actual)
    {
        if (actual.length() != 0)
            return raiseInvalidNumberOfFields(kind, 0, actual.length());

        return Ok();
    }
};

} // namespace frontend
} // namespace js

#endif // frontend_BinTokenReaderTester_h
