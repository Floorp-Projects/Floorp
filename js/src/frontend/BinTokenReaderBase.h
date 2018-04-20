/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinTokenReaderBase_h
#define frontend_BinTokenReaderBase_h

#include "mozilla/Maybe.h"

#include "frontend/BinToken.h"
#include "frontend/TokenStream.h"

#include "js/TypeDecls.h"


namespace js {
namespace frontend {

using namespace mozilla;
using namespace JS;

// A constant used by tokenizers to represent a null float.
extern const uint64_t NULL_FLOAT_REPRESENTATION;

class MOZ_STACK_CLASS BinTokenReaderBase
{
  public:
    template<typename T> using ErrorResult = mozilla::GenericErrorResult<T>;

    /**
     * Return the position of the latest token.
     */
    TokenPos pos();
    TokenPos pos(size_t startOffset);
    size_t offset() const;

     /**
      * Poison this tokenizer.
      */
    void poison();

    /**
     * Raise an error.
     *
     * Once `raiseError` has been called, the tokenizer is poisoned.
     */
    MOZ_MUST_USE ErrorResult<JS::Error&> raiseError(const char* description);
    MOZ_MUST_USE ErrorResult<JS::Error&> raiseOOM();
    MOZ_MUST_USE ErrorResult<JS::Error&> raiseInvalidNumberOfFields(
        const BinKind kind, const uint32_t expected, const uint32_t got);
    MOZ_MUST_USE ErrorResult<JS::Error&> raiseInvalidField(const char* kind,
        const BinField field);

  protected:
    BinTokenReaderBase(JSContext* cx, const uint8_t* start, const size_t length)
        : cx_(cx)
        , start_(start)
        , current_(start)
        , stop_(start + length)
        , latestKnownGoodPos_(0)
    { }

    /**
     * Read a single byte.
     */
    MOZ_MUST_USE JS::Result<uint8_t> readByte();

    /**
     * Read several bytes.
     *
     * If there is not enough data, or if the tokenizer has previously been
     * poisoned, return an error.
     */
    MOZ_MUST_USE JS::Result<Ok> readBuf(uint8_t* bytes, uint32_t len);

    /**
     * Read a sequence of chars, ensuring that they match an expected
     * sequence of chars.
     *
     * @param value The sequence of chars to expect, NUL-terminated.
     */
     template <size_t N>
     MOZ_MUST_USE JS::Result<Ok> readConst(const char (&value)[N])
     {
        updateLatestKnownGood();
        if (!matchConst(value, false))
            return raiseError("Could not find expected literal");
        return Ok();
     }

     /**
     * Read a sequence of chars, consuming the bytes only if they match an expected
     * sequence of chars.
     *
     * @param value The sequence of chars to expect, NUL-terminated.
     * @param expectNul If true, expect NUL in the stream, otherwise don't.
     * @return true if `value` represents the next few chars in the
     * internal buffer, false otherwise. If `true`, the chars are consumed,
     * otherwise there is no side-effect.
     */
    template <size_t N>
    MOZ_MUST_USE bool matchConst(const char (&value)[N], bool expectNul) {
        MOZ_ASSERT(N > 0);
        MOZ_ASSERT(value[N - 1] == 0);
        MOZ_ASSERT(!cx_->isExceptionPending());

        if (current_ + N - 1 > stop_)
            return false;

#ifndef FUZZING
        // Perform lookup, without side-effects.
        // For fuzzing, we disable this check to avoid spending unnecessary
        // time on getting the constants right. Instead, the constant fields
        // may contain any data for such builds.
        if (!std::equal(current_, current_ + N + (expectNul ? 0 : -1)/*implicit NUL*/, value))
            return false;
#endif

        // Looks like we have a match. Now perform side-effects
        current_ += N + (expectNul ? 0 : -1);
        updateLatestKnownGood();
        return true;
    }

    void updateLatestKnownGood();

    JSContext* cx_;

    // `true` if we have encountered an error. Errors are non recoverable.
    // Attempting to read from a poisoned tokenizer will cause assertion errors.
    bool poisoned_;

    // The first byte of the buffer. Not owned.
    const uint8_t* start_;

    // The current position.
    const uint8_t* current_;

    // The last+1 byte of the buffer.
    const uint8_t* stop_;

    // Latest known good position. Used for error reporting.
    size_t latestKnownGoodPos_;

  private:
    BinTokenReaderBase(const BinTokenReaderBase&) = delete;
    BinTokenReaderBase(BinTokenReaderBase&&) = delete;
    BinTokenReaderBase& operator=(BinTokenReaderBase&) = delete;
};

} // namespace frontend
} // namespace js

#endif // frontend_BinTokenReaderBase_h
