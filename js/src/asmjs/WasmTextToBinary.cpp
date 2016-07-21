/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "asmjs/WasmTextToBinary.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"

#include "jsdtoa.h"
#include "jsnum.h"
#include "jsprf.h"
#include "jsstr.h"

#include "asmjs/WasmAST.h"
#include "asmjs/WasmBinary.h"
#include "asmjs/WasmTypes.h"
#include "ds/LifoAlloc.h"
#include "js/CharacterEncoding.h"
#include "js/HashTable.h"

using namespace js;
using namespace js::wasm;

using mozilla::BitwiseCast;
using mozilla::CeilingLog2;
using mozilla::CountLeadingZeroes32;
using mozilla::CheckedInt;
using mozilla::FloatingPoint;
using mozilla::Maybe;
using mozilla::PositiveInfinity;
using mozilla::SpecificNaN;

/*****************************************************************************/
// wasm text token stream

namespace {

class WasmToken
{
  public:
    enum FloatLiteralKind
    {
        HexNumber,
        DecNumber,
        Infinity,
        NaN
    };

    enum Kind
    {
        Align,
        BinaryOpcode,
        Block,
        Br,
        BrIf,
        BrTable,
        Call,
        CallImport,
        CallIndirect,
        CloseParen,
        ComparisonOpcode,
        Const,
        ConversionOpcode,
        Data,
        Elem,
        Else,
        EndOfFile,
        Equal,
        Error,
        Export,
        Float,
        Func,
        GetLocal,
        If,
        Import,
        Index,
        Memory,
        NegativeZero,
        Load,
        Local,
        Loop,
        Module,
        Name,
        Nop,
        Offset,
        OpenParen,
        Param,
        Resizable,
        Result,
        Return,
        Segment,
        SetLocal,
        SignedInteger,
        Start,
        Store,
        Table,
        TernaryOpcode,
        Text,
        Then,
        Type,
        UnaryOpcode,
        Unreachable,
        UnsignedInteger,
        ValueType
    };
  private:
    Kind kind_;
    const char16_t* begin_;
    const char16_t* end_;
    union {
        uint32_t index_;
        uint64_t uint_;
        int64_t sint_;
        FloatLiteralKind floatLiteralKind_;
        ValType valueType_;
        Expr expr_;
    } u;
  public:
    explicit WasmToken() = default;
    WasmToken(Kind kind, const char16_t* begin, const char16_t* end)
      : kind_(kind),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(kind_ != Error);
        MOZ_ASSERT((kind == EndOfFile) == (begin == end));
    }
    explicit WasmToken(uint32_t index, const char16_t* begin, const char16_t* end)
      : kind_(Index),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.index_ = index;
    }
    explicit WasmToken(uint64_t uint, const char16_t* begin, const char16_t* end)
      : kind_(UnsignedInteger),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.uint_ = uint;
    }
    explicit WasmToken(int64_t sint, const char16_t* begin, const char16_t* end)
      : kind_(SignedInteger),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.sint_ = sint;
    }
    explicit WasmToken(FloatLiteralKind floatLiteralKind,
                       const char16_t* begin, const char16_t* end)
      : kind_(Float),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.floatLiteralKind_ = floatLiteralKind;
    }
    explicit WasmToken(Kind kind, ValType valueType, const char16_t* begin, const char16_t* end)
      : kind_(kind),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        MOZ_ASSERT(kind_ == ValueType || kind_ == Const);
        u.valueType_ = valueType;
    }
    explicit WasmToken(Kind kind, Expr expr, const char16_t* begin, const char16_t* end)
      : kind_(kind),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        MOZ_ASSERT(kind_ == UnaryOpcode || kind_ == BinaryOpcode || kind_ == TernaryOpcode ||
                   kind_ == ComparisonOpcode || kind_ == ConversionOpcode ||
                   kind_ == Load || kind_ == Store);
        u.expr_ = expr;
    }
    explicit WasmToken(const char16_t* begin)
      : kind_(Error),
        begin_(begin),
        end_(begin)
    {}
    Kind kind() const {
        return kind_;
    }
    const char16_t* begin() const {
        return begin_;
    }
    const char16_t* end() const {
        return end_;
    }
    AstName text() const {
        MOZ_ASSERT(kind_ == Text);
        MOZ_ASSERT(begin_[0] == '"');
        MOZ_ASSERT(end_[-1] == '"');
        MOZ_ASSERT(end_ - begin_ >= 2);
        return AstName(begin_ + 1, end_ - begin_ - 2);
    }
    AstName name() const {
        return AstName(begin_, end_ - begin_);
    }
    uint32_t index() const {
        MOZ_ASSERT(kind_ == Index);
        return u.index_;
    }
    uint64_t uint() const {
        MOZ_ASSERT(kind_ == UnsignedInteger);
        return u.uint_;
    }
    int64_t sint() const {
        MOZ_ASSERT(kind_ == SignedInteger);
        return u.sint_;
    }
    FloatLiteralKind floatLiteralKind() const {
        MOZ_ASSERT(kind_ == Float);
        return u.floatLiteralKind_;
    }
    ValType valueType() const {
        MOZ_ASSERT(kind_ == ValueType || kind_ == Const);
        return u.valueType_;
    }
    Expr expr() const {
        MOZ_ASSERT(kind_ == UnaryOpcode || kind_ == BinaryOpcode || kind_ == TernaryOpcode ||
                   kind_ == ComparisonOpcode || kind_ == ConversionOpcode ||
                   kind_ == Load || kind_ == Store);
        return u.expr_;
    }
};

} // end anonymous namespace

static bool
IsWasmNewLine(char16_t c)
{
    return c == '\n';
}

static bool
IsWasmSpace(char16_t c)
{
    switch (c) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
      case '\v':
      case '\f':
        return true;
      default:
        return false;
    }
}

static bool
IsWasmDigit(char16_t c)
{
    return c >= '0' && c <= '9';
}

static bool
IsWasmLetter(char16_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool
IsNameAfterDollar(char16_t c)
{
    return IsWasmLetter(c) || IsWasmDigit(c) || c == '_' || c == '$' || c == '-' || c == '.';
}

static bool
IsHexDigit(char c, uint8_t* value)
{
    if (c >= '0' && c <= '9') {
        *value = c - '0';
        return true;
    }

    if (c >= 'a' && c <= 'f') {
        *value = 10 + (c - 'a');
        return true;
    }

    if (c >= 'A' && c <= 'F') {
        *value = 10 + (c - 'A');
        return true;
    }

    return false;
}

static WasmToken
LexHexFloatLiteral(const char16_t* begin, const char16_t* end, const char16_t** curp)
{
    const char16_t* cur = begin;

    if (cur != end && (*cur == '-' || *cur == '+'))
        cur++;

    MOZ_ASSERT(cur != end && *cur == '0');
    cur++;
    MOZ_ASSERT(cur != end && *cur == 'x');
    cur++;

    uint8_t digit;
    while (cur != end && IsHexDigit(*cur, &digit))
        cur++;

    if (cur != end && *cur == '.')
        cur++;

    while (cur != end && IsHexDigit(*cur, &digit))
        cur++;

    if (cur != end && *cur == 'p') {
        cur++;

        if (cur != end && (*cur == '-' || *cur == '+'))
            cur++;

        while (cur != end && IsWasmDigit(*cur))
            cur++;
    }

    *curp = cur;
    return WasmToken(WasmToken::HexNumber, begin, cur);
}

static WasmToken
LexDecFloatLiteral(const char16_t* begin, const char16_t* end, const char16_t** curp)
{
    const char16_t* cur = begin;

    if (cur != end && (*cur == '-' || *cur == '+'))
        cur++;

    while (cur != end && IsWasmDigit(*cur))
        cur++;

    if (cur != end && *cur == '.')
        cur++;

    while (cur != end && IsWasmDigit(*cur))
        cur++;

    if (cur != end && *cur == 'e') {
        cur++;

        if (cur != end && (*cur == '-' || *cur == '+'))
            cur++;

        while (cur != end && IsWasmDigit(*cur))
            cur++;
    }

    *curp = cur;
    return WasmToken(WasmToken::DecNumber, begin, cur);
}

static bool
ConsumeTextByte(const char16_t** curp, const char16_t* end, uint8_t *byte = nullptr)
{
    const char16_t*& cur = *curp;
    MOZ_ASSERT(cur != end);

    if (*cur != '\\') {
        if (byte)
            *byte = *cur;
        cur++;
        return true;
    }

    if (++cur == end)
        return false;

    uint8_t u8;
    switch (*cur) {
      case 'n': u8 = '\n'; break;
      case 't': u8 = '\t'; break;
      case '\\': u8 = '\\'; break;
      case '\"': u8 = '\"'; break;
      case '\'': u8 = '\''; break;
      default: {
        uint8_t highNibble;
        if (!IsHexDigit(*cur, &highNibble))
            return false;

        if (++cur == end)
            return false;

        uint8_t lowNibble;
        if (!IsHexDigit(*cur, &lowNibble))
            return false;

        u8 = lowNibble | (highNibble << 4);
        break;
      }
    }

    if (byte)
        *byte = u8;
    cur++;
    return true;
}

namespace {

class WasmTokenStream
{
    static const uint32_t LookaheadSize = 2;

    const char16_t* cur_;
    const char16_t* const end_;
    const char16_t* lineStart_;
    unsigned line_;
    uint32_t lookaheadIndex_;
    uint32_t lookaheadDepth_;
    WasmToken lookahead_[LookaheadSize];

    bool consume(const char16_t* match) {
        const char16_t* p = cur_;
        for (; *match; p++, match++) {
            if (p == end_ || *p != *match)
                return false;
        }
        cur_ = p;
        return true;
    }
    WasmToken fail(const char16_t* begin) const {
        return WasmToken(begin);
    }

    WasmToken nan(const char16_t* begin);
    WasmToken literal(const char16_t* begin);
    WasmToken next();
    void skipSpaces();

  public:
    WasmTokenStream(const char16_t* text, UniqueChars* error)
      : cur_(text),
        end_(text + js_strlen(text)),
        lineStart_(text),
        line_(1),
        lookaheadIndex_(0),
        lookaheadDepth_(0)
    {}
    void generateError(WasmToken token, UniqueChars* error) {
        unsigned column = token.begin() - lineStart_ + 1;
        error->reset(JS_smprintf("parsing wasm text at %u:%u", line_, column));
    }
    void generateError(WasmToken token, const char* msg, UniqueChars* error) {
        unsigned column = token.begin() - lineStart_ + 1;
        error->reset(JS_smprintf("parsing wasm text at %u:%u: %s", line_, column, msg));
    }
    WasmToken peek() {
        if (!lookaheadDepth_) {
            lookahead_[lookaheadIndex_] = next();
            lookaheadDepth_ = 1;
        }
        return lookahead_[lookaheadIndex_];
    }
    WasmToken get() {
        static_assert(LookaheadSize == 2, "can just flip");
        if (lookaheadDepth_) {
            lookaheadDepth_--;
            WasmToken ret = lookahead_[lookaheadIndex_];
            lookaheadIndex_ ^= 1;
            return ret;
        }
        return next();
    }
    void unget(WasmToken token) {
        static_assert(LookaheadSize == 2, "can just flip");
        lookaheadDepth_++;
        lookaheadIndex_ ^= 1;
        lookahead_[lookaheadIndex_] = token;
    }

    // Helpers:
    bool getIf(WasmToken::Kind kind, WasmToken* token) {
        if (peek().kind() == kind) {
            *token = get();
            return true;
        }
        return false;
    }
    bool getIf(WasmToken::Kind kind) {
        WasmToken token;
        if (getIf(kind, &token))
            return true;
        return false;
    }
    AstName getIfName() {
        WasmToken token;
        if (getIf(WasmToken::Name, &token))
            return token.name();
        return AstName();
    }
    bool getIfRef(AstRef* ref) {
        WasmToken token = peek();
        if (token.kind() == WasmToken::Name || token.kind() == WasmToken::Index)
            return matchRef(ref, nullptr);
        return false;
    }
    bool match(WasmToken::Kind expect, WasmToken* token, UniqueChars* error) {
        *token = get();
        if (token->kind() == expect)
            return true;
        generateError(*token, error);
        return false;
    }
    bool match(WasmToken::Kind expect, UniqueChars* error) {
        WasmToken token;
        return match(expect, &token, error);
    }
    bool matchRef(AstRef* ref, UniqueChars* error) {
        WasmToken token = get();
        switch (token.kind()) {
          case WasmToken::Name:
            *ref = AstRef(token.name(), AstNoIndex);
            break;
          case WasmToken::Index:
            *ref = AstRef(AstName(), token.index());
            break;
          default:
            generateError(token, error);
            return false;
        }
        return true;
    }
};

} // end anonymous namespace

WasmToken
WasmTokenStream::nan(const char16_t* begin)
{
    if (consume(u":")) {
        if (!consume(u"0x"))
            return fail(begin);

        uint8_t digit;
        while (cur_ != end_ && IsHexDigit(*cur_, &digit))
            cur_++;
    }

    return WasmToken(WasmToken::NaN, begin, cur_);
}

WasmToken
WasmTokenStream::literal(const char16_t* begin)
{
    CheckedInt<uint64_t> u = 0;
    if (consume(u"0x")) {
        if (cur_ == end_)
            return fail(begin);

        do {
            if (*cur_ == '.' || *cur_ == 'p')
                return LexHexFloatLiteral(begin, end_, &cur_);

            uint8_t digit;
            if (!IsHexDigit(*cur_, &digit))
                break;

            u *= 16;
            u += digit;
            if (!u.isValid())
                return LexHexFloatLiteral(begin, end_, &cur_);

            cur_++;
        } while (cur_ != end_);

        if (*begin == '-') {
            uint64_t value = u.value();
            if (value == 0)
                return WasmToken(WasmToken::NegativeZero, begin, cur_);
            if (value > uint64_t(INT64_MIN))
                return LexHexFloatLiteral(begin, end_, &cur_);

            value = -value;
            return WasmToken(int64_t(value), begin, cur_);
        }
    } else {
        while (cur_ != end_) {
            if (*cur_ == '.' || *cur_ == 'e')
                return LexDecFloatLiteral(begin, end_, &cur_);

            if (!IsWasmDigit(*cur_))
                break;

            u *= 10;
            u += *cur_ - '0';
            if (!u.isValid())
                return LexDecFloatLiteral(begin, end_, &cur_);

            cur_++;
        }

        if (*begin == '-') {
            uint64_t value = u.value();
            if (value == 0)
                return WasmToken(WasmToken::NegativeZero, begin, cur_);
            if (value > uint64_t(INT64_MIN))
                return LexDecFloatLiteral(begin, end_, &cur_);

            value = -value;
            return WasmToken(int64_t(value), begin, cur_);
        }
    }

    CheckedInt<uint32_t> index = u.value();
    if (index.isValid())
        return WasmToken(index.value(), begin, cur_);

    return WasmToken(u.value(), begin, cur_);
}

void
WasmTokenStream::skipSpaces()
{
    while (cur_ != end_) {
        char16_t ch = *cur_;
        if (ch == ';' && consume(u";;")) {
            // Skipping single line comment.
            while (cur_ != end_ && !IsWasmNewLine(*cur_))
                cur_++;
        } else if (ch == '(' && consume(u"(;")) {
            // Skipping multi-line and possibly nested comments.
            size_t level = 1;
            while (cur_ != end_) {
                char16_t ch = *cur_;
                if (ch == '(' && consume(u"(;")) {
                    level++;
                } else if (ch == ';' && consume(u";)")) {
                    if (--level == 0)
                        break;
                } else {
                    cur_++;
                    if (IsWasmNewLine(ch)) {
                        lineStart_ = cur_;
                        line_++;
                    }
                }
            }
        } else if (IsWasmSpace(ch)) {
            cur_++;
            if (IsWasmNewLine(ch)) {
                lineStart_ = cur_;
                line_++;
            }
        } else
            break; // non-whitespace found
    }
}

WasmToken
WasmTokenStream::next()
{
    skipSpaces();

    if (cur_ == end_)
        return WasmToken(WasmToken::EndOfFile, cur_, cur_);

    const char16_t* begin = cur_;
    switch (*begin) {
      case '"':
        cur_++;
        while (true) {
            if (cur_ == end_)
                return fail(begin);
            if (*cur_ == '"')
                break;
            if (!ConsumeTextByte(&cur_, end_))
                return fail(begin);
        }
        cur_++;
        return WasmToken(WasmToken::Text, begin, cur_);

      case '$':
        cur_++;
        while (cur_ != end_ && IsNameAfterDollar(*cur_))
            cur_++;
        return WasmToken(WasmToken::Name, begin, cur_);

      case '(':
        cur_++;
        return WasmToken(WasmToken::OpenParen, begin, cur_);

      case ')':
        cur_++;
        return WasmToken(WasmToken::CloseParen, begin, cur_);

      case '=':
        cur_++;
        return WasmToken(WasmToken::Equal, begin, cur_);

      case '+': case '-':
        cur_++;
        if (consume(u"infinity"))
            return WasmToken(WasmToken::Infinity, begin, cur_);
        if (consume(u"nan"))
            return nan(begin);
        if (!IsWasmDigit(*cur_))
            break;
        MOZ_FALLTHROUGH;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        return literal(begin);

      case 'a':
        if (consume(u"align"))
            return WasmToken(WasmToken::Align, begin, cur_);
        break;

      case 'b':
        if (consume(u"block"))
            return WasmToken(WasmToken::Block, begin, cur_);
        if (consume(u"br")) {
            if (consume(u"_table"))
                return WasmToken(WasmToken::BrTable, begin, cur_);
            if (consume(u"_if"))
                return WasmToken(WasmToken::BrIf, begin, cur_);
            return WasmToken(WasmToken::Br, begin, cur_);
        }
        break;

      case 'c':
        if (consume(u"call")) {
            if (consume(u"_indirect"))
                return WasmToken(WasmToken::CallIndirect, begin, cur_);
            if (consume(u"_import"))
                return WasmToken(WasmToken::CallImport, begin, cur_);
            return WasmToken(WasmToken::Call, begin, cur_);
        }
        break;

      case 'd':
        if (consume(u"data"))
            return WasmToken(WasmToken::Data, begin, cur_);
        break;

      case 'e':
        if (consume(u"elem"))
            return WasmToken(WasmToken::Elem, begin, cur_);
        if (consume(u"else"))
            return WasmToken(WasmToken::Else, begin, cur_);
        if (consume(u"export"))
            return WasmToken(WasmToken::Export, begin, cur_);
        break;

      case 'f':
        if (consume(u"func"))
            return WasmToken(WasmToken::Func, begin, cur_);

        if (consume(u"f32")) {
            if (!consume(u"."))
                return WasmToken(WasmToken::ValueType, ValType::F32, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(u"abs"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Abs, begin, cur_);
                if (consume(u"add"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Add, begin, cur_);
                break;
              case 'c':
                if (consume(u"ceil"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Ceil, begin, cur_);
                if (consume(u"const"))
                    return WasmToken(WasmToken::Const, ValType::F32, begin, cur_);
                if (consume(u"convert_s/i32")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ConvertSI32,
                                     begin, cur_);
                }
                if (consume(u"convert_u/i32")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ConvertUI32,
                                     begin, cur_);
                }
                if (consume(u"convert_s/i64")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ConvertSI64,
                                     begin, cur_);
                }
                if (consume(u"convert_u/i64")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ConvertUI64,
                                     begin, cur_);
                }
                if (consume(u"copysign"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32CopySign, begin, cur_);
                break;
              case 'd':
                if (consume(u"demote/f64"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32DemoteF64,
                                     begin, cur_);
                if (consume(u"div"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Div, begin, cur_);
                break;
              case 'e':
                if (consume(u"eq"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Eq, begin, cur_);
                break;
              case 'f':
                if (consume(u"floor"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Floor, begin, cur_);
                break;
              case 'g':
                if (consume(u"ge"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Ge, begin, cur_);
                if (consume(u"gt"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Gt, begin, cur_);
                break;
              case 'l':
                if (consume(u"le"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Le, begin, cur_);
                if (consume(u"lt"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Lt, begin, cur_);
                if (consume(u"load"))
                    return WasmToken(WasmToken::Load, Expr::F32Load, begin, cur_);
                break;
              case 'm':
                if (consume(u"max"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Max, begin, cur_);
                if (consume(u"min"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Min, begin, cur_);
                if (consume(u"mul"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Mul, begin, cur_);
                break;
              case 'n':
                if (consume(u"nearest"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Nearest, begin, cur_);
                if (consume(u"neg"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Neg, begin, cur_);
                if (consume(u"ne"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Ne, begin, cur_);
                break;
              case 'r':
                if (consume(u"reinterpret/i32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ReinterpretI32,
                                     begin, cur_);
                break;
              case 's':
                if (consume(u"sqrt"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Sqrt, begin, cur_);
                if (consume(u"sub"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Sub, begin, cur_);
                if (consume(u"store"))
                    return WasmToken(WasmToken::Store, Expr::F32Store, begin, cur_);
                break;
              case 't':
                if (consume(u"trunc"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Trunc, begin, cur_);
                break;
            }
            break;
        }
        if (consume(u"f64")) {
            if (!consume(u"."))
                return WasmToken(WasmToken::ValueType, ValType::F64, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(u"abs"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Abs, begin, cur_);
                if (consume(u"add"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Add, begin, cur_);
                break;
              case 'c':
                if (consume(u"ceil"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Ceil, begin, cur_);
                if (consume(u"const"))
                    return WasmToken(WasmToken::Const, ValType::F64, begin, cur_);
                if (consume(u"convert_s/i32")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64ConvertSI32,
                                     begin, cur_);
                }
                if (consume(u"convert_u/i32")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64ConvertUI32,
                                     begin, cur_);
                }
                if (consume(u"convert_s/i64")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64ConvertSI64,
                                     begin, cur_);
                }
                if (consume(u"convert_u/i64")) {
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64ConvertUI64,
                                     begin, cur_);
                }
                if (consume(u"copysign"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64CopySign, begin, cur_);
                break;
              case 'd':
                if (consume(u"div"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Div, begin, cur_);
                break;
              case 'e':
                if (consume(u"eq"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Eq, begin, cur_);
                break;
              case 'f':
                if (consume(u"floor"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Floor, begin, cur_);
                break;
              case 'g':
                if (consume(u"ge"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Ge, begin, cur_);
                if (consume(u"gt"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Gt, begin, cur_);
                break;
              case 'l':
                if (consume(u"le"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Le, begin, cur_);
                if (consume(u"lt"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Lt, begin, cur_);
                if (consume(u"load"))
                    return WasmToken(WasmToken::Load, Expr::F64Load, begin, cur_);
                break;
              case 'm':
                if (consume(u"max"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Max, begin, cur_);
                if (consume(u"min"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Min, begin, cur_);
                if (consume(u"mul"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Mul, begin, cur_);
                break;
              case 'n':
                if (consume(u"nearest"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Nearest, begin, cur_);
                if (consume(u"neg"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Neg, begin, cur_);
                if (consume(u"ne"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Ne, begin, cur_);
                break;
              case 'p':
                if (consume(u"promote/f32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64PromoteF32,
                                     begin, cur_);
                break;
              case 'r':
                if (consume(u"reinterpret/i64"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64ReinterpretI64,
                                     begin, cur_);
                break;
              case 's':
                if (consume(u"sqrt"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Sqrt, begin, cur_);
                if (consume(u"sub"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Sub, begin, cur_);
                if (consume(u"store"))
                    return WasmToken(WasmToken::Store, Expr::F64Store, begin, cur_);
                break;
              case 't':
                if (consume(u"trunc"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Trunc, begin, cur_);
                break;
            }
            break;
        }
        break;

      case 'g':
        if (consume(u"get_local"))
            return WasmToken(WasmToken::GetLocal, begin, cur_);
        break;

      case 'i':
        if (consume(u"i32")) {
            if (!consume(u"."))
                return WasmToken(WasmToken::ValueType, ValType::I32, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(u"add"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Add, begin, cur_);
                if (consume(u"and"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32And, begin, cur_);
                break;
              case 'c':
                if (consume(u"const"))
                    return WasmToken(WasmToken::Const, ValType::I32, begin, cur_);
                if (consume(u"clz"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32Clz, begin, cur_);
                if (consume(u"ctz"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32Ctz, begin, cur_);
                break;
              case 'd':
                if (consume(u"div_s"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32DivS, begin, cur_);
                if (consume(u"div_u"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32DivU, begin, cur_);
                break;
              case 'e':
                if (consume(u"eqz"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32Eqz, begin, cur_);
                if (consume(u"eq"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32Eq, begin, cur_);
                break;
              case 'g':
                if (consume(u"ge_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GeS, begin, cur_);
                if (consume(u"ge_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GeU, begin, cur_);
                if (consume(u"gt_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GtS, begin, cur_);
                if (consume(u"gt_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GtU, begin, cur_);
                break;
              case 'l':
                if (consume(u"le_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LeS, begin, cur_);
                if (consume(u"le_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LeU, begin, cur_);
                if (consume(u"lt_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LtS, begin, cur_);
                if (consume(u"lt_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LtU, begin, cur_);
                if (consume(u"load")) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Load, Expr::I32Load, begin, cur_);
                    if (consume(u"8_s"))
                        return WasmToken(WasmToken::Load, Expr::I32Load8S, begin, cur_);
                    if (consume(u"8_u"))
                        return WasmToken(WasmToken::Load, Expr::I32Load8U, begin, cur_);
                    if (consume(u"16_s"))
                        return WasmToken(WasmToken::Load, Expr::I32Load16S, begin, cur_);
                    if (consume(u"16_u"))
                        return WasmToken(WasmToken::Load, Expr::I32Load16U, begin, cur_);
                    break;
                }
                break;
              case 'm':
                if (consume(u"mul"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Mul, begin, cur_);
                break;
              case 'n':
                if (consume(u"ne"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32Ne, begin, cur_);
                break;
              case 'o':
                if (consume(u"or"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Or, begin, cur_);
                break;
              case 'p':
                if (consume(u"popcnt"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32Popcnt, begin, cur_);
                break;
              case 'r':
                if (consume(u"reinterpret/f32"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32ReinterpretF32,
                                     begin, cur_);
                if (consume(u"rem_s"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32RemS, begin, cur_);
                if (consume(u"rem_u"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32RemU, begin, cur_);
                if (consume(u"rotr"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Rotr, begin, cur_);
                if (consume(u"rotl"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Rotl, begin, cur_);
                break;
              case 's':
                if (consume(u"sub"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Sub, begin, cur_);
                if (consume(u"shl"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Shl, begin, cur_);
                if (consume(u"shr_s"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32ShrS, begin, cur_);
                if (consume(u"shr_u"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32ShrU, begin, cur_);
                if (consume(u"store")) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Store, Expr::I32Store, begin, cur_);
                    if (consume(u"8"))
                        return WasmToken(WasmToken::Store, Expr::I32Store8, begin, cur_);
                    if (consume(u"16"))
                        return WasmToken(WasmToken::Store, Expr::I32Store16, begin, cur_);
                    break;
                }
                break;
              case 't':
                if (consume(u"trunc_s/f32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncSF32,
                                     begin, cur_);
                if (consume(u"trunc_s/f64"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncSF64,
                                     begin, cur_);
                if (consume(u"trunc_u/f32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncUF32,
                                     begin, cur_);
                if (consume(u"trunc_u/f64"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncUF64,
                                     begin, cur_);
                break;
              case 'w':
                if (consume(u"wrap/i64"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32WrapI64,
                                     begin, cur_);
                break;
              case 'x':
                if (consume(u"xor"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Xor, begin, cur_);
                break;
            }
            break;
        }
        if (consume(u"i64")) {
            if (!consume(u"."))
                return WasmToken(WasmToken::ValueType, ValType::I64, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(u"add"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Add, begin, cur_);
                if (consume(u"and"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64And, begin, cur_);
                break;
              case 'c':
                if (consume(u"const"))
                    return WasmToken(WasmToken::Const, ValType::I64, begin, cur_);
                if (consume(u"clz"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64Clz, begin, cur_);
                if (consume(u"ctz"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64Ctz, begin, cur_);
                break;
              case 'd':
                if (consume(u"div_s"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64DivS, begin, cur_);
                if (consume(u"div_u"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64DivU, begin, cur_);
                break;
              case 'e':
                if (consume(u"eqz"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64Eqz, begin, cur_);
                if (consume(u"eq"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64Eq, begin, cur_);
                if (consume(u"extend_s/i32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64ExtendSI32,
                                     begin, cur_);
                if (consume(u"extend_u/i32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64ExtendUI32,
                                     begin, cur_);
                break;
              case 'g':
                if (consume(u"ge_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GeS, begin, cur_);
                if (consume(u"ge_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GeU, begin, cur_);
                if (consume(u"gt_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GtS, begin, cur_);
                if (consume(u"gt_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GtU, begin, cur_);
                break;
              case 'l':
                if (consume(u"le_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LeS, begin, cur_);
                if (consume(u"le_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LeU, begin, cur_);
                if (consume(u"lt_s"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LtS, begin, cur_);
                if (consume(u"lt_u"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LtU, begin, cur_);
                if (consume(u"load")) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Load, Expr::I64Load, begin, cur_);
                    if (consume(u"8_s"))
                        return WasmToken(WasmToken::Load, Expr::I64Load8S, begin, cur_);
                    if (consume(u"8_u"))
                        return WasmToken(WasmToken::Load, Expr::I64Load8U, begin, cur_);
                    if (consume(u"16_s"))
                        return WasmToken(WasmToken::Load, Expr::I64Load16S, begin, cur_);
                    if (consume(u"16_u"))
                        return WasmToken(WasmToken::Load, Expr::I64Load16U, begin, cur_);
                    if (consume(u"32_s"))
                        return WasmToken(WasmToken::Load, Expr::I64Load32S, begin, cur_);
                    if (consume(u"32_u"))
                        return WasmToken(WasmToken::Load, Expr::I64Load32U, begin, cur_);
                    break;
                }
                break;
              case 'm':
                if (consume(u"mul"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Mul, begin, cur_);
                break;
              case 'n':
                if (consume(u"ne"))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64Ne, begin, cur_);
                break;
              case 'o':
                if (consume(u"or"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Or, begin, cur_);
                break;
              case 'p':
                if (consume(u"popcnt"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64Popcnt, begin, cur_);
                break;
              case 'r':
                if (consume(u"reinterpret/f64"))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64ReinterpretF64,
                                     begin, cur_);
                if (consume(u"rem_s"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64RemS, begin, cur_);
                if (consume(u"rem_u"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64RemU, begin, cur_);
                if (consume(u"rotr"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Rotr, begin, cur_);
                if (consume(u"rotl"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Rotl, begin, cur_);
                break;
              case 's':
                if (consume(u"sub"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Sub, begin, cur_);
                if (consume(u"shl"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Shl, begin, cur_);
                if (consume(u"shr_s"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64ShrS, begin, cur_);
                if (consume(u"shr_u"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64ShrU, begin, cur_);
                if (consume(u"store")) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Store, Expr::I64Store, begin, cur_);
                    if (consume(u"8"))
                        return WasmToken(WasmToken::Store, Expr::I64Store8, begin, cur_);
                    if (consume(u"16"))
                        return WasmToken(WasmToken::Store, Expr::I64Store16, begin, cur_);
                    if (consume(u"32"))
                        return WasmToken(WasmToken::Store, Expr::I64Store32, begin, cur_);
                    break;
                }
                break;
              case 't':
                if (consume(u"trunc_s/f32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncSF32,
                                     begin, cur_);
                if (consume(u"trunc_s/f64"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncSF64,
                                     begin, cur_);
                if (consume(u"trunc_u/f32"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncUF32,
                                     begin, cur_);
                if (consume(u"trunc_u/f64"))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncUF64,
                                     begin, cur_);
                break;
              case 'x':
                if (consume(u"xor"))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Xor, begin, cur_);
                break;
            }
            break;
        }
        if (consume(u"import"))
            return WasmToken(WasmToken::Import, begin, cur_);
        if (consume(u"infinity"))
            return WasmToken(WasmToken::Infinity, begin, cur_);
        if (consume(u"if"))
            return WasmToken(WasmToken::If, begin, cur_);
        break;

      case 'l':
        if (consume(u"local"))
            return WasmToken(WasmToken::Local, begin, cur_);
        if (consume(u"loop"))
            return WasmToken(WasmToken::Loop, begin, cur_);
        break;

      case 'm':
        if (consume(u"module"))
            return WasmToken(WasmToken::Module, begin, cur_);
        if (consume(u"memory"))
            return WasmToken(WasmToken::Memory, begin, cur_);
        break;

      case 'n':
        if (consume(u"nan"))
            return nan(begin);
        if (consume(u"nop"))
            return WasmToken(WasmToken::Nop, begin, cur_);
        break;

      case 'o':
        if (consume(u"offset"))
            return WasmToken(WasmToken::Offset, begin, cur_);
        break;

      case 'p':
        if (consume(u"param"))
            return WasmToken(WasmToken::Param, begin, cur_);
        break;

      case 'r':
        if (consume(u"resizable"))
            return WasmToken(WasmToken::Resizable, begin, cur_);
        if (consume(u"result"))
            return WasmToken(WasmToken::Result, begin, cur_);
        if (consume(u"return"))
            return WasmToken(WasmToken::Return, begin, cur_);
        break;

      case 's':
        if (consume(u"select"))
            return WasmToken(WasmToken::TernaryOpcode, Expr::Select, begin, cur_);
        if (consume(u"set_local"))
            return WasmToken(WasmToken::SetLocal, begin, cur_);
        if (consume(u"segment"))
            return WasmToken(WasmToken::Segment, begin, cur_);
        if (consume(u"start"))
            return WasmToken(WasmToken::Start, begin, cur_);
        break;

      case 't':
        if (consume(u"table"))
            return WasmToken(WasmToken::Table, begin, cur_);
        if (consume(u"then"))
            return WasmToken(WasmToken::Then, begin, cur_);
        if (consume(u"type"))
            return WasmToken(WasmToken::Type, begin, cur_);
        break;

      case 'u':
        if (consume(u"unreachable"))
            return WasmToken(WasmToken::Unreachable, begin, cur_);
        break;

      default:
        break;
    }

    return fail(begin);
}

/*****************************************************************************/
// wasm text format parser

namespace {

struct WasmParseContext
{
    WasmTokenStream ts;
    LifoAlloc& lifo;
    UniqueChars* error;
    DtoaState* dtoaState;

    WasmParseContext(const char16_t* text, LifoAlloc& lifo, UniqueChars* error)
      : ts(text, error),
        lifo(lifo),
        error(error),
        dtoaState(NewDtoaState())
    {}

    bool fail(const char* message) {
        error->reset(JS_smprintf(message));
        return false;
    }
    ~WasmParseContext() {
        DestroyDtoaState(dtoaState);
    }
};

} // end anonymous namespace

static AstExpr*
ParseExprInsideParens(WasmParseContext& c);

static AstExpr*
ParseExpr(WasmParseContext& c)
{
    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;

    AstExpr* expr = ParseExprInsideParens(c);
    if (!expr)
        return nullptr;

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;

    return expr;
}

static bool
ParseExprList(WasmParseContext& c, AstExprVector* exprs)
{
    while (c.ts.getIf(WasmToken::OpenParen)) {
        AstExpr* expr = ParseExprInsideParens(c);
        if (!expr || !exprs->append(expr))
            return false;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return false;
    }
    return true;
}

static AstBlock*
ParseBlock(WasmParseContext& c, Expr expr)
{
    AstExprVector exprs(c.lifo);

    AstName breakName = c.ts.getIfName();

    AstName continueName;
    if (expr == Expr::Loop)
        continueName = c.ts.getIfName();

    if (!ParseExprList(c, &exprs))
        return nullptr;

    return new(c.lifo) AstBlock(expr, breakName, continueName, Move(exprs));
}

static AstBranch*
ParseBranch(WasmParseContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::Br || expr == Expr::BrIf);

    AstRef target;
    if (!c.ts.matchRef(&target, c.error))
        return nullptr;

    AstExpr* value = nullptr;
    if (c.ts.getIf(WasmToken::OpenParen)) {
        value = ParseExprInsideParens(c);
        if (!value)
            return nullptr;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    AstExpr* cond = nullptr;
    if (expr == Expr::BrIf) {
        if (c.ts.getIf(WasmToken::OpenParen)) {
            cond = ParseExprInsideParens(c);
            if (!cond)
                return nullptr;
            if (!c.ts.match(WasmToken::CloseParen, c.error))
                return nullptr;
        } else {
            cond = value;
            value = nullptr;
        }
    }

    return new(c.lifo) AstBranch(expr, cond, target, value);
}

static bool
ParseArgs(WasmParseContext& c, AstExprVector* args)
{
    while (c.ts.getIf(WasmToken::OpenParen)) {
        AstExpr* arg = ParseExprInsideParens(c);
        if (!arg || !args->append(arg))
            return false;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return false;
    }

    return true;
}

static AstCall*
ParseCall(WasmParseContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::Call || expr == Expr::CallImport);

    AstRef func;
    if (!c.ts.matchRef(&func, c.error))
        return nullptr;

    AstExprVector args(c.lifo);
    if (!ParseArgs(c, &args))
        return nullptr;

    return new(c.lifo) AstCall(expr, func, Move(args));
}

static AstCallIndirect*
ParseCallIndirect(WasmParseContext& c)
{
    AstRef sig;
    if (!c.ts.matchRef(&sig, c.error))
        return nullptr;

    AstExpr* index = ParseExpr(c);
    if (!index)
        return nullptr;

    AstExprVector args(c.lifo);
    if (!ParseArgs(c, &args))
        return nullptr;

    return new(c.lifo) AstCallIndirect(sig, index, Move(args));
}

static uint_fast8_t
CountLeadingZeroes4(uint8_t x)
{
    MOZ_ASSERT((x & -0x10) == 0);
    return CountLeadingZeroes32(x) - 28;
}

template <typename T>
static T
ushl(T lhs, unsigned rhs)
{
    return rhs < sizeof(T) * CHAR_BIT ? (lhs << rhs) : 0;
}

template <typename T>
static T
ushr(T lhs, unsigned rhs)
{
    return rhs < sizeof(T) * CHAR_BIT ? (lhs >> rhs) : 0;
}

template<typename Float>
static bool
ParseNaNLiteral(const char16_t* cur, const char16_t* end, Float* result)
{
    MOZ_ALWAYS_TRUE(*cur++ == 'n' && *cur++ == 'a' && *cur++ == 'n');
    typedef FloatingPoint<Float> Traits;
    typedef typename Traits::Bits Bits;

    if (cur != end) {
        MOZ_ALWAYS_TRUE(*cur++ == ':' && *cur++ == '0' && *cur++ == 'x');
        if (cur == end)
            return false;
        CheckedInt<Bits> u = 0;
        do {
            uint8_t digit;
            MOZ_ALWAYS_TRUE(IsHexDigit(*cur, &digit));
            u *= 16;
            u += digit;
            cur++;
        } while (cur != end);
        if (!u.isValid())
            return false;
        Bits value = u.value();
        if ((value & ~Traits::kSignificandBits) != 0)
            return false;
        // NaN payloads must contain at least one set bit.
        if (value == 0)
            return false;
        *result = SpecificNaN<Float>(0, value);
    } else {
        // Produce the spec's default NaN.
        *result = SpecificNaN<Float>(0, (Traits::kSignificandBits + 1) >> 1);
    }
    return true;
}

template <typename Float>
static bool
ParseHexFloatLiteral(const char16_t* cur, const char16_t* end, Float* result)
{
    MOZ_ALWAYS_TRUE(*cur++ == '0' && *cur++ == 'x');
    typedef FloatingPoint<Float> Traits;
    typedef typename Traits::Bits Bits;
    static const unsigned numBits = sizeof(Float) * CHAR_BIT;
    static const Bits allOnes = ~Bits(0);
    static const Bits mostSignificantBit = ~(allOnes >> 1);

    // Significand part.
    Bits significand = 0;
    CheckedInt<int32_t> exponent = 0;
    bool sawFirstNonZero = false;
    bool discardedExtraNonZero = false;
    const char16_t* dot = nullptr;
    int significandPos;
    for (; cur != end; cur++) {
        if (*cur == '.') {
            MOZ_ASSERT(!dot);
            dot = cur;
            continue;
        }

        uint8_t digit;
        if (!IsHexDigit(*cur, &digit))
            break;
        if (!sawFirstNonZero) {
            if (digit == 0)
                continue;
            // We've located the first non-zero digit; we can now determine the
            // initial exponent. If we're after the dot, count the number of
            // zeros from the dot to here, and adjust for the number of leading
            // zero bits in the digit. Set up significandPos to put the first
            // nonzero at the most significant bit.
            int_fast8_t lz = CountLeadingZeroes4(digit);
            ptrdiff_t zeroAdjustValue = !dot ? 1 : dot + 1 - cur;
            CheckedInt<ptrdiff_t> zeroAdjust = zeroAdjustValue;
            zeroAdjust *= 4;
            zeroAdjust -= lz + 1;
            if (!zeroAdjust.isValid())
                return false;
            exponent = zeroAdjust.value();
            significandPos = numBits - (4 - lz);
            sawFirstNonZero = true;
        } else {
            // We've already seen a non-zero; just take 4 more bits.
            if (!dot)
                exponent += 4;
            if (significandPos > -4)
                significandPos -= 4;
        }

        // Or the newly parsed digit into significand at signicandPos.
        if (significandPos >= 0) {
            significand |= ushl(Bits(digit), significandPos);
        } else if (significandPos > -4) {
            significand |= ushr(digit, 4 - significandPos);
            discardedExtraNonZero = (digit & ~ushl(allOnes, 4 - significandPos)) != 0;
        } else if (digit != 0) {
            discardedExtraNonZero = true;
        }
    }

    // Exponent part.
    if (cur != end) {
        MOZ_ALWAYS_TRUE(*cur++ == 'p');
        bool isNegated = false;
        if (cur != end && (*cur == '-' || *cur == '+'))
            isNegated = *cur++ == '-';
        CheckedInt<int32_t> parsedExponent = 0;
        while (cur != end && IsWasmDigit(*cur))
            parsedExponent = parsedExponent * 10 + (*cur++ - '0');
        if (isNegated)
            parsedExponent = -parsedExponent;
        exponent += parsedExponent;
    }

    MOZ_ASSERT(cur == end);
    if (!exponent.isValid())
        return false;

    // Create preliminary exponent and significand encodings of the results.
    Bits encodedExponent, encodedSignificand, discardedSignificandBits;
    if (significand == 0) {
        // Zero. The exponent is encoded non-biased.
        encodedExponent = 0;
        encodedSignificand = 0;
        discardedSignificandBits = 0;
    } else if (MOZ_UNLIKELY(exponent.value() <= int32_t(-Traits::kExponentBias))) {
        // Underflow to subnormal or zero.
        encodedExponent = 0;
        encodedSignificand = ushr(significand,
                                  numBits - Traits::kExponentShift -
                                  exponent.value() - Traits::kExponentBias);
        discardedSignificandBits =
            ushl(significand,
                 Traits::kExponentShift + exponent.value() + Traits::kExponentBias);
    } else if (MOZ_LIKELY(exponent.value() <= int32_t(Traits::kExponentBias))) {
        // Normal (non-zero). The significand's leading 1 is encoded implicitly.
        encodedExponent = (Bits(exponent.value()) + Traits::kExponentBias) <<
                          Traits::kExponentShift;
        MOZ_ASSERT(significand & mostSignificantBit);
        encodedSignificand = ushr(significand, numBits - Traits::kExponentShift - 1) &
                             Traits::kSignificandBits;
        discardedSignificandBits = ushl(significand, Traits::kExponentShift + 1);
    } else {
        // Overflow to infinity.
        encodedExponent = Traits::kExponentBits;
        encodedSignificand = 0;
        discardedSignificandBits = 0;
    }
    MOZ_ASSERT((encodedExponent & ~Traits::kExponentBits) == 0);
    MOZ_ASSERT((encodedSignificand & ~Traits::kSignificandBits) == 0);
    MOZ_ASSERT(encodedExponent != Traits::kExponentBits || encodedSignificand == 0);
    Bits bits = encodedExponent | encodedSignificand;

    // Apply rounding. If this overflows the significand, it carries into the
    // exponent bit according to the magic of the IEEE 754 encoding.
    bits += (discardedSignificandBits & mostSignificantBit) &&
            ((discardedSignificandBits & ~mostSignificantBit) ||
             discardedExtraNonZero ||
             // ties to even
             (encodedSignificand & 1));

    *result = BitwiseCast<Float>(bits);
    return true;
}

template <typename Float>
static bool
ParseFloatLiteral(WasmParseContext& c, WasmToken token, Float* result)
{
    *result = 0;

    switch (token.kind()) {
      case WasmToken::Index:
        *result = token.index();
        return true;
      case WasmToken::UnsignedInteger:
        *result = token.uint();
        return true;
      case WasmToken::SignedInteger:
        *result = token.sint();
        return true;
      case WasmToken::NegativeZero:
        *result = -0.0;
        return true;
      case WasmToken::Float:
        break;
      default:
        c.ts.generateError(token, c.error);
        return false;
    }

    const char16_t* begin = token.begin();
    const char16_t* end = token.end();
    const char16_t* cur = begin;

    bool isNegated = false;
    if (*cur == '-' || *cur == '+')
        isNegated = *cur++ == '-';

    switch (token.floatLiteralKind()) {
      case WasmToken::Infinity:
        *result = PositiveInfinity<Float>();
        break;
      case WasmToken::NaN:
        if (!ParseNaNLiteral(cur, end, result)) {
            c.ts.generateError(token, c.error);
            return false;
        }
        break;
      case WasmToken::HexNumber:
        if (!ParseHexFloatLiteral(cur, end, result)) {
            c.ts.generateError(token, c.error);
            return false;
        }
        break;
      case WasmToken::DecNumber: {
        // Call into JS' strtod. Tokenization has already required that the
        // string is well-behaved.
        LifoAlloc::Mark mark = c.lifo.mark();
        char* buffer = c.lifo.newArray<char>(end - cur + 1);
        if (!buffer)
            return false;
        for (ptrdiff_t i = 0; i < end - cur; ++i)
            buffer[i] = char(cur[i]);
        buffer[end - cur] = '\0';
        char* strtod_end;
        int err;
        Float d = (Float)js_strtod_harder(c.dtoaState, buffer, &strtod_end, &err);
        if (err != 0 || strtod_end == buffer) {
            c.lifo.release(mark);
            c.ts.generateError(token, c.error);
            return false;
        }
        c.lifo.release(mark);
        *result = d;
        break;
      }
    }

    if (isNegated)
        *result = -*result;

    return true;
}

static AstConst*
ParseConst(WasmParseContext& c, WasmToken constToken)
{
    WasmToken val = c.ts.get();
    switch (constToken.valueType()) {
      case ValType::I32: {
        switch (val.kind()) {
          case WasmToken::Index:
            return new(c.lifo) AstConst(Val(val.index()));
          case WasmToken::SignedInteger: {
            CheckedInt<int32_t> sint = val.sint();
            if (!sint.isValid())
                break;
            return new(c.lifo) AstConst(Val(uint32_t(sint.value())));
          }
          case WasmToken::NegativeZero:
            return new(c.lifo) AstConst(Val(uint32_t(0)));
          default:
            break;
        }
        break;
      }
      case ValType::I64: {
        switch (val.kind()) {
          case WasmToken::Index:
            return new(c.lifo) AstConst(Val(uint64_t(val.index())));
          case WasmToken::UnsignedInteger:
            return new(c.lifo) AstConst(Val(val.uint()));
          case WasmToken::SignedInteger:
            return new(c.lifo) AstConst(Val(uint64_t(val.sint())));
          case WasmToken::NegativeZero:
            return new(c.lifo) AstConst(Val(uint64_t(0)));
          default:
            break;
        }
        break;
      }
      case ValType::F32: {
        float result;
        if (!ParseFloatLiteral(c, val, &result))
            break;
        return new(c.lifo) AstConst(Val(result));
      }
      case ValType::F64: {
        double result;
        if (!ParseFloatLiteral(c, val, &result))
            break;
        return new(c.lifo) AstConst(Val(result));
      }
      default:
        break;
    }
    c.ts.generateError(constToken, c.error);
    return nullptr;
}

static AstGetLocal*
ParseGetLocal(WasmParseContext& c)
{
    AstRef local;
    if (!c.ts.matchRef(&local, c.error))
        return nullptr;

    return new(c.lifo) AstGetLocal(local);
}

static AstSetLocal*
ParseSetLocal(WasmParseContext& c)
{
    AstRef local;
    if (!c.ts.matchRef(&local, c.error))
        return nullptr;

    AstExpr* value = ParseExpr(c);
    if (!value)
        return nullptr;

    return new(c.lifo) AstSetLocal(local, *value);
}

static AstReturn*
ParseReturn(WasmParseContext& c)
{
    AstExpr* maybeExpr = nullptr;

    if (c.ts.peek().kind() != WasmToken::CloseParen) {
        maybeExpr = ParseExpr(c);
        if (!maybeExpr)
            return nullptr;
    }

    return new(c.lifo) AstReturn(maybeExpr);
}

static AstUnaryOperator*
ParseUnaryOperator(WasmParseContext& c, Expr expr)
{
    AstExpr* op = ParseExpr(c);
    if (!op)
        return nullptr;

    return new(c.lifo) AstUnaryOperator(expr, op);
}

static AstBinaryOperator*
ParseBinaryOperator(WasmParseContext& c, Expr expr)
{
    AstExpr* lhs = ParseExpr(c);
    if (!lhs)
        return nullptr;

    AstExpr* rhs = ParseExpr(c);
    if (!rhs)
        return nullptr;

    return new(c.lifo) AstBinaryOperator(expr, lhs, rhs);
}

static AstComparisonOperator*
ParseComparisonOperator(WasmParseContext& c, Expr expr)
{
    AstExpr* lhs = ParseExpr(c);
    if (!lhs)
        return nullptr;

    AstExpr* rhs = ParseExpr(c);
    if (!rhs)
        return nullptr;

    return new(c.lifo) AstComparisonOperator(expr, lhs, rhs);
}

static AstTernaryOperator*
ParseTernaryOperator(WasmParseContext& c, Expr expr)
{
    AstExpr* op0 = ParseExpr(c);
    if (!op0)
        return nullptr;

    AstExpr* op1 = ParseExpr(c);
    if (!op1)
        return nullptr;

    AstExpr* op2 = ParseExpr(c);
    if (!op2)
        return nullptr;

    return new(c.lifo) AstTernaryOperator(expr, op0, op1, op2);
}

static AstConversionOperator*
ParseConversionOperator(WasmParseContext& c, Expr expr)
{
    AstExpr* op = ParseExpr(c);
    if (!op)
        return nullptr;

    return new(c.lifo) AstConversionOperator(expr, op);
}

static AstIf*
ParseIf(WasmParseContext& c)
{
    AstExpr* cond = ParseExpr(c);
    if (!cond)
        return nullptr;

    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;

    AstName thenName;
    AstExprVector thenExprs(c.lifo);
    if (c.ts.getIf(WasmToken::Then)) {
        thenName = c.ts.getIfName();
        if (!ParseExprList(c, &thenExprs))
            return nullptr;
    } else {
        AstExpr* thenBranch = ParseExprInsideParens(c);
        if (!thenBranch || !thenExprs.append(thenBranch))
            return nullptr;
    }
    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;

    AstName elseName;
    AstExprVector elseExprs(c.lifo);
    if (c.ts.getIf(WasmToken::OpenParen)) {
        if (c.ts.getIf(WasmToken::Else)) {
            elseName = c.ts.getIfName();
            if (!ParseExprList(c, &elseExprs))
                return nullptr;
        } else {
            AstExpr* elseBranch = ParseExprInsideParens(c);
            if (!elseBranch || !elseExprs.append(elseBranch))
                return nullptr;
        }
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    return new(c.lifo) AstIf(cond, thenName, Move(thenExprs), elseName, Move(elseExprs));
}

static bool
ParseLoadStoreAddress(WasmParseContext& c, int32_t* offset, uint32_t* alignLog2, AstExpr** base)
{
    *offset = 0;
    if (c.ts.getIf(WasmToken::Offset)) {
        if (!c.ts.match(WasmToken::Equal, c.error))
            return false;
        WasmToken val = c.ts.get();
        switch (val.kind()) {
          case WasmToken::Index:
            *offset = val.index();
            break;
          default:
            c.ts.generateError(val, c.error);
            return false;
        }
    }

    *alignLog2 = UINT32_MAX;
    if (c.ts.getIf(WasmToken::Align)) {
        if (!c.ts.match(WasmToken::Equal, c.error))
            return false;
        WasmToken val = c.ts.get();
        switch (val.kind()) {
          case WasmToken::Index:
            if (!IsPowerOfTwo(val.index())) {
                c.ts.generateError(val, "non-power-of-two alignment", c.error);
                return false;
            }
            *alignLog2 = CeilingLog2(val.index());
            break;
          default:
            c.ts.generateError(val, c.error);
            return false;
        }
    }

    *base = ParseExpr(c);
    if (!*base)
        return false;

    return true;
}

static AstLoad*
ParseLoad(WasmParseContext& c, Expr expr)
{
    int32_t offset;
    uint32_t alignLog2;
    AstExpr* base;
    if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base))
        return nullptr;

    if (alignLog2 == UINT32_MAX) {
        switch (expr) {
          case Expr::I32Load8S:
          case Expr::I32Load8U:
          case Expr::I64Load8S:
          case Expr::I64Load8U:
            alignLog2 = 0;
            break;
          case Expr::I32Load16S:
          case Expr::I32Load16U:
          case Expr::I64Load16S:
          case Expr::I64Load16U:
            alignLog2 = 1;
            break;
          case Expr::I32Load:
          case Expr::F32Load:
          case Expr::I64Load32S:
          case Expr::I64Load32U:
            alignLog2 = 2;
            break;
          case Expr::I64Load:
          case Expr::F64Load:
            alignLog2 = 3;
            break;
          default:
            MOZ_CRASH("Bad load expr");
        }
    }

    uint32_t flags = alignLog2;

    return new(c.lifo) AstLoad(expr, AstLoadStoreAddress(base, flags, offset));
}

static AstStore*
ParseStore(WasmParseContext& c, Expr expr)
{
    int32_t offset;
    uint32_t alignLog2;
    AstExpr* base;
    if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base))
        return nullptr;

    if (alignLog2 == UINT32_MAX) {
        switch (expr) {
          case Expr::I32Store8:
          case Expr::I64Store8:
            alignLog2 = 0;
            break;
          case Expr::I32Store16:
          case Expr::I64Store16:
            alignLog2 = 1;
            break;
          case Expr::I32Store:
          case Expr::F32Store:
          case Expr::I64Store32:
            alignLog2 = 2;
            break;
          case Expr::I64Store:
          case Expr::F64Store:
            alignLog2 = 3;
            break;
          default:
            MOZ_CRASH("Bad load expr");
        }
    }

    AstExpr* value = ParseExpr(c);
    if (!value)
        return nullptr;

    uint32_t flags = alignLog2;

    return new(c.lifo) AstStore(expr, AstLoadStoreAddress(base, flags, offset), value);
}

static AstBranchTable*
ParseBranchTable(WasmParseContext& c, WasmToken brTable)
{
    AstRefVector table(c.lifo);

    AstRef target;
    while (c.ts.getIfRef(&target)) {
        if (!table.append(target))
            return nullptr;
    }

    if (table.empty()) {
        c.ts.generateError(brTable, c.error);
        return nullptr;
    }

    AstRef def = table.popCopy();

    AstExpr* index = ParseExpr(c);
    if (!index)
        return nullptr;

    AstExpr* value = nullptr;
    if (c.ts.getIf(WasmToken::OpenParen)) {
        value = index;
        index = ParseExprInsideParens(c);
        if (!index)
            return nullptr;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    return new(c.lifo) AstBranchTable(*index, def, Move(table), value);
}

static AstExpr*
ParseExprInsideParens(WasmParseContext& c)
{
    WasmToken token = c.ts.get();

    switch (token.kind()) {
      case WasmToken::Nop:
        return new(c.lifo) AstNop;
      case WasmToken::Unreachable:
        return new(c.lifo) AstUnreachable;
      case WasmToken::BinaryOpcode:
        return ParseBinaryOperator(c, token.expr());
      case WasmToken::Block:
        return ParseBlock(c, Expr::Block);
      case WasmToken::Br:
        return ParseBranch(c, Expr::Br);
      case WasmToken::BrIf:
        return ParseBranch(c, Expr::BrIf);
      case WasmToken::BrTable:
        return ParseBranchTable(c, token);
      case WasmToken::Call:
        return ParseCall(c, Expr::Call);
      case WasmToken::CallImport:
        return ParseCall(c, Expr::CallImport);
      case WasmToken::CallIndirect:
        return ParseCallIndirect(c);
      case WasmToken::ComparisonOpcode:
        return ParseComparisonOperator(c, token.expr());
      case WasmToken::Const:
        return ParseConst(c, token);
      case WasmToken::ConversionOpcode:
        return ParseConversionOperator(c, token.expr());
      case WasmToken::If:
        return ParseIf(c);
      case WasmToken::GetLocal:
        return ParseGetLocal(c);
      case WasmToken::Load:
        return ParseLoad(c, token.expr());
      case WasmToken::Loop:
        return ParseBlock(c, Expr::Loop);
      case WasmToken::Return:
        return ParseReturn(c);
      case WasmToken::SetLocal:
        return ParseSetLocal(c);
      case WasmToken::Store:
        return ParseStore(c, token.expr());
      case WasmToken::TernaryOpcode:
        return ParseTernaryOperator(c, token.expr());
      case WasmToken::UnaryOpcode:
        return ParseUnaryOperator(c, token.expr());
      default:
        c.ts.generateError(token, c.error);
        return nullptr;
    }
}

static bool
ParseValueTypeList(WasmParseContext& c, AstValTypeVector* vec)
{
    WasmToken token;
    while (c.ts.getIf(WasmToken::ValueType, &token)) {
        if (!vec->append(token.valueType()))
            return false;
    }

    return true;
}

static bool
ParseResult(WasmParseContext& c, ExprType* result)
{
    if (*result != ExprType::Void) {
        c.ts.generateError(c.ts.peek(), c.error);
        return false;
    }

    WasmToken token;
    if (!c.ts.match(WasmToken::ValueType, &token, c.error))
        return false;

    *result = ToExprType(token.valueType());
    return true;
}

static bool
ParseLocalOrParam(WasmParseContext& c, AstNameVector* locals, AstValTypeVector* localTypes)
{
    if (c.ts.peek().kind() != WasmToken::Name)
        return locals->append(AstName()) && ParseValueTypeList(c, localTypes);

    WasmToken token;
    return locals->append(c.ts.get().name()) &&
           c.ts.match(WasmToken::ValueType, &token, c.error) &&
           localTypes->append(token.valueType());
}

static AstFunc*
ParseFunc(WasmParseContext& c, AstModule* module)
{
    AstValTypeVector vars(c.lifo);
    AstValTypeVector args(c.lifo);
    AstNameVector locals(c.lifo);

    AstName funcName = c.ts.getIfName();

    AstRef sig;

    WasmToken openParen;
    if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
        if (c.ts.getIf(WasmToken::Type)) {
            if (!c.ts.matchRef(&sig, c.error))
                return nullptr;
            if (!c.ts.match(WasmToken::CloseParen, c.error))
                return nullptr;
        } else {
            c.ts.unget(openParen);
        }
    }

    AstExprVector body(c.lifo);
    ExprType result = ExprType::Void;

    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmToken token = c.ts.get();
        switch (token.kind()) {
          case WasmToken::Local:
            if (!ParseLocalOrParam(c, &locals, &vars))
                return nullptr;
            break;
          case WasmToken::Param:
            if (!vars.empty()) {
                c.ts.generateError(token, c.error);
                return nullptr;
            }
            if (!ParseLocalOrParam(c, &locals, &args))
                return nullptr;
            break;
          case WasmToken::Result:
            if (!ParseResult(c, &result))
                return nullptr;
            break;
          default:
            c.ts.unget(token);
            AstExpr* expr = ParseExprInsideParens(c);
            if (!expr || !body.append(expr))
                return nullptr;
            break;
        }
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    if (sig.isInvalid()) {
        uint32_t sigIndex;
        if (!module->declare(AstSig(Move(args), result), &sigIndex))
            return nullptr;
        sig.setIndex(sigIndex);
    }

    return new(c.lifo) AstFunc(funcName, sig, Move(vars), Move(locals), Move(body));
}

static bool
ParseFuncType(WasmParseContext& c, AstSig* sig)
{
    AstValTypeVector args(c.lifo);
    ExprType result = ExprType::Void;

    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmToken token = c.ts.get();
        switch (token.kind()) {
          case WasmToken::Param:
            if (!ParseValueTypeList(c, &args))
                return false;
            break;
          case WasmToken::Result:
            if (!ParseResult(c, &result))
                return false;
            break;
          default:
            c.ts.generateError(token, c.error);
            return false;
        }
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return false;
    }

    *sig = AstSig(Move(args), result);
    return true;
}

static AstSig*
ParseTypeDef(WasmParseContext& c)
{
    AstName name = c.ts.getIfName();

    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::Func, c.error))
        return nullptr;

    AstSig sig(c.lifo);
    if (!ParseFuncType(c, &sig))
        return nullptr;

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;

    return new(c.lifo) AstSig(name, Move(sig));
}

static AstDataSegment*
ParseDataSegment(WasmParseContext& c)
{
    WasmToken dstOffset;
    if (!c.ts.match(WasmToken::Index, &dstOffset, c.error))
        return nullptr;

    WasmToken text;
    if (!c.ts.match(WasmToken::Text, &text, c.error))
        return nullptr;

    return new(c.lifo) AstDataSegment(dstOffset.index(), text.text());
}

static bool
ParseResizable(WasmParseContext& c, AstResizable* resizable)
{
    WasmToken initial;
    if (!c.ts.match(WasmToken::Index, &initial, c.error))
        return false;

    Maybe<uint32_t> maximum;
    WasmToken token;
    if (c.ts.getIf(WasmToken::Index, &token))
        maximum.emplace(token.index());

    *resizable = AstResizable(initial.index(), maximum);
    return true;
}

static bool
ParseMemory(WasmParseContext& c, WasmToken token, AstModule* module)
{
    AstResizable memory;
    if (!ParseResizable(c, &memory))
        return false;

    while (c.ts.getIf(WasmToken::OpenParen)) {
        if (!c.ts.match(WasmToken::Segment, c.error))
            return false;
        AstDataSegment* segment = ParseDataSegment(c);
        if (!segment || !module->append(segment))
            return false;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return false;
    }

    if (!module->setMemory(memory)) {
        c.ts.generateError(token, c.error);
        return false;
    }

    return true;
}

static bool
ParseStartFunc(WasmParseContext& c, WasmToken token, AstModule* module)
{
    AstRef func;
    if (!c.ts.matchRef(&func, c.error))
        return false;

    if (!module->setStartFunc(AstStartFunc(func))) {
        c.ts.generateError(token, c.error);
        return false;
    }

    return true;
}

static AstImport*
ParseImport(WasmParseContext& c, bool newFormat, AstModule* module)
{
    AstName name = c.ts.getIfName();

    WasmToken moduleName;
    if (!c.ts.match(WasmToken::Text, &moduleName, c.error))
        return nullptr;

    WasmToken fieldName;
    if (!c.ts.match(WasmToken::Text, &fieldName, c.error))
        return nullptr;

    AstRef sigRef;
    WasmToken openParen;
    if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
        if (newFormat) {
            if (c.ts.getIf(WasmToken::Memory)) {
                AstResizable memory;
                if (!ParseResizable(c, &memory))
                    return nullptr;
                if (!c.ts.match(WasmToken::CloseParen, c.error))
                    return nullptr;
                return new(c.lifo) AstImport(name, moduleName.text(), fieldName.text(),
                                             DefinitionKind::Memory, memory);
            }
            if (c.ts.getIf(WasmToken::Table)) {
                AstResizable table;
                if (!ParseResizable(c, &table))
                    return nullptr;
                if (!c.ts.match(WasmToken::CloseParen, c.error))
                    return nullptr;
                return new(c.lifo) AstImport(name, moduleName.text(), fieldName.text(),
                                             DefinitionKind::Table, table);
            }
        }

        if (c.ts.getIf(WasmToken::Type)) {
            if (!c.ts.matchRef(&sigRef, c.error))
                return nullptr;
            if (!c.ts.match(WasmToken::CloseParen, c.error))
                return nullptr;
        } else {
            c.ts.unget(openParen);
        }
    }

    if (sigRef.isInvalid()) {
        AstSig sig(c.lifo);
        if (!ParseFuncType(c, &sig))
            return nullptr;

        uint32_t sigIndex;
        if (!module->declare(Move(sig), &sigIndex))
            return nullptr;
        sigRef.setIndex(sigIndex);
    }

    return new(c.lifo) AstImport(name, moduleName.text(), fieldName.text(), sigRef);
}

static AstExport*
ParseExport(WasmParseContext& c)
{
    WasmToken name;
    if (!c.ts.match(WasmToken::Text, &name, c.error))
        return nullptr;

    WasmToken exportee = c.ts.get();
    switch (exportee.kind()) {
      case WasmToken::Index:
        return new(c.lifo) AstExport(name.text(), AstRef(AstName(), exportee.index()));
      case WasmToken::Name:
        return new(c.lifo) AstExport(name.text(), AstRef(exportee.name(), AstNoIndex));
      case WasmToken::Table:
        return new(c.lifo) AstExport(name.text(), DefinitionKind::Table);
      case WasmToken::Memory:
        return new(c.lifo) AstExport(name.text(), DefinitionKind::Memory);
      default:
        break;
    }

    c.ts.generateError(exportee, c.error);
    return nullptr;

}

static bool
ParseTable(WasmParseContext& c, WasmToken token, AstModule* module)
{
    if (c.ts.getIf(WasmToken::OpenParen)) {
        if (!c.ts.match(WasmToken::Resizable, c.error))
            return false;
        AstResizable table;
        if (!ParseResizable(c, &table))
            return false;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return false;
        if (!module->setTable(table)) {
            c.ts.generateError(token, c.error);
            return false;
        }
        return true;
    }

    AstRefVector elems(c.lifo);

    AstRef elem;
    while (c.ts.getIfRef(&elem)) {
        if (!elems.append(elem))
            return false;
    }

    if (!module->setTable(AstResizable(elems.length(), Some<uint32_t>(elems.length())))) {
        c.ts.generateError(token, c.error);
        return false;
    }

    AstElemSegment* segment = new(c.lifo) AstElemSegment(0, Move(elems));
    return segment && module->append(segment);
}

static AstElemSegment*
ParseElemSegment(WasmParseContext& c)
{
    WasmToken offset;
    if (!c.ts.match(WasmToken::Index, &offset, c.error))
        return nullptr;

    AstRefVector elems(c.lifo);

    AstRef elem;
    while (c.ts.getIfRef(&elem)) {
        if (!elems.append(elem))
            return nullptr;
    }

    return new(c.lifo) AstElemSegment(offset.index(), Move(elems));
}

static AstModule*
ParseModule(const char16_t* text, bool newFormat, LifoAlloc& lifo, UniqueChars* error)
{
    WasmParseContext c(text, lifo, error);

    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::Module, c.error))
        return nullptr;

    auto module = new(c.lifo) AstModule(c.lifo);
    if (!module || !module->init())
        return nullptr;

    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmToken section = c.ts.get();

        switch (section.kind()) {
          case WasmToken::Type: {
            AstSig* sig = ParseTypeDef(c);
            if (!sig || !module->append(sig))
                return nullptr;
            break;
          }
          case WasmToken::Start: {
            if (!ParseStartFunc(c, section, module))
                return nullptr;
            break;
          }
          case WasmToken::Memory: {
            if (!ParseMemory(c, section, module))
                return nullptr;
            break;
          }
          case WasmToken::Data: {
            AstDataSegment* segment = ParseDataSegment(c);
            if (!segment || !module->append(segment))
                return nullptr;
            break;
          }
          case WasmToken::Import: {
            AstImport* imp = ParseImport(c, newFormat, module);
            if (!imp || !module->append(imp))
                return nullptr;
            break;
          }
          case WasmToken::Export: {
            AstExport* exp = ParseExport(c);
            if (!exp || !module->append(exp))
                return nullptr;
            break;
          }
          case WasmToken::Table: {
            if (!ParseTable(c, section, module))
                return nullptr;
            break;
          }
          case WasmToken::Elem: {
            AstElemSegment* segment = ParseElemSegment(c);
            if (!segment || !module->append(segment))
                return nullptr;
            break;
          }
          case WasmToken::Func: {
            AstFunc* func = ParseFunc(c, module);
            if (!func || !module->append(func))
                return nullptr;
            break;
          }
          default:
            c.ts.generateError(section, c.error);
            return nullptr;
        }

        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::EndOfFile, c.error))
        return nullptr;

    return module;
}

/*****************************************************************************/
// wasm name resolution

namespace {

class Resolver
{
    UniqueChars* error_;
    AstNameMap varMap_;
    AstNameMap sigMap_;
    AstNameMap funcMap_;
    AstNameMap importMap_;
    AstNameVector targetStack_;

    bool registerName(AstNameMap& map, AstName name, size_t index) {
        AstNameMap::AddPtr p = map.lookupForAdd(name);
        if (!p) {
            if (!map.add(p, name, index))
                return false;
        } else {
            return false;
        }
        return true;
    }
    bool resolveName(AstNameMap& map, AstName name, size_t* index) {
        AstNameMap::Ptr p = map.lookup(name);
        if (p) {
            *index = p->value();
            return true;
        }
        return false;
    }
    bool resolveRef(AstNameMap& map, AstRef& ref) {
        AstNameMap::Ptr p = map.lookup(ref.name());
        if (p) {
            ref.setIndex(p->value());
            return true;
        }
        return false;
    }
    bool failResolveLabel(const char* kind, AstName name) {
        Vector<char16_t, 0, SystemAllocPolicy> nameWithNull;
        if (!nameWithNull.append(name.begin(), name.length()))
            return false;
        if (!nameWithNull.append(0))
            return false;
        error_->reset(JS_smprintf("%s label '%hs' not found", kind, nameWithNull.begin()));
        return false;
    }

  public:
    explicit Resolver(LifoAlloc& lifo, UniqueChars* error)
      : error_(error),
        varMap_(lifo),
        sigMap_(lifo),
        funcMap_(lifo),
        importMap_(lifo),
        targetStack_(lifo)
    {}
    bool init() {
        return sigMap_.init() && funcMap_.init() && importMap_.init() && varMap_.init();
    }
    void beginFunc() {
        varMap_.clear();
        MOZ_ASSERT(targetStack_.empty());
    }
    bool registerSigName(AstName name, size_t index) {
        return name.empty() || registerName(sigMap_, name, index);
    }
    bool registerFuncName(AstName name, size_t index) {
        return name.empty() || registerName(funcMap_, name, index);
    }
    bool registerImportName(AstName name, size_t index) {
        return name.empty() || registerName(importMap_, name, index);
    }
    bool registerVarName(AstName name, size_t index) {
        return name.empty() || registerName(varMap_, name, index);
    }
    bool pushTarget(AstName name) {
        return targetStack_.append(name);
    }
    void popTarget(AstName name) {
        MOZ_ASSERT(targetStack_.back() == name);
        targetStack_.popBack();
    }

    bool resolveSignature(AstRef& ref) {
        if (!ref.name().empty() && !resolveRef(sigMap_, ref))
            return failResolveLabel("signature", ref.name());
        return true;
    }
    bool resolveFunction(AstRef& ref) {
        if (!ref.name().empty() && !resolveRef(funcMap_, ref))
            return failResolveLabel("function", ref.name());
        return true;
    }
    bool resolveImport(AstRef& ref) {
        if (!ref.name().empty() && !resolveRef(importMap_, ref))
            return failResolveLabel("import", ref.name());
        return true;
    }
    bool resolveLocal(AstRef& ref) {
        if (!ref.name().empty() && !resolveRef(varMap_, ref))
            return failResolveLabel("local", ref.name());
        return true;
    }
    bool resolveBranchTarget(AstRef& ref) {
        if (ref.name().empty())
            return true;
        for (size_t i = 0, e = targetStack_.length(); i < e; i++) {
            if (targetStack_[e - i - 1] == ref.name()) {
                ref.setIndex(i);
                return true;
            }
        }
        return failResolveLabel("branch target", ref.name());
    }

    bool fail(const char* message) {
        error_->reset(JS_smprintf("%s", message));
        return false;
    }
};

} // end anonymous namespace

static bool
ResolveExpr(Resolver& r, AstExpr& expr);

static bool
ResolveExprList(Resolver& r, const AstExprVector& v)
{
    for (size_t i = 0; i < v.length(); i++) {
        if (!ResolveExpr(r, *v[i]))
            return false;
    }
    return true;
}

static bool
ResolveBlock(Resolver& r, AstBlock& b)
{
    if (!r.pushTarget(b.breakName()))
        return false;

    if (b.expr() == Expr::Loop) {
        if (!r.pushTarget(b.continueName()))
            return false;
    }

    if (!ResolveExprList(r, b.exprs()))
        return false;

    if (b.expr() == Expr::Loop)
        r.popTarget(b.continueName());
    r.popTarget(b.breakName());
    return true;
}

static bool
ResolveBranch(Resolver& r, AstBranch& br)
{
    if (!r.resolveBranchTarget(br.target()))
        return false;

    if (br.maybeValue() && !ResolveExpr(r, *br.maybeValue()))
        return false;

    if (br.expr() == Expr::BrIf) {
        if (!ResolveExpr(r, br.cond()))
            return false;
    }

    return true;
}

static bool
ResolveArgs(Resolver& r, const AstExprVector& args)
{
    for (AstExpr* arg : args) {
        if (!ResolveExpr(r, *arg))
            return false;
    }

    return true;
}

static bool
ResolveCall(Resolver& r, AstCall& c)
{
    if (!ResolveArgs(r, c.args()))
        return false;

    if (c.expr() == Expr::Call) {
        if (!r.resolveFunction(c.func()))
            return false;
    } else {
        MOZ_ASSERT(c.expr() == Expr::CallImport);
        if (!r.resolveImport(c.func()))
            return false;
    }

    return true;
}

static bool
ResolveCallIndirect(Resolver& r, AstCallIndirect& c)
{
    if (!ResolveExpr(r, *c.index()))
        return false;

    if (!ResolveArgs(r, c.args()))
        return false;

    if (!r.resolveSignature(c.sig()))
        return false;

    return true;
}

static bool
ResolveGetLocal(Resolver& r, AstGetLocal& gl)
{
    return r.resolveLocal(gl.local());
}

static bool
ResolveSetLocal(Resolver& r, AstSetLocal& sl)
{
    if (!ResolveExpr(r, sl.value()))
        return false;

    if (!r.resolveLocal(sl.local()))
        return false;

    return true;
}

static bool
ResolveUnaryOperator(Resolver& r, AstUnaryOperator& b)
{
    return ResolveExpr(r, *b.op());
}

static bool
ResolveBinaryOperator(Resolver& r, AstBinaryOperator& b)
{
    return ResolveExpr(r, *b.lhs()) &&
           ResolveExpr(r, *b.rhs());
}

static bool
ResolveTernaryOperator(Resolver& r, AstTernaryOperator& b)
{
    return ResolveExpr(r, *b.op0()) &&
           ResolveExpr(r, *b.op1()) &&
           ResolveExpr(r, *b.op2());
}

static bool
ResolveComparisonOperator(Resolver& r, AstComparisonOperator& b)
{
    return ResolveExpr(r, *b.lhs()) &&
           ResolveExpr(r, *b.rhs());
}

static bool
ResolveConversionOperator(Resolver& r, AstConversionOperator& b)
{
    return ResolveExpr(r, *b.op());
}

static bool
ResolveIfElse(Resolver& r, AstIf& i)
{
    if (!ResolveExpr(r, i.cond()))
        return false;
    if (!r.pushTarget(i.thenName()))
        return false;
    if (!ResolveExprList(r, i.thenExprs()))
        return false;
    r.popTarget(i.thenName());
    if (i.hasElse()) {
        if (!r.pushTarget(i.elseName()))
            return false;
        if (!ResolveExprList(r, i.elseExprs()))
            return false;
        r.popTarget(i.elseName());
    }
    return true;
}

static bool
ResolveLoadStoreAddress(Resolver& r, const AstLoadStoreAddress &address)
{
    return ResolveExpr(r, address.base());
}

static bool
ResolveLoad(Resolver& r, AstLoad& l)
{
    return ResolveLoadStoreAddress(r, l.address());
}

static bool
ResolveStore(Resolver& r, AstStore& s)
{
    return ResolveLoadStoreAddress(r, s.address()) &&
           ResolveExpr(r, s.value());
}

static bool
ResolveReturn(Resolver& r, AstReturn& ret)
{
    return !ret.maybeExpr() || ResolveExpr(r, *ret.maybeExpr());
}

static bool
ResolveBranchTable(Resolver& r, AstBranchTable& bt)
{
    if (!r.resolveBranchTarget(bt.def()))
        return false;

    for (AstRef& elem : bt.table()) {
        if (!r.resolveBranchTarget(elem))
            return false;
    }

    if (bt.maybeValue() && !ResolveExpr(r, *bt.maybeValue()))
        return false;

    return ResolveExpr(r, bt.index());
}

static bool
ResolveExpr(Resolver& r, AstExpr& expr)
{
    switch (expr.kind()) {
      case AstExprKind::Nop:
      case AstExprKind::Unreachable:
        return true;
      case AstExprKind::BinaryOperator:
        return ResolveBinaryOperator(r, expr.as<AstBinaryOperator>());
      case AstExprKind::Block:
        return ResolveBlock(r, expr.as<AstBlock>());
      case AstExprKind::Branch:
        return ResolveBranch(r, expr.as<AstBranch>());
      case AstExprKind::Call:
        return ResolveCall(r, expr.as<AstCall>());
      case AstExprKind::CallIndirect:
        return ResolveCallIndirect(r, expr.as<AstCallIndirect>());
      case AstExprKind::ComparisonOperator:
        return ResolveComparisonOperator(r, expr.as<AstComparisonOperator>());
      case AstExprKind::Const:
        return true;
      case AstExprKind::ConversionOperator:
        return ResolveConversionOperator(r, expr.as<AstConversionOperator>());
      case AstExprKind::GetLocal:
        return ResolveGetLocal(r, expr.as<AstGetLocal>());
      case AstExprKind::If:
        return ResolveIfElse(r, expr.as<AstIf>());
      case AstExprKind::Load:
        return ResolveLoad(r, expr.as<AstLoad>());
      case AstExprKind::Return:
        return ResolveReturn(r, expr.as<AstReturn>());
      case AstExprKind::SetLocal:
        return ResolveSetLocal(r, expr.as<AstSetLocal>());
      case AstExprKind::Store:
        return ResolveStore(r, expr.as<AstStore>());
      case AstExprKind::BranchTable:
        return ResolveBranchTable(r, expr.as<AstBranchTable>());
      case AstExprKind::TernaryOperator:
        return ResolveTernaryOperator(r, expr.as<AstTernaryOperator>());
      case AstExprKind::UnaryOperator:
        return ResolveUnaryOperator(r, expr.as<AstUnaryOperator>());
    }
    MOZ_CRASH("Bad expr kind");
}

static bool
ResolveFunc(Resolver& r, AstFunc& func)
{
    r.beginFunc();

    size_t numVars = func.locals().length();
    for (size_t i = 0; i < numVars; i++) {
        if (!r.registerVarName(func.locals()[i], i))
            return r.fail("duplicate var");
    }

    for (AstExpr* expr : func.body()) {
        if (!ResolveExpr(r, *expr))
            return false;
    }
    return true;
}

static bool
ResolveModule(LifoAlloc& lifo, AstModule* module, UniqueChars* error)
{
    Resolver r(lifo, error);

    if (!r.init())
        return false;

    size_t numSigs = module->sigs().length();
    for (size_t i = 0; i < numSigs; i++) {
        AstSig* sig = module->sigs()[i];
        if (!r.registerSigName(sig->name(), i))
            return r.fail("duplicate signature");
    }

    size_t numFuncs = module->funcs().length();
    for (size_t i = 0; i < numFuncs; i++) {
        AstFunc* func = module->funcs()[i];
        if (!r.resolveSignature(func->sig()))
            return false;
        if (!r.registerFuncName(func->name(), i))
            return r.fail("duplicate function");
    }

    for (AstElemSegment* seg : module->elemSegments()) {
        for (AstRef& ref : seg->elems()) {
            if (!r.resolveFunction(ref))
                return false;
        }
    }

    size_t numImports = module->imports().length();
    for (size_t i = 0; i < numImports; i++) {
        AstImport* imp = module->imports()[i];
        if (!r.registerImportName(imp->name(), i))
            return r.fail("duplicate import");

        switch (imp->kind()) {
          case DefinitionKind::Function:
            if (!r.resolveSignature(imp->funcSig()))
                return false;
            break;
          case DefinitionKind::Memory:
          case DefinitionKind::Table:
            break;
        }
    }

    for (AstExport* export_ : module->exports()) {
        if (export_->kind() != DefinitionKind::Function)
            continue;
        if (!r.resolveFunction(export_->func()))
            return false;
    }

    for (AstFunc* func : module->funcs()) {
        if (!ResolveFunc(r, *func))
            return false;
    }

    if (module->hasStartFunc()) {
        if (!r.resolveFunction(module->startFunc().func()))
            return false;
    }

    return true;
}

/*****************************************************************************/
// wasm function body serialization

static bool
EncodeExpr(Encoder& e, AstExpr& expr);

static bool
EncodeExprList(Encoder& e, const AstExprVector& v)
{
    for (size_t i = 0; i < v.length(); i++) {
        if (!EncodeExpr(e, *v[i]))
            return false;
    }
    return true;
}

static bool
EncodeBlock(Encoder& e, AstBlock& b)
{
    if (!e.writeExpr(b.expr()))
        return false;

    if (!EncodeExprList(e, b.exprs()))
        return false;

    if (!e.writeExpr(Expr::End))
        return false;

    return true;
}

static bool
EncodeBranch(Encoder& e, AstBranch& br)
{
    MOZ_ASSERT(br.expr() == Expr::Br || br.expr() == Expr::BrIf);

    uint32_t arity = 0;
    if (br.maybeValue()) {
        arity = 1;
        if (!EncodeExpr(e, *br.maybeValue()))
            return false;
    }

    if (br.expr() == Expr::BrIf) {
        if (!EncodeExpr(e, br.cond()))
            return false;
    }

    if (!e.writeExpr(br.expr()))
        return false;

    if (!e.writeVarU32(arity))
        return false;

    if (!e.writeVarU32(br.target().index()))
        return false;

    return true;
}

static bool
EncodeArgs(Encoder& e, const AstExprVector& args)
{
    for (AstExpr* arg : args) {
        if (!EncodeExpr(e, *arg))
            return false;
    }

    return true;
}

static bool
EncodeCall(Encoder& e, AstCall& c)
{
    if (!EncodeArgs(e, c.args()))
        return false;

    if (!e.writeExpr(c.expr()))
        return false;

    if (!e.writeVarU32(c.args().length()))
        return false;

    if (!e.writeVarU32(c.func().index()))
        return false;

    return true;
}

static bool
EncodeCallIndirect(Encoder& e, AstCallIndirect& c)
{
    if (!EncodeExpr(e, *c.index()))
        return false;

    if (!EncodeArgs(e, c.args()))
        return false;

    if (!e.writeExpr(Expr::CallIndirect))
        return false;

    if (!e.writeVarU32(c.args().length()))
        return false;

    if (!e.writeVarU32(c.sig().index()))
        return false;

    return true;
}

static bool
EncodeConst(Encoder& e, AstConst& c)
{
    switch (c.val().type()) {
      case ValType::I32:
        return e.writeExpr(Expr::I32Const) &&
               e.writeVarS32(c.val().i32());
      case ValType::I64:
        return e.writeExpr(Expr::I64Const) &&
               e.writeVarS64(c.val().i64());
      case ValType::F32:
        return e.writeExpr(Expr::F32Const) &&
               e.writeFixedF32(c.val().f32());
      case ValType::F64:
        return e.writeExpr(Expr::F64Const) &&
               e.writeFixedF64(c.val().f64());
      default:
        break;
    }
    MOZ_CRASH("Bad value type");
}

static bool
EncodeGetLocal(Encoder& e, AstGetLocal& gl)
{
    return e.writeExpr(Expr::GetLocal) &&
           e.writeVarU32(gl.local().index());
}

static bool
EncodeSetLocal(Encoder& e, AstSetLocal& sl)
{
    return EncodeExpr(e, sl.value()) &&
           e.writeExpr(Expr::SetLocal) &&
           e.writeVarU32(sl.local().index());
}

static bool
EncodeUnaryOperator(Encoder& e, AstUnaryOperator& b)
{
    return EncodeExpr(e, *b.op()) &&
           e.writeExpr(b.expr());
}

static bool
EncodeBinaryOperator(Encoder& e, AstBinaryOperator& b)
{
    return EncodeExpr(e, *b.lhs()) &&
           EncodeExpr(e, *b.rhs()) &&
           e.writeExpr(b.expr());
}

static bool
EncodeTernaryOperator(Encoder& e, AstTernaryOperator& b)
{
    return EncodeExpr(e, *b.op0()) &&
           EncodeExpr(e, *b.op1()) &&
           EncodeExpr(e, *b.op2()) &&
           e.writeExpr(b.expr());
}

static bool
EncodeComparisonOperator(Encoder& e, AstComparisonOperator& b)
{
    return EncodeExpr(e, *b.lhs()) &&
           EncodeExpr(e, *b.rhs()) &&
           e.writeExpr(b.expr());
}

static bool
EncodeConversionOperator(Encoder& e, AstConversionOperator& b)
{
    return EncodeExpr(e, *b.op()) &&
           e.writeExpr(b.expr());
}

static bool
EncodeIf(Encoder& e, AstIf& i)
{
    if (!EncodeExpr(e, i.cond()) || !e.writeExpr(Expr::If))
        return false;

    if (!EncodeExprList(e, i.thenExprs()))
        return false;

    if (i.hasElse()) {
        if (!e.writeExpr(Expr::Else))
            return false;
        if (!EncodeExprList(e, i.elseExprs()))
            return false;
    }

    return e.writeExpr(Expr::End);
}

static bool
EncodeLoadStoreAddress(Encoder &e, const AstLoadStoreAddress &address)
{
    return EncodeExpr(e, address.base());
}

static bool
EncodeLoadStoreFlags(Encoder &e, const AstLoadStoreAddress &address)
{
    return e.writeVarU32(address.flags()) &&
           e.writeVarU32(address.offset());
}

static bool
EncodeLoad(Encoder& e, AstLoad& l)
{
    return EncodeLoadStoreAddress(e, l.address()) &&
           e.writeExpr(l.expr()) &&
           EncodeLoadStoreFlags(e, l.address());
}

static bool
EncodeStore(Encoder& e, AstStore& s)
{
    return EncodeLoadStoreAddress(e, s.address()) &&
           EncodeExpr(e, s.value()) &&
           e.writeExpr(s.expr()) &&
           EncodeLoadStoreFlags(e, s.address());
}

static bool
EncodeReturn(Encoder& e, AstReturn& r)
{
    uint32_t arity = 0;
    if (r.maybeExpr()) {
        arity = 1;
        if (!EncodeExpr(e, *r.maybeExpr()))
           return false;
    }

    if (!e.writeExpr(Expr::Return))
        return false;

    if (!e.writeVarU32(arity))
        return false;

    return true;
}

static bool
EncodeBranchTable(Encoder& e, AstBranchTable& bt)
{
    uint32_t arity = 0;
    if (bt.maybeValue()) {
        arity = 1;
        if (!EncodeExpr(e, *bt.maybeValue()))
            return false;
    }

    if (!EncodeExpr(e, bt.index()))
        return false;

    if (!e.writeExpr(Expr::BrTable))
        return false;

    if (!e.writeVarU32(arity))
        return false;

    if (!e.writeVarU32(bt.table().length()))
        return false;

    for (const AstRef& elem : bt.table()) {
        if (!e.writeFixedU32(elem.index()))
            return false;
    }

    if (!e.writeFixedU32(bt.def().index()))
        return false;

    return true;
}

static bool
EncodeExpr(Encoder& e, AstExpr& expr)
{
    switch (expr.kind()) {
      case AstExprKind::Nop:
        return e.writeExpr(Expr::Nop);
      case AstExprKind::Unreachable:
        return e.writeExpr(Expr::Unreachable);
      case AstExprKind::BinaryOperator:
        return EncodeBinaryOperator(e, expr.as<AstBinaryOperator>());
      case AstExprKind::Block:
        return EncodeBlock(e, expr.as<AstBlock>());
      case AstExprKind::Branch:
        return EncodeBranch(e, expr.as<AstBranch>());
      case AstExprKind::Call:
        return EncodeCall(e, expr.as<AstCall>());
      case AstExprKind::CallIndirect:
        return EncodeCallIndirect(e, expr.as<AstCallIndirect>());
      case AstExprKind::ComparisonOperator:
        return EncodeComparisonOperator(e, expr.as<AstComparisonOperator>());
      case AstExprKind::Const:
        return EncodeConst(e, expr.as<AstConst>());
      case AstExprKind::ConversionOperator:
        return EncodeConversionOperator(e, expr.as<AstConversionOperator>());
      case AstExprKind::GetLocal:
        return EncodeGetLocal(e, expr.as<AstGetLocal>());
      case AstExprKind::If:
        return EncodeIf(e, expr.as<AstIf>());
      case AstExprKind::Load:
        return EncodeLoad(e, expr.as<AstLoad>());
      case AstExprKind::Return:
        return EncodeReturn(e, expr.as<AstReturn>());
      case AstExprKind::SetLocal:
        return EncodeSetLocal(e, expr.as<AstSetLocal>());
      case AstExprKind::Store:
        return EncodeStore(e, expr.as<AstStore>());
      case AstExprKind::BranchTable:
        return EncodeBranchTable(e, expr.as<AstBranchTable>());
      case AstExprKind::TernaryOperator:
        return EncodeTernaryOperator(e, expr.as<AstTernaryOperator>());
      case AstExprKind::UnaryOperator:
        return EncodeUnaryOperator(e, expr.as<AstUnaryOperator>());
    }
    MOZ_CRASH("Bad expr kind");
}

/*****************************************************************************/
// wasm AST binary serialization

static bool
EncodeTypeSection(Encoder& e, AstModule& module)
{
    if (module.sigs().empty())
        return true;

    size_t offset;
    if (!e.startSection(TypeSectionId, &offset))
        return false;

    if (!e.writeVarU32(module.sigs().length()))
        return false;

    for (AstSig* sig : module.sigs()) {
        if (!e.writeVarU32(uint32_t(TypeConstructor::Function)))
            return false;

        if (!e.writeVarU32(sig->args().length()))
            return false;

        for (ValType t : sig->args()) {
            if (!e.writeValType(t))
                return false;
        }

        if (!e.writeVarU32(!IsVoid(sig->ret())))
            return false;

        if (!IsVoid(sig->ret())) {
            if (!e.writeValType(NonVoidToValType(sig->ret())))
                return false;
        }
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeFunctionSection(Encoder& e, AstModule& module)
{
    if (module.funcs().empty())
        return true;

    size_t offset;
    if (!e.startSection(FunctionSectionId, &offset))
        return false;

    if (!e.writeVarU32(module.funcs().length()))
        return false;

    for (AstFunc* func : module.funcs()) {
        if (!e.writeVarU32(func->sig().index()))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeBytes(Encoder& e, AstName wasmName)
{
    TwoByteChars range(wasmName.begin(), wasmName.length());
    UniqueChars utf8(JS::CharsToNewUTF8CharsZ(nullptr, range).c_str());
    return utf8 && e.writeBytes(utf8.get(), strlen(utf8.get()));
}

static bool
EncodeResizable(Encoder& e, const AstResizable& resizable)
{
    uint32_t flags = uint32_t(ResizableFlags::Default);
    if (resizable.maximum())
        flags |= uint32_t(ResizableFlags::HasMaximum);

    if (!e.writeVarU32(flags))
        return false;

    if (!e.writeVarU32(resizable.initial()))
        return false;

    if (resizable.maximum()) {
        if (!e.writeVarU32(*resizable.maximum()))
            return false;
    }

    return true;
}

static bool
EncodeImport(Encoder& e, bool newFormat, AstImport& imp)
{
    if (!newFormat) {
        if (!e.writeVarU32(imp.funcSig().index()))
            return false;

        if (!EncodeBytes(e, imp.module()))
            return false;

        if (!EncodeBytes(e, imp.field()))
            return false;

        return true;
    }

    if (!EncodeBytes(e, imp.module()))
        return false;

    if (!EncodeBytes(e, imp.field()))
        return false;

    if (!e.writeVarU32(uint32_t(imp.kind())))
        return false;

    switch (imp.kind()) {
      case DefinitionKind::Function:
        if (!e.writeVarU32(imp.funcSig().index()))
            return false;
        break;
      case DefinitionKind::Table:
      case DefinitionKind::Memory:
        if (!EncodeResizable(e, imp.resizable()))
            return false;
        break;
    }

    return true;
}

static bool
EncodeImportSection(Encoder& e, bool newFormat, AstModule& module)
{
    if (module.imports().empty())
        return true;

    size_t offset;
    if (!e.startSection(ImportSectionId, &offset))
        return false;

    if (!e.writeVarU32(module.imports().length()))
        return false;

    for (AstImport* imp : module.imports()) {
        if (!EncodeImport(e, newFormat, *imp))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeMemorySection(Encoder& e, bool newFormat, AstModule& module)
{
    if (!module.hasMemory())
        return true;

    size_t offset;
    if (!e.startSection(MemorySectionId, &offset))
        return false;

    const AstResizable& memory = module.memory();

    if (newFormat) {
        if (!EncodeResizable(e, memory))
            return false;
    } else {
        if (!e.writeVarU32(memory.initial()))
            return false;

        uint32_t maxSize = memory.maximum() ? *memory.maximum() : memory.initial();
        if (!e.writeVarU32(maxSize))
            return false;

        uint8_t exported = 0;
        for (AstExport* exp : module.exports()) {
            if (exp->kind() == DefinitionKind::Memory) {
                exported = 1;
                break;
            }
        }

        if (!e.writeFixedU8(exported))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeExport(Encoder& e, bool newFormat, AstExport& exp)
{
    if (!newFormat) {
        if (exp.kind() != DefinitionKind::Function)
            return true;

        if (!e.writeVarU32(exp.func().index()))
            return false;

        if (!EncodeBytes(e, exp.name()))
            return false;

        return true;
    }

    if (!EncodeBytes(e, exp.name()))
        return false;

    if (!e.writeVarU32(uint32_t(exp.kind())))
        return false;

    switch (exp.kind()) {
      case DefinitionKind::Function:
        if (!e.writeVarU32(exp.func().index()))
            return false;
        break;
      case DefinitionKind::Table:
      case DefinitionKind::Memory:
        if (!e.writeVarU32(0))
            return false;
        break;
    }

    return true;
}

static bool
EncodeExportSection(Encoder& e, bool newFormat, AstModule& module)
{
    uint32_t numExports = 0;
    if (newFormat) {
        numExports = module.exports().length();
    } else {
        for (AstExport* exp : module.exports()) {
            if (exp->kind() == DefinitionKind::Function)
                numExports++;
        }
    }

    if (!numExports)
        return true;

    size_t offset;
    if (!e.startSection(ExportSectionId, &offset))
        return false;

    if (!e.writeVarU32(numExports))
        return false;

    for (AstExport* exp : module.exports()) {
        if (!EncodeExport(e, newFormat, *exp))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeTableSection(Encoder& e, bool newFormat, AstModule& module)
{
    if (!module.hasTable())
        return true;

    size_t offset;
    if (!e.startSection(TableSectionId, &offset))
        return false;

    const AstResizable& table = module.table();

    if (newFormat) {
        if (!EncodeResizable(e, table))
            return false;
    } else {
        if (module.elemSegments().length() != 1)
            return false;

        const AstElemSegment& seg = *module.elemSegments()[0];

        if (!e.writeVarU32(seg.elems().length()))
            return false;

        for (const AstRef& ref : seg.elems()) {
            if (!e.writeVarU32(ref.index()))
                return false;
        }
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeFunctionBody(Encoder& e, AstFunc& func)
{
    size_t bodySizeAt;
    if (!e.writePatchableVarU32(&bodySizeAt))
        return false;

    size_t beforeBody = e.currentOffset();

    ValTypeVector varTypes;
    if (!varTypes.appendAll(func.vars()))
        return false;
    if (!EncodeLocalEntries(e, varTypes))
        return false;

    for (AstExpr* expr : func.body()) {
        if (!EncodeExpr(e, *expr))
            return false;
    }

    e.patchVarU32(bodySizeAt, e.currentOffset() - beforeBody);
    return true;
}

static bool
EncodeStartSection(Encoder& e, AstModule& module)
{
    if (!module.hasStartFunc())
        return true;

    size_t offset;
    if (!e.startSection(StartSectionId, &offset))
        return false;

    if (!e.writeVarU32(module.startFunc().func().index()))
        return false;

    e.finishSection(offset);
    return true;
}

static bool
EncodeCodeSection(Encoder& e, AstModule& module)
{
    if (module.funcs().empty())
        return true;

    size_t offset;
    if (!e.startSection(CodeSectionId, &offset))
        return false;

    if (!e.writeVarU32(module.funcs().length()))
        return false;

    for (AstFunc* func : module.funcs()) {
        if (!EncodeFunctionBody(e, *func))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeDataSegment(Encoder& e, bool newFormat, AstDataSegment& segment)
{
    if (newFormat) {
        if (!e.writeVarU32(0))  // linear memory index
            return false;

        if (!e.writeExpr(Expr::I32Const))
            return false;
    }

    if (!e.writeVarU32(segment.offset()))
        return false;

    AstName text = segment.text();

    Vector<uint8_t, 0, SystemAllocPolicy> bytes;
    if (!bytes.reserve(text.length()))
        return false;

    const char16_t* cur = text.begin();
    const char16_t* end = text.end();
    while (cur != end) {
        uint8_t byte;
        MOZ_ALWAYS_TRUE(ConsumeTextByte(&cur, end, &byte));
        bytes.infallibleAppend(byte);
    }

    if (!e.writeBytes(bytes.begin(), bytes.length()))
        return false;

    return true;
}

static bool
EncodeDataSection(Encoder& e, bool newFormat, AstModule& module)
{
    if (module.dataSegments().empty())
        return true;

    size_t offset;
    if (!e.startSection(DataSectionId, &offset))
        return false;

    if (!e.writeVarU32(module.dataSegments().length()))
        return false;

    for (AstDataSegment* segment : module.dataSegments()) {
        if (!EncodeDataSegment(e, newFormat, *segment))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeElemSegment(Encoder& e, AstElemSegment& segment)
{
    if (!e.writeVarU32(0)) // table index
        return false;

    if (!e.writeExpr(Expr::I32Const))
        return false;
    if (!e.writeVarU32(segment.offset()))
        return false;

    if (!e.writeVarU32(segment.elems().length()))
        return false;

    for (const AstRef& elem : segment.elems()) {
        if (!e.writeVarU32(elem.index()))
            return false;
    }

    return true;
}

static bool
EncodeElemSection(Encoder& e, bool newFormat, AstModule& module)
{
    if (!newFormat || module.elemSegments().empty())
        return true;

    size_t offset;
    if (!e.startSection(ElemSectionId, &offset))
        return false;

    if (!e.writeVarU32(module.elemSegments().length()))
        return false;

    for (AstElemSegment* segment : module.elemSegments()) {
        if (!EncodeElemSegment(e, *segment))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeModule(AstModule& module, bool newFormat, Bytes* bytes)
{
    Encoder e(*bytes);

    if (!e.writeFixedU32(MagicNumber))
        return false;

    if (!e.writeFixedU32(EncodingVersion))
        return false;

    if (!EncodeTypeSection(e, module))
        return false;

    if (!EncodeImportSection(e, newFormat, module))
        return false;

    if (!EncodeFunctionSection(e, module))
        return false;

    if (!EncodeTableSection(e, newFormat, module))
        return false;

    if (!EncodeMemorySection(e, newFormat, module))
        return false;

    if (!EncodeExportSection(e, newFormat, module))
        return false;

    if (!EncodeStartSection(e, module))
        return false;

    if (!EncodeCodeSection(e, module))
        return false;

    if (!EncodeDataSection(e, newFormat, module))
        return false;

    if (!EncodeElemSection(e, newFormat, module))
        return false;

    return true;
}

/*****************************************************************************/

bool
wasm::TextToBinary(const char16_t* text, bool newFormat, Bytes* bytes, UniqueChars* error)
{
    LifoAlloc lifo(AST_LIFO_DEFAULT_CHUNK_SIZE);
    AstModule* module = ParseModule(text, newFormat, lifo, error);
    if (!module)
        return false;

    if (!ResolveModule(lifo, module, error))
        return false;

    return EncodeModule(*module, newFormat, bytes);
}
