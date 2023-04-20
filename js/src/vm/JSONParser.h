/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSONParser_h
#define vm_JSONParser_h

#include "mozilla/Attributes.h"
#include "mozilla/Range.h"

#include "jspubtd.h"

#include "ds/IdValuePair.h"
#include "vm/StringType.h"

namespace js {

enum class JSONToken {
  String,
  Number,
  True,
  False,
  Null,
  ArrayOpen,
  ArrayClose,
  ObjectOpen,
  ObjectClose,
  Colon,
  Comma,
  OOM,
  Error
};

template <typename CharT>
class JSONParser;

enum class JSONStringType { PropertyName, LiteralValue };

template <typename CharT>
class MOZ_STACK_CLASS JSONTokenizer {
 public:
  using CharPtr = mozilla::RangedPtr<const CharT>;

 protected:
  CharPtr current;
  const CharPtr begin, end;

  JSONParser<CharT>* parser = nullptr;

 public:
  JSONTokenizer(CharPtr current, const CharPtr begin, const CharPtr end,
                JSONParser<CharT>* parser)
      : current(current), begin(begin), end(end), parser(parser) {
    MOZ_ASSERT(current <= end);
    MOZ_ASSERT(parser);
  }

  explicit JSONTokenizer(mozilla::Range<const CharT> data,
                         JSONParser<CharT>* parser)
      : JSONTokenizer(data.begin(), data.begin(), data.end(), parser) {}

  JSONTokenizer(JSONTokenizer<CharT>&& other) noexcept
      : JSONTokenizer(other.current, other.begin, other.end, other.parser) {}

  JSONTokenizer(const JSONTokenizer<CharT>& other) = delete;
  void operator=(const JSONTokenizer<CharT>& other) = delete;

  void fixupParser(JSONParser<CharT>* newParser) { parser = newParser; }

  void getTextPosition(uint32_t* column, uint32_t* line);

  bool consumeTrailingWhitespaces();

  JSONToken advance();
  JSONToken advancePropertyName();
  JSONToken advancePropertyColon();
  JSONToken advanceAfterProperty();
  JSONToken advanceAfterObjectOpen();
  JSONToken advanceAfterArrayElement();

  void unget() { --current; }

#ifdef DEBUG
  bool finished() { return end == current; }
#endif

  JSONToken token(JSONToken t) {
    MOZ_ASSERT(t != JSONToken::String);
    MOZ_ASSERT(t != JSONToken::Number);
    return t;
  }

  JSONToken stringToken(JSString* str);
  JSONToken numberToken(double d);

  template <JSONStringType ST>
  JSONToken readString();

  JSONToken readNumber();

  void error(const char* msg);
};

// JSONParser base class. JSONParser is templatized to work on either Latin1
// or TwoByte input strings, JSONParserBase holds all state and methods that
// can be shared between the two encodings.
class MOZ_STACK_CLASS JSONParserBase {
 public:
  enum class ParseType {
    // Parsing a string as if by JSON.parse.
    JSONParse,
    // Parsing what may or may not be JSON in a string of eval code.
    // In this case, a failure to parse indicates either syntax that isn't JSON,
    // or syntax that has different semantics in eval code than in JSON.
    AttemptForEval,
  };

 public:
  /* Data members */

  Value v;
  JSContext* const cx;

 protected:
  const ParseType parseType;

  // State related to the parser's current position. At all points in the
  // parse this keeps track of the stack of arrays and objects which have
  // been started but not finished yet. The actual JS object is not
  // allocated until the literal is closed, so that the result can be sized
  // according to its contents and have its type and shape filled in using
  // caches.

  // State for an array that is currently being parsed. This includes all
  // elements that have been seen so far.
  typedef GCVector<Value, 20> ElementVector;

  // State for an object that is currently being parsed. This includes all
  // the key/value pairs that have been seen so far.
  typedef GCVector<IdValuePair, 10> PropertyVector;

  // Possible states the parser can be in between values.
  enum ParserState {
    // An array element has just being parsed.
    FinishArrayElement,

    // An object property has just been parsed.
    FinishObjectMember,

    // At the start of the parse, before any values have been processed.
    JSONValue
  };

  // Stack element for an in progress array or object.
  struct StackEntry {
    ElementVector& elements() {
      MOZ_ASSERT(state == FinishArrayElement);
      return *static_cast<ElementVector*>(vector);
    }

    PropertyVector& properties() {
      MOZ_ASSERT(state == FinishObjectMember);
      return *static_cast<PropertyVector*>(vector);
    }

    explicit StackEntry(ElementVector* elements)
        : state(FinishArrayElement), vector(elements) {}

    explicit StackEntry(PropertyVector* properties)
        : state(FinishObjectMember), vector(properties) {}

    ParserState state;

   private:
    void* vector;
  };

  // All in progress arrays and objects being parsed, in order from outermost
  // to innermost.
  Vector<StackEntry, 10> stack;

  // Unused element and property vectors for previous in progress arrays and
  // objects. These vectors are not freed until the end of the parse to avoid
  // unnecessary freeing and allocation.
  Vector<ElementVector*, 5> freeElements;
  Vector<PropertyVector*, 5> freeProperties;

  JSONParserBase(JSContext* cx, ParseType parseType)
      : cx(cx),
        parseType(parseType),
        stack(cx),
        freeElements(cx),
        freeProperties(cx) {}
  ~JSONParserBase();

  // Allow move construction for use with Rooted.
  JSONParserBase(JSONParserBase&& other)
      : v(other.v),
        cx(other.cx),
        parseType(other.parseType),
        stack(std::move(other.stack)),
        freeElements(std::move(other.freeElements)),
        freeProperties(std::move(other.freeProperties)) {}

  Value numberValue() const {
    MOZ_ASSERT(v.isNumber());
    return v;
  }

  Value stringValue() const {
    MOZ_ASSERT(v.isString());
    return v;
  }

  JSAtom* atomValue() const {
    Value strval = stringValue();
    return &strval.toString()->asAtom();
  }

  bool errorReturn();

  bool finishObject(MutableHandleValue vp, PropertyVector& properties);
  bool finishArray(MutableHandleValue vp, ElementVector& elements);

  void trace(JSTracer* trc);

 private:
  JSONParserBase(const JSONParserBase& other) = delete;
  void operator=(const JSONParserBase& other) = delete;
};

template <typename CharT>
class MOZ_STACK_CLASS JSONParser : public JSONParserBase {
  using Tokenizer = JSONTokenizer<CharT>;

  Tokenizer tokenizer;

 public:
  /* Public API */

  /* Create a parser for the provided JSON data. */
  JSONParser(JSContext* cx, mozilla::Range<const CharT> data,
             ParseType parseType)
      : JSONParserBase(cx, parseType), tokenizer(data, this) {}

  /* Allow move construction for use with Rooted. */
  JSONParser(JSONParser&& other)
      : JSONParserBase(std::move(other)),
        tokenizer(std::move(other.tokenizer)) {
    tokenizer.fixupParser(this);
  }

  /*
   * Parse the JSON data specified at construction time.  If it parses
   * successfully, store the prescribed value in *vp and return true.  If an
   * internal error (e.g. OOM) occurs during parsing, return false.
   * Otherwise, if invalid input was specifed but no internal error occurred,
   * behavior depends upon the error handling specified at construction: if
   * error handling is RaiseError then throw a SyntaxError and return false,
   * otherwise return true and set *vp to |undefined|.  (JSON syntax can't
   * represent |undefined|, so the JSON data couldn't have specified it.)
   */
  bool parse(MutableHandleValue vp);

  void trace(JSTracer* trc) { JSONParserBase::trace(trc); }

  void error(const char* msg);

 private:
  JSONParser(const JSONParser& other) = delete;
  void operator=(const JSONParser& other) = delete;
};

template <typename CharT, typename Wrapper>
class MutableWrappedPtrOperations<JSONParser<CharT>, Wrapper>
    : public WrappedPtrOperations<JSONParser<CharT>, Wrapper> {
 public:
  bool parse(MutableHandleValue vp) {
    return static_cast<Wrapper*>(this)->get().parse(vp);
  }
};

} /* namespace js */

#endif /* vm_JSONParser_h */
