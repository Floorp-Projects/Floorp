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

#include "token.h"
#include "formatter.h"
#include "world.h"

namespace JS = JavaScript;


const char *const JS::Token::kindNames[kindsEnd] = {

    // Special
    "end of input",                 // end
    "number",                       // number
    "string",                       // string
    "unit",                         // unit
    "regular expression",           // regExp

    // Punctuators
    "(",                            // openParenthesis
    ")",                            // closeParenthesis
    "[",                            // openBracket
    "]",                            // closeBracket
    "{",                            // openBrace
    "}",                            // closeBrace
    ",",                            // comma
    ";",                            // semicolon
    ".",                            // dot
    "..",                           // doubleDot
    "...",                          // tripleDot
    "->",                           // arrow
    ":",                            // colon
    "::",                           // doubleColon
    "#",                            // pound
    "@",                            // at
    "++",                           // increment
    "--",                           // decrement
    "~",                            // complement
    "!",                            // logicalNot
    "*",                            // times
    "/",                            // divide
    "%",                            // modulo
    "+",                            // plus
    "-",                            // minus
    "<<",                           // leftShift
    ">>",                           // rightShift
    ">>>",                          // logicalRightShift
    "&&",                           // logicalAnd
    "^^",                           // logicalXor
    "||",                           // logicalOr
    "&",                            // bitwiseAnd
    "^",                            // bitwiseXor
    "|",                            // bitwiseOr
    "=",                            // assignment
    "*=",                           // timesEquals
    "/=",                           // divideEquals
    "%=",                           // moduloEquals
    "+=",                           // plusEquals
    "-=",                           // minusEquals
    "<<=",                          // leftShiftEquals
    ">>=",                          // rightShiftEquals
    ">>>=",                         // logicalRightShiftEquals
    "&&=",                          // logicalAndEquals
    "^^=",                          // logicalXorEquals
    "||=",                          // logicalOrEquals
    "&=",                           // bitwiseAndEquals
    "^=",                           // bitwiseXorEquals
    "|=",                           // bitwiseOrEquals
    "==",                           // equal
    "!=",                           // notEqual
    "<",                            // lessThan
    "<=",                           // lessThanOrEqual
    ">",                            // greaterThan
    ">=",                           // greaterThanOrEqual
    "===",                          // identical
    "!==",                          // notIdentical
    "?",                            // question

    // Reserved words
    "abstract",                     // Abstract
    "as",                           // As
    "break",                        // Break
    "case",                         // Case
    "catch",                        // Catch
    "class",                        // Class
    "const",                        // Const
    "continue",                     // Continue
    "debugger",                     // Debugger
    "default",                      // Default
    "delete",                       // Delete
    "do",                           // Do
    "else",                         // Else
    "enum",                         // Enum
    "export",                       // Export
    "extends",                      // Extends
    "false",                        // False
    "final",                        // Final
    "finally",                      // Finally
    "for",                          // For
    "function",                     // Function
    "goto",                         // Goto
    "if",                           // If
    "implements",                   // Implements
    "import",                       // Import
    "in",                           // In
    "instanceof",                   // Instanceof
    "interface",                    // Interface
    "is",                           // Is
    "namespace",                    // Namespace
    "native",                       // Native
    "new",                          // New
    "null",                         // Null
    "package",                      // Package
    "private",                      // Private
    "protected",                    // Protected
    "public",                       // Public
    "return",                       // Return
    "static",                       // Static
    "super",                        // Super
    "switch",                       // Switch
    "synchronized",                 // Synchronized
    "this",                         // This
    "throw",                        // Throw
    "throws",                       // Throws
    "transient",                    // Transient
    "true",                         // True
    "try",                          // Try
    "typeof",                       // Typeof
    "use",                          // Use
    "var",                          // Var
    "void",                         // Void
    "volatile",                     // Volatile
    "while",                        // While
    "with",                         // With

// Non-reserved words
    "ecmascript",                   // Ecmascript
    "eval",                         // Eval
    "exclude",                      // Exclude
    "get",                          // Get
    "include",                      // Include
    "javascript",                   // Javascript
    "named",                        // Named
    "set",                          // Set
    "strict"                        // Strict
};


static const uchar nonreserved = 1<<JS::Token::isNonreserved;
static const uchar followAttr = 1<<JS::Token::canFollowAttribute;
static const uchar isAttr = 1<<JS::Token::isAttribute | followAttr;
static const uchar isNEAttr = 1<<JS::Token::isNonExpressionAttribute | isAttr;
static const uchar followRet = 1<<JS::Token::canFollowReturn;

const uchar JS::Token::kindFlags[kindsEnd] = {
    // Special
    followRet,            // end
    0,                    // number
    0,                    // string
    0,                    // unit
    0,                    // regExp

    // Punctuators
    0,                    // openParenthesis
    0,                    // closeParenthesis
    0,                    // openBracket
    0,                    // closeBracket
    followAttr,           // openBrace
    followRet,            // closeBrace
    0,                    // comma
    followRet,            // semicolon
    0,                    // dot
    0,                    // doubleDot
    0,                    // tripleDot
    0,                    // arrow
    0,                    // colon
    0,                    // doubleColon
    0,                    // pound
    0,                    // at
    0,                    // increment
    0,                    // decrement
    0,                    // complement
    0,                    // logicalNot
    0,                    // times
    0,                    // divide
    0,                    // modulo
    0,                    // plus
    0,                    // minus
    0,                    // leftShift
    0,                    // rightShift
    0,                    // logicalRightShift
    0,                    // logicalAnd
    0,                    // logicalXor
    0,                    // logicalOr
    0,                    // bitwiseAnd
    0,                    // bitwiseXor
    0,                    // bitwiseOr
    0,                    // assignment
    0,                    // timesEquals
    0,                    // divideEquals
    0,                    // moduloEquals
    0,                    // plusEquals
    0,                    // minusEquals
    0,                    // leftShiftEquals
    0,                    // rightShiftEquals
    0,                    // logicalRightShiftEquals
    0,                    // logicalAndEquals
    0,                    // logicalXorEquals
    0,                    // logicalOrEquals
    0,                    // bitwiseAndEquals
    0,                    // bitwiseXorEquals
    0,                    // bitwiseOrEquals
    0,                    // equal
    0,                    // notEqual
    0,                    // lessThan
    0,                    // lessThanOrEqual
    0,                    // greaterThan
    0,                    // greaterThanOrEqual
    0,                    // identical
    0,                    // notIdentical
    0,                    // question

    // Reserved words
    isNEAttr,             // Abstract
    0,                    // As
    0,                    // Break
    0,                    // Case
    0,                    // Catch
    followAttr,           // Class
    followAttr,           // Const
    0,                    // Continue
    0,                    // Debugger
    0,                    // Default
    0,                    // Delete
    0,                    // Do
    followRet,            // Else
    0,                    // Enum
    followAttr,           // Export
    0,                    // Extends
    isAttr,               // False
    isNEAttr,             // Final
    0,                    // Finally
    0,                    // For
    followAttr,           // Function
    0,                    // Goto
    0,                    // If
    0,                    // Implements
    followAttr,           // Import
    0,                    // In
    0,                    // Instanceof
    followAttr,           // Interface
    0,                    // Is
    followAttr,           // Namespace
    followAttr,           // Native
    0,                    // New
    0,                    // Null
    0,                    // Package
    isAttr,               // Private
    followAttr,           // Protected
    isAttr,               // Public
    0,                    // Return
    isNEAttr,             // Static
    0,                    // Super
    0,                    // Switch
    followAttr,           // Synchronized
    0,                    // This
    0,                    // Throw
    0,                    // Throws
    followAttr,           // Transient
    isAttr,               // True
    0,                    // Try
    0,                    // Typeof
    followAttr,           // Use
    followAttr,           // Var
    0,                    // Void
    isNEAttr,             // Volatile
    followRet,            // While
    0,                    // With

    // Non-reserved words
    isAttr|nonreserved,   // Ecmascript
    isAttr|nonreserved,   // Eval
    isAttr|nonreserved,   // Exclude
    isAttr|nonreserved,   // Get
    isAttr|nonreserved,   // Include
    isAttr|nonreserved,   // Javascript
    isAttr|nonreserved,   // Named
    isAttr|nonreserved,   // Set
    isAttr|nonreserved,   // Strict

    isAttr|nonreserved    // identifier
};


// Initialize the keywords in the given world.
void JS::Token::initKeywords(World &world)
{
    const char *const*keywordName = kindNames + keywordsBegin;
    for (Kind kind = keywordsBegin; kind != keywordsEnd; kind = Kind(kind+1)) {
        String s = widenCString(*keywordName++);
        world.identifiers[s].tokenKind = kind;
    }
}


// Print a description of the token to f.
void JS::Token::print(Formatter &f, bool debug) const
{
    switch (getKind()) {
        case end:
            f << "[end]";
            break;

        case number:
            if (debug)
                f << "[number " << /* getValue() <<  */ ']';
            f << getChars();
            break;

        case unit:
            if (debug)
                f << "[unit]";
            /* no break */
        case string:
            quoteString(f, getChars(), '"');
            break;

        case regExp:
            f << '/' << getIdentifier() << '/' << getChars();
            break;

        case identifier:
            if (debug)
                f << "[identifier]";
            f << getIdentifier();
            break;

        default:
            f << getKind();
    }
}
