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
 * Milen Nankov
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
        ENUM_INIT_KEYS = 57,
        ENUM_INIT_VALUES = 58,
        ENUM_NEXT      = 59,
        ENUM_ID        = 60,
        THISFN         = 61,
        RETURN_RESULT  = 62, // to return prevoisly stored return result
        ARRAYLIT       = 63, // array literal
        OBJECTLIT      = 64, // object literal
        GET_REF        = 65, // *reference
        SET_REF        = 66, // *reference    = something
        DEL_REF        = 67, // delete reference
        REF_CALL       = 68, // f(args)    = something or f(args)++
        SPECIAL_REF    = 69, // reference for special properties like __proto
        GENERIC_REF    = 70, // generic reference to generate runtime ref errors

        // For XML support:
        DEFAULTNAMESPACE = 71, // default xml namespace =
        COLONCOLON     = 72, // ::
        ESCXMLATTR     = 73,
        ESCXMLTEXT     = 74,
        TOATTRNAME     = 75,
        DESCENDANTS    = 76,
        XML_REF        = 77,

        LAST_BYTECODE_TOKEN    = 77,
        // End of interpreter bytecodes

        TRY            = 78,
        SEMI           = 79,  // semicolon
        LB             = 80,  // left and right brackets
        RB             = 81,
        LC             = 82,  // left and right curlies (braces)
        RC             = 83,
        LP             = 84,  // left and right parentheses
        RP             = 85,
        COMMA          = 86,  // comma operator
        ASSIGN         = 87, // simple assignment  (=)
        ASSIGNOP       = 88, // assignment with operation (+= -= etc.)
        HOOK           = 89, // conditional (?:)
        COLON          = 90,
        OR             = 91, // logical or (||)
        AND            = 92, // logical and (&&)
        INC            = 93, // increment/decrement (++ --)
        DEC            = 94,
        DOT            = 95, // member operator (.)
        FUNCTION       = 96, // function keyword
        EXPORT         = 97, // export keyword
        IMPORT         = 98, // import keyword
        IF             = 99, // if keyword
        ELSE           = 100, // else keyword
        SWITCH         = 101, // switch keyword
        CASE           = 102, // case keyword
        DEFAULT        = 103, // default keyword
        WHILE          = 104, // while keyword
        DO             = 105, // do keyword
        FOR            = 106, // for keyword
        BREAK          = 107, // break keyword
        CONTINUE       = 108, // continue keyword
        VAR            = 109, // var keyword
        WITH           = 110, // with keyword
        CATCH          = 111, // catch keyword
        FINALLY        = 112, // finally keyword
        VOID           = 113, // void keyword
        RESERVED       = 114, // reserved keywords

        EMPTY          = 115,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */

        BLOCK          = 116, // statement block
        LABEL          = 117, // label
        TARGET         = 118,
        LOOP           = 119,
        EXPR_VOID      = 120, // expression statement in functions
        EXPR_RESULT    = 121, // expression statement in scripts
        JSR            = 122,
        SCRIPT         = 123, // top-level node for entire script
        TYPEOFNAME     = 124, // for typeof(simple-name)
        USE_STACK      = 125,
        SETPROP_OP     = 126, // x.y op= something
        SETELEM_OP     = 127, // x[y] op= something
        LOCAL_BLOCK    = 128,
        SET_REF_OP     = 129, // *reference op= something

        // For XML support:
        DOTDOT         = 130,  // member operator (..)
        XML            = 131,  // XML type
        DOTQUERY       = 132,  // .() -- e.g., x.emps.emp.(name == "terry")
        XMLATTR        = 133,  // @
        XMLEND         = 134,

        LAST_TOKEN     = 134;

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
          case ENUM_INIT_KEYS:  return "ENUM_INIT_KEYS";
          case ENUM_INIT_VALUES:  return "ENUM_INIT_VALUES";
          case ENUM_NEXT:       return "ENUM_NEXT";
          case ENUM_ID:         return "ENUM_ID";
          case THISFN:          return "THISFN";
          case RETURN_RESULT:   return "RETURN_RESULT";
          case ARRAYLIT:        return "ARRAYLIT";
          case OBJECTLIT:       return "OBJECTLIT";
          case GET_REF:         return "GET_REF";
          case SET_REF:         return "SET_REF";
          case DEL_REF:         return "DEL_REF";
          case REF_CALL:        return "REF_CALL";
          case SPECIAL_REF:     return "SPECIAL_REF";
          case GENERIC_REF:     return "GENERIC_REF";
          case DEFAULTNAMESPACE:return "DEFAULTNAMESPACE";
          case COLONCOLON:      return "COLONCOLON";
          case ESCXMLTEXT:      return "ESCXMLTEXT";
          case ESCXMLATTR:      return "ESCXMLATTR";
          case TOATTRNAME:      return "TOATTRNAME";
          case DESCENDANTS:     return "DESCENDANTS";
          case XML_REF:         return "XML_REF";
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
          case DOTDOT:          return "DOTDOT";
          case XML:             return "XML";
          case DOTQUERY:        return "DOTQUERY";
          case XMLATTR:         return "XMLATTR";
          case XMLEND:          return "XMLEND";
        }

        // Token without name
        throw new IllegalStateException(String.valueOf(token));
    }
}
