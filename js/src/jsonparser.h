/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey JSON.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    JSONParser(const JSONParser &other) MOZ_DELETE;
    void operator=(const JSONParser &other) MOZ_DELETE;

  public:
    enum ErrorHandling { RaiseError, NoError };
    enum ParsingMode { StrictJSON, LegacyJSON };

  private:
    /* Data members */

    JSContext * const cx;
    mozilla::RangedPtr<const jschar> current;
    const mozilla::RangedPtr<const jschar> end;

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

    Token numberToken(jsdouble d) {
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
};

#endif /* jsonparser_h___ */
