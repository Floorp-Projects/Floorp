/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Roger Lawrence
 * Mike McCabe
 * Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript;

import java.io.*;

/**
 * This class implements the JavaScript scanner.
 *
 * It is based on the C source files jsscan.c and jsscan.h
 * in the jsref package.
 *
 * @see org.mozilla.javascript.Parser
 *
 * @author Mike McCabe
 * @author Brendan Eich
 */

public class Token
{

    // debug flags
    static final boolean printTrees = false;
    static final boolean printICode = false;
    static final boolean printNames = printTrees || printICode;


    /**
     * Token types.  These values correspond to JSTokenType values in
     * jsscan.c.
     */

    public final static int
    // start enum
        ERROR       = -1, // well-known as the only code < EOF
        EOF         = 0,  // end of file token - (not EOF_CHAR)
        EOL         = 1,  // end of line

        // Interpreter reuses the following as bytecodes
        FIRST_BYTECODE_TOKEN = 2,

        POPV        = 2,
        ENTERWITH   = 3,
        LEAVEWITH   = 4,
        RETURN      = 5,
        GOTO        = 6,
        IFEQ        = 7,
        IFNE        = 8,
        SETNAME     = 9,
        BITOR       = 10,
        BITXOR      = 11,
        BITAND      = 12,
        EQ          = 13,
        NE          = 14,
        LT          = 15,
        LE          = 16,
        GT          = 17,
        GE          = 18,
        LSH         = 19,
        RSH         = 20,
        URSH        = 21,
        ADD         = 22,
        SUB         = 23,
        MUL         = 24,
        DIV         = 25,
        MOD         = 26,
        BITNOT      = 27,
        NEG         = 28,
        NEW         = 29,
        DELPROP     = 30,
        TYPEOF      = 31,
        GETPROP     = 32,
        SETPROP     = 33,
        GETELEM     = 34,
        SETELEM     = 35,
        CALL        = 36,
        NAME        = 37,
        NUMBER      = 38,
        STRING      = 39,
        ZERO        = 40,
        ONE         = 41,
        NULL        = 42,
        THIS        = 43,
        FALSE       = 44,
        TRUE        = 45,
        SHEQ        = 46,   // shallow equality (===)
        SHNE        = 47,   // shallow inequality (!==)
        REGEXP      = 48,
        POP         = 49,
        POS         = 50,
        BINDNAME    = 51,
        THROW       = 52,
        IN          = 53,
        INSTANCEOF  = 54,
        GETTHIS     = 55,
        NEWTEMP     = 56,
        USETEMP     = 57,
        GETBASE     = 58,
        GETVAR      = 59,
        SETVAR      = 60,
        UNDEFINED   = 61,
        NEWSCOPE    = 62,
        ENUMINIT    = 63,
        ENUMNEXT    = 64,
        THISFN      = 65,

        LAST_BYTECODE_TOKEN = 65,
        // End of interpreter bytecodes

        TRY         = 66,
        SEMI        = 67,  // semicolon
        LB          = 68,  // left and right brackets
        RB          = 69,
        LC          = 70,  // left and right curlies (braces)
        RC          = 71,
        LP          = 72,  // left and right parentheses
        RP          = 73,
        COMMA       = 74,  // comma operator
        ASSIGN      = 75, // assignment ops (= += -= etc.)
        HOOK        = 76, // conditional (?:)
        COLON       = 77,
        OR          = 78, // logical or (||)
        AND         = 79, // logical and (&&)
        EQOP        = 80, // equality ops (== !=)
        RELOP       = 81, // relational ops (< <= > >=)
        SHOP        = 82, // shift ops (<< >> >>>)
        UNARYOP     = 83, // unary prefix operator
        INC         = 84, // increment/decrement (++ --)
        DEC         = 85,
        DOT         = 86, // member operator (.)
        PRIMARY     = 87, // true, false, null, this
        FUNCTION    = 88, // function keyword
        EXPORT      = 89, // export keyword
        IMPORT      = 90, // import keyword
        IF          = 91, // if keyword
        ELSE        = 92, // else keyword
        SWITCH      = 93, // switch keyword
        CASE        = 94, // case keyword
        DEFAULT     = 95, // default keyword
        WHILE       = 96, // while keyword
        DO          = 97, // do keyword
        FOR         = 98, // for keyword
        BREAK       = 99, // break keyword
        CONTINUE    = 100, // continue keyword
        VAR         = 101, // var keyword
        WITH        = 102, // with keyword
        CATCH       = 103, // catch keyword
        FINALLY     = 104, // finally keyword
        RESERVED    = 105, // reserved keywords

        /** Added by Mike - these are JSOPs in the jsref, but I
         * don't have them yet in the java implementation...
         * so they go here.  Also whatever I needed.

         * Most of these go in the 'op' field when returning
         * more general token types, eg. 'DIV' as the op of 'ASSIGN'.
         */
        NOP         = 106, // NOP
        NOT         = 107, // etc.
        PRE         = 108, // for INC, DEC nodes.
        POST        = 109,

        /**
         * For JSOPs associated with keywords...
         * eg. op = THIS; token = PRIMARY
         */

        VOID        = 110,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */
        BLOCK       = 111, // statement block
        ARRAYLIT    = 112, // array literal
        OBJLIT      = 113, // object literal
        LABEL       = 114, // label
        TARGET      = 115,
        LOOP        = 116,
        ENUMDONE    = 117,
        EXPRSTMT    = 118,
        PARENT      = 119,
        CONVERT     = 120,
        JSR         = 121,
        NEWLOCAL    = 122,
        USELOCAL    = 123,
        SCRIPT      = 124,   // top-level node for entire script

        LAST_TOKEN  = 124;

    public static String name(int token)
    {
        if (printNames) {
            switch (token) {
                case ERROR:           return "error";
                case EOF:             return "eof";
                case EOL:             return "eol";
                case POPV:            return "popv";
                case ENTERWITH:       return "enterwith";
                case LEAVEWITH:       return "leavewith";
                case RETURN:          return "return";
                case GOTO:            return "goto";
                case IFEQ:            return "ifeq";
                case IFNE:            return "ifne";
                case SETNAME:         return "setname";
                case BITOR:           return "bitor";
                case BITXOR:          return "bitxor";
                case BITAND:          return "bitand";
                case EQ:              return "eq";
                case NE:              return "ne";
                case LT:              return "lt";
                case LE:              return "le";
                case GT:              return "gt";
                case GE:              return "ge";
                case LSH:             return "lsh";
                case RSH:             return "rsh";
                case URSH:            return "ursh";
                case ADD:             return "add";
                case SUB:             return "sub";
                case MUL:             return "mul";
                case DIV:             return "div";
                case MOD:             return "mod";
                case BITNOT:          return "bitnot";
                case NEG:             return "neg";
                case NEW:             return "new";
                case DELPROP:         return "delprop";
                case TYPEOF:          return "typeof";
                case GETPROP:         return "getprop";
                case SETPROP:         return "setprop";
                case GETELEM:         return "getelem";
                case SETELEM:         return "setelem";
                case CALL:            return "call";
                case NAME:            return "name";
                case NUMBER:          return "number";
                case STRING:          return "string";
                case ZERO:            return "zero";
                case ONE:             return "one";
                case NULL:            return "null";
                case THIS:            return "this";
                case FALSE:           return "false";
                case TRUE:            return "true";
                case SHEQ:            return "sheq";
                case SHNE:            return "shne";
                case REGEXP:          return "object";
                case POP:             return "pop";
                case POS:             return "pos";
                case BINDNAME:        return "bindname";
                case THROW:           return "throw";
                case IN:              return "in";
                case INSTANCEOF:      return "instanceof";
                case GETTHIS:         return "getthis";
                case NEWTEMP:         return "newtemp";
                case USETEMP:         return "usetemp";
                case GETBASE:         return "getbase";
                case GETVAR:          return "getvar";
                case SETVAR:          return "setvar";
                case UNDEFINED:       return "undefined";
                case TRY:             return "try";
                case NEWSCOPE:        return "newscope";
                case ENUMINIT:        return "enuminit";
                case ENUMNEXT:        return "enumnext";
                case THISFN:          return "thisfn";
                case SEMI:            return "semi";
                case LB:              return "lb";
                case RB:              return "rb";
                case LC:              return "lc";
                case RC:              return "rc";
                case LP:              return "lp";
                case RP:              return "rp";
                case COMMA:           return "comma";
                case ASSIGN:          return "assign";
                case HOOK:            return "hook";
                case COLON:           return "colon";
                case OR:              return "or";
                case AND:             return "and";
                case EQOP:            return "eqop";
                case RELOP:           return "relop";
                case SHOP:            return "shop";
                case UNARYOP:         return "unaryop";
                case INC:             return "inc";
                case DEC:             return "dec";
                case DOT:             return "dot";
                case PRIMARY:         return "primary";
                case FUNCTION:        return "function";
                case EXPORT:          return "export";
                case IMPORT:          return "import";
                case IF:              return "if";
                case ELSE:            return "else";
                case SWITCH:          return "switch";
                case CASE:            return "case";
                case DEFAULT:         return "default";
                case WHILE:           return "while";
                case DO:              return "do";
                case FOR:             return "for";
                case BREAK:           return "break";
                case CONTINUE:        return "continue";
                case VAR:             return "var";
                case WITH:            return "with";
                case CATCH:           return "catch";
                case FINALLY:         return "finally";
                case RESERVED:        return "reserved";
                case NOP:             return "nop";
                case NOT:             return "not";
                case PRE:             return "pre";
                case POST:            return "post";
                case VOID:            return "void";
                case BLOCK:           return "block";
                case ARRAYLIT:        return "arraylit";
                case OBJLIT:          return "objlit";
                case LABEL:           return "label";
                case TARGET:          return "target";
                case LOOP:            return "loop";
                case ENUMDONE:        return "enumdone";
                case EXPRSTMT:        return "exprstmt";
                case PARENT:          return "parent";
                case CONVERT:         return "convert";
                case JSR:             return "jsr";
                case NEWLOCAL:        return "newlocal";
                case USELOCAL:        return "uselocal";
                case SCRIPT:          return "script";
            }
            return "<unknown="+token+">";
        }
        return "";
    }
}
