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
    static final boolean printTokenNames = printTrees || printICode;


    /**
     * Token types.  These values correspond to JSTokenType values in
     * jsscan.c.
     */

    public final static int
    // start enum
        ERROR       = -1, // well-known as the only code < EOF
        EOF         = 0,  // end of file token - (not EOF_CHAR)
        EOL         = 1,  // end of line

        // Beginning here are interpreter bytecodes.
        POPV        = 2,
        ENTERWITH   = 3,
        LEAVEWITH   = 4,
        RETURN      = 5,
        GOTO        = 6,
        IFEQ        = 7,
        IFNE        = 8,
        DUP         = 9,
        SETNAME     = 10,
        BITOR       = 11,
        BITXOR      = 12,
        BITAND      = 13,
        EQ          = 14,
        NE          = 15,
        LT          = 16,
        LE          = 17,
        GT          = 18,
        GE          = 19,
        LSH         = 20,
        RSH         = 21,
        URSH        = 22,
        ADD         = 23,
        SUB         = 24,
        MUL         = 25,
        DIV         = 26,
        MOD         = 27,
        BITNOT      = 28,
        NEG         = 29,
        NEW         = 30,
        DELPROP     = 31,
        TYPEOF      = 32,
        NAMEINC     = 33,
        PROPINC     = 34,
        ELEMINC     = 35,
        NAMEDEC     = 36,
        PROPDEC     = 37,
        ELEMDEC     = 38,
        GETPROP     = 39,
        SETPROP     = 40,
        GETELEM     = 41,
        SETELEM     = 42,
        CALL        = 43,
        NAME        = 44,
        NUMBER      = 45,
        STRING      = 46,
        ZERO        = 47,
        ONE         = 48,
        NULL        = 49,
        THIS        = 50,
        FALSE       = 51,
        TRUE        = 52,
        SHEQ        = 53,   // shallow equality (===)
        SHNE        = 54,   // shallow inequality (!==)
        CLOSURE     = 55,
        REGEXP      = 56,
        POP         = 57,
        POS         = 58,
        VARINC      = 59,
        VARDEC      = 60,
        BINDNAME    = 61,
        THROW       = 62,
        IN          = 63,
        INSTANCEOF  = 64,
        CALLSPECIAL = 65,
        GETTHIS     = 66,
        NEWTEMP     = 67,
        USETEMP     = 68,
        GETBASE     = 69,
        GETVAR      = 70,
        SETVAR      = 71,
        UNDEFINED   = 72,
        TRY         = 73,
        NEWSCOPE    = 74,
        TYPEOFNAME  = 75,
        ENUMINIT    = 76,
        ENUMNEXT    = 77,
        GETPROTO    = 78,
        GETPARENT   = 79,
        SETPROTO    = 80,
        SETPARENT   = 81,
        SCOPE       = 82,
        GETSCOPEPARENT = 83,
        THISFN      = 84,

        // End of interpreter bytecodes
        SEMI        = 85,  // semicolon
        LB          = 86,  // left and right brackets
        RB          = 87,
        LC          = 88,  // left and right curlies (braces)
        RC          = 89,
        LP          = 90,  // left and right parentheses
        RP          = 91,
        COMMA       = 92,  // comma operator
        ASSIGN      = 93, // assignment ops (= += -= etc.)
        HOOK        = 94, // conditional (?:)
        COLON       = 95,
        OR          = 96, // logical or (||)
        AND         = 97, // logical and (&&)
        EQOP        = 98, // equality ops (== !=)
        RELOP       = 99, // relational ops (< <= > >=)
        SHOP        = 100, // shift ops (<< >> >>>)
        UNARYOP     = 101, // unary prefix operator
        INC         = 102, // increment/decrement (++ --)
        DEC         = 103,
        DOT         = 104, // member operator (.)
        PRIMARY     = 105, // true, false, null, this
        FUNCTION    = 106, // function keyword
        EXPORT      = 107, // export keyword
        IMPORT      = 108, // import keyword
        IF          = 109, // if keyword
        ELSE        = 110, // else keyword
        SWITCH      = 111, // switch keyword
        CASE        = 112, // case keyword
        DEFAULT     = 113, // default keyword
        WHILE       = 114, // while keyword
        DO          = 115, // do keyword
        FOR         = 116, // for keyword
        BREAK       = 117, // break keyword
        CONTINUE    = 118, // continue keyword
        VAR         = 119, // var keyword
        WITH        = 120, // with keyword
        CATCH       = 121, // catch keyword
        FINALLY     = 122, // finally keyword
        RESERVED    = 123, // reserved keywords

        /** Added by Mike - these are JSOPs in the jsref, but I
         * don't have them yet in the java implementation...
         * so they go here.  Also whatever I needed.

         * Most of these go in the 'op' field when returning
         * more general token types, eg. 'DIV' as the op of 'ASSIGN'.
         */
        NOP         = 124, // NOP
        NOT         = 125, // etc.
        PRE         = 126, // for INC, DEC nodes.
        POST        = 127,

        /**
         * For JSOPs associated with keywords...
         * eg. op = THIS; token = PRIMARY
         */

        VOID        = 128,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */
        BLOCK       = 129, // statement block
        ARRAYLIT    = 130, // array literal
        OBJLIT      = 131, // object literal
        LABEL       = 132, // label
        TARGET      = 133,
        LOOP        = 134,
        ENUMDONE    = 135,
        EXPRSTMT    = 136,
        PARENT      = 137,
        CONVERT     = 138,
        JSR         = 139,
        NEWLOCAL    = 140,
        USELOCAL    = 141,
        SCRIPT      = 142,   // top-level node for entire script

        LAST_TOKEN  = 142;

    public static String name(int token)
    {
        if (printTokenNames) {
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
                case DUP:             return "dup";
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
                case CALLSPECIAL:     return "callspecial";
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
                case GETPROTO:        return "getproto";
                case GETPARENT:       return "getparent";
                case SETPROTO:        return "setproto";
                case SETPARENT:       return "setparent";
                case SCOPE:           return "scope";
                case GETSCOPEPARENT:  return "getscopeparent";
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
