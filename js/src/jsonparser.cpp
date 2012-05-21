/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsarray.h"
#include "jsnum.h"
#include "jsonparser.h"

#include "vm/StringBuffer.h"

#include "jsobjinlines.h"

using namespace js;

void
JSONParser::error(const char *msg)
{
    if (errorHandling == RaiseError)
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE, msg);
}

bool
JSONParser::errorReturn()
{
    return errorHandling == NoError;
}

template<JSONParser::StringType ST>
JSONParser::Token
JSONParser::readString()
{
    JS_ASSERT(current < end);
    JS_ASSERT(*current == '"');

    /*
     * JSONString:
     *   /^"([^\u0000-\u001F"\\]|\\(["/\\bfnrt]|u[0-9a-fA-F]{4}))*"$/
     */

    if (++current == end) {
        error("unterminated string literal");
        return token(Error);
    }

    /*
     * Optimization: if the source contains no escaped characters, create the
     * string directly from the source text.
     */
    RangedPtr<const jschar> start = current;
    for (; current < end; current++) {
        if (*current == '"') {
            size_t length = current - start;
            current++;
            JSFlatString *str = (ST == JSONParser::PropertyName)
                                ? js_AtomizeChars(cx, start.get(), length)
                                : js_NewStringCopyN(cx, start.get(), length);
            if (!str)
                return token(OOM);
            return stringToken(str);
        }

        if (*current == '\\')
            break;

        if (*current <= 0x001F) {
            error("bad control character in string literal");
            return token(Error);
        }
    }

    /*
     * Slow case: string contains escaped characters.  Copy a maximal sequence
     * of unescaped characters into a temporary buffer, then an escaped
     * character, and repeat until the entire string is consumed.
     */
    StringBuffer buffer(cx);
    do {
        if (start < current && !buffer.append(start.get(), current.get()))
            return token(OOM);

        if (current >= end)
            break;

        jschar c = *current++;
        if (c == '"') {
            JSFlatString *str = (ST == JSONParser::PropertyName)
                                ? buffer.finishAtom()
                                : buffer.finishString();
            if (!str)
                return token(OOM);
            return stringToken(str);
        }

        if (c != '\\') {
            error("bad character in string literal");
            return token(Error);
        }

        if (current >= end)
            break;

        switch (*current++) {
          case '"':  c = '"';  break;
          case '/':  c = '/';  break;
          case '\\': c = '\\'; break;
          case 'b':  c = '\b'; break;
          case 'f':  c = '\f'; break;
          case 'n':  c = '\n'; break;
          case 'r':  c = '\r'; break;
          case 't':  c = '\t'; break;

          case 'u':
            if (end - current < 4) {
                error("bad Unicode escape");
                return token(Error);
            }
            if (JS7_ISHEX(current[0]) &&
                JS7_ISHEX(current[1]) &&
                JS7_ISHEX(current[2]) &&
                JS7_ISHEX(current[3]))
            {
                c = (JS7_UNHEX(current[0]) << 12)
                  | (JS7_UNHEX(current[1]) << 8)
                  | (JS7_UNHEX(current[2]) << 4)
                  | (JS7_UNHEX(current[3]));
                current += 4;
                break;
            }
            /* FALL THROUGH */

          default:
            error("bad escaped character");
            return token(Error);
        }
        if (!buffer.append(c))
            return token(OOM);

        start = current;
        for (; current < end; current++) {
            if (*current == '"' || *current == '\\' || *current <= 0x001F)
                break;
        }
    } while (current < end);

    error("unterminated string");
    return token(Error);
}

JSONParser::Token
JSONParser::readNumber()
{
    JS_ASSERT(current < end);
    JS_ASSERT(JS7_ISDEC(*current) || *current == '-');

    /*
     * JSONNumber:
     *   /^-?(0|[1-9][0-9]+)(\.[0-9]+)?([eE][\+\-]?[0-9]+)?$/
     */

    bool negative = *current == '-';

    /* -? */
    if (negative && ++current == end) {
        error("no number after minus sign");
        return token(Error);
    }

    const RangedPtr<const jschar> digitStart = current;

    /* 0|[1-9][0-9]+ */
    if (!JS7_ISDEC(*current)) {
        error("unexpected non-digit");
        return token(Error);
    }
    if (*current++ != '0') {
        for (; current < end; current++) {
            if (!JS7_ISDEC(*current))
                break;
        }
    }

    /* Fast path: no fractional or exponent part. */
    if (current == end || (*current != '.' && *current != 'e' && *current != 'E')) {
        const jschar *dummy;
        double d;
        if (!GetPrefixInteger(cx, digitStart.get(), current.get(), 10, &dummy, &d))
            return token(OOM);
        JS_ASSERT(current == dummy);
        return numberToken(negative ? -d : d);
    }

    /* (\.[0-9]+)? */
    if (current < end && *current == '.') {
        if (++current == end) {
            error("missing digits after decimal point");
            return token(Error);
        }
        if (!JS7_ISDEC(*current)) {
            error("unterminated fractional number");
            return token(Error);
        }
        while (++current < end) {
            if (!JS7_ISDEC(*current))
                break;
        }
    }

    /* ([eE][\+\-]?[0-9]+)? */
    if (current < end && (*current == 'e' || *current == 'E')) {
        if (++current == end) {
            error("missing digits after exponent indicator");
            return token(Error);
        }
        if (*current == '+' || *current == '-') {
            if (++current == end) {
                error("missing digits after exponent sign");
                return token(Error);
            }
        }
        if (!JS7_ISDEC(*current)) {
            error("exponent part is missing a number");
            return token(Error);
        }
        while (++current < end) {
            if (!JS7_ISDEC(*current))
                break;
        }
    }

    double d;
    const jschar *finish;
    if (!js_strtod(cx, digitStart.get(), current.get(), &finish, &d))
        return token(OOM);
    JS_ASSERT(current == finish);
    return numberToken(negative ? -d : d);
}

static inline bool
IsJSONWhitespace(jschar c)
{
    return c == '\t' || c == '\r' || c == '\n' || c == ' ';
}

JSONParser::Token
JSONParser::advance()
{
    while (current < end && IsJSONWhitespace(*current))
        current++;
    if (current >= end) {
        error("unexpected end of data");
        return token(Error);
    }

    switch (*current) {
      case '"':
        return readString<LiteralValue>();

      case '-':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        return readNumber();

      case 't':
        if (end - current < 4 || current[1] != 'r' || current[2] != 'u' || current[3] != 'e') {
            error("unexpected keyword");
            return token(Error);
        }
        current += 4;
        return token(True);

      case 'f':
        if (end - current < 5 ||
            current[1] != 'a' || current[2] != 'l' || current[3] != 's' || current[4] != 'e')
        {
            error("unexpected keyword");
            return token(Error);
        }
        current += 5;
        return token(False);

      case 'n':
        if (end - current < 4 || current[1] != 'u' || current[2] != 'l' || current[3] != 'l') {
            error("unexpected keyword");
            return token(Error);
        }
        current += 4;
        return token(Null);

      case '[':
        current++;
        return token(ArrayOpen);
      case ']':
        current++;
        return token(ArrayClose);

      case '{':
        current++;
        return token(ObjectOpen);
      case '}':
        current++;
        return token(ObjectClose);

      case ',':
        current++;
        return token(Comma);

      case ':':
        current++;
        return token(Colon);

      default:
        error("unexpected character");
        return token(Error);
    }
}

JSONParser::Token
JSONParser::advanceAfterObjectOpen()
{
    JS_ASSERT(current[-1] == '{');

    while (current < end && IsJSONWhitespace(*current))
        current++;
    if (current >= end) {
        error("end of data while reading object contents");
        return token(Error);
    }

    if (*current == '"')
        return readString<PropertyName>();

    if (*current == '}') {
        current++;
        return token(ObjectClose);
    }

    error("expected property name or '}'");
    return token(Error);
}

static inline void
AssertPastValue(const RangedPtr<const jschar> current)
{
    /*
     * We're past an arbitrary JSON value, so the previous character is
     * *somewhat* constrained, even if this assertion is pretty broad.  Don't
     * knock it till you tried it: this assertion *did* catch a bug once.
     */
    JS_ASSERT((current[-1] == 'l' &&
               current[-2] == 'l' &&
               current[-3] == 'u' &&
               current[-4] == 'n') ||
              (current[-1] == 'e' &&
               current[-2] == 'u' &&
               current[-3] == 'r' &&
               current[-4] == 't') ||
              (current[-1] == 'e' &&
               current[-2] == 's' &&
               current[-3] == 'l' &&
               current[-4] == 'a' &&
               current[-5] == 'f') ||
              current[-1] == '}' ||
              current[-1] == ']' ||
              current[-1] == '"' ||
              JS7_ISDEC(current[-1]));
}

JSONParser::Token
JSONParser::advanceAfterArrayElement()
{
    AssertPastValue(current);

    while (current < end && IsJSONWhitespace(*current))
        current++;
    if (current >= end) {
        error("end of data when ',' or ']' was expected");
        return token(Error);
    }

    if (*current == ',') {
        current++;
        return token(Comma);
    }

    if (*current == ']') {
        current++;
        return token(ArrayClose);
    }

    error("expected ',' or ']' after array element");
    return token(Error);
}

JSONParser::Token
JSONParser::advancePropertyName()
{
    JS_ASSERT(current[-1] == ',');

    while (current < end && IsJSONWhitespace(*current))
        current++;
    if (current >= end) {
        error("end of data when property name was expected");
        return token(Error);
    }

    if (*current == '"')
        return readString<PropertyName>();

    if (parsingMode == LegacyJSON && *current == '}') {
        /*
         * Previous JSON parsing accepted trailing commas in non-empty object
         * syntax, and some users depend on this.  (Specifically, Places data
         * serialization in versions of Firefox before 4.0.  We can remove this
         * mode when profile upgrades from 3.6 become unsupported.)  Permit
         * such trailing commas only when legacy parsing is specifically
         * requested.
         */
        current++;
        return token(ObjectClose);
    }

    error("expected double-quoted property name");
    return token(Error);
}

JSONParser::Token
JSONParser::advancePropertyColon()
{
    JS_ASSERT(current[-1] == '"');

    while (current < end && IsJSONWhitespace(*current))
        current++;
    if (current >= end) {
        error("end of data after property name when ':' was expected");
        return token(Error);
    }

    if (*current == ':') {
        current++;
        return token(Colon);
    }

    error("expected ':' after property name in object");
    return token(Error);
}

JSONParser::Token
JSONParser::advanceAfterProperty()
{
    AssertPastValue(current);

    while (current < end && IsJSONWhitespace(*current))
        current++;
    if (current >= end) {
        error("end of data after property value in object");
        return token(Error);
    }

    if (*current == ',') {
        current++;
        return token(Comma);
    }

    if (*current == '}') {
        current++;
        return token(ObjectClose);
    }

    error("expected ',' or '}' after property value in object");
    return token(Error);
}

/*
 * This enum is local to JSONParser::parse, below, but ISO C++98 doesn't allow
 * templates to depend on local types.  Boo-urns!
 */
enum ParserState { FinishArrayElement, FinishObjectMember, JSONValue };

bool
JSONParser::parse(Value *vp)
{
    Vector<ParserState> stateStack(cx);
    AutoValueVector valueStack(cx);

    *vp = UndefinedValue();

    Token token;
    ParserState state = JSONValue;
    while (true) {
        switch (state) {
          case FinishObjectMember: {
            Value v = valueStack.popCopy();
            RootedVarId propid(cx, AtomToId(&valueStack.popCopy().toString()->asAtom()));
            RootedVarObject obj(cx, &valueStack.back().toObject());
            if (!DefineNativeProperty(cx, obj, propid, v,
                                      JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE,
                                      0, 0))
            {
                return false;
            }
            token = advanceAfterProperty();
            if (token == ObjectClose)
                break;
            if (token != Comma) {
                if (token == OOM)
                    return false;
                if (token != Error)
                    error("expected ',' or '}' after property-value pair in object literal");
                return errorReturn();
            }
            token = advancePropertyName();
            /* FALL THROUGH */
          }

          JSONMember:
            if (token == String) {
                if (!valueStack.append(atomValue()))
                    return false;
                token = advancePropertyColon();
                if (token != Colon) {
                    JS_ASSERT(token == Error);
                    return errorReturn();
                }
                if (!stateStack.append(FinishObjectMember))
                    return false;
                goto JSONValue;
            }
            if (token == ObjectClose) {
                JS_ASSERT(state == FinishObjectMember);
                JS_ASSERT(parsingMode == LegacyJSON);
                break;
            }
            if (token == OOM)
                return false;
            if (token != Error)
                error("property names must be double-quoted strings");
            return errorReturn();

          case FinishArrayElement: {
            Value v = valueStack.popCopy();
            if (!js_NewbornArrayPush(cx, RootedVarObject(cx, &valueStack.back().toObject()), v))
                return false;
            token = advanceAfterArrayElement();
            if (token == Comma) {
                if (!stateStack.append(FinishArrayElement))
                    return false;
                goto JSONValue;
            }
            if (token == ArrayClose)
                break;
            JS_ASSERT(token == Error);
            return errorReturn();
          }

          JSONValue:
          case JSONValue:
            token = advance();
          JSONValueSwitch:
            switch (token) {
              case String:
              case Number:
                if (!valueStack.append(token == String ? stringValue() : numberValue()))
                    return false;
                break;
              case True:
                if (!valueStack.append(BooleanValue(true)))
                    return false;
                break;
              case False:
                if (!valueStack.append(BooleanValue(false)))
                    return false;
                break;
              case Null:
                if (!valueStack.append(NullValue()))
                    return false;
                break;

              case ArrayOpen: {
                JSObject *obj = NewDenseEmptyArray(cx);
                if (!obj || !valueStack.append(ObjectValue(*obj)))
                    return false;
                token = advance();
                if (token == ArrayClose)
                    break;
                if (!stateStack.append(FinishArrayElement))
                    return false;
                goto JSONValueSwitch;
              }

              case ObjectOpen: {
                JSObject *obj = NewBuiltinClassInstance(cx, &ObjectClass);
                if (!obj || !valueStack.append(ObjectValue(*obj)))
                    return false;
                token = advanceAfterObjectOpen();
                if (token == ObjectClose)
                    break;
                goto JSONMember;
              }

              case ArrayClose:
                if (parsingMode == LegacyJSON &&
                    !stateStack.empty() &&
                    stateStack.back() == FinishArrayElement) {
                    /*
                     * Previous JSON parsing accepted trailing commas in
                     * non-empty array syntax, and some users depend on this.
                     * (Specifically, Places data serialization in versions of
                     * Firefox prior to 4.0.  We can remove this mode when
                     * profile upgrades from 3.6 become unsupported.)  Permit
                     * such trailing commas only when specifically
                     * instructed to do so.
                     */
                    stateStack.popBack();
                    break;
                }
                /* FALL THROUGH */

              case ObjectClose:
              case Colon:
              case Comma:
                error("unexpected character");
                return errorReturn();

              case OOM:
                return false;

              case Error:
                return errorReturn();
            }
            break;
        }

        if (stateStack.empty())
            break;
        state = stateStack.popCopy();
    }

    for (; current < end; current++) {
        if (!IsJSONWhitespace(*current)) {
            error("unexpected non-whitespace character after JSON data");
            return errorReturn();
        }
    }

    JS_ASSERT(end == current);
    JS_ASSERT(valueStack.length() == 1);
    *vp = valueStack[0];
    return true;
}
