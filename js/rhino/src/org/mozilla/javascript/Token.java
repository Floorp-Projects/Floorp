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
        ERROR          = -1, // well-known as the only code < EOF
        EOF            = 0,  // end of file token - (not EOF_CHAR)
        EOL            = 1,  // end of line

        // Interpreter reuses the following as bytecodes
        FIRST_BYTECODE_TOKEN    = 2,

        ENTERWITH      = 2,
        LEAVEWITH      = 3,
        RETURN         = 4,
        GOTO           = 5,
        IFEQ           = 6,
        IFNE           = 7,
        SETNAME        = 8,
        BITOR          = 9,
        BITXOR         = 10,
        BITAND         = 11,
        EQ             = 12,
        NE             = 13,
        LT             = 14,
        LE             = 15,
        GT             = 16,
        GE             = 17,
        LSH            = 18,
        RSH            = 19,
        URSH           = 20,
        ADD            = 21,
        SUB            = 22,
        MUL            = 23,
        DIV            = 24,
        MOD            = 25,
        NOT            = 26,
        BITNOT         = 27,
        POS            = 28,
        NEG            = 29,
        NEW            = 30,
        DELPROP        = 31,
        TYPEOF         = 32,
        GETPROP        = 33,
        SETPROP        = 34,
        GETELEM        = 35,
        SETELEM        = 36,
        CALL           = 37,
        NAME           = 38,
        NUMBER         = 39,
        STRING         = 40,
        NULL           = 41,
        THIS           = 42,
        FALSE          = 43,
        TRUE           = 44,
        SHEQ           = 45,   // shallow equality (===)
        SHNE           = 46,   // shallow inequality (!==)
        REGEXP         = 47,
        BINDNAME       = 48,
        THROW          = 49,
        IN             = 50,
        INSTANCEOF     = 51,
        LOCAL_SAVE     = 52,
        LOCAL_LOAD     = 53,
        GETVAR         = 54,
        SETVAR         = 55,
        CATCH_SCOPE    = 56,
        ENUM_INIT      = 57,
        ENUM_NEXT      = 58,
        ENUM_ID        = 59,
        THISFN         = 60,
        RETURN_RESULT  = 61, // to return prevoisly stored return result
        ARRAYLIT       = 62, // array literal
        OBJECTLIT      = 63, // object literal
        GET_REF        = 64, // *reference
        SET_REF        = 65, // *reference    = something
        REF_CALL       = 66, // f(args)    = something or f(args)++
        SPECIAL_REF    = 67, // reference for special properties like __proto
        GENERIC_REF    = 68, // generic reference to generate runtime ref errors

        LAST_BYTECODE_TOKEN    = 68,
        // End of interpreter bytecodes

        TRY            = 69,
        SEMI           = 70,  // semicolon
        LB             = 71,  // left and right brackets
        RB             = 72,
        LC             = 73,  // left and right curlies (braces)
        RC             = 74,
        LP             = 75,  // left and right parentheses
        RP             = 76,
        COMMA          = 77,  // comma operator
        ASSIGN         = 78, // simple assignment  (=)
        ASSIGNOP       = 79, // assignment with operation (+= -= etc.)
        HOOK           = 80, // conditional (?:)
        COLON          = 81,
        OR             = 82, // logical or (||)
        AND            = 83, // logical and (&&)
        INC            = 84, // increment/decrement (++ --)
        DEC            = 85,
        DOT            = 86, // member operator (.)
        FUNCTION       = 87, // function keyword
        EXPORT         = 88, // export keyword
        IMPORT         = 89, // import keyword
        IF             = 90, // if keyword
        ELSE           = 91, // else keyword
        SWITCH         = 92, // switch keyword
        CASE           = 93, // case keyword
        DEFAULT        = 94, // default keyword
        WHILE          = 95, // while keyword
        DO             = 96, // do keyword
        FOR            = 97, // for keyword
        BREAK          = 98, // break keyword
        CONTINUE       = 99, // continue keyword
        VAR            = 100, // var keyword
        WITH           = 101, // with keyword
        CATCH          = 102, // catch keyword
        FINALLY        = 103, // finally keyword
        VOID           = 104, // void keyword
        RESERVED       = 105, // reserved keywords

        EMPTY          = 106,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */

        BLOCK          = 107, // statement block
        LABEL          = 108, // label
        TARGET         = 109,
        LOOP           = 110,
        EXPR_VOID      = 111, // expression statement in functions
        EXPR_RESULT    = 112, // expression statement in scripts
        JSR            = 113,
        SCRIPT         = 114, // top-level node for entire script
        TYPEOFNAME     = 115, // for typeof(simple-name)
        USE_STACK      = 116,
        SETPROP_OP     = 117, // x.y op= something
        SETELEM_OP     = 118, // x[y] op= something
        LOCAL_BLOCK    = 119,
        SET_REF_OP     = 120, // *reference op= something

        LAST_TOKEN     = 120;

    public static String name(int token)
    {
        if (!(-1 <= token && token <= LAST_TOKEN)) {
            throw new IllegalArgumentException(String.valueOf(token));
        }

        if (!printNames) {
            return String.valueOf(token);
        }

        switch (token) {
          case ERROR:           return "ERROR";
          case EOF:             return "EOF";
          case EOL:             return "EOL";
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
          case NULL:            return "NULL";
          case THIS:            return "THIS";
          case FALSE:           return "FALSE";
          case TRUE:            return "TRUE";
          case SHEQ:            return "SHEQ";
          case SHNE:            return "SHNE";
          case REGEXP:          return "OBJECT";
          case BINDNAME:        return "BINDNAME";
          case THROW:           return "THROW";
          case IN:              return "IN";
          case INSTANCEOF:      return "INSTANCEOF";
          case LOCAL_SAVE:      return "LOCAL_SAVE";
          case LOCAL_LOAD:      return "LOCAL_LOAD";
          case GETVAR:          return "GETVAR";
          case SETVAR:          return "SETVAR";
          case CATCH_SCOPE:     return "CATCH_SCOPE";
          case ENUM_INIT:       return "ENUM_INIT";
          case ENUM_NEXT:       return "ENUM_NEXT";
          case ENUM_ID:         return "ENUM_ID";
          case THISFN:          return "THISFN";
          case RETURN_RESULT:   return "RETURN_RESULT";
          case ARRAYLIT:        return "ARRAYLIT";
          case OBJECTLIT:       return "OBJECTLIT";
          case GET_REF:         return "GET_REF";
          case SET_REF:         return "SET_REF";
          case REF_CALL:        return "REF_CALL";
          case SPECIAL_REF:     return "SPECIAL_REF";
          case GENERIC_REF:     return "GENERIC_REF";
          case TRY:             return "TRY";
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
          case LABEL:           return "LABEL";
          case TARGET:          return "TARGET";
          case LOOP:            return "LOOP";
          case EXPR_VOID:       return "EXPR_VOID";
          case EXPR_RESULT:     return "EXPR_RESULT";
          case JSR:             return "JSR";
          case SCRIPT:          return "SCRIPT";
          case TYPEOFNAME:      return "TYPEOFNAME";
          case USE_STACK:       return "USE_STACK";
          case SETPROP_OP:      return "SETPROP_OP";
          case SETELEM_OP:      return "SETELEM_OP";
          case LOCAL_BLOCK:     return "LOCAL_BLOCK";
          case SET_REF_OP:      return "SET_REF_OP";
        }

        // Token without name
        throw new IllegalStateException(String.valueOf(token));
    }
}
