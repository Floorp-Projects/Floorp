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
    public static final boolean printTrees = false;
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
        NOT         = 27,
        BITNOT      = 28,
        POS         = 29,
        NEG         = 30,
        NEW         = 31,
        DELPROP     = 32,
        TYPEOF      = 33,
        GETPROP     = 34,
        SETPROP     = 35,
        GETELEM     = 36,
        SETELEM     = 37,
        CALL        = 38,
        NAME        = 39,
        NUMBER      = 40,
        STRING      = 41,
        ZERO        = 42,
        ONE         = 43,
        NULL        = 44,
        THIS        = 45,
        FALSE       = 46,
        TRUE        = 47,
        SHEQ        = 48,   // shallow equality (===)
        SHNE        = 49,   // shallow inequality (!==)
        REGEXP      = 50,
        POP         = 51,
        BINDNAME    = 52,
        THROW       = 53,
        IN          = 54,
        INSTANCEOF  = 55,
        LOCAL_SAVE  = 56,
        LOCAL_LOAD  = 57,
        GETVAR      = 58,
        SETVAR      = 59,
        UNDEFINED   = 60,
        CATCH_SCOPE = 61,
        ENUM_INIT   = 62,
        ENUM_NEXT   = 63,
        ENUM_ID     = 64,
        THISFN      = 65,
        RETURN_POPV = 66, // to return result stored as popv in functions

        LAST_BYTECODE_TOKEN = 66,
        // End of interpreter bytecodes

        TRY         = 67,
        SEMI        = 68,  // semicolon
        LB          = 69,  // left and right brackets
        RB          = 70,
        LC          = 71,  // left and right curlies (braces)
        RC          = 72,
        LP          = 73,  // left and right parentheses
        RP          = 74,
        COMMA       = 75,  // comma operator
        ASSIGN      = 76, // simple assignment  (=)
        ASSIGNOP    = 77, // assignment with operation (+= -= etc.)
        HOOK        = 78, // conditional (?:)
        COLON       = 79,
        OR          = 80, // logical or (||)
        AND         = 81, // logical and (&&)
        INC         = 82, // increment/decrement (++ --)
        DEC         = 83,
        DOT         = 84, // member operator (.)
        FUNCTION    = 85, // function keyword
        EXPORT      = 86, // export keyword
        IMPORT      = 87, // import keyword
        IF          = 88, // if keyword
        ELSE        = 89, // else keyword
        SWITCH      = 90, // switch keyword
        CASE        = 91, // case keyword
        DEFAULT     = 92, // default keyword
        WHILE       = 93, // while keyword
        DO          = 94, // do keyword
        FOR         = 95, // for keyword
        BREAK       = 96, // break keyword
        CONTINUE    = 97, // continue keyword
        VAR         = 98, // var keyword
        WITH        = 99, // with keyword
        CATCH       = 100, // catch keyword
        FINALLY     = 101, // finally keyword
        VOID        = 102, // void keyword
        RESERVED    = 103, // reserved keywords

        EMPTY       = 104,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */

        BLOCK       = 105, // statement block
        ARRAYLIT    = 106, // array literal
        OBJLIT      = 107, // object literal
        LABEL       = 108, // label
        TARGET      = 109,
        LOOP        = 110,
        EXPRSTMT    = 111,
        JSR         = 112,
        SCRIPT      = 113,   // top-level node for entire script
        TYPEOFNAME  = 114,  // for typeof(simple-name)
        USE_STACK   = 115,
        SETPROP_OP  = 116, // x.y op= something
        SETELEM_OP  = 117, // x[y] op= something
        INIT_LIST   = 118,
        LOCAL_BLOCK = 119,

        LAST_TOKEN  = 119;

    public static String name(int token)
    {
        if (printNames) {
            switch (token) {
                case ERROR:           return "ERROR";
                case EOF:             return "EOF";
                case EOL:             return "EOL";
                case POPV:            return "POPV";
                case ENTERWITH:       return "ENTERWITH";
                case LEAVEWITH:       return "LEAVEWITH";
                case RETURN:          return "RETURN";
                case GOTO:            return "GOTO";
                case IFEQ:            return "IFEQ";
                case IFNE:            return "IFNE";
                case SETNAME:         return "SETNAME";
                case BITOR:           return "BITOR";
                case BITXOR:          return "BITXOR";
                case BITAND:          return "BITAND";
                case EQ:              return "EQ";
                case NE:              return "NE";
                case LT:              return "LT";
                case LE:              return "LE";
                case GT:              return "GT";
                case GE:              return "GE";
                case LSH:             return "LSH";
                case RSH:             return "RSH";
                case URSH:            return "URSH";
                case ADD:             return "ADD";
                case SUB:             return "SUB";
                case MUL:             return "MUL";
                case DIV:             return "DIV";
                case MOD:             return "MOD";
                case NOT:             return "NOT";
                case BITNOT:          return "BITNOT";
                case POS:             return "POS";
                case NEG:             return "NEG";
                case NEW:             return "NEW";
                case DELPROP:         return "DELPROP";
                case TYPEOF:          return "TYPEOF";
                case GETPROP:         return "GETPROP";
                case SETPROP:         return "SETPROP";
                case GETELEM:         return "GETELEM";
                case SETELEM:         return "SETELEM";
                case CALL:            return "CALL";
                case NAME:            return "NAME";
                case NUMBER:          return "NUMBER";
                case STRING:          return "STRING";
                case ZERO:            return "ZERO";
                case ONE:             return "ONE";
                case NULL:            return "NULL";
                case THIS:            return "THIS";
                case FALSE:           return "FALSE";
                case TRUE:            return "TRUE";
                case SHEQ:            return "SHEQ";
                case SHNE:            return "SHNE";
                case REGEXP:          return "OBJECT";
                case POP:             return "POP";
                case BINDNAME:        return "BINDNAME";
                case THROW:           return "THROW";
                case IN:              return "IN";
                case INSTANCEOF:      return "INSTANCEOF";
                case LOCAL_SAVE:      return "LOCAL_SAVE";
                case LOCAL_LOAD:      return "LOCAL_LOAD";
                case GETVAR:          return "GETVAR";
                case SETVAR:          return "SETVAR";
                case UNDEFINED:       return "UNDEFINED";
                case TRY:             return "TRY";
                case CATCH_SCOPE:     return "CATCH_SCOPE";
                case ENUM_INIT:       return "ENUM_INIT";
                case ENUM_NEXT:       return "ENUM_NEXT";
                case ENUM_ID:         return "ENUM_ID";
                case THISFN:          return "THISFN";
                case RETURN_POPV:     return "RETURN_POPV";
                case SEMI:            return "SEMI";
                case LB:              return "LB";
                case RB:              return "RB";
                case LC:              return "LC";
                case RC:              return "RC";
                case LP:              return "LP";
                case RP:              return "RP";
                case COMMA:           return "COMMA";
                case ASSIGN:          return "ASSIGN";
                case ASSIGNOP:        return "ASSIGNOP";
                case HOOK:            return "HOOK";
                case COLON:           return "COLON";
                case OR:              return "OR";
                case AND:             return "AND";
                case INC:             return "INC";
                case DEC:             return "DEC";
                case DOT:             return "DOT";
                case FUNCTION:        return "FUNCTION";
                case EXPORT:          return "EXPORT";
                case IMPORT:          return "IMPORT";
                case IF:              return "IF";
                case ELSE:            return "ELSE";
                case SWITCH:          return "SWITCH";
                case CASE:            return "CASE";
                case DEFAULT:         return "DEFAULT";
                case WHILE:           return "WHILE";
                case DO:              return "DO";
                case FOR:             return "FOR";
                case BREAK:           return "BREAK";
                case CONTINUE:        return "CONTINUE";
                case VAR:             return "VAR";
                case WITH:            return "WITH";
                case CATCH:           return "CATCH";
                case FINALLY:         return "FINALLY";
                case RESERVED:        return "RESERVED";
                case EMPTY:           return "EMPTY";
                case BLOCK:           return "BLOCK";
                case ARRAYLIT:        return "ARRAYLIT";
                case OBJLIT:          return "OBJLIT";
                case LABEL:           return "LABEL";
                case TARGET:          return "TARGET";
                case LOOP:            return "LOOP";
                case EXPRSTMT:        return "EXPRSTMT";
                case JSR:             return "JSR";
                case SCRIPT:          return "SCRIPT";
                case TYPEOFNAME:      return "TYPEOFNAME";
                case USE_STACK:       return "USE_STACK";
                case SETPROP_OP:      return "SETPROP_OP";
                case SETELEM_OP:      return "SETELEM_OP";
                case INIT_LIST:       return "INIT_LIST";
                case LOCAL_BLOCK:     return "LOCAL_BLOCK";
            }
            return "<unknown="+token+">";
        }
        return null;
    }
}
