/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinASTTokenReaderBase_h
#define frontend_BinASTTokenReaderBase_h

#include "frontend/BinASTToken.h"
#include "frontend/ErrorReporter.h"
#include "frontend/TokenStream.h"

#include "js/Result.h"
#include "js/TypeDecls.h"

namespace js {
namespace frontend {

// A constant used by tokenizers to represent a null float.
extern const uint64_t NULL_FLOAT_REPRESENTATION;

class MOZ_STACK_CLASS BinASTTokenReaderBase {
 public:
  template <typename T>
  using ErrorResult = mozilla::GenericErrorResult<T>;

  // The context in which we read a token.
  struct Context {
    // Construct a context for a root node.
    constexpr static Context topLevel() {
      return Context(BinASTKind::_Null, 0, ElementOf::TaggedTuple);
    }

    Context arrayElement() const {
      return Context(kind, fieldIndex, ElementOf::Array);
    }

    // Construct a context for a field of a tagged tuple.
    constexpr static Context firstField(BinASTKind kind) {
      return Context(kind, 0, ElementOf::TaggedTuple);
    }

    const Context operator++(int) {
      MOZ_ASSERT(elementOf == ElementOf::TaggedTuple);
      Context result = *this;
      fieldIndex++;
      return result;
    }

    // The kind of the tagged tuple containing the token.
    //
    // If the parent is the root, use `BinASTKind::_Null`.
    BinASTKind kind;

    // The index of the token as a field of the parent.
    uint8_t fieldIndex;

    enum class ElementOf {
      // This token is an element of an array.
      Array,

      // This token is a field of a tagged tuple.
      TaggedTuple,
    };
    ElementOf elementOf;

    Context() = delete;

   private:
    constexpr Context(BinASTKind kind_, uint8_t fieldIndex_,
                      ElementOf elementOf_)
        : kind(kind_), fieldIndex(fieldIndex_), elementOf(elementOf_) {}
  };

  // The information needed to skip a subtree.
  class SkippableSubTree {
   public:
    SkippableSubTree(const size_t startOffset, const size_t length)
        : startOffset_(startOffset), length_(length) {}

    // The position in the source buffer at which the subtree starts.
    //
    // `SkippableSubTree` does *not* attempt to keep anything alive.
    size_t startOffset() const { return startOffset_; }

    // The length of the subtree.
    size_t length() const { return length_; }

   private:
    const size_t startOffset_;
    const size_t length_;
  };

  /**
   * Return the position of the latest token.
   */
  TokenPos pos();
  TokenPos pos(size_t startOffset);
  size_t offset() const;

  // Set the tokenizer's cursor in the file. Use with caution.
  void seek(size_t offset);

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
      const BinASTKind kind, const uint32_t expected, const uint32_t got);
  MOZ_MUST_USE ErrorResult<JS::Error&> raiseInvalidField(
      const char* kind, const BinASTField field);

 protected:
  BinASTTokenReaderBase(JSContext* cx, ErrorReporter* er, const uint8_t* start,
                        const size_t length)
      : cx_(cx),
        errorReporter_(er),
        poisoned_(false),
        start_(start),
        current_(start),
        stop_(start + length),
        latestKnownGoodPos_(0) {
    MOZ_ASSERT(errorReporter_);
  }

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
  MOZ_MUST_USE JS::Result<Ok> readConst(const char (&value)[N]) {
    updateLatestKnownGood();
    if (!matchConst(value, false)) {
      return raiseError("Could not find expected literal");
    }
    return Ok();
  }

  /**
   * Read a sequence of chars, consuming the bytes only if they match an
   * expected sequence of chars.
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
    MOZ_ASSERT(!hasRaisedError());

    if (current_ + N - 1 > stop_) {
      return false;
    }

#ifndef FUZZING
    // Perform lookup, without side-effects.
    if (!std::equal(current_,
                    current_ + N + (expectNul ? 0 : -1) /*implicit NUL*/,
                    value)) {
      return false;
    }
#endif

    // Looks like we have a match. Now perform side-effects
    current_ += N + (expectNul ? 0 : -1);
    updateLatestKnownGood();
    return true;
  }

  void updateLatestKnownGood();

  bool hasRaisedError() const;

  JSContext* cx_;

  ErrorReporter* errorReporter_;

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
  BinASTTokenReaderBase(const BinASTTokenReaderBase&) = delete;
  BinASTTokenReaderBase(BinASTTokenReaderBase&&) = delete;
  BinASTTokenReaderBase& operator=(BinASTTokenReaderBase&) = delete;
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_BinASTTokenReaderBase_h
