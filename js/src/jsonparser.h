/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsonparser_h___
#define jsonparser_h___

#include "mozilla/Attributes.h"
#include "mozilla/RangedPtr.h"

#include "jscntxt.h"
#include "jsstr.h"

/*
 * NB: This class must only be used on the stack as it contains a js::Value.
 */
class JSONParser
{
  public:
    enum ErrorHandling { RaiseError, NoError };
    enum ParsingMode { StrictJSON, LegacyJSON };

  private:
    /* Data members */

    JSContext * const cx;
    mozilla::RangedPtr<const jschar> current;
    const mozilla::RangedPtr<const jschar> end;

    /* For current/end as cursors into a string. */
    js::SkipRoot root;

    js::Value v;

    const ParsingMode parsingMode;
    const ErrorHandling errorHandling;

    enum Token { String, Number, True, False, Null,
                 ArrayOpen, ArrayClose,
                 ObjectOpen, ObjectClose,
                 Colon, Comma,
                 OOM, Error };
#ifdef DEBUG
    Token lastToken;
#endif

    JSONParser *thisDuringConstruction() { return this; }

  public:
    /* Public API */

    /*
     * Create a parser for the provided JSON data.  The parser will accept
     * certain legacy, non-JSON syntax if decodingMode is LegacyJSON.
     * Description of this syntax is deliberately omitted: new code should only
     * use strict JSON parsing.
     */
    JSONParser(JSContext *cx, const jschar *data, size_t length,
               ParsingMode parsingMode = StrictJSON,
               ErrorHandling errorHandling = RaiseError)
      : cx(cx),
        current(data, length),
        end(data + length, data, length),
        root(cx, thisDuringConstruction()),
        parsingMode(parsingMode),
        errorHandling(errorHandling)
#ifdef DEBUG
      , lastToken(Error)
#endif
    {
        JS_ASSERT(current <= end);
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
    bool parse(js::Value *vp);

  private:
    js::Value numberValue() const {
        JS_ASSERT(lastToken == Number);
        JS_ASSERT(v.isNumber());
        return v;
    }

    js::Value stringValue() const {
        JS_ASSERT(lastToken == String);
        JS_ASSERT(v.isString());
        return v;
    }

    js::Value atomValue() const {
        js::Value strval = stringValue();
        JS_ASSERT(strval.toString()->isAtom());
        return strval;
    }

    Token token(Token t) {
        JS_ASSERT(t != String);
        JS_ASSERT(t != Number);
#ifdef DEBUG
        lastToken = t;
#endif
        return t;
    }

    Token stringToken(JSString *str) {
        this->v = js::StringValue(str);
#ifdef DEBUG
        lastToken = String;
#endif
        return String;
    }

    Token numberToken(double d) {
        this->v = js::NumberValue(d);
#ifdef DEBUG
        lastToken = Number;
#endif
        return Number;
    }

    enum StringType { PropertyName, LiteralValue };
    template<StringType ST> Token readString();

    Token readNumber();

    Token advance();
    Token advancePropertyName();
    Token advancePropertyColon();
    Token advanceAfterProperty();
    Token advanceAfterObjectOpen();
    Token advanceAfterArrayElement();

    void error(const char *msg);
    bool errorReturn();

  private:
    JSONParser(const JSONParser &other) MOZ_DELETE;
    void operator=(const JSONParser &other) MOZ_DELETE;
};

#endif /* jsonparser_h___ */
