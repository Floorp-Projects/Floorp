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
        NAMEINC     = 32,
        PROPINC     = 33,
        ELEMINC     = 34,
        NAMEDEC     = 35,
        PROPDEC     = 36,
        ELEMDEC     = 37,
        GETPROP     = 38,
        SETPROP     = 39,
        GETELEM     = 40,
        SETELEM     = 41,
        CALL        = 42,
        NAME        = 43,
        NUMBER      = 44,
        STRING      = 45,
        ZERO        = 46,
        ONE         = 47,
        NULL        = 48,
        THIS        = 49,
        FALSE       = 50,
        TRUE        = 51,
        SHEQ        = 52,   // shallow equality (===)
        SHNE        = 53,   // shallow inequality (!==)
        CLOSURE     = 54,
        REGEXP      = 55,
        POP         = 56,
        POS         = 57,
        VARINC      = 58,
        VARDEC      = 59,
        BINDNAME    = 60,
        THROW       = 61,
        IN          = 62,
        INSTANCEOF  = 63,
        GETTHIS     = 64,
        NEWTEMP     = 65,
        USETEMP     = 66,
        GETBASE     = 67,
        GETVAR      = 68,
        SETVAR      = 69,
        UNDEFINED   = 70,
        TRY         = 71,
        NEWSCOPE    = 72,
        TYPEOFNAME  = 73,
        ENUMINIT    = 74,
        ENUMNEXT    = 75,
        THISFN      = 76,

        LAST_BYTECODE_TOKEN = 76,
        // End of interpreter bytecodes

        SEMI        = 77,  // semicolon
        LB          = 78,  // left and right brackets
        RB          = 79,
        LC          = 80,  // left and right curlies (braces)
        RC          = 81,
        LP          = 82,  // left and right parentheses
        RP          = 83,
        COMMA       = 84,  // comma operator
        ASSIGN      = 85, // assignment ops (= += -= etc.)
        HOOK        = 86, // conditional (?:)
        COLON       = 87,
        OR          = 88, // logical or (||)
        AND         = 89, // logical and (&&)
        EQOP        = 90, // equality ops (== !=)
        RELOP       = 91, // relational ops (< <= > >=)
        SHOP        = 92, // shift ops (<< >> >>>)
        UNARYOP     = 93, // unary prefix operator
        INC         = 94, // increment/decrement (++ --)
        DEC         = 95,
        DOT         = 96, // member operator (.)
        PRIMARY     = 97, // true, false, null, this
        FUNCTION    = 98, // function keyword
        EXPORT      = 99, // export keyword
        IMPORT      = 100, // import keyword
        IF          = 101, // if keyword
        ELSE        = 102, // else keyword
        SWITCH      = 103, // switch keyword
        CASE        = 104, // case keyword
        DEFAULT     = 105, // default keyword
        WHILE       = 106, // while keyword
        DO          = 107, // do keyword
        FOR         = 108, // for keyword
        BREAK       = 109, // break keyword
        CONTINUE    = 110, // continue keyword
        VAR         = 111, // var keyword
        WITH        = 112, // with keyword
        CATCH       = 113, // catch keyword
        FINALLY     = 114, // finally keyword
        RESERVED    = 115, // reserved keywords

        /** Added by Mike - these are JSOPs in the jsref, but I
         * don't have them yet in the java implementation...
         * so they go here.  Also whatever I needed.

         * Most of these go in the 'op' field when returning
         * more general token types, eg. 'DIV' as the op of 'ASSIGN'.
         */
        NOP         = 116, // NOP
        NOT         = 117, // etc.
        PRE         = 118, // for INC, DEC nodes.
        POST        = 119,

        /**
         * For JSOPs associated with keywords...
         * eg. op = THIS; token = PRIMARY
         */

        VOID        = 120,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */
        BLOCK       = 121, // statement block
        ARRAYLIT    = 122, // array literal
        OBJLIT      = 123, // object literal
        LABEL       = 124, // label
        TARGET      = 125,
        LOOP        = 126,
        ENUMDONE    = 127,
        EXPRSTMT    = 128,
        PARENT      = 129,
        CONVERT     = 130,
        JSR         = 131,
        NEWLOCAL    = 132,
        USELOCAL    = 133,
        SCRIPT      = 134,   // top-level node for entire script

        LAST_TOKEN  = 134;

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
                case NAMEINC:         return "nameinc";
                case PROPINC:         return "propinc";
                case ELEMINC:         return "eleminc";
                case NAMEDEC:         return "namedec";
                case PROPDEC:         return "propdec";
                case ELEMDEC:         return "elemdec";
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
                case CLOSURE:         return "closure";
                case REGEXP:          return "object";
                case POP:             return "pop";
                case POS:             return "pos";
                case VARINC:          return "varinc";
                case VARDEC:          return "vardec";
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
                case TYPEOFNAME:      return "typeofname";
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
