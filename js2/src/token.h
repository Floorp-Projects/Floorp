/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef token_h___
#define token_h___

#include "systemtypes.h"
#include "utilities.h"
#include "strings.h"
#include "formatter.h"

namespace JavaScript
{
    class StringAtom;
    class World;

    class Token {
      public:
        enum Kind {
            // Keep synchronized with the kindNames, kindFlags, and tokenBinaryOperatorInfos tables

            // Special
            end,                        // End of token stream
            number,                     // Number
            string,                     // String
            unit,                       // Unit after number
            regExp,                     // Regular expression

            // Punctuators
            openParenthesis,            // (
            closeParenthesis,           // )
            openBracket,                // [
            closeBracket,               // ]
            openBrace,                  // {
            closeBrace,                 // }
            comma,                      // ,
            semicolon,                  // ;
            dot,                        // .
            doubleDot,                  // ..
            tripleDot,                  // ...
            arrow,                      // ->
            colon,                      // :
            doubleColon,                // ::
            pound,                      // #
            at,                         // @

            increment,                  // ++
            decrement,                  // --

            complement,                 // ~
            logicalNot,                 // !

            times,                      // *
            divide,                     // /
            modulo,                     // %
            plus,                       // +
            minus,                      // -
            leftShift,                  // <<
            rightShift,                 // >>
            logicalRightShift,          // >>>
            logicalAnd,                 // &&
            logicalXor,                 // ^^
            logicalOr,                  // ||
            bitwiseAnd,                 // &    These must be at constant offsets from logicalAnd ... logicalOr
            bitwiseXor,                 // ^
            bitwiseOr,                  // |

            assignment,                 // =
            timesEquals,                // *=   These must be at constant offsets from times ... bitwiseOr
            divideEquals,               // /=
            moduloEquals,               // %=
            plusEquals,                 // +=
            minusEquals,                // -=
            leftShiftEquals,            // <<=
            rightShiftEquals,           // >>=
            logicalRightShiftEquals,    // >>>=
            logicalAndEquals,           // &&=
            logicalXorEquals,           // ^^=
            logicalOrEquals,            // ||=
            bitwiseAndEquals,           // &=
            bitwiseXorEquals,           // ^=
            bitwiseOrEquals,            // |=

            equal,                      // ==
            notEqual,                   // !=
            lessThan,                   // <
            lessThanOrEqual,            // <=
            greaterThan,                // >    >, >= must be at constant offsets from <, <=
            greaterThanOrEqual,         // >=
            identical,                  // ===
            notIdentical,               // !==

            question,                   // ?

            // Reserved words
            Abstract,                   // abstract
            As,                         // as
            Break,                      // break
            Case,                       // case
            Catch,                      // catch
            Class,                      // class
            Const,                      // const
            Continue,                   // continue
            Debugger,                   // debugger
            Default,                    // default
            Delete,                     // delete
            Do,                         // do
            Else,                       // else
            Enum,                       // enum
            Export,                     // export
            Extends,                    // extends
            False,                      // false
            Final,                      // final
            Finally,                    // finally
            For,                        // for
            Function,                   // function
            Goto,                       // goto
            If,                         // if
            Implements,                 // implements
            Import,                     // import
            In,                         // in
            Instanceof,                 // instanceof
            Interface,                  // interface
            Is,                         // is
            Namespace,                  // namespace
            Native,                     // native
            New,                        // new
            Null,                       // null
            Package,                    // package
            Private,                    // private
            Protected,                  // protected
            Public,                     // public
            Return,                     // return
            Static,                     // static
            Super,                      // super
            Switch,                     // switch
            Synchronized,               // synchronized
            This,                       // this
            Throw,                      // throw
            Throws,                     // throws
            Transient,                  // transient
            True,                       // true
            Try,                        // try
            Typeof,                     // typeof
            Use,                        // use
            Var,                        // var
            Void,                       // void
            Volatile,                   // volatile
            While,                      // while
            With,                       // with

            // Non-reserved words
            Ecmascript,                 // ecmascript
            Eval,                       // eval
            Exclude,                    // exclude
            Get,                        // get
            Include,                    // include
            Javascript,                 // javascript
            Named,                      // named
            Set,                        // set
            Strict,                     // strict

            identifier,                 // Non-keyword identifier (may be same as a keyword if it contains an escape code)
            kindsEnd,                     // End of token kinds

            keywordsBegin = Abstract,     // Beginning of range of special identifier tokens
            keywordsEnd = identifier,     // End of range of special identifier tokens
            nonreservedBegin = Ecmascript,// Beginning of range of non-reserved words
            nonreservedEnd = identifier,  // End of range of non-reserved words
            kindsWithCharsBegin = number, // Beginning of range of tokens for which the chars field (below) is valid
            kindsWithCharsEnd = regExp+1  // End of range of tokens for which the chars field (below) is valid
        };

// Keep synchronized with isNonreserved below
#define CASE_TOKEN_NONRESERVED     \
             Token::Ecmascript:    \
        case Token::Eval:          \
        case Token::Exclude:       \
        case Token::Get:           \
        case Token::Include:       \
        case Token::Javascript:    \
        case Token::Named:         \
        case Token::Set:           \
        case Token::Strict:        \
        case Token::identifier

#define CASE_TOKEN_NONRESERVED_NONINCLUDE \
             Token::Ecmascript:    \
        case Token::Eval:          \
        case Token::Exclude:       \
        case Token::Get:           \
        case Token::Javascript:    \
        case Token::Named:         \
        case Token::Set:           \
        case Token::Strict:        \
        case Token::identifier

// Keep synchronized with isNonExpressionAttribute below
#define CASE_TOKEN_NONEXPRESSION_ATTRIBUTE \
             Token::Abstract:      \
        case Token::Final:         \
        case Token::Static:        \
        case Token::Volatile

        enum Flag {
            isNonreserved,              // True if this token is a non-reserved identifier
            isAttribute,                // True if this token is an attribute
            isNonExpressionAttribute,   // True if this token is an attribute but not an expression
            canFollowAttribute,         // True if this token is an attribute or can follow an attribute
            canFollowReturn             // True if this token can follow a return without an expression
        };

      private:
        static const char *const kindNames[kindsEnd];
        static const uchar kindFlags[kindsEnd];

#ifdef DEBUG
        bool valid;             // True if this token has been initialized
#endif
        Kind kind;              // The token's kind
        bool lineBreak;         // True if line break precedes this token
        size_t pos;             // Source position of this token
        const StringAtom *id;   // The token's characters; non-nil for identifiers, keywords, and regular expressions only
        String chars;           // The token's characters; valid for strings, units, numbers, and regular expression flags only
        float64 value;          // The token's value (numbers only)

#ifdef DEBUG
        Token(): valid(false) {}
#endif

      public:
        static void initKeywords(World &world);
        static bool isSpecialKind(Kind kind) {return kind <= regExp || kind == identifier;}
        static const char *kindName(Kind kind) {ASSERT(uint(kind) < kindsEnd); return kindNames[kind];}
        Kind getKind() const {ASSERT(valid); return kind;}
        bool hasKind(Kind k) const {ASSERT(valid); return kind == k;}
        bool hasIdentifierKind() const {ASSERT(nonreservedEnd == identifier && kindsEnd == identifier+1); return kind >= nonreservedBegin;}
        bool getFlag(Flag f) const {ASSERT(valid); return (kindFlags[kind] & 1<<f) != 0;}
        bool getLineBreak() const {ASSERT(valid); return lineBreak;}
        size_t getPos() const {ASSERT(valid); return pos;}
        const StringAtom &getIdentifier() const {ASSERT(valid && id); return *id;}
        const String &getChars() const {ASSERT(valid && kind >= kindsWithCharsBegin && kind < kindsWithCharsEnd); return chars;}
        float64 getValue() const {ASSERT(valid && kind == number); return value;}
        friend Formatter &operator<<(Formatter &f, Kind k) {f << kindName(k); return f;}
        void print(Formatter &f, bool debug = false) const;

        friend class Lexer;
    };

}

#endif /* token_h___ */
